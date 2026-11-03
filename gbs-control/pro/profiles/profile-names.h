// Profile name rules (pure, host-testable)
#pragma once
#include <stdint.h>
#include <stddef.h>

static const uint8_t PROFILE_NAME_MAX = 24;
static const uint8_t PROFILE_NAME_BUF = PROFILE_NAME_MAX + 1;   // char buffer for a name
static const uint8_t PROFILE_BASE_NONE = 0xFF;                   // profile.base_preset not set
// Regions (profile.region, stored by name): the order is the menu's cycle order
static const uint8_t PROFILE_REGION_NTSC_U = 0;
static const uint8_t PROFILE_REGION_NTSC_J = 1;
static const uint8_t PROFILE_REGION_PAL = 2;
static const uint8_t PROFILE_REGION_PAL60 = 3;
static const uint8_t PROFILE_REGION_NONE = 4;
static const uint8_t PROFILE_REGION_COUNT = 5;
const char* profileRegionName(uint8_t r);              // out of range -> "None"
uint8_t profileRegionIndex(const char* s);              // old "NTSC"/"JP" accepted; unknown -> PROFILE_REGION_NONE
// "<base> <REGION>" within 24 characters (the base is shortened, the suffix kept); None adds nothing
void profileNameWithRegion(const char* base, uint8_t region, char* out, size_t n);
// The name without a trailing " <its own region>" word (or that region's old name: NTSC-U "NTSC", NTSC-J "JP"),
// for rows that show the region apart; region None or a name that is only the region: unchanged
void profileNameWithoutRegion(const char* name, uint8_t region, char* out, size_t n);
// What messages name (toasts, pop-up, log, web status): the name with its own region once, within 24
void profileLabel(const char* name, uint8_t region, char* out, size_t n);
// Region of a new profile: PAL-60 when the decoder reports PAL60 (AD_RESULT 3), PAL on a
// 50 Hz source, else NTSC-U
uint8_t profileDefaultRegion(bool pal50Hz, int8_t advStandard);
// Region a Replace writes: with a new name that name's region (PROFILE_REGION_NONE for a typed name, which has
// none), without one PROFILE_REGION_COUNT (keep the profile's own)
uint8_t profileReplaceRegion(bool named, uint8_t region);

// Disallowed characters become spaces; trimmed; cut to 24; false when nothing is left
bool profileNameSanitize(const char* in, char* out, size_t n);
// Case-insensitive (ASCII); equal ignoring case only when the bytes are equal too
int profileNameCompare(const char* a, const char* b);
// Uniqueness test: equal ignoring case
bool profileNameSame(const char* a, const char* b);

typedef bool (*ProfileNameTaken)(const char* name, void* ctx);
// base if free, else "base 2", "base 3"... (base shortened so the result stays within 24 characters)
void profileNameUnique(const char* base, ProfileNameTaken taken, void* ctx, char* out, size_t n);

void profileExportFileName(const char* name, char* out, size_t n);   // "<name>.gbscprofile.txt", / : ' and space -> _
// "<input> <resolution>" for OSD/OLED "Save as new" without a keyboard; "Profile" without a signal
void profileGeneratedName(uint8_t input, uint8_t sourceMode, bool interlaced, bool hasSignal, char* out, size_t n);
