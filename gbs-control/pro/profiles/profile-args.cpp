#include <string.h>
#include "profile-args.h"

// Exact API name -> index; false for anything else
static bool regionParam(const char* s, uint8_t* out) {
    for (uint8_t r = 0; r < PROFILE_REGION_COUNT; r++)
        if (!strcmp(s, profileRegionName(r))) { *out = r; return true; }
    return false;
}

ProfileArgError profileSaveAsArgs(const char* name, const char* region, ProfileOpArgs* out) {
    out->name[0] = '\0';
    out->region = PROFILE_REGION_NONE;
    if (!name || !profileNameSanitize(name, out->name, PROFILE_NAME_BUF)) return PARG_NAME;
    if (region && !regionParam(region, &out->region)) return PARG_REGION;
    return PARG_OK;
}

ProfileArgError profileReplaceArgs(const char* name, const char* region, ProfileOpArgs* out) {
    out->name[0] = '\0';
    out->region = PROFILE_REGION_NONE;
    if (!name) return region ? PARG_REGION : PARG_OK;
    if (!profileNameSanitize(name, out->name, PROFILE_NAME_BUF)) return PARG_NAME;
    if (region && !regionParam(region, &out->region)) return PARG_REGION;
    return PARG_OK;
}
