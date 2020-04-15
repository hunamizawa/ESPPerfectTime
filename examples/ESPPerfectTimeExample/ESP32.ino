#include <Arduino.h>
#include <WiFi.h>
#include <ESPPerfectTime.h>

const char *ssid      = "your_ssid";
const char *password  = "your_password";

// Japanese NTP server
const char *ntpServer = "ntp.nict.jp";

void connectWiFi() {
  WiFi.begin(ssid, password);

  Serial.print("\nconnecting...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nconnected. ");
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

  // Configure SNTP client in the same way as built-in one
  pftime::configTime(9 * 3600, 0, ntpServer);
  // Or you can use configTzTime()
  pftime::configTzTime("JST-9", ntpServer);
}

void loop() {

  // Get current local time as struct tm
  // by calling pftime::localtime(nullptr)
  struct tm *tm = pftime::localtime(nullptr);

  // You can get microseconds at the same time
  // by passing suseconds_t* as 2nd argument
  suseconds_t us;
  tm = pftime::localtime(nullptr, &us);

  // Get current time as UNIX time
  time_t t = pftime::time(nullptr);

  // If time_t is passed as 1st argument,
  // pftime::localtime() behaves as with built-in localtime() function
  tm = pftime::localtime(t);
}