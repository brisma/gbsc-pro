// Request parameters of the profile routes (pure, host-tested): POST /api/profiles/saveas and /replace
#pragma once
#include <stdint.h>
#include "profile-names.h"

struct ProfileOpArgs {
    char name[PROFILE_NAME_BUF];   // sanitised; "" = none (replace keeps the profile's own name and region)
    uint8_t region;                // profileRegionName() index
};
enum ProfileArgError : uint8_t { PARG_OK, PARG_NAME, PARG_REGION };

// name required; region optional (absent: None); region names exactly as profileRegionName ("NTSC-U", "NTSC-J",
// "PAL", "PAL60", "None"); the old file names ("NTSC", "JP") are not API names
ProfileArgError profileSaveAsArgs(const char* name, const char* region, ProfileOpArgs* out);
// Both optional; a name brings its region (absent: None, as a typed name in the menu); a region without a name is
// refused (without a name the profile keeps its name and region)
ProfileArgError profileReplaceArgs(const char* name, const char* region, ProfileOpArgs* out);
