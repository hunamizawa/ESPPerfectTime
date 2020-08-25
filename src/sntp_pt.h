#ifndef ESPPERFECTTIME_SNTP_H_
#define ESPPERFECTTIME_SNTP_H_

#include <sys/time.h>
#include <time.h>
#ifdef ESP8266
#include <coredecls.h>
#include <os_type.h>
#include <osapi.h>
#endif // ESP8266
#include "ESPPerfectTime.h"
#include <arch/cc.h>

/** Set this to 1 to allow config of SNTP server(s) by DNS name */
#ifndef SNTP_SERVER_DNS
#define SNTP_SERVER_DNS        1
#endif

#define USECS_IN_SEC           1000000

#define LI_ntoa(x) (                                     \
  x == LI_NO_WARNING         ? "LI_NO_WARNING" :         \
  x == LI_LAST_MINUTE_61_SEC ? "LI_LAST_MINUTE_61_SEC" : \
  x == LI_LAST_MINUTE_59_SEC ? "LI_LAST_MINUTE_59_SEC" : \
  x == LI_ALARM_CONDITION    ? "LI_ALARM_CONDITION" :    \
                               "UNDEFINED_VALUE"         \
)

namespace pftime_sntp {

/**
 * Set SNTP sync callback
 */
void setsynccallback(pftime::sync_callback_t);

/**
 * Set SNTP fail callback
 */
void setfailcallback(pftime::fail_callback_t);

/**
 * Initialize this module.
 * Send out request instantly or after SNTP_STARTUP_DELAY(_FUNC).
 */
void init(void);
/**
 * Stop this module.
 */
void stop(void);
#if SNTP_SERVER_DNS
/**
 * Initialize one of the NTP servers by name
 *
 * @param numdns the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param dnsserver DNS name of the NTP server to set, to be resolved at contact time
 */
void setservername(u8_t idx, const char *server);
/**
 * Obtain one of the currently configured by name NTP servers.
 *
 * @param numdns the index of the NTP server
 * @return IP address of the indexed NTP server or NULL if the NTP
 *         server has not been configured by name (or at all)
 */
const char *getservername(u8_t idx);
#endif /* SNTP_SERVER_DNS */
} // namespace pftime_sntp

#endif // ESPPERFECTTIME_SNTP_H_
