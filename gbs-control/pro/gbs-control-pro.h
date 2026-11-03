// Pro additions shared with gbs-control.ino: firmware version, IR receiver, input path switching, picture registers,
// the no-signal info panel state and the 75R sync termination. The menu lives in ui/, settings in settings/.
#pragma once

#include <Arduino.h>
#include "options-pro.h"
#include "link/adv-defaults.h"
#include "input/input-types.h"

#define GBS_FW_VERSION "3.0.0"  // GBS-Control firmware version X.Y.Z (ESP8266); tools parse this line
// Build channel, set by tools/release.py: 0 Stable, 1 Beta. A plain local build is Beta "-dev0", below every
// published build, so a developer's device is offered the real ones
#ifndef GBS_FW_CHANNEL
#define GBS_FW_CHANNEL 1
#endif
#ifndef GBS_FW_VERSION_SUFFIX
#define GBS_FW_VERSION_SUFFIX "-dev0"
#endif
#define GBS_FW_FULL_VERSION GBS_FW_VERSION GBS_FW_VERSION_SUFFIX   // the running version: "3.1.0-beta12"
static_assert(sizeof(GBS_FW_FULL_VERSION) <= 16, "versions are at most 15 characters (manifest, menus)");
// Found by the web UI inside uploaded images (version check before installing); reported by /api/info
#define GBS_FW_VERSION_MARKER "GBSCVER=" GBS_FW_FULL_VERSION ";"

// TV5725 ADC_SOGEN / ADC_INPUT_SEL and SP_EXT_SYNC_SEL values used by the input paths
#define RGB1 0x01
#define YUV0 0x00
#define HV_Enable  0x00
#define HV_Disable 0x01

struct runTimeOptions;
struct userOptions;

#include <IRremoteESP8266.h>
#include <IRrecv.h>

extern IRrecv irrecv;
extern decode_results results;

extern unsigned long lastSystemTime;
extern unsigned long lastWebUpdateTime;

extern uint8_t isInfoDisplayActive;        // sync detection pauses while the no-signal info panel is shown
extern uint16_t horizontalBlankStart;
extern uint16_t horizontalBlankStop;

extern bool advSyncTermination;            // experimental 75R sync termination (RGB SCART, runtime only)
bool ADV_setSyncTermination(bool enable);  // false when the input is not RGB SCART or there is no ADV Controller

void applySavedInputSource(void);                // setup(): the GBS side of the path in activeInputType, no link kick
void switchInputPath(uint8_t type, bool save);   // type 1..6; save = false: no device.cfg write (auto scan)
// switchInputPath forced the sync watcher on for the detection: the profile's choice (dev.syncwatcher) is applied
// again once the new signal is stable, even when no preset reload follows (same mode)
extern bool inputWatcherForced;
void inputAdoptDetected(uint8_t type);           // standard board in Auto: record what the detection found

void applyPictureColor(void);                    // every VDS colour register from the settings (picture-color.cpp)
void applyPictureSharpness(void);                // picture.sharpness -> VDS peaking
void applyScanlineStrength(void);                // picture.scanline_strength -> MADPT offsets while scanlines are on
void pictureStep(uint8_t sid, int delta);        // clamped, persisted step of a picture setting
void pictureResetColors(void);                   // Red/Green/Blue back to neutral 128

