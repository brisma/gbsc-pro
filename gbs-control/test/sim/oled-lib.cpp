// Compiles the real ThingPulse OLEDDisplay.cpp for native tests (see oled-host.h)
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define ARDUINO 100
#define NO_GLOBAL_SERIAL
#include "Arduino.h"
#include "../../.pio/libdeps/native/ESP8266 and ESP32 OLED driver for SSD1306 displays/src/OLEDDisplay.cpp"
