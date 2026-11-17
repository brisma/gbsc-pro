// Where the sync processor takes a composite sync from (SP_SOG_SRC_SEL): the ADC's sync slicer on the SOG pin, or the
// HS input pin itself. On the boards with the sync separator the composite sync of the RGB paths reaches the HS input
// as a logic signal (the separator's output for RGB SCART and RGB SoG, the source's own on the VGA connector) and
// the SOG pin only through a small capacitor, a path made for sync at video level. The HS pin reads it directly.
// Component and the decoder paths carry the sync in the picture: the slicer is the only way there. Pure.
#pragma once
#include <stdint.h>
#include "input-types.h"

// wanted: input.digital_sync; separatorBoard: the board has the sync separator (CAP_ADV); compositeSync: the source
// has no separate vertical sync
inline bool syncFromHsPin(bool wanted, bool separatorBoard, uint8_t input, bool compositeSync) {
    if (!wanted || !separatorBoard || !compositeSync) return false;
    return input == InputTypeRGBs || input == InputTypeRGsB || input == InputTypeVGA;
}

// input.sync_dejitter: the input pads of the HS/VS pins pass the Schmitt trigger's output either as it is or through a
// filter (S0_48 bit 6 for HSIN1/VSIN1, 0 = filter on), meant for a sync with more jitter. rama's code leaves it off in
// the scaling presets and turns it on for the RGBHV bypass and the source search; Auto keeps that, On / Off force it
// wherever the pins carry the sync.
static const uint8_t kSyncDejitterAuto = 0;
static const uint8_t kSyncDejitterOn = 1;
static const uint8_t kSyncDejitterOff = 2;

// The pins carry the sync: separate H and V (csync false), the RGBHV bypass, or Digital sync reading a composite sync
// (fromHs: syncFromHsPin()). A composite sync through the slicer leaves them unused.
inline bool syncPinsRead(bool csync, bool fromHs, bool rgbhvBypass) {
    return rgbhvBypass || !csync || fromHs;
}

// upstreamOn: the filter as rama's code sets it for this path
inline bool syncDejitterOn(uint8_t setting, bool pinsRead, bool upstreamOn) {
    if (!pinsRead || setting == kSyncDejitterAuto) return upstreamOn;
    return setting == kSyncDejitterOn;
}
