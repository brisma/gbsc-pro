// Boot splash on the OLED: the artwork (oled-splash-art.h, "GBSC" + a CRT with PRO + a rule),
// then "by Brisma" (centred under GBSC), the firmware version (right) and the project link in ArialMT_Plain_10. Shown by setup() for 1.5 s.
#pragma once
#include "oled-display.h"

void oledDrawSplash(OLEDDisplay& d, const char* version);   // version without the "v", e.g. GBS_FW_FULL_VERSION
