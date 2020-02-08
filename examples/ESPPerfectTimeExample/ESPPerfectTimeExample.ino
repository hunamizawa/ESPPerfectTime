#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <TZ.h>
#endif 
#ifdef ESP32
#include <WiFi.h>
#endif
#include <ESPPerfectTime.h>

const char *ssid      = "your_ssid";
const char *password  = "your_password";
const char *ntpServer = "ntp.nict.jp";

#ifdef ESP8266
static void _delayMicroseconds(uint16_t us) {
  // avoid lagging & soft WDT reset
  uint32_t start = micros();
  while (micros() - start < us)
    yield();
}
#else /* !ESP8266 */
#define _delayMicroseconds(us)   delayMicroseconds(us)
#endif

void connectWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("\nWiFi connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(" connected. ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

  //pftime::configTime(9 * 3600, 0, ntpServer);
#ifdef ESP8266
  // On ESP8266, you can use <TZ.h> that includes many timezone definitions
  pftime::configTime(TZ_Asia_Tokyo, ntpServer);
#endif 
#ifdef ESP32
  pftime::configTzTime("JST-9", ntpServer);
#endif
}

void loop() {
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

  time_t t = pftime::time(nullptr);

  suseconds_t us;
  struct tm  *tm = pftime::localtime(nullptr, &us);

  Serial.printf("%d  %04d/%02d/%02d(%s) %02d:%02d:%02d.%06d\n",
        t,
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        wd[tm->tm_wday],
        tm->tm_hour, tm->tm_min, tm->tm_sec,
        us);

  _delayMicroseconds(1000000 - us);
}