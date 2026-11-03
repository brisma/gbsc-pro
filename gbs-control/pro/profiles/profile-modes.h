// Video modes with custom register capture, and the firmware's mode remaps (pure, host-testable)
#pragma once
#include <stdint.h>

static const uint8_t PROFILE_MODE_COUNT = 6;   // 1 ntsc, 2 pal, 3 ntsc_480p, 4 pal_576p, 8 medium_res, 14 vga_upscale

int8_t profileModeIndex(uint8_t gbsMode);               // -1 when the mode has no register capture
const char* profileModeName(uint8_t index);
int8_t profileModeIndexByName(const char* name);
uint8_t profileModeAt(uint8_t index);

// Register mode: the mode the GBS runs, with only the PalForce60 remap undone (1->2, 3->4)
uint8_t profileRegModeOf(uint8_t videoStandardInput, bool palForce60Active);
// Output > Force 60Hz changed: the register mode it remaps (PAL 2, 4) needs a preset reload to apply now
bool profileForce60Reloads(uint8_t regMode);
// Force 60Hz: IF_VB_SP for a PAL source in a 60 Hz preset, the preset's own value + 56 (the middle 240 of 288 lines)
uint16_t profileForce60VbStop(uint16_t presetStop);
// Always 60Hz on a PAL mode (2, 4): the output runs at 60 Hz (no clock sync to the source). The base preset is
// remapped to its NTSC twin (2->1, 4->3); registers captured at 60 Hz already hold that timing and keep their mode
bool profileForce60Active(bool force60, uint8_t gbsMode);
uint8_t profileForce60PresetMode(bool force60, bool customRegs, uint8_t gbsMode);
// Sync watcher after a preset load, from the profile's dev.syncwatcher (0 Inherit, 1 On, 2 Off): On and Off force it;
// Inherit turns it back on only when a profile's Off turned it off (*offByProfile), else leaves it
bool profileSyncWatcherNext(uint8_t mode, bool current, bool* offByProfile);
// Source mode (fingerprint): register mode with the decoder's I2P doubling undone on S-Video/Composite
uint8_t profileSourceModeOf(uint8_t regMode, uint8_t inputType, bool i2p);

// Interlaced source, with the firmware's own rule (areScanLinesAllowed): in modes 1/2 the GBS counts an odd
// VPERIOD_IF for progressive 240p/288p (521/523/525, 623/625/627) and an even one for 480i/576i
bool profileIsInterlaced(uint8_t gbsMode, uint16_t vperiodIf);
// The source's field rate in the fingerprint, in mHz (0 = not known). A console's line rate (field rate x lines per
// field) comes from its own clock and line length: it stays the same from 240p to 480i and between games that pick
// another line count, and differs from console to console (SNES 15.746 kHz, Mega Drive 15.700, PlayStation 15.734).
// Judged for the 15 kHz modes (1, 2) on the scaler's own inputs; true wherever it cannot be judged: a rate not
// known, S-Video and Composite (the decoder's line doubling hides the scan type), 480p and up
bool profileRateFits(uint8_t input, uint8_t mode, uint16_t fpLines, bool fpInterlaced, uint32_t fpMilliHz,
                     uint16_t lines, bool interlaced, uint32_t milliHz);
// A measured field rate in mHz; 0 outside 47..86 Hz (a reading of a signal that is settling)
uint32_t profileRateMilliHz(float hz);
// One rate from up to three measurements (0 = failed): the mean of two that agree within 10 mHz, else 0
uint32_t profileRatePick(uint32_t a, uint32_t b, uint32_t c);
// Preset to run for this load: Custom without registers falls back to the profile's built-in base preset
// (unknown, Custom or Bypass base -> 960p, the firmware's default fallback)
uint8_t profilePresetForLoad(uint8_t preference, bool regsAvailable, uint8_t basePreset);
// Defaults (by hand or Startup = Defaults) keep the output resolution in use; Custom gives way to the base preset of
// the profile whose registers it ran
uint8_t profileDefaultsPreset(uint8_t preference, uint8_t basePreset);

// The web's resolution highlight
char profilePresetStatusChar(uint8_t presetID, bool isCustom, uint8_t hdBypassId, uint8_t rgbhvBypassId);

// The profile store's rules: Unsaved = settings differ or a register action ran since the
// last save/load (only with a profile active); Save changes keeps a stored fingerprint, Replace moves it; Startup
// Last used (0) applies the saved working state at boot, Defaults (1) does not
bool profileModifiedRule(bool crcDiffers, uint16_t regsTouched, bool active);
bool profileSaveKeepsFp(bool replace, bool hasFp);
bool profileStartupLoadsCurrent(uint8_t startup);

// Manual profile load: switch to the input the profile was saved on (fingerprint input 1..6)
bool profileLoadSwitchesInput(bool enabled, bool hasFp, uint8_t fpInput, uint8_t currentInput);

// Always 60Hz and captured registers: offset of register 1_2C in a profile's register dump (segment 0 0x40-0x5F and
// 0x90-0x9F come first), whose bit 2 is GBS_OPTION_PALFORCED60_ENABLED. PAL-mode registers (modes 2, 4) fit only
// the rate they were captured at; registers of another rate give way to the base preset (upstream's remap then
// applies the chosen rate) until Save changes captures them again
static const uint16_t PROFILE_REG_FORCE60_OFFSET = 92;
// 1_2B: GBS_PRESET_ID (bits 0-6) + GBS_PRESET_CUSTOM (bit 7); Bypass captures (PresetHdBypass 0x21,
// PresetBypassRGBHV 0x22 in gbs-control.ino) keep passthrough whatever Always 60Hz says
static const uint16_t PROFILE_REG_PRESET_ID_OFFSET = 91;
bool profileRegsFitForce60(const uint8_t* regs, uint8_t gbsMode, bool force60);

