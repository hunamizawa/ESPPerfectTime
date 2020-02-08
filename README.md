# ESPPerfectTime

ESP8266/ESP32用 SNTP 時刻同期 Arduino ライブラリ<br>
An ESP8266/ESP32 Arduino library that provides more accurate time using SNTP

# Features

以下の機能により、ESP8266/ESP32 上でより正確な時刻を取り扱うことができます。

- NTP サーバーとの通信に要する時間を計算して補正<br>
  Compute round-trip delay for time synchronization
- うるう秒対応<br>
  Support for leap seconds

ESP8266/ESP32 で時計などを製作する際に役立ちます。

# Installation

1. [zip をここからダウンロード](https://github.com/hunamizawa/ESPPerfectTime/archive/master.zip)<br>
   [Download zip file from here](https://github.com/hunamizawa/ESPPerfectTime/archive/master.zip)
1. Arduino IDE を開く<br>
   Open Arduino IDE
1. \[スケッチ\] → \[ライブラリをインクルード\] → \[.ZIP形式のライブラリをインストール...\] を選択<br>
   Select menu \[Sketch\] -> \[Include library\] -> \[Add .ZIP library...\]
1. ダウンロードした zip を選択<br>
   Select downloaded zip file

# Usage
## とりあえず動かす / Migration

1. Include `<time.h>` &amp; `<ESPPerfectTime.h>`
1. 以下の関数の頭に `pftime::` をつける<br>
   Add a prefix `pftime::` to functions below:<br>
```
configTime
configTzTime (ESP32 only)
time
gmtime
localtime
gettimeofday
settimeofday
```

Like this:

```cpp
// configTime(EST, 0, "time.nist.gov", "time.cloudflare.com");
pftime::configTime(EST, 0, "time.nist.gov", "time.cloudflare.com");

// time_t t = time(nullptr);
time_t t = pftime::time(nullptr);
```

## うるう秒の表示を有効にする / How to enable +1 leap second

UNIX 時間（`time_t` 型）では、（挿入される）うるう秒を表現できません。したがって、うるう秒挿入時には 23時59分59秒 (UTC) が 2 秒間続きます。<br>
`time_t` can't express +1 leap seconds (23:59:60 UTC). On inserting a leap second, the last second of the day (23:59:59 UTC) will repeat.

一方、`struct tm` 型なら、うるう秒 23時59分60秒 (UTC) を表現できます。そこで、このライブラリでは関数 `gmtime` および `localtime` を次のように宣言し、新しい使い方を追加しました。<br>
On the other hand, `struct tm` can do it. So this library provides new definition of `gmtime`/`localtime`:

```cpp
struct tm *gmtime   (const time_t *timer, suseconds_t *res_usec = nullptr);
struct tm *localtime(const time_t *timer, suseconds_t *res_usec = nullptr);
```

この2つの関数は、次のように動作します。<br>
These functions perform as below:

1. `timer != nullptr` なら、従来通り `time_t` を `struct tm` に変換する関数として動作します。<br>
If `timer` is not null, converts `time_t` to `struct tm` as usual.
1. `timer == nullptr` なら、現在時刻を表す `struct tm` を返します。<br>
If `timer` is null, returns the current time as `struct tm`.
1. `res_usec != nullptr` なら、現在時刻の小数点以下を `res_usec` に格納します。<br>
If `res_usec` is not null, sets microseconds of the current time to `res_usec`.

よって、現在時刻を `struct tm` 型で取得したい場合は、`pftime::localtime(nullptr)` を呼び出すことにより、直接カレンダー形式の値を取得してください。<br>
You should call `pftime::localtime(nullptr)` to get the current time as `struct tm`.

```cpp
/* BAD: */
time_t     timer;
struct tm *tm;
pftime::time(&timer);
tm = pftime::localtime(&timer);

/* GOOD: */
struct tm *tm = pftime::localtime(nullptr);
```

# うるう秒の補正が必要な理由

そもそも、うるう秒とは原子時計に依存する**協定世界時 (UTC)** と、地球の自転速度の僅かなゆらぎを同調させるために、UTC の1年の長さを±1秒変化させる操作です。

1秒足す場合は、12月31日（または7月1日）の最後（23時59分59秒の後）に、23時59分**60秒** という1秒を付け足します。

このときの時間の流れは以下のようになります。

|real time (UTC)|pftime::time()|pftime::gmtime(nullptr)|pftime::gmtime(&timer)|::time()|::gmtime(&timer)|
|:---:|:---:|:---:|:---:|:---:|:---:|
|2016-12-31<br>23:59:58|1483228798|2016-12-31<br>23:59:58|2016-12-31<br>23:59:58|1483228798|2016-12-31<br>23:59:58|
|2016-12-31<br>23:59:59|1483228799|2016-12-31<br>23:59:59|2016-12-31<br>23:59:59|1483228799|2016-12-31<br>23:59:59|
|**2016-12-31<br>23:59:60**|**1483228799**|**2016-12-31<br>23:59:60**|**2016-12-31<br>23:59:59**|**1483228800**|**2017-01-01<br>00:00:00**|
|2017-01-01<br>00:00:00|1483228800|2017-01-01<br>00:00:00|2017-01-01<br>00:00:00|1483228801|2017-01-01<br>00:00:01|
|2017-01-01<br>00:00:01|1483228801|2017-01-01<br>00:00:01|2017-01-01<br>00:00:01|1483228802|2017-01-01<br>00:00:02|

うるう秒が挿入された瞬間から、従来の `time()` は UTC より1秒先に進んでしまいます（これは次回の SNTP 同期により修正されます）。`pftime::time()` を用いると、うるう秒が正しく挿入され、UTC とのズレは発生しません。さらに `pftime::gmtime(nullptr)` を用いることで、通常ならあり得ない 23時59分**60秒** という表現を得ることができます。

逆に1秒引く場合は、12月31日（または7月1日）の最後（23時59分59秒）を切り取って、23時59分58秒からいきなり翌日の0時0分0秒にジャンプさせます。

## うるう秒と NTP

NTP (Network Time Protocol) は、コンピュータの時刻を合わせるためのプロトコルとして広く使用されていますが、実はうるう秒の挿入・削除を予告する機能が備わっています。このライブラリは、NTP パケットに含まれる LI (Leap second Indicator) フラグを読み取って、ESP8266/ESP32 上でうるう秒を取り扱うことができます。<br>
In Network Time Protocol (NTP), NTP packets has LI (Leap second Indicator) flag. The NTP server informs the user that a leap second will insert or remove in 24 hours by setting LI to 1 (insert) or 2 (remove). This library uses LI flag to handle leap seconds.

なお、うるう秒を挿入することで発生する異常動作を防ぐため、1日かけて少しずつ時計をずらす（SLEW モード）NTP サーバーも存在します（time.google.com とか）。SLEW モードを使う NTP サーバーは当然 LI フラグが立ちません。このようなサーバーを同期先として設定していると、ESP 上でもうるう秒挿入・削除が反映されません。<br>
Note that some NTP servers (like google's) uses *clock drifting* instead of inserting/removing 1 sec, so they don't use LI flag.

# License

3-clause BSD license (same as lwIP)