// The ThingPulse OLEDDisplay class on the host: ARDUINO is defined only while its header is read
// (settings-pgm.h and stv9426-udc.cpp switch on ARDUINO). The library is downloaded by lib_deps of
// [env:native] and never built by PlatformIO (lib_ignore); oled-lib.cpp compiles its OLEDDisplay.cpp.
#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define ARDUINO 100
#include "Arduino.h"
#include "../../.pio/libdeps/native/ESP8266 and ESP32 OLED driver for SSD1306 displays/src/OLEDDisplay.h"
#undef ARDUINO
#pragma GCC diagnostic pop
