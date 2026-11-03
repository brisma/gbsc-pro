// In-RAM list of profiles (pure, host-testable)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../settings/settings-store.h"
#include "profile-names.h"

static const uint8_t PROFILE_MAX = 36;

struct ProfileEntry {
    bool used;
    char name[PROFILE_NAME_BUF];
    uint8_t modes;            // register modes captured (bit per profile-modes index)
    bool hasFp;
    uint8_t fpInput, fpMode, fpInterlaced;
    uint16_t fpLines;
    uint32_t fpMilliHz;       // field rate in mHz, 0 = not known (profileRateFits)
    uint8_t basePreset;
    uint8_t region;           // profileRegionName() index; PROFILE_REGION_NONE when unset
    uint32_t lastused;
};

class ProfileList {
public:
    void clear();
    int allocate();                                   // lowest free index, -1 when full
    ProfileEntry& at(uint8_t i) { return e_[i]; }
    const ProfileEntry& at(uint8_t i) const { return e_[i]; }
    void remove(uint8_t i);
    int findByName(const char* name) const;           // case-insensitive, -1 when absent
    // Same label: case-insensitive name without its region word, and the same region
    int findByNameRegion(const char* name, uint8_t region, int skip = -1) const;
    uint8_t count() const;
    uint8_t sorted(uint8_t* idx, uint8_t max) const;  // used indices sorted by name
    static void idOf(uint8_t i, char out[4]);         // "p07"
    static int indexOfId(const char* id);             // "p07" -> 7, -1 when invalid
    void serializeLastused(SettingsSink sink, void* ctx) const;
    void parseLastusedLine(const char* line);
    uint32_t nextLastused() const;
private:
    ProfileEntry e_[PROFILE_MAX];
};

// Uniqueness context for profileNameUnique: (name, region) must be free among the other profiles
struct ProfileNameCtx { const ProfileList* list; int self; uint8_t region; };
bool profileNameTakenIn(const ProfileList& list, const char* name, uint8_t region, int self);
