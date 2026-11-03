// Profile file format: header lines, register lines (pure, host-testable).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../settings/settings-store.h"
#include "profile-names.h"

static const size_t PROFILE_MAX_FILE_BYTES = 16384;
static const size_t PROFILE_HEADER_BYTES = 1024;   // the boot list reads only this much
static const uint16_t PROFILE_REG_BYTES = 432;
static const uint8_t PROFILE_REG_LINES = 9;        // 48 bytes = 96 hex chars per line

struct ProfileMeta {
    char name[PROFILE_NAME_BUF];
    char region[8];          // profileRegionName(): "NTSC-U", "NTSC-J", "PAL", "PAL60", "None" (old files: "NTSC", "JP")
    uint8_t basePreset;      // output.preset value before Custom; PROFILE_BASE_NONE = unknown
    uint8_t modes;           // bit per register mode index (profile-modes.h)
    bool hasFp;
    uint8_t fpInput, fpMode, fpInterlaced;
    uint16_t fpLines;
    uint32_t fpMilliHz;      // field rate of the source in mHz when the fingerprint was taken; 0 = not known
    uint8_t adcGain;         // 255 - ADC_RGCTRL, 0 = no override
};

void profileMetaInit(ProfileMeta& m);
struct ProfileEntry;
void profileEntryFromMeta(ProfileEntry& e, const ProfileMeta& m);   // list entry fields from a parsed header
void profileWriteHeader(const ProfileMeta& m, SettingsSink sink, void* ctx);   // schema + profile.* lines
bool profileParseHeaderLine(const char* line, ProfileMeta& m);               // true for schema/profile.* lines
void profileWriteRegs(uint8_t modeIndex, const uint8_t* regs, SettingsSink sink, void* ctx);

// Collects the 9 reg.<mode>.<n> lines of one mode; lines of other modes are ignored
struct ProfileRegReader {
    uint8_t modeIndex;
    uint8_t* out;
    uint16_t seenMask;
    bool bad;
    void begin(uint8_t mode, uint8_t* buffer);
    void line(const char* l);
    bool complete() const;
};

// Save: true when a line of the old file must be copied (register lines of other modes)
// PROFILE_KEEP_ALL keeps every register line (save without a signal or in a mode without capture)
static const uint8_t PROFILE_KEEP_ALL = 0xFF;
bool profileKeepOldLine(const char* line, uint8_t currentModeIndex);
// Reset geometry: copy a profile without one mode's registers; header lines are dropped (rewritten)
bool profileForgetKeepsLine(const char* line, uint8_t modeIndex);
