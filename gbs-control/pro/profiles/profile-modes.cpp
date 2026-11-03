#include <string.h>
#include "profile-modes.h"
#include "../input/input-types.h"
#include "../../options.h"
#include "profile-names.h"

static const uint8_t kModes[PROFILE_MODE_COUNT] = {1, 2, 3, 4, 8, 14};
static const char* const kNames[PROFILE_MODE_COUNT] = {"ntsc", "pal", "ntsc_480p", "pal_576p", "medium_res", "vga_upscale"};

int8_t profileModeIndex(uint8_t gbsMode) {
    for (uint8_t i = 0; i < PROFILE_MODE_COUNT; i++) if (kModes[i] == gbsMode) return (int8_t)i;
    return -1;
}

const char* profileModeName(uint8_t index) { return index < PROFILE_MODE_COUNT ? kNames[index] : ""; }

int8_t profileModeIndexByName(const char* name) {
    for (uint8_t i = 0; i < PROFILE_MODE_COUNT; i++) if (!strcmp(kNames[i], name)) return (int8_t)i;
    return -1;
}

uint8_t profileModeAt(uint8_t index) { return index < PROFILE_MODE_COUNT ? kModes[index] : 0; }

uint8_t profileRegModeOf(uint8_t mode, bool palForce60Active) {
    if (palForce60Active) {
        if (mode == 1) return 2;
        if (mode == 3) return 4;
    }
    return mode;
}

bool profileForce60Reloads(uint8_t regMode) { return regMode == 2 || regMode == 4; }

uint16_t profileForce60VbStop(uint16_t presetStop) {
    uint32_t v = (uint32_t)presetStop + 56;
    return (uint16_t)(v > 0x7FF ? 0x7FF : v);
}

bool profileForce60Active(bool force60, uint8_t gbsMode) { return force60 && (gbsMode == 2 || gbsMode == 4); }

uint8_t profileForce60PresetMode(bool force60, bool customRegs, uint8_t gbsMode) {
    if (!profileForce60Active(force60, gbsMode) || customRegs) return gbsMode;
    return gbsMode == 2 ? 1 : 3;
}

bool profileSyncWatcherNext(uint8_t mode, bool current, bool* offByProfile) {
    if (mode == 1) { *offByProfile = false; return true; }
    if (mode == 2) { *offByProfile = true; return false; }
    if (*offByProfile) { *offByProfile = false; return true; }
    return current;
}

uint8_t profileSourceModeOf(uint8_t regMode, uint8_t inputType, bool i2p) {
    if (i2p && inputIsDecoder(inputType)) {   // S-Video, Composite: the decoder doubles the lines
        if (regMode == 3) return 1;
        if (regMode == 4) return 2;
    }
    return regMode;
}

char profilePresetStatusChar(uint8_t presetID, bool isCustom, uint8_t hdBypassId, uint8_t rgbhvBypassId) {
    if (isCustom) return '9';
    if (presetID == hdBypassId || presetID == rgbhvBypassId) return '8';
    uint8_t low = presetID & 0x0F;
    if ((presetID & 0xE0) == 0 && low >= 1 && low <= 7) return (char)('0' + low);   // 7: 1200p
    return '0';
}

bool profileIsInterlaced(uint8_t gbsMode, uint16_t v) {
    if (gbsMode != 1 && gbsMode != 2) return false;
    bool progressive = v == 521 || v == 523 || v == 525 || v == 623 || v == 625 || v == 627;
    return !progressive;
}

// Half-lines per field of a 15 kHz mode: an interlaced field is half a standard frame; a progressive one reads
// 2 x lines - 1 (odd). 0: not known (a progressive count that is not exact)
static uint16_t halfLinesPerField(uint8_t mode, uint16_t vperiodIf, bool interlaced) {
    if (interlaced) return mode == 2 ? 625 : 525;
    return (vperiodIf & 1) ? (uint16_t)(vperiodIf + 1) : 0;
}

bool profileRateFits(uint8_t input, uint8_t mode, uint16_t fpLines, bool fpInterlaced, uint32_t fpMilliHz,
                     uint16_t lines, bool interlaced, uint32_t milliHz) {
    if (!fpMilliHz || !milliHz) return true;
    if ((mode != 1 && mode != 2) || inputIsDecoder(input)) return true;
    uint16_t a = halfLinesPerField(mode, fpLines, fpInterlaced), b = halfLinesPerField(mode, lines, interlaced);
    if (!a || !b) return true;
    uint64_t la = (uint64_t)fpMilliHz * a, lb = (uint64_t)milliHz * b;   // twice the line rate, in mHz
    uint64_t diff = la > lb ? la - lb : lb - la;
    // 300 ppm: a console against itself stays within a few tens (its crystal, the measurement); the nearest pair
    // of consoles with one standard, SNES and PlayStation, is 740 ppm apart
    return diff * 10000u <= (la > lb ? la : lb) * 3u;
}

uint32_t profileRateMilliHz(float hz) {
    if (!(hz >= 47.0f && hz <= 86.0f)) return 0;
    return (uint32_t)(hz * 1000.0f + 0.5f);
}

uint32_t profileRatePick(uint32_t a, uint32_t b, uint32_t c) {
    const uint32_t v[3] = {a, b, c};
    for (uint8_t i = 0; i < 3; i++)
        for (uint8_t j = (uint8_t)(i + 1); j < 3; j++) {
            if (!v[i] || !v[j]) continue;
            uint32_t d = v[i] > v[j] ? v[i] - v[j] : v[j] - v[i];
            if (d <= 10) return (v[i] + v[j] + 1) / 2;
        }
    return 0;
}

uint8_t profilePresetForLoad(uint8_t preference, bool regsAvailable, uint8_t basePreset) {
    if (preference != OutputCustomized || regsAvailable) return preference;
    if (basePreset == PROFILE_BASE_NONE || basePreset == OutputCustomized || basePreset == OutputBypass) return 0;
    return basePreset;
}

uint8_t profileDefaultsPreset(uint8_t preference, uint8_t basePreset) {
    return profilePresetForLoad(preference, false, basePreset);
}

bool profileLoadSwitchesInput(bool enabled, bool hasFp, uint8_t fpInput, uint8_t currentInput) {
    return enabled && hasFp && inputTypeValid(fpInput) && fpInput != currentInput;
}

bool profileModifiedRule(bool crcDiffers, uint16_t regsTouched, bool active) {
    return active && (crcDiffers || regsTouched > 0);
}

bool profileSaveKeepsFp(bool replace, bool hasFp) { return !replace && hasFp; }

bool profileStartupLoadsCurrent(uint8_t startup) { return startup == 0; }

bool profileRegsFitForce60(const uint8_t* regs, uint8_t gbsMode, bool force60) {
    if (gbsMode != 2 && gbsMode != 4) return true;
    uint8_t preset = regs[PROFILE_REG_PRESET_ID_OFFSET] & 0x7F;
    if (preset == 0x21 || preset == 0x22) return true;   // Bypass: the output follows the source rate
    bool captured60 = (regs[PROFILE_REG_FORCE60_OFFSET] >> 2) & 1;
    return captured60 == force60;
}

