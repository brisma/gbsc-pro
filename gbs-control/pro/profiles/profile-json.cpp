#include <stdio.h>
#include "profile-json.h"
#include "profile-modes.h"
#include "profile-names.h"
#include "../input/input-types.h"
#include "../util/pgm.h"

void profilesSnapshotTake(ProfilesSnapshot& s, const ProfileList& list, int active, bool modified, uint8_t autoload,
                          uint64_t match) {
    s.list = list;
    for (uint8_t i = 0; i < PROFILE_MAX; i++)   // allocated by a save as whose file is still being written: no name yet
        if (s.list.at(i).used && !s.list.at(i).name[0]) s.list.at(i).used = false;
    s.active = active;
    s.modified = modified;
    s.autoload = autoload;
    s.match = match;
}

static void fpSummary(const ProfileEntry& e, char* out, size_t n) {
    if (!e.hasFp || !inputTypeValid(e.fpInput)) { if (n) out[0] = '\0'; return; }
    bool pal = e.fpMode == 2 || e.fpMode == 4;
    snprintf_P(out, n, PSTR("%s \xC2\xB7 %u lines \xC2\xB7 %s%s"), inputLongName(e.fpInput), e.fpLines, pal ? "PAL" : "NTSC",
               e.fpInterlaced ? " \xC2\xB7 interlaced" : "");
}

static const uint8_t kOutputCustomized = 2;   // options.h OutputCustomized

void profilesJson(void (*sink)(const char*, void*), void* ctx, const void* state) {
    const ProfilesSnapshot& s = *(const ProfilesSnapshot*)state;
    char t[200], id[4] = "";
    if (s.active >= 0) ProfileList::idOf((uint8_t)s.active, id);
    // base: the output.preset value behind the active profile's Custom (0 without a profile or when it is unknown)
    uint8_t base = 0;
    if (s.active >= 0) {
        uint8_t b = s.list.at((uint8_t)s.active).basePreset;
        if (b != PROFILE_BASE_NONE && b != kOutputCustomized) base = b;
    }
    snprintf_P(t, sizeof(t), PSTR("{\"active\":\"%s\",\"modified\":%u,\"autoload\":%u,\"base\":%u,\"profiles\":["), id,
               s.modified ? 1 : 0, s.autoload, base);
    sink(t, ctx);
    uint8_t idx[PROFILE_MAX];
    uint8_t n = s.list.sorted(idx, PROFILE_MAX);
    for (uint8_t k = 0; k < n; k++) {
        const ProfileEntry& e = s.list.at(idx[k]);
        ProfileList::idOf(idx[k], id);
        char fp[64]; fpSummary(e, fp, sizeof(fp));
        snprintf_P(t, sizeof(t), PSTR("%s{\"id\":\"%s\",\"name\":\"%s\",\"region\":\"%s\",\"fp\":\"%s\",\"lastused\":%lu,\"match\":%u,\"modes\":["),
                   k ? "," : "", id, e.name, profileRegionName(e.region), fp, (unsigned long)e.lastused,
                   (unsigned)((s.match >> idx[k]) & 1));
        sink(t, ctx);
        bool first = true;
        for (uint8_t m = 0; m < PROFILE_MODE_COUNT; m++) {
            if (!(e.modes & (1 << m))) continue;
            snprintf(t, sizeof(t), "%s\"%s\"", first ? "" : ",", profileModeName(m));
            sink(t, ctx);
            first = false;
        }
        sink("]}", ctx);
    }
    sink("]}", ctx);
}
