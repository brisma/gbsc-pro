// Device glue of the automatic input:
// drives InputAuto from the TV5725 detection and the ADV Controller signal report, switches paths on the Pro
#pragma once
#include <stdint.h>

// Boot: the input the hardware starts on for this hint (input.last): the scan's first path
uint8_t inputAutoBootInput(uint8_t hint);
void inputAutoBegin();                 // setup(), after the boot input is set: the scan starts from input.last
void inputAutoLoop();                  // loop(), every pass
void inputAutoChoose(uint8_t choice);  // settingsApply(APPLY_INPUT): input.source changed (0 Auto, 1..6 manual)
bool inputAutoSearching();
// Console log: a scan past its first round (every path tried); its repeated detail lines are left out
bool inputAutoQuietRound();             // Auto with no source found yet: menus say so, auto-load and pop-up wait
bool inputAutoHoldDetection();         // the TV5725 detection waits (S-Video/Composite probe, decoder still on)
bool inputAutoHoldsSyncWatcher();      // the input search identifies the RGB SCART cable: no preset loads meanwhile
void inputAutoDetectPass(uint8_t sog); // loop(): one TV5725 detection pass ran on the current path at this SOG level
                                       // (>= 8: the pass moved the ADC input to the path first, no level tried)
bool inputAutoTakePathSwitched();      // true once after a path switch: the next detection pass waits its 500 ms
bool inputDetectMayToggle();           // standard board: the detection alternates its two ADC inputs itself
bool inputDetectAnyRgb();              // standard board in Auto: RGBS, RGsB and RGBHV all accepted (upstream)
// Pro: only the VGA path can be RGBHV. RGB SCART and RGB SoG carry composite sync only, but the TV5725 may see a
// vertical sync there for moments (a console powering up, a path switch): taking it as RGBHV gave the "scaling RGB"
// mode with a wrong line count and a picture off centre
bool inputRgbHvAllowed();
bool inputAdvSourcePresent();          // S-Video/Composite: a fresh report says the decoder locks on a source
