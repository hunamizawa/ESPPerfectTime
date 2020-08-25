#!/bin/bash -eux

# The MIT License (MIT)
#
# Copyright (c) 2014-2020 Benoit BLANCHON
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

/sbin/start-stop-daemon --start --quiet --pidfile /tmp/custom_xvfb_1.pid --make-pidfile --background --exec /usr/bin/Xvfb -- :1 -ac -screen 0 1280x1024x16
sleep 3
export DISPLAY=:1.0

sudo iptables -P INPUT DROP
sudo iptables -P FORWARD DROP
sudo iptables -P OUTPUT ACCEPT
sudo iptables -A INPUT -i lo -j ACCEPT
sudo iptables -A OUTPUT -o lo -j ACCEPT
sudo iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

mkdir -p /tmp/arduino
curl -sS http://downloads.arduino.cc/arduino-$VERSION-linux64.tar.xz | tar xJ -C /tmp/arduino --strip 1 ||
curl -sS http://downloads.arduino.cc/arduino-$VERSION-linux64.tgz | tar xz -C /tmp/arduino --strip 1
export PATH=$PATH:/tmp/arduino/

if [[ "$BOARD" =~ "esp8266:esp8266:" ]]; then
  arduino --pref "boardsmanager.additional.urls=http://arduino.esp8266.com/stable/package_esp8266com_index.json" --save-prefs
  arduino --install-boards esp8266:esp8266
elif [[ "$BOARD" =~ "esp32:esp32:" ]]; then
  arduino --pref "boardsmanager.additional.urls=http://dl.espressif.com/dl/package_esp32_index.json" --save-prefs
  arduino --install-boards esp32:esp32
fi

ln -s $PWD /tmp/arduino/libraries/ESPPerfectTime

for EXAMPLE in $PWD/examples/*/*.ino; do
	arduino --verify --board $BOARD $EXAMPLE
done
