// OLEDDisplay for the renderer: the ThingPulse library on the device, the same library built natively in tests
#pragma once
#ifdef ARDUINO
#include <OLEDDisplay.h>
#else
#include "oled-host.h"
#endif

// The fonts, through these only. The library's font header defines each font in every file that names it, and the
// library itself keeps one ArialMT_Plain_10 as its default font: oledFont10() returns that copy (read from the
// display before the first font change), so the firmware carries one copy instead of three. oledFont16() is named in
// one file. Every font change goes through them, so the first call sees the display's own default.
const uint8_t* oledFont10(OLEDDisplay& d);
const uint8_t* oledFont16(OLEDDisplay& d);
