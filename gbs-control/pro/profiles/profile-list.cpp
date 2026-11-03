#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "profile-list.h"
#include "profile-names.h"

void ProfileList::clear() {
    memset(e_, 0, sizeof(e_));
    for (uint8_t i = 0; i < PROFILE_MAX; i++) e_[i].region = PROFILE_REGION_NONE;
}

int ProfileList::allocate() {
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {
        if (!e_[i].used) {
            memset(&e_[i], 0, sizeof(e_[i]));
            e_[i].used = true;
            e_[i].basePreset = PROFILE_BASE_NONE;
            e_[i].region = PROFILE_REGION_NONE;
            return i;
        }
    }
    return -1;
}

void ProfileList::remove(uint8_t i) { if (i < PROFILE_MAX) memset(&e_[i], 0, sizeof(e_[i])); }

int ProfileList::findByName(const char* name) const {
    for (uint8_t i = 0; i < PROFILE_MAX; i++) if (e_[i].used && profileNameSame(e_[i].name, name)) return i;
    return -1;
}

int ProfileList::findByNameRegion(const char* name, uint8_t region, int skip) const {
    char a[PROFILE_NAME_BUF], b[PROFILE_NAME_BUF];
    profileNameWithoutRegion(name, region, a, sizeof(a));
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {
        if (!e_[i].used || (int)i == skip || e_[i].region != region) continue;
        profileNameWithoutRegion(e_[i].name, e_[i].region, b, sizeof(b));
        if (profileNameSame(a, b)) return i;
    }
    return -1;
}

bool profileNameTakenIn(const ProfileList& list, const char* name, uint8_t region, int self) {
    return list.findByNameRegion(name, region, self) >= 0;
}

uint8_t ProfileList::count() const {
    uint8_t c = 0;
    for (uint8_t i = 0; i < PROFILE_MAX; i++) if (e_[i].used) c++;
    return c;
}

uint8_t ProfileList::sorted(uint8_t* idx, uint8_t max) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < PROFILE_MAX && n < max; i++) {
        if (!e_[i].used) continue;
        uint8_t j = n++;
        while (j > 0 && profileNameCompare(e_[idx[j - 1]].name, e_[i].name) > 0) { idx[j] = idx[j - 1]; j--; }   // insertion sort
        idx[j] = i;
    }
    return n;
}

void ProfileList::idOf(uint8_t i, char out[4]) { snprintf(out, 4, "p%02u", (unsigned)i); }

int ProfileList::indexOfId(const char* id) {
    if (!id || id[0] != 'p' || id[1] < '0' || id[1] > '9' || id[2] < '0' || id[2] > '9' || id[3]) return -1;
    int v = (id[1] - '0') * 10 + (id[2] - '0');
    return v < PROFILE_MAX ? v : -1;
}

void ProfileList::serializeLastused(SettingsSink sink, void* ctx) const {
    char line[24];
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {
        if (!e_[i].used) continue;
        snprintf(line, sizeof(line), "p%02u=%lu\n", (unsigned)i, (unsigned long)e_[i].lastused);
        sink(line, ctx);
    }
}

void ProfileList::parseLastusedLine(const char* line) {
    char id[4];
    if (strlen(line) < 5 || line[3] != '=') return;
    memcpy(id, line, 3); id[3] = '\0';
    int i = indexOfId(id);
    if (i < 0 || !e_[i].used) return;
    e_[i].lastused = (uint32_t)strtoul(line + 4, nullptr, 10);
}

uint32_t ProfileList::nextLastused() const {
    uint32_t m = 0;
    for (uint8_t i = 0; i < PROFILE_MAX; i++) if (e_[i].used && e_[i].lastused > m) m = e_[i].lastused;
    return m + 1;
}
