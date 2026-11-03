// OLEDDisplay for the renderer: the ThingPulse library on the device, the same library built natively in tests
#pragma once
#ifdef ARDUINO
#include <OLEDDisplay.h>
#else
#include "oled-host.h"
#endif
