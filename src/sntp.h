#ifndef ESPPERFECTTIME_SNTP_H_
#define ESPPERFECTTIME_SNTP_H_

#include <time.h>
#include <sys/time.h>
#ifdef ESP8266
#include <osapi.h>
#include <os_type.h>
#include <coredecls.h>
#endif // ESP8266
#include <arch/cc.h>
#include "ESPPerfectTime.h"

/** Set this to 1 to allow config of SNTP server(s) by DNS name */
#ifndef SNTP_SERVER_DNS
#define SNTP_SERVER_DNS             1
#endif

#define USECS_IN_SEC           1000000

#define LI_NO_WARNING          0x00
#define LI_LAST_MINUTE_61_SEC  0x01
#define LI_LAST_MINUTE_59_SEC  0x02
#define LI_ALARM_CONDITION     0x03 /* (clock not synchronized) */

#define LI_ntoa(x) (                                     \
  x == LI_NO_WARNING         ? "LI_NO_WARNING" :         \
  x == LI_LAST_MINUTE_61_SEC ? "LI_LAST_MINUTE_61_SEC" : \
  x == LI_LAST_MINUTE_59_SEC ? "LI_LAST_MINUTE_59_SEC" : \
  x == LI_ALARM_CONDITION    ? "LI_ALARM_CONDITION" :    \
                               "UNDEFINED_VALUE"         \
)

namespace pftime_sntp {
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
  void setservername(u8_t idx, char *server);
  /**
   * Obtain one of the currently configured by name NTP servers.
   *
   * @param numdns the index of the NTP server
   * @return IP address of the indexed NTP server or NULL if the NTP
   *         server has not been configured by name (or at all)
   */
  char *getservername(u8_t idx);
#endif /* SNTP_SERVER_DNS */
}

#endif // ESPPERFECTTIME_SNTP_H_