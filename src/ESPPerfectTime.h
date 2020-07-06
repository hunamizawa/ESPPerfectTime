#ifndef ESPPERFECTTIME_H_
#define ESPPERFECTTIME_H_

#include <sys/time.h>
#include <time.h>

namespace pftime {
time_t     time(time_t *timer);
struct tm *gmtime   (const time_t *timer, suseconds_t *res_usec = nullptr);
struct tm *localtime(const time_t *timer, suseconds_t *res_usec = nullptr);
int        gettimeofday(struct timeval *tv, struct timezone *unused);
int        settimeofday(const struct timeval *tv, const struct timezone *unused, uint8_t li = 0);

[[deprecated("Use configTzTime(const char *tz, ...). Also, ESP8266 Arduino core (2.7.0 to 2.7.1) has a bug for this function, which can avoid by using configTzTime(const char *tz, ...).")]]
void configTime(long gmtOffset_sec, int daylightOffset_sec, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);
#ifdef ESP8266
void configTime(const char *tz, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);
#endif
/*
  * configTzTime
  * sntp setup using TZ environment variable
  * */
void configTzTime(const char *tz, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);
} // namespace pftime

#endif // ESPPERFECTTIME_H_