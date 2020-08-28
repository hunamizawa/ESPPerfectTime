/*
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Simon Goldschmidt (lwIP raw API part)
 */

#include <Arduino.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#ifdef ESP8266
#include <coredecls.h>
#include <os_type.h>
#include <osapi.h>
#endif // ESP8266
#include <arch/cc.h>
#include <lwip/def.h>
#include <lwip/dns.h>
#include <lwip/err.h>
#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <lwip/timeouts.h>
#include <lwip/udp.h>
#ifdef ESP8266
#include <sntp-lwip2.h>
#endif // ESP8266
#include "ESPPerfectTime.h"
#include <sntp_pt.h>

#ifndef os_memset
#define os_memset(s, c, n) memset(s, c, n)
#endif

#ifndef S64_F
#define S64_F "lld"
#endif

// (for ESP8266) defines ESP32-compatible logging functions
#if defined(DEBUG_ESP_PORT)
#define PFTIME_DEBUG_LOG(letter, format, ...) DEBUG_ESP_PORT.printf("[" #letter "][sntp_pt.cpp:%u] %s(): " format "\n", __LINE__, __FUNCTION__, ##__VA_ARGS__)
#else
#define PFTIME_DEBUG_LOG(letter, format, ...)
#endif

#ifndef log_v
#define log_v(format, ...) PFTIME_DEBUG_LOG(V, format, ##__VA_ARGS__)
#endif

#ifndef log_d
#define log_d(format, ...) PFTIME_DEBUG_LOG(D, format, ##__VA_ARGS__)
#endif

#ifndef log_w
#define log_w(format, ...) PFTIME_DEBUG_LOG(W, format, ##__VA_ARGS__)
#endif

#ifndef log_e
#define log_e(format, ...) PFTIME_DEBUG_LOG(E, format, ##__VA_ARGS__)
#endif

#ifndef log_n
#define log_n(format, ...) PFTIME_DEBUG_LOG(N, format, ##__VA_ARGS__)
#endif

/** SNTP server port */
#ifndef SNTP_PORT
#define SNTP_PORT                   123
#endif

#ifndef SNTP_MAX_SERVERS
#define SNTP_MAX_SERVERS            3
#endif

/** Handle support for more than one server via NTP_MAX_SERVERS,
 * but catch legacy style of setting SNTP_SUPPORT_MULTIPLE_SERVERS, probably outside of this file
 */
#ifndef SNTP_SUPPORT_MULTIPLE_SERVERS
#if SNTP_MAX_SERVERS > 1
#define SNTP_SUPPORT_MULTIPLE_SERVERS 1
#else /* SNTP_MAX_SERVERS <= 1 */
#define SNTP_SUPPORT_MULTIPLE_SERVERS 0
#endif /* SNTP_MAX_SERVERS > 1 */
#else  /* !SNTP_SUPPORT_MULTIPLE_SERVERS */
/* The developer has defined SNTP_SUPPORT_MULTIPLE_SERVERS, probably from old code */
#if SNTP_MAX_SERVERS <= 1
#error "SNTP_MAX_SERVERS needs to be defined to the max amount of servers if SNTP_SUPPORT_MULTIPLE_SERVERS is defined"
#endif /* SNTP_MAX_SERVERS <= 1 */
#endif /* SNTP_SUPPORT_MULTIPLE_SERVERS */

/** Sanity check:
 * Define this to
 * - 0 to turn off sanity checks (default; smaller code)
 * - >= 1 to check address and port of the response packet to ensure the
 *        response comes from the server we sent the request to.
 * - >= 2 to check returned Originate Timestamp against Transmit Timestamp
 *        sent to the server (to ensure response to older request).
 * - >= 3 @todo: discard reply if any of the LI, Stratum, or Transmit Timestamp
 *        fields is 0 or the Mode field is not 4 (unicast) or 5 (broadcast).
 * - >= 4 @todo: to check that the Root Delay and Root Dispersion fields are each
 *        greater than or equal to 0 and less than infinity, where infinity is
 *        currently a cozy number like one second. This check avoids using a
 *        server whose synchronization source has expired for a very long time.
 */
#ifndef SNTP_CHECK_RESPONSE
#define SNTP_CHECK_RESPONSE         2
#endif

/** SNTP receive timeout - in milliseconds
 * Also used as retry timeout - this shouldn't be too low.
 * Default is 3 seconds.
 */
#ifndef SNTP_RECV_TIMEOUT
#define SNTP_RECV_TIMEOUT           3000
#endif

/** SNTP update delay - in milliseconds
 * Default is 1 hour.
 */
#ifndef SNTP_UPDATE_DELAY
#define SNTP_UPDATE_DELAY           3600000
#endif
// #if (SNTP_UPDATE_DELAY < 15000) && !SNTP_SUPPRESS_DELAY_CHECK
// #error "SNTPv4 RFC 4330 enforces a minimum update time of 15 seconds!"
// #endif

/** Default retry timeout (in milliseconds) if the response
 * received is invalid.
 * This is doubled with each retry until SNTP_RETRY_TIMEOUT_MAX is reached.
 */
#ifndef SNTP_RETRY_TIMEOUT
#define SNTP_RETRY_TIMEOUT          SNTP_RECV_TIMEOUT
#endif

/** Maximum retry timeout (in milliseconds). */
#ifndef SNTP_RETRY_TIMEOUT_MAX
#define SNTP_RETRY_TIMEOUT_MAX      (SNTP_RETRY_TIMEOUT * 10)
#endif

/** Increase retry timeout with every retry sent
 * Default is on to conform to RFC.
 */
#ifndef SNTP_RETRY_TIMEOUT_EXP
#define SNTP_RETRY_TIMEOUT_EXP      1
#endif

#define SNTP_ERR_KOD                1

/* SNTP protocol defines */
#define SNTP_MSG_LEN                48

#define SNTP_OFFSET_LI_VN_MODE      0
#define SNTP_LI_MASK                0xC0

#define SNTP_VERSION_MASK           0x38
#define SNTP_VERSION                (4/* NTP Version 4*/ << 3) 

#define SNTP_MODE_MASK              0x07
#define SNTP_MODE_CLIENT            0x03
#define SNTP_MODE_SERVER            0x04
#define SNTP_MODE_BROADCAST         0x05

#define SNTP_OFFSET_STRATUM         1
#define SNTP_STRATUM_KOD            0x00

#define SNTP_OFFSET_ORIGINATE_TIME  24
#define SNTP_OFFSET_RECEIVE_TIME    32
#define SNTP_OFFSET_TRANSMIT_TIME   40

#define SNTP_RECEIVE_TIME_SIZE      2

/* number of seconds between 1900 and 1970 (MSB=1)*/
#define DIFF_SEC_1900_1970          2208988800
/* number of seconds between 1970 and Feb 7, 2036 (6:28:16 UTC) (MSB=0) */
#define DIFF_SEC_1970_2036          2085978496

#define UNIXSEC_TO_NTPSEC(sec)      (((sec) >= DIFF_SEC_1970_2036) ? ((sec) - DIFF_SEC_1970_2036) : ((sec) + DIFF_SEC_1900_1970))
//#define NTPSEC_TO_UNIXSEC(sec)     ((((sec) & 0x80000000) == 0) ? ((sec) + DIFF_SEC_1970_2036) : ((sec) - DIFF_SEC_1900_1970))
#define COMBINE_TO_USEC(sec, us)    ((sec) * USECS_IN_SEC + (us))
#define SEPARATE_USEC(us)           (u32_t)((us) / USECS_IN_SEC), (u32_t)((us) % USECS_IN_SEC)

#ifdef ESP32
typedef uint8_t  uint8;
typedef int8_t   sint8;
typedef uint16_t uint16;
typedef int16_t  sint16;
typedef uint32_t uint32;
typedef int32_t  sint32;
#endif
typedef uint64_t u64_t;
typedef int64_t  s64_t;

namespace pftime_sntp {

/**
 * SNTP packet format (without optional fields)
 * Timestamps are coded as 64 bits:
 * - 32 bits seconds since Jan 01, 1970, 00:00
 * - 32 bits seconds fraction (0-padded)
 * For future use, if the MSB in the seconds part is set, seconds are based
 * on Feb 07, 2036, 06:28:16.
 */
#ifdef PACK_STRUCT_USE_INCLUDES
#include <arch/bpstruct.h>
#endif
PACK_STRUCT_BEGIN
#ifndef PACK_STRUCT_FLD_8
#define PACK_STRUCT_FLD_8(x) PACK_STRUCT_FIELD(x)
#endif // PACK_STRUCT_FLD_8
struct sntp_msg {
  PACK_STRUCT_FLD_8(u8_t  li_vn_mode);
  PACK_STRUCT_FLD_8(u8_t  stratum);
  PACK_STRUCT_FLD_8(u8_t  poll);
  PACK_STRUCT_FLD_8(u8_t  precision);
  PACK_STRUCT_FIELD(u32_t root_delay);
  PACK_STRUCT_FIELD(u32_t root_dispersion);
  PACK_STRUCT_FIELD(u32_t reference_identifier);
  PACK_STRUCT_FIELD(u32_t reference_timestamp[2]);
  PACK_STRUCT_FIELD(u32_t originate_timestamp[2]);
  PACK_STRUCT_FIELD(u32_t receive_timestamp[2]);
  PACK_STRUCT_FIELD(u32_t transmit_timestamp[2]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#include <arch/bpstruct.h>
#endif

/* function prototypes */
static void request(void *arg);

/** The UDP pcb used by the SNTP client */
static struct udp_pcb *_sntp_pcb;

/** Names/Addresses of servers */
struct sntp_server {
#if SNTP_SERVER_DNS
  const char *name;
#endif /* SNTP_SERVER_DNS */
  ip_addr_t addr;
};
static struct sntp_server _servers[SNTP_MAX_SERVERS];

#if SNTP_GET_SERVERS_FROM_DHCP
static u8_t _set_servers_from_dhcp;
#endif
#if SNTP_SUPPORT_MULTIPLE_SERVERS
/** The currently used server (initialized to 0) */
static u8_t _current_server;
#else /* SNTP_SUPPORT_MULTIPLE_SERVERS */
#define _current_server 0
#endif /* SNTP_SUPPORT_MULTIPLE_SERVERS */

#if SNTP_RETRY_TIMEOUT_EXP
#define SNTP_RESET_RETRY_TIMEOUT() _retry_timeout = SNTP_RETRY_TIMEOUT
/** Retry time, initialized with SNTP_RETRY_TIMEOUT and doubled with each retry. */
static u32_t _retry_timeout;
#else /* SNTP_RETRY_TIMEOUT_EXP */
#define SNTP_RESET_RETRY_TIMEOUT()
#define _retry_timeout SNTP_RETRY_TIMEOUT
#endif /* SNTP_RETRY_TIMEOUT_EXP */

#if SNTP_CHECK_RESPONSE >= 1
/** Saves the last server address to compare with response */
static ip_addr_t _last_server_address;
#endif /* SNTP_CHECK_RESPONSE >= 1 */

#if SNTP_CHECK_RESPONSE >= 2
/** Saves the last timestamp sent (which is sent back by the server)
 * to compare against in response */
static u32_t _last_timestamp_sent[2];
#endif /* SNTP_CHECK_RESPONSE >= 2 */

static uint32 _update_delay = SNTP_UPDATE_DELAY;

static pftime::sync_callback_t _cb;
static pftime::fail_callback_t _failcb;

static void ICACHE_FLASH_ATTR
set_system_time_us(const u32_t sec, const u32_t us, const u8_t li) {
  struct timeval tv = {(time_t)sec, (suseconds_t)us};
  pftime::settimeofday(&tv, nullptr, li);
}

static void ICACHE_FLASH_ATTR
get_system_time_us(u32_t *sec, u32_t *us) {
  struct timeval tv;
  ::gettimeofday(&tv, nullptr);
  *sec = (u32_t)tv.tv_sec;
  *us  = (u32_t)tv.tv_usec;
}

/* convert SNTP time (1900-based) to unix GMT time (1970-based)
  * if MSB is 0, SNTP time is 2036-based!
  */
static u32_t ICACHE_FLASH_ATTR
sntpsec_to_unixsec(const u32_t ntpsec) {
  u32_t sec = ntohl(ntpsec);
  return (sec & 0x80000000) == 0 ? sec + DIFF_SEC_1970_2036 : sec - DIFF_SEC_1900_1970;
}

/**
 * SNTP processing of received timestamp
 */
static void ICACHE_FLASH_ATTR
process(u32_t *originate_timestamp, u32_t *receive_timestamp, u32_t *transmit_timestamp, u8_t li) {
  if (originate_timestamp == nullptr || receive_timestamp == nullptr) {
    u32_t sec = sntpsec_to_unixsec(transmit_timestamp[0]);
    u32_t us  = ntohl(transmit_timestamp[1]) / 4295;
    set_system_time_us(sec, us, li);
    log_d("time = %d.%06d, LI = %s", sec, us, LI_ntoa(li));
    if (_cb != nullptr) {
    	_cb();
    }
    return;
  }

  /* no need to convert for originate timestamp (see initialize_request) */
  s64_t orig_sec = (s64_t)originate_timestamp[0];
  s64_t orig_us  = (s64_t)originate_timestamp[1];
  s64_t orig     = COMBINE_TO_USEC(orig_sec, orig_us);

  s64_t tx_sec   = (s64_t)sntpsec_to_unixsec(transmit_timestamp[0]);
  s64_t tx_us    = (s64_t)(ntohl(transmit_timestamp[1]) / 4295);
  s64_t tx       = COMBINE_TO_USEC(tx_sec, tx_us);
  
  s64_t rx_sec   = (s64_t)sntpsec_to_unixsec(receive_timestamp[0]);
  s64_t rx_us    = (s64_t)(ntohl(receive_timestamp[1]) / 4295);
  s64_t rx       = COMBINE_TO_USEC(rx_sec, rx_us);
  
  u32_t now_sec, now_us;
  get_system_time_us(&now_sec, &now_us);
  s64_t now = COMBINE_TO_USEC((s64_t)now_sec, (s64_t)now_us);

  s64_t toffset  = ((rx + tx) - (orig + now)) >> 1; /* x / 2 == x >> 1 */
  s64_t true_now = now + toffset;
  set_system_time_us(SEPARATE_USEC(true_now), li);
  /* display local time from GMT time */
  log_d("time = %d.%06d, LI = %s", SEPARATE_USEC(true_now), LI_ntoa(li));
  log_d("RTT  = %" S64_F " us, ", (now - orig) - (tx - rx));
  if (_cb != nullptr) {
  	_cb();
  }
}

/**
 * Initialize request struct to be sent to server.
 */
static void ICACHE_FLASH_ATTR
initialize_request(struct sntp_msg *req) {
  os_memset(req, 0, SNTP_MSG_LEN);
  req->li_vn_mode = LI_NO_WARNING | SNTP_VERSION | SNTP_MODE_CLIENT;

  u32_t sntp_time_sec, sntp_time_us;
  /* fill in transmit timestamp */
  get_system_time_us(&sntp_time_sec, &sntp_time_us);
  /* we send/save us instead of fraction, and don't convert endian to be faster... */
  req->transmit_timestamp[0] = sntp_time_sec;
  req->transmit_timestamp[1] = sntp_time_us;

#if SNTP_CHECK_RESPONSE >= 2
  /* save transmit timestamp in '_last_timestamp_sent' */
  _last_timestamp_sent[0] = req->transmit_timestamp[0];
  _last_timestamp_sent[1] = req->transmit_timestamp[1];
#endif /* SNTP_CHECK_RESPONSE >= 2 */
}

/**
 * Retry: send a new request (and increase retry timeout).
 *
 * @param arg is unused (only necessary to conform to sys_timeout)
 */
static void ICACHE_FLASH_ATTR
retry(void *arg) {
  LWIP_UNUSED_ARG(arg);

  log_v("Next request will be sent in %" U32_F " ms",
    _retry_timeout);

  /* set up a timer to send a retry and increase the retry delay */
  sys_timeout(_retry_timeout, request, nullptr);

#if SNTP_RETRY_TIMEOUT_EXP
  {
    u32_t new_retry_timeout;
    /* increase the timeout for next retry */
    new_retry_timeout = _retry_timeout << 1;
    /* limit to maximum timeout and prevent overflow */
    if ((new_retry_timeout <= SNTP_RETRY_TIMEOUT_MAX) &&
        (new_retry_timeout > _retry_timeout)) {
      _retry_timeout = new_retry_timeout;
    }
  }
#endif /* SNTP_RETRY_TIMEOUT_EXP */
}

#if SNTP_SUPPORT_MULTIPLE_SERVERS
/**
 * If Kiss-of-Death is received (or another packet parsing error),
 * try the next server or retry the current server and increase the retry
 * timeout if only one server is available.
 * (implicitly, SNTP_MAX_SERVERS > 1)
 *
 * @param arg is unused (only necessary to conform to sys_timeout)
 */
static void ICACHE_FLASH_ATTR
try_next_server(void *arg) {
  u8_t old_server, i;
  LWIP_UNUSED_ARG(arg);

  old_server = _current_server;
  for (i = 0; i < SNTP_MAX_SERVERS - 1; i++) {
    _current_server++;
    if (_current_server >= SNTP_MAX_SERVERS) {
      _current_server = 0;
    }
    if (!ip_addr_isany(&_servers[_current_server].addr)
#if SNTP_SERVER_DNS
        || (_servers[_current_server].name != nullptr)
#endif
        ) {
      log_v("Sending request to server %" U16_F,
        (u16_t)_current_server);
      /* new server: reset retry timeout */
      SNTP_RESET_RETRY_TIMEOUT();
      /* instantly send a request to the next server */
      request(nullptr);
      return;
    }
  }
  /* no other valid server found */
  _current_server = old_server;
  retry(nullptr);
}
#else /* SNTP_SUPPORT_MULTIPLE_SERVERS */
/* Always retry on error if only one server is supported */
#define try_next_server    retry
#endif /* SNTP_SUPPORT_MULTIPLE_SERVERS */

err_t recv_check(struct pbuf *p, const ip_addr_t *addr, const u16_t port,
                 u8_t  *li,
                 u8_t  *mode,
                 u32_t *originate_timestamp,
                 u32_t *receive_timestamp,
                 u32_t *transmit_timestamp) {

  u8_t stratum;

#if SNTP_CHECK_RESPONSE >= 1
  /* check server address and port */
  if (!(ip_addr_cmp(addr, &_last_server_address)) || (port != SNTP_PORT)) {
    log_w("Invalid server address or port");
	if (_failcb != nullptr) {
		_failcb("Invalid server address or port");
	}
    return ERR_ARG;
  }
#else  /* SNTP_CHECK_RESPONSE < 1 */
  LWIP_UNUSED_ARG(addr);
  LWIP_UNUSED_ARG(port);
#endif /* SNTP_CHECK_RESPONSE >= 1 */

  /* process the response */
  if (p->tot_len < SNTP_MSG_LEN) {
    log_w("Invalid packet length: %" U16_F, p->tot_len);
	if (_failcb != nullptr) {
		_failcb("Invalid packet length");
	}
    return ERR_ARG;
  }
  
  pbuf_copy_partial(p, mode, 1, SNTP_OFFSET_LI_VN_MODE);
  *li = (*mode & SNTP_LI_MASK) >> 6;
  *mode &= SNTP_MODE_MASK;
  /* check SNTP mode */
  if ((*mode != SNTP_MODE_SERVER) &&
      (*mode != SNTP_MODE_BROADCAST)) {
    log_w("Invalid mode in response: %" U16_F, (u16_t)*mode);
	if (_failcb != nullptr) {
		_failcb("Invalid mode in response");
	}
    return ERR_ARG;
  }

  /* check stratum and LI */
  pbuf_copy_partial(p, &stratum, 1, SNTP_OFFSET_STRATUM);
  if (stratum == SNTP_STRATUM_KOD) {
    /* Kiss-of-death packet. Use another server or increase UPDATE_DELAY. */
    log_v("Received Kiss-of-Death");
	if (_failcb != nullptr) {
		_failcb("Kiss of death");
	}
    return SNTP_ERR_KOD;
  }
  if (*li == LI_ALARM_CONDITION) {
    /* LI indicates alarm condition. Use another server or increase UPDATE_DELAY. */
    log_v("Received LI_ALARM_CONDITION");
	if (_failcb != nullptr) {
		_failcb("Received LI_ALARM_CONDITION");
	}
    return SNTP_ERR_KOD;
  }

  if (*mode == SNTP_MODE_SERVER) {
    pbuf_copy_partial(p, originate_timestamp, 8, SNTP_OFFSET_ORIGINATE_TIME);
#if SNTP_CHECK_RESPONSE >= 2
    /* check originate_timetamp against _last_timestamp_sent */
    if ((originate_timestamp[0] != _last_timestamp_sent[0]) ||
        (originate_timestamp[1] != _last_timestamp_sent[1])) {
      log_w("Invalid originate timestamp in response");
	if (_failcb != nullptr) {
		_failcb("Invalid originate timestamp in response");
	}
      return ERR_ARG;
    }
#endif /* SNTP_CHECK_RESPONSE >= 2 */
    /* @todo: add code for SNTP_CHECK_RESPONSE >= 3 and >= 4 here */

    /* correct answer */
    pbuf_copy_partial(p, receive_timestamp, SNTP_RECEIVE_TIME_SIZE * 4, SNTP_OFFSET_RECEIVE_TIME);
  }
  pbuf_copy_partial(p, transmit_timestamp, SNTP_RECEIVE_TIME_SIZE * 4, SNTP_OFFSET_TRANSMIT_TIME);
  return ERR_OK;
}

/** UDP recv callback for the sntp pcb */
static void ICACHE_FLASH_ATTR
recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  u8_t  li, mode;
  u32_t originate_timestamp[2];
  u32_t receive_timestamp  [SNTP_RECEIVE_TIME_SIZE];
  u32_t transmit_timestamp [SNTP_RECEIVE_TIME_SIZE];
//os_printf("recv\n");
  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(pcb);

  /* packet received: stop retry timeout  */
  sys_untimeout(try_next_server, nullptr);
  sys_untimeout(request, nullptr);

  err_t err = recv_check(p, addr, port, &li, &mode, originate_timestamp, receive_timestamp, transmit_timestamp);
  pbuf_free(p);
  if (err == ERR_OK) {
    /* Correct response, reset retry timeout */
    SNTP_RESET_RETRY_TIMEOUT();

    if (mode == SNTP_MODE_SERVER)
      process(originate_timestamp, receive_timestamp, transmit_timestamp, li);
    else
      process(nullptr,             nullptr,           transmit_timestamp, li);

    /* Set up timeout for next request */
    sys_timeout((u32_t)_update_delay, request, nullptr);
    log_v("Scheduled next time request: %" U32_F " ms",
      (u32_t)_update_delay);
  } else if (err == SNTP_ERR_KOD) {
    /* Kiss-of-death packet. Use another server or increase UPDATE_DELAY. */
	if (_failcb != nullptr) {
		_failcb("Kiss of death");
	}

    try_next_server(nullptr);
  } else {
    /* another error, try the same server again */
    retry(nullptr);
  }
}

/** Actually send an sntp request to a server.
 *
 * @param server_addr resolved IP address of the SNTP server
 */
static void ICACHE_FLASH_ATTR
send_request(const ip_addr_t *server_addr) {
  struct pbuf *p;
//  os_printf("send_request\n");
  p = pbuf_alloc(PBUF_TRANSPORT, SNTP_MSG_LEN, PBUF_RAM);
  if (p != nullptr) {
    struct sntp_msg *sntpmsg = (struct sntp_msg *)p->payload;
    log_v("Sending request to server");
    /* initialize request message */
    initialize_request(sntpmsg);
    /* send request */
    udp_sendto(_sntp_pcb, p, server_addr, SNTP_PORT);
    /* free the pbuf after sending it */
    pbuf_free(p);
    /* set up receive timeout: try next server or retry on timeout */
    sys_timeout((u32_t)SNTP_RECV_TIMEOUT, try_next_server, nullptr);
#if SNTP_CHECK_RESPONSE >= 1
    /* save server address to verify it in recv() */ 
    ip_addr_set(&_last_server_address, server_addr);
#endif /* SNTP_CHECK_RESPONSE >= 1 */
  } else {
    log_n("Out of memory, trying again in %" U32_F " ms",
      (u32_t)SNTP_RETRY_TIMEOUT);
    /* out of memory: set up a timer to send a retry */
    sys_timeout((u32_t)SNTP_RETRY_TIMEOUT, request, nullptr);
  }
}

#if SNTP_SERVER_DNS
/**
 * DNS found callback when using DNS names as server address.
 */
static void ICACHE_FLASH_ATTR
dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
  LWIP_UNUSED_ARG(hostname);
  LWIP_UNUSED_ARG(arg);

  if (ipaddr != nullptr) {
    /* Address resolved, send request */
    log_v("Server address resolved, sending request");
    send_request(ipaddr);
  } else {
    /* DNS resolving failed -> try another server */
    log_w("Failed to resolve server address resolved, trying next server");
    try_next_server(nullptr);
  }
}
#endif /* SNTP_SERVER_DNS */

/**
 * Send out an sntp request.
 *
 * @param arg is unused (only necessary to conform to sys_timeout)
 */
static void ICACHE_FLASH_ATTR
request(void *arg) {
  ip_addr_t sntp_server_address;
  err_t     err;

  LWIP_UNUSED_ARG(arg);

  /* initialize SNTP server address */
#if SNTP_SERVER_DNS

  if (_servers[_current_server].name) {
    /* always resolve the name and rely on dns-internal caching & timeout */
    ip_addr_set_any(false, &_servers[_current_server].addr);
    err = dns_gethostbyname(_servers[_current_server].name, &sntp_server_address,
      dns_found, nullptr);
    if (err == ERR_INPROGRESS) {
      /* DNS request sent, wait for dns_found being called */
      log_v("Waiting for server address to be resolved.");
      return;
    } else if (err == ERR_OK) {
      _servers[_current_server].addr = sntp_server_address;
    }
  } else
#endif /* SNTP_SERVER_DNS */
  {
    sntp_server_address = _servers[_current_server].addr;
//    os_printf("sntp_server_address ip %d\n",sntp_server_address.addr);
    err = (ip_addr_isany(&sntp_server_address)) ? ERR_ARG : ERR_OK;
  }

  if (err == ERR_OK) {
    log_d("current server address is %s",
      ipaddr_ntoa(&sntp_server_address));
    send_request(&sntp_server_address);
  } else {
    /* address conversion failed, try another server */
    log_w("Invalid server address, trying next server.");
    sys_timeout((u32_t)SNTP_RETRY_TIMEOUT, try_next_server, nullptr);
  }
}

/**
 * Set a callback to be called after a successful time sync
 */
void ICACHE_FLASH_ATTR
setsynccallback(pftime::sync_callback_t cb) {
  _cb = cb;
}

/**
 * Set a callback to be called after a successful time sync
 */
void ICACHE_FLASH_ATTR
setfailcallback(pftime::fail_callback_t cb) {
  _failcb = cb;
}

/**
 * Initialize this module.
 * Send out request instantly or after SNTP_STARTUP_DELAY(_FUNC).
 */
void ICACHE_FLASH_ATTR
init(void) {
#ifdef SNTP_SERVER_ADDRESS
#if SNTP_SERVER_DNS
  setservername(0, SNTP_SERVER_ADDRESS);
#else
#error SNTP_SERVER_ADDRESS string not supported SNTP_SERVER_DNS==0
#endif
#endif /* SNTP_SERVER_ADDRESS */

  if (_sntp_pcb == nullptr) {
    SNTP_RESET_RETRY_TIMEOUT();
    _sntp_pcb = udp_new();
    LWIP_ASSERT("Failed to allocate udp pcb for sntp client", _sntp_pcb != nullptr);
    if (_sntp_pcb != nullptr) {
      udp_recv(_sntp_pcb, recv, nullptr);
#if SNTP_STARTUP_DELAY
      sys_timeout((u32_t)SNTP_STARTUP_DELAY_FUNC, request, nullptr);
#else
      request(nullptr);
#endif
    }
  }
}

/**
 * Stop this module.
 */
void ICACHE_FLASH_ATTR
stop(void) {
  if (_sntp_pcb != nullptr) {
    sys_untimeout(request, nullptr);
    udp_remove(_sntp_pcb);
    _sntp_pcb = nullptr;
  }
}

#if SNTP_GET_SERVERS_FROM_DHCP
/**
 * Config SNTP server handling by IP address, name, or DHCP; clear table
 * @param set_servers_from_dhcp enable or disable getting server addresses from dhcp
 */
void servermode_dhcp(int set_servers_from_dhcp) {
  u8_t new_mode = set_servers_from_dhcp ? 1 : 0;
  if (_set_servers_from_dhcp != new_mode) {
    _set_servers_from_dhcp = new_mode;
  }
}
#endif /* SNTP_GET_SERVERS_FROM_DHCP */

/**
 * Initialize one of the NTP servers by IP address
 *
 * @param numdns the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param dnsserver IP address of the NTP server to set
 */
void ICACHE_FLASH_ATTR
setserver(u8_t idx, ip_addr_t *server) {
  if (idx < SNTP_MAX_SERVERS) {
    if (server != nullptr) {
      _servers[idx].addr = (*server);
//      os_printf("server ip %d\n",server->addr);
    } else {
      ip_addr_set_any(false, &_servers[idx].addr);
    }
#if SNTP_SERVER_DNS
    _servers[idx].name = nullptr;
#endif
  }
}

#if LWIP_DHCP && SNTP_GET_SERVERS_FROM_DHCP
/**
 * Initialize one of the NTP servers by IP address, required by DHCP
 *
 * @param numdns the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param dnsserver IP address of the NTP server to set
 */
void dhcp_set_ntp_servers(u8_t num, ip_addr_t *server) {
  log_d("%s %s as NTP server #%u via DHCP",
    (_set_servers_from_dhcp ? "Got" : "Rejected"),
    ipaddr_ntoa(&server), num);
  if (_set_servers_from_dhcp && num) {
    u8_t i;
    for (i = 0; (i < num) && (i < SNTP_MAX_SERVERS); i++) {
      setserver(i, &server[i]);
    }
    for (i = num; i < SNTP_MAX_SERVERS; i++) {
      setserver(i, nullptr);
    }
  }
}
#endif /* LWIP_DHCP && SNTP_GET_SERVERS_FROM_DHCP */

/**
 * Obtain one of the currently configured by IP address (or DHCP) NTP servers 
 *
 * @param numdns the index of the NTP server
 * @return IP address of the indexed NTP server or "ip_addr_any" if the NTP
 *         server has not been configured by address (or at all).
 */
ip_addr_t ICACHE_FLASH_ATTR
getserver(u8_t idx) {
  if (idx < SNTP_MAX_SERVERS) {
    return _servers[idx].addr;
  }
  return *IP_ADDR_ANY;
}

#if SNTP_SERVER_DNS
/**
 * Initialize one of the NTP servers by name
 *
 * @param numdns the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param dnsserver DNS name of the NTP server to set, to be resolved at contact time
 */
void ICACHE_FLASH_ATTR
setservername(u8_t idx, const char *server) {
  if (idx < SNTP_MAX_SERVERS) {
    _servers[idx].name = server;
  }
}

/**
 * Obtain one of the currently configured by name NTP servers.
 *
 * @param numdns the index of the NTP server
 * @return IP address of the indexed NTP server or nullptr if the NTP
 *         server has not been configured by name (or at all)
 */
const char * ICACHE_FLASH_ATTR
getservername(u8_t idx) {
  if (idx < SNTP_MAX_SERVERS) {
    return _servers[idx].name;
  }
  return nullptr;
}
#endif /* SNTP_SERVER_DNS */

void ICACHE_FLASH_ATTR
set_update_delay(uint32 ms) {
	_update_delay = ms > 15000 ? ms : 15000;
}

} // namespace pftime_sntp

#undef PFTIME_DEBUG_LOG