[![Build Status](https://travis-ci.org/hunamizawa/ESPPerfectTime.svg?branch=master)](https://travis-ci.org/hunamizawa/ESPPerfectTime)

# ESPPerfectTime

ESP8266/ESP32用 SNTP 時刻同期 Arduino ライブラリ<br>
SNTP Time Synchronization Library for ESP8266/ESP32 with Arduino

# Features

以下の機能により、ESP8266/ESP32 上でより正確な時刻を取り扱うことができます。<br>
The following features allow you to handle the time more accurately on ESP8266/ESP32:

- NTP サーバーとの通信に要する時間を計算して補正<br>
  Computing round-trip delay for time synchronization
- うるう秒対応<br>
  Supporting for leap seconds in STEP mode

ESP8266/ESP32 で時計などを製作する際に役立ちます。<br>
It's useful for making clocks, etc.

# Installation

## Arduino IDE

1. Arduino IDE を開く<br>
   Execute Arduino IDE
1. ライブラリマネージャを開く（\[ツール\] → \[ライブラリを管理...\] を選択）<br>
   Open Library Manager (Select menu \[Tools\] -> \[Manage Libraries...\])
1. 「ESPPerfectTime」を検索<br>
   Search "ESPPerfectTime"
1. 「インストール」を押す<br>
   Push "Install"

## PlatformIO

```
> pio lib install "ESPPerfectTime"
```

# Usage

## Sample code

```cpp
// Configure SNTP client in the same way as built-in one
#ifdef ESP8266
pftime::configTime(TZ_Asia_Tokyo, ntpServer);
#else /* ESP32 */
// For ESP32 users, you can refer to following link to get POSIX-style tz string:
// https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
pftime::configTzTime("JST-9", ntpServer);
#endif

// This is deprecated
pftime::configTime(9 * 3600, 0, ntpServer);

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
```

## 組み込み SNTP クライアントからの移行 / Migration

1. ソースファイルの先頭に以下を記述する

```cpp
#include <Arduino.h>
#include <ESPPerfectTime.h>
```

2. 以下の関数の頭に `pftime::` をつける<br>
   Add a prefix `pftime::` to functions below:

```
configTime
configTzTime
time
gmtime
localtime
gettimeofday
settimeofday
```

Like this:

```cpp
// (Instead of) configTime(TZ_America_New_York, 0, "time.nist.gov", "time.cloudflare.com");
pftime::configTime(TZ_America_New_York, 0, "time.nist.gov", "time.cloudflare.com");
// For ESP32:
pftime::configTzTime("EST5EDT,M3.2.0,M11.1.0", 0, "time.nist.gov", "time.cloudflare.com");

// (Instead of) time_t t = time(nullptr);
time_t t = pftime::time(nullptr);
```

3. 次の呼び出しパターンを見つけて、書き換える

Before: 

```cpp
// パターン 1 / pattern 1
// 現在時刻を struct tm 型で取得するために
// time() で取得した time_t 値を localtime() に渡し、struct tm 型に変換する
// To get current time as struct tm,
// call time(), then pass the return value to localtime()
time_t t = time(nullptr);
struct tm *tm = localtime(t);

// パターン 2 / pattern 2
// 現在時刻の小数点以下を取得するために gettimeofday() を呼ぶ
// time_t は localtime() に渡し、struct tm 型に変換する
// To get microsconds of current time,
// call gettimeofday(), then pass the value tv.tv_sec to localtime()
struct timeval tv;
pftime::gettimeofday(&tv, nullptr);
struct tm  *tm = localtime(tv.tv_sec);
suseconds_t us = tv.tv_usec;
```

After:

```cpp
// パターン 1
// 現在時刻は pftime::localtime(nullptr) で直接取得する
// You should use pftime::localtime(nullptr) (NOT pftime::localtime(&timer))
// to get current time as struct tm
struct tm *tm = pftime::localtime(nullptr);

// パターン 2
// 現在時刻の小数点以下は、pftime::localtime() の第2引数で取得できる
// You can get microsconds of current time
// by using pftime::localtime(nullptr, us)
suseconds_t us;
struct tm *tm = pftime::localtime(nullptr, &us);
```

## うるう秒挿入時の注意 / Notes for correct handling of leap seconds

UNIX 時間（`time_t` 型）では、（挿入される）うるう秒を表現できません。したがって、うるう秒挿入時には 23時59分59秒 (UTC) が 2 秒間続きます。<br>
`time_t` can't represent +1 leap seconds (23:59:60 UTC). Therefore, when a leap second is inserted, the last second of the day (23:59:59 UTC) will repeat for two seconds, in `time_t` representation.

また、うるう秒が挿入されると、1分が60秒ではなく61秒になります。よって、次のようなコードは、想定した結果を得られない場合があります。<br>
Also, when a leap second is inserted, the minute becomes 61 seconds (NOT 60 secs), so the following code may give the unexpected results:

```cpp
// 毎分00秒に何か測定をする
// Measures something on the zero second of each minute

void loop() {
  time_t t;

  // 現在時刻を取得
  // Gets current time
  t = pftime::time(nullptr);

  // 計測処理
  // Measuring
  measure();

  // 次の分の00秒まで待つ
  // Waits for the zero second of next minute
  time_t interval_for_next_zero = 60 - t % 60;
  delay(interval_for_next_zero * 1000);
}
```

このコードを、うるう秒が挿入される日の 23時59分00秒 (UTC) に実行したとします。<br>
If this code is executed at 23:59:00 UTC on the day the leap second is inserted, it works like this:

1. 変数 `interval_for_next_zero` は「1分が60秒」だと仮定して、「23時59分00秒 から 60秒後」の時刻を計算しています。<br>
The variable `interval_for_next_zero` is the result of the calculation of the time 60 seconds after 23:59:00, which is based on the assumption that one minute is 60 seconds.
1. `delay()` を抜けた瞬間の時刻は（通常なら翌日の 00時00分00秒 になるところですが）うるう秒が入るために 23時59分**60**秒 となります。<br>
The time at the moment of exiting the `delay()` is 23:59:**60** because of the leap seconds, which would normally be 00:00:00 the next day.
1. 先述の通り、この時の `time_t` の値は 23時59分59秒 ですから、（`measure()` が1秒未満で完了すると）`t % 60 == 59` となり、1秒後に再び計測が実行されてしまいます。<br>
As explained, the value of `time_t` at this time is 23:59:59. If `measure()` completes in less than one second, then `t % 60 == 59` will be true, and the measurement will be performed again one second later.

この問題を回避するには、`loop()` の冒頭を次のように書き換えるとよいでしょう。<br>
To avoid this problem, you need to rewrite the head of `loop()` with the following:

```cpp
void loop() {
  time_t t;

  // 今が本当に00秒か確認する
  // Ensures now is the 00 sec of the minute, not 60 sec
  while ((t = pftime::time(nullptr)) % 60 != 0) {
    delay(100);
  }

...
```

# うるう秒について

うるう秒とは、原子時計に依存する**協定世界時 (UTC)** と、地球の自転速度の僅かなゆらぎを同調させるために、UTC の1年の長さを±1秒変化させる操作です。

1秒足す場合は、12月31日（または7月1日）の最後（23時59分59秒の後）に、23時59分**60秒** という1秒を付け足します。

このときの時間の流れと、各関数の戻り値は以下のようになります。<br>
When the leap second is inserted, the return value of each function is as follows:

|real time (UTC)|pftime::time()|pftime::gmtime(nullptr)|pftime::gmtime(&timer)|pftime::getLeapIndicator()|::time()|::gmtime(&timer)|
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
|2016-12-31<br>23:59:58|1483228798|2016-12-31<br>23:59:58|2016-12-31<br>23:59:58|LI_LAST_MINUTE_61_SEC|1483228798|2016-12-31<br>23:59:58|
|2016-12-31<br>23:59:59|1483228799|2016-12-31<br>23:59:59|2016-12-31<br>23:59:59|LI_LAST_MINUTE_61_SEC|1483228799|2016-12-31<br>23:59:59|
|**2016-12-31<br>23:59:60**|**1483228799**|**2016-12-31<br>23:59:60**|**2016-12-31<br>23:59:59**|**LI_LAST_MINUTE_61_SEC**|**1483228800**|**2017-01-01<br>00:00:00**|
|2017-01-01<br>00:00:00|1483228800|2017-01-01<br>00:00:00|2017-01-01<br>00:00:00|LI_NO_WARNING|1483228801|2017-01-01<br>00:00:01|
|2017-01-01<br>00:00:01|1483228801|2017-01-01<br>00:00:01|2017-01-01<br>00:00:01|LI_NO_WARNING|1483228802|2017-01-01<br>00:00:02|

うるう秒が挿入された瞬間から、従来の `time()` は UTC より1秒先に進んでしまいます（これは次回の SNTP 同期により修正されます）。`pftime::time()` を用いると、うるう秒が正しく挿入され、UTC とのズレは発生しません。さらに `pftime::gmtime(nullptr)` を用いることで、通常ならあり得ない 23時59分**60秒** という表現を得ることができます。

逆に1秒引く場合は、12月31日（または7月1日）の最後（23時59分59秒）を切り取って、23時59分58秒からいきなり翌日の0時0分0秒にジャンプさせます。

## うるう秒と NTP

NTP (Network Time Protocol) は、コンピュータの時刻を合わせるためのプロトコルとして広く使用されていますが、実はうるう秒の挿入・削除を予告する機能が備わっています。このライブラリは、NTP パケットに含まれる LI (Leap second Indicator) フラグを読み取って、ESP8266/ESP32 上でうるう秒を取り扱うことができます。<br>
In Network Time Protocol (NTP), NTP packets has LI (Leap second Indicator) flag. The NTP server informs the user that a leap second will insert or remove in 24 hours by setting LI to 1 (insert) or 2 (remove). This library uses LI flag to handle leap seconds.

なお、うるう秒を挿入することで発生する異常動作を防ぐため、1日かけて少しずつ時計をずらす（SLEW モード）NTP サーバーも存在します（time.google.com とか）。SLEW モードを使う NTP サーバーは当然 LI フラグが立ちません。このようなサーバーを同期先として設定していると、ESP 上でもうるう秒挿入・削除が反映されません。<br>
Note that some NTP servers (like google's) uses *clock drifting* instead of inserting/removing 1 sec, so they don't use LI flag.

# License

3-clause BSD license (same as lwIP)