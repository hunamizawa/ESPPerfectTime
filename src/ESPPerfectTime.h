/**
 * @file ESPPerfectTime.h
 */

#ifndef ESPPERFECTTIME_H_
#define ESPPERFECTTIME_H_

#include <sys/time.h>
#include <time.h>

//! @brief Time leaping not scheduled
#define LI_NO_WARNING         0x00
//! @brief Last minute has 61 seconds
#define LI_LAST_MINUTE_61_SEC 0x01
//! @brief Last minute has 59 seconds
#define LI_LAST_MINUTE_59_SEC 0x02
//! @brief The NTP server's clock not synchronized
#define LI_ALARM_CONDITION    0x03

namespace pftime {

/**
 * @brief Returns the current calendar time.
 * 
 * @param[out] timer   Pointer to a @c time_t object where the time will be stored (can be null pointer)
 * @retval     -1      When failure
 * @return             The number of seconds since the UNIX Epoch
 */
time_t time(time_t *timer);

/**
 * @brief (1) If @c timer is NOT null pointer, converts given time into calendar time, expressed in UTC, in the <tt>struct tm</tt> format. @c res_usec unused. @n
 *        (2) If @c timer is null pointer, same as (1), expect that the function uses the current time for convert, and also stores microseconds part of the time into @c res_usec (unless it's a null pointer).
 * 
 * @param[in]  timer      Pointer to a @c time_t object for convert
 * @param[out] res_usec   Pointer to a @c suseconds_t object for result
 * @retval     nullptr    When conversion failure
 * @return                Pointer to a internal <tt>static tm</tt> object
 */
struct tm *gmtime(const time_t *timer, suseconds_t *res_usec = nullptr);

/**
 * @brief (1) If @c timer is NOT null pointer, converts given time into calendar time, expressed in local time, in the <tt>struct tm</tt> format. @c res_usec unused. @n
 *        (2) If @c timer is null pointer, same as (1), expect that the function uses the current time for convert, and also stores microseconds part of the time into @c res_usec (unless it's a null pointer).
 * 
 * @param[in]  timer      Pointer to a @c time_t object for convert
 * @param[out] res_usec   Pointer to a @c suseconds_t object for result
 * @retval     nullptr    When conversion failure
 * @return                Pointer to a internal <tt>static tm</tt> object
 */
struct tm *localtime(const time_t *timer, suseconds_t *res_usec = nullptr);

/**
 * @brief Gets the current calendar time, the number of seconds and microseconds since the UNIX Epoch.
 * 
 * @param[out] tv      Pointer to a timeval object for result
 * @param[out] unused  Unused
 * @retval          0  When success
 * @retval         !0  When failure
 */
int gettimeofday(struct timeval *tv, struct timezone *unused);

/**
 * @brief Sets the current calendar time, the number of seconds and microseconds since the UNIX Epoch.
 * 
 * @param[in] tv      Pointer to a timeval object for set
 * @param[in] unused  Unused
 * @param[in] li      Leap indicator flag of SNTP
 * @retval         0  When success
 * @retval        !0  When failure
 */
int settimeofday(const struct timeval *tv, const struct timezone *unused, uint8_t li = 0);

/**
 * @brief Initializes SNTP client with given timezone, and starts it.
 * @deprecated Use configTzTime() instead. It can handles DST automatically.
 * @bug ESP8266 Arduino core (2.7.0 to 2.7.1) has a sign-reversal bug for @c gmtOffset_sec, which can avoid by using configTzTime(). This was fixed in 2.7.2. See: https://github.com/esp8266/Arduino/issues/7319
 * 
 * @param gmtOffset_sec       Local time offset from UTC (in seconds)
 * @param daylightOffset_sec  DST offset (in seconds)
 * @param server1             The primary NTP server address
 * @param server2             The secondary NTP server address (optional)
 * @param server3             The tertiary NTP server address (optional)
 */
[[deprecated("Use configTzTime(const char *tz, ...). Also, ESP8266 Arduino core (2.7.0 to 2.7.1) has a bug for this function, which can avoid by using configTzTime(const char *tz, ...).")]]
void configTime(long gmtOffset_sec, int daylightOffset_sec, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);

#ifdef ESP8266
/**
 * @brief <b>[ESP8266 only]</b> Initializes SNTP client with given timezone, and starts it.
 * 
 * @param tz      A timezone definition, expressed in POSIX-style tz format
 * @param server1 The primary NTP server address
 * @param server2 The secondary NTP server address (optional)
 * @param server3 The tertiary NTP server address (optional)
 */
void configTime(const char *tz, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);
#endif

/**
 * @brief Initializes SNTP client with given timezone, and starts it.
 * 
 * @param tz      A timezone definition, expressed in POSIX-style tz format
 * @param server1 The primary NTP server address
 * @param server2 The secondary NTP server address (optional)
 * @param server3 The tertiary NTP server address (optional)
 */
void configTzTime(const char *tz, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);

/**
 * @brief Get current Leap Indicator value
 */
uint8_t getLeapIndicator();

/**
 * @brief The callback function type for setSyncSuccessCallback().
 */
using sync_callback_t = void (*)();

/**
 * @brief Set the callback called when time syncing completed successfully.
 * 
 * @param cb The callback function as a @c sync_callback_t object
 */
void setSyncSuccessCallback(sync_callback_t cb);

/**
 * @brief The callback function type for setSyncFailCallback().
 */
using fail_callback_t = void (*)(const char *);

/**
 * @brief Set the callback called when time syncing failed.
 * 
 * @param cb The callback function as a @c fail_callback_t object
 */
void setSyncFailCallback(fail_callback_t cb);

} // namespace pftime

#endif // ESPPERFECTTIME_H_