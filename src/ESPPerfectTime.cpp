#include <Arduino.h>
#include <lwip/apps/sntp.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#ifdef ESP8266
#include <sntp-lwip2.h>
#endif // ESP8266
#ifdef ESP32
#include <esp32-hal.h>
#endif // ESP32
#include "ESPPerfectTime.h"
#include <sntp_pt.h>

#define EPOCH_YEAR                   1970
#define TIME_2020_01_01_00_00_00_UTC 1577836800
#define SECS_PER_MIN                 60
#define SECS_PER_HOUR                3600
#define SECS_PER_DAY                 86400

#define IS_LEAP_YEAR(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#define TO_TM_YEAR(m)   ((m) - 1900)
#define TO_TM_MONTH(m)  ((m) - 1)

static const int _ydays[2][13] = {
  {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
  {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

static uint8_t   _leap_indicator = LI_NO_WARNING;
static time_t    _leap_time      = 0; // The second of the end of the month -- 23:59:59 in UTC
static struct tm _tm_result;

uint8_t pftime::getLeapIndicator() {
  if (_leap_indicator == LI_ALARM_CONDITION)
    return LI_ALARM_CONDITION;

  struct timeval tv;
  ::gettimeofday(&tv, nullptr);

  // Actually _leap_indicator doesn't change just after leaping (it will change the next syncing)
  if ((_leap_indicator == LI_LAST_MINUTE_61_SEC && tv.tv_sec > _leap_time + 1) || (_leap_indicator == LI_LAST_MINUTE_59_SEC && tv.tv_sec >= _leap_time))
    return LI_NO_WARNING;

  return _leap_indicator;
}

static void adjustLeapSec(time_t *t) {
  if (_leap_indicator == LI_LAST_MINUTE_61_SEC && *t > _leap_time)
    (*t)--;
  else if (_leap_indicator == LI_LAST_MINUTE_59_SEC && *t >= _leap_time)
    (*t)++;
}

static time_t mkgmtime(tm *tm) {
  size_t is_leap_year = IS_LEAP_YEAR(tm->tm_year) ? 1 : 0;
  tm->tm_yday         = _ydays[is_leap_year][tm->tm_mon] + tm->tm_mday - 1;

  int    epoch_year;
  time_t secs_from_epoch;
  if (tm->tm_year < TO_TM_YEAR(2020)) {
    epoch_year      = EPOCH_YEAR;
    secs_from_epoch = 0;
  } else {
    // If tm is after 2020-01-01, use pre-calculated UNIX time to be more faster
    epoch_year      = 2020;
    secs_from_epoch = TIME_2020_01_01_00_00_00_UTC;
  }

  int days = 0;
  for (int y = epoch_year; TO_TM_YEAR(y) < tm->tm_year; y++)
    days += IS_LEAP_YEAR(y) ? 366 : 365;
  days += tm->tm_yday;

  secs_from_epoch += days       * SECS_PER_DAY
                  + tm->tm_hour * SECS_PER_HOUR
                  + tm->tm_min  * SECS_PER_MIN
                  + tm->tm_sec;

  return secs_from_epoch;
}

static time_t calcNextLeapPoint(const time_t current) {
  struct tm now       = *::gmtime(&current);
  struct tm next_leap = {0};
  if (now.tm_mon == TO_TM_MONTH(12)) {
    next_leap.tm_year = now.tm_year + 1;
    next_leap.tm_mon  = TO_TM_MONTH(1);
  } else {
    next_leap.tm_year = now.tm_year;
    next_leap.tm_mon  = now.tm_mon + 1;
  }
  next_leap.tm_mday = 1;

  return mkgmtime(&next_leap) - 1;
}

time_t pftime::time(time_t *timer) {
#ifdef ESP8266
  // time() implemented in ESP8266 core is incorrect
  // so we use gettimeofday() instead of time()
  // see: https://github.com/esp8266/Arduino/issues/4637
  struct timeval tv;
  pftime::gettimeofday(&tv, nullptr);
  if (timer)
    *timer = tv.tv_sec;
  return tv.tv_sec;
#else
  // ESP32's time() is correct
  time_t t = ::time(nullptr);
  adjustLeapSec(&t);
  if (timer)
    *timer = t;
  return t;
#endif
}

#define DEFINE_FUNC_FOOTIME(name)                                                  \
  struct tm *pftime::name(const time_t *timer, suseconds_t *res_usec) {            \
    if (timer)                                                                     \
      return ::name(timer);                                                        \
                                                                                   \
    struct timeval tv;                                                             \
    ::gettimeofday(&tv, nullptr);                                                  \
                                                                                   \
    if (res_usec)                                                                  \
      *res_usec = tv.tv_usec;                                                      \
                                                                                   \
    if (_leap_indicator == LI_LAST_MINUTE_61_SEC && tv.tv_sec == _leap_time + 1) { \
      _tm_result        = *::name(&_leap_time);                                    \
      _tm_result.tm_sec = 60;                                                      \
      return &_tm_result;                                                          \
    }                                                                              \
                                                                                   \
    adjustLeapSec(&tv.tv_sec);                                                     \
    return ::name(&tv.tv_sec);                                                     \
  }

DEFINE_FUNC_FOOTIME(gmtime);
DEFINE_FUNC_FOOTIME(localtime);

int pftime::gettimeofday(struct timeval *tv, struct timezone *unused) {
  (void)unused;

  if (tv) {
    ::gettimeofday(tv, nullptr);
    adjustLeapSec(&tv->tv_sec);
  }
  return 0;
}

int pftime::settimeofday(const struct timeval *tv, const struct timezone *unused, uint8_t li) {
  (void)unused;
  
  if (tv) {
    int result      = ::settimeofday(tv, nullptr);
    _leap_indicator = li;
    if (li != LI_NO_WARNING) {
      _leap_time = calcNextLeapPoint(tv->tv_sec);
      //Serial.printf("Leap second will insert/delete after %d\n", _leap_time);
    }
    return result;
  }
  return 1;
}

#ifdef ESP32
static void setTZ(const char *tz) {

  char tzram[strlen_P(tz) + 1];
  memcpy_P(tzram, tz, sizeof(tzram));
  setenv("TZ", tz, 1);
  tzset();
}
#endif

// from esp32-hal-time.c
static void setTimeZone(long offset, int daylight) {
  using std::abs;

  char cst[17] = {0};
  char cdt[17] = "DST";
  char tz[33]  = {0};

  if (offset % 3600) {
    sprintf(cst, "UTC%ld:%02ld:%02ld", offset / 3600, abs((offset % 3600) / 60), abs(offset % 60));
  } else {
    sprintf(cst, "UTC%ld", offset / 3600);
  }
  if (daylight != 3600) {
    long tz_dst = offset - daylight;
    if (tz_dst % 3600) {
      sprintf(cdt, "DST%ld:%02ld:%02ld", tz_dst / 3600, abs((tz_dst % 3600) / 60), abs(tz_dst % 60));
    } else {
      sprintf(cdt, "DST%ld", tz_dst / 3600);
    }
  }
  sprintf(tz, "%s%s", cst, cdt);
  setTZ(tz);
}

/*
 * configTime
 * Source: https://github.com/esp8266/Arduino/blob/master/cores/esp8266/time.c
 * */
void pftime::configTime(long gmtOffset_sec, int daylightOffset_sec, const char *server1, const char *server2, const char *server3) {
  if (sntp_enabled())
    sntp_stop();
  pftime_sntp::stop();

  //pftime_sntp::setoperatingmode(SNTP_OPMODE_POLL);
  pftime_sntp::setservername(0, server1);
  pftime_sntp::setservername(1, server2);
  pftime_sntp::setservername(2, server3);

  setTimeZone(-gmtOffset_sec, daylightOffset_sec);

  pftime_sntp::init();
}

#ifdef ESP8266
void pftime::configTime(const char *tz, const char *server1, const char *server2, const char *server3) {
  pftime::configTzTime(tz, server1, server2, server3);
}
#endif

void pftime::configTzTime(const char *tz, const char *server1, const char *server2, const char *server3) {
  if (sntp_enabled())
    sntp_stop();
  pftime_sntp::stop();

  pftime_sntp::setservername(0, server1);
  pftime_sntp::setservername(1, server2);
  pftime_sntp::setservername(2, server3);

  setTZ(tz);

  pftime_sntp::init();
}

void pftime::setSyncSuccessCallback(sync_callback_t cb) {
  pftime_sntp::setsynccallback(cb);
}

void pftime::setSyncFailCallback(fail_callback_t cb) {
  pftime_sntp::setfailcallback(cb);
}
