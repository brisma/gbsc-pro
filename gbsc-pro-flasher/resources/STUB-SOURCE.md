# ESP8266 flasher stub

`esp8266-stub.json` is esptool's ESP8266 flasher stub, copied unchanged from esptool `v4.8.1`:
https://github.com/espressif/esptool/blob/v4.8.1/esptool/targets/stub_flasher/1/esp8266.json
(SHA-256 `3b3b43661fc6607dd163e9bc08b8efbb04ac8f570b8e1d3e2c786ca4a2f37f31`).

It is Free Software under the GNU General Public License version 2 or later
(`LICENSES/esptool-stub-GPL-2.0.txt`). Its sources: https://github.com/espressif/esptool-legacy-flasher-stub/releases/tag/v1.3.0

The GBSC PRO Flasher (GPL-3.0) does not contain it: the stub is a separate file that the flasher reads and sends to the
ESP8266's RAM, where it runs. Without this file the flasher still works, through the ESP8266 ROM loader: slower
(115200 baud, no compression) and without the MD5 check.
