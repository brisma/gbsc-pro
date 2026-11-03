#include <Arduino.h>
#include <Wire.h>
#include "../../options.h"
#include "../gbs-control-pro.h"
#include "../drivers/pt2257.h"
#include "settings-device.h"
#include "../input/input-auto-device.h"

extern userOptions* uopt;
extern String userCommandBuffer;   // gbs-control.ino deferred command queue
extern runTimeOptions* rto;
void applyPresets(uint8_t result);

bool gSettingsPresetPending = false;   // read by settingsLoop()

void settingsApply(uint16_t mask) {
    // input.source is the choice (0 Auto, 1..6 a temporary manual input); the auto-input glue switches the paths
    if (mask & APPLY_INPUT) inputAutoChoose(uopt->inputChoice);
    // ADV groups (sync stripper, formats, I2P, ACE, BCSH, filters): the link v3 state follows uopt by itself
    if (mask & APPLY_SYNC) {
        userCommandBuffer += '&';   // same resync as the menu path: reset sync processor on RGB inputs
    }
    if (mask & APPLY_ADV_I2P) {
        if (!uopt->advI2P) uopt->advSmooth = 0;   // smooth requires I2P
    }
    if (mask & APPLY_COLOR) applyPictureColor();
    if (mask & APPLY_PICTURE) { applyPictureSharpness(); applyScanlineStrength(); }
    // APPLY_OSD: nothing to do, the menu renderer reads uopt->osdTheme on every draw
    if (mask & APPLY_AUDIO) { PT2257_setVolume(uopt->volume); PT2257_mute(uopt->audioMuted || pt2257Silent(uopt->volume)); }
    if (mask & APPLY_PRESET) gSettingsPresetPending = true;   // heavy: run from loop, not from web handlers
}
