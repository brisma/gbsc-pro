// Device side of the TV5725 icon OSD on a standard board (rules in tv-osd.h): the TV OSD mode driven by the menu
// engine and the indicator shown on user adjustments
#pragma once
#include <stdint.h>

void tvOsdNav(uint8_t nav);                    // TVOSD_*: from the menu backend while the TV OSD overlay is open
void tvOsdIndicate(uint8_t icon, int8_t dir);  // a user adjustment ran (api-device hooks); dir for move/scale
void tvOsdLoop(uint32_t now);                  // menu device loop: hides the indicator after its timeout
