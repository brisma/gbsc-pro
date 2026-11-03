// /api/profiles JSON (pure, host-tested): built from a copy of the list taken when the request arrives, so every
// chunk of the chunked answer rebuilds the same text whatever the loop changes meanwhile
#pragma once
#include <stdint.h>
#include "profile-list.h"

struct ProfilesSnapshot {
    ProfileList list;
    int active;          // list index, -1 none
    bool modified;
    uint8_t autoload;
    uint64_t match;      // bit per list index: the fingerprint matches the signal now (profileMatches)
};

void profilesSnapshotTake(ProfilesSnapshot& s, const ProfileList& list, int active, bool modified, uint8_t autoload,
                          uint64_t match = 0);
// JsonReplay builder; state = const ProfilesSnapshot*
void profilesJson(void (*sink)(const char*, void*), void* ctx, const void* state);
