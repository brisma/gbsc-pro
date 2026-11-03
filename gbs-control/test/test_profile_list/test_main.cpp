#include <string.h>
#include <string>
#include <unity.h>
#include "profile-list.h"
#include "profile-names.h"
#include "profile-json.h"
#include "profile-args.h"
#include "api-json.h"

void setUp(void) {}
void tearDown(void) {}
static void sink(const char* t, void* c) { ((std::string*)c)->append(t); }

static int add(ProfileList& l, const char* name) {
    int i = l.allocate();
    if (i >= 0) strcpy(l.at((uint8_t)i).name, name);
    return i;
}

void test_allocate_fills_holes_and_limit(void) {
    ProfileList l; l.clear();
    TEST_ASSERT_EQUAL_INT(0, add(l, "a"));
    TEST_ASSERT_EQUAL_INT(1, add(l, "b"));
    TEST_ASSERT_EQUAL_INT(2, add(l, "c"));
    l.remove(1);
    TEST_ASSERT_EQUAL_UINT8(2, l.count());
    TEST_ASSERT_EQUAL_INT(1, add(l, "d"));
    for (int i = 3; i < PROFILE_MAX; i++) add(l, "x");
    TEST_ASSERT_EQUAL_UINT8(PROFILE_MAX, l.count());
    TEST_ASSERT_EQUAL_INT(-1, l.allocate());
}

void test_find_and_sort(void) {
    ProfileList l; l.clear();
    add(l, "snes"); add(l, "Amiga"); add(l, "mega drive"); add(l, "NES");
    TEST_ASSERT_EQUAL_INT(2, l.findByName("MEGA DRIVE"));
    TEST_ASSERT_EQUAL_INT(-1, l.findByName("Saturn"));
    uint8_t idx[PROFILE_MAX];
    TEST_ASSERT_EQUAL_UINT8(4, l.sorted(idx, PROFILE_MAX));
    TEST_ASSERT_EQUAL_STRING("Amiga", l.at(idx[0]).name);
    TEST_ASSERT_EQUAL_STRING("mega drive", l.at(idx[1]).name);
    TEST_ASSERT_EQUAL_STRING("NES", l.at(idx[2]).name);
    TEST_ASSERT_EQUAL_STRING("snes", l.at(idx[3]).name);
}

void test_ids(void) {
    char id[4];
    ProfileList::idOf(7, id);
    TEST_ASSERT_EQUAL_STRING("p07", id);
    TEST_ASSERT_EQUAL_INT(35, ProfileList::indexOfId("p35"));
    TEST_ASSERT_EQUAL_INT(0, ProfileList::indexOfId("p00"));
    TEST_ASSERT_EQUAL_INT(-1, ProfileList::indexOfId("p36"));
    TEST_ASSERT_EQUAL_INT(-1, ProfileList::indexOfId("x07"));
    TEST_ASSERT_EQUAL_INT(-1, ProfileList::indexOfId("p7"));
    TEST_ASSERT_EQUAL_INT(-1, ProfileList::indexOfId("p070"));
}

void test_lastused_round_trip(void) {
    ProfileList l; l.clear();
    add(l, "a"); add(l, "b");
    l.at(0).lastused = 5; l.at(1).lastused = 9;
    std::string out; l.serializeLastused(sink, &out);
    TEST_ASSERT_EQUAL_STRING("p00=5\np01=9\n", out.c_str());
    ProfileList r; r.clear(); add(r, "a"); add(r, "b");
    r.parseLastusedLine("p01=9"); r.parseLastusedLine("p00=5"); r.parseLastusedLine("p30=7"); r.parseLastusedLine("junk");
    TEST_ASSERT_EQUAL_UINT32(5, r.at(0).lastused);
    TEST_ASSERT_EQUAL_UINT32(9, r.at(1).lastused);
    TEST_ASSERT_EQUAL_UINT32(10, r.nextLastused());
}

void test_region_defaults_to_none(void) {
    ProfileList l; l.clear();
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, l.at(0).region);
    int i = l.allocate();
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, l.at((uint8_t)i).region);
}

static int addR(ProfileList& l, const char* name, uint8_t region) {
    int i = add(l, name);
    l.at((uint8_t)i).region = region;
    return i;
}

// Uniqueness is (name, region); a trailing region word does not make two names different
void test_find_by_name_region(void) {
    ProfileList l; l.clear();
    int u = addR(l, "SNES", PROFILE_REGION_NTSC_U), j = addR(l, "SNES", PROFILE_REGION_NTSC_J);
    TEST_ASSERT_EQUAL_INT(j, l.findByNameRegion("snes", PROFILE_REGION_NTSC_J));
    TEST_ASSERT_EQUAL_INT(u, l.findByNameRegion("SNES", PROFILE_REGION_NTSC_U));
    TEST_ASSERT_EQUAL_INT(-1, l.findByNameRegion("SNES", PROFILE_REGION_PAL));
    TEST_ASSERT_EQUAL_INT(j, l.findByNameRegion("SNES NTSC-J", PROFILE_REGION_NTSC_J));   // same label
}

static bool takenIn(const char* name, void* ctx) {
    ProfileNameCtx* c = (ProfileNameCtx*)ctx;
    return profileNameTakenIn(*c->list, name, c->region, c->self);
}

void test_unique_name_per_region(void) {
    ProfileList l; l.clear();
    addR(l, "SNES", PROFILE_REGION_NTSC_U);
    int j = addR(l, "SNES", PROFILE_REGION_NTSC_J);
    char out[25];
    ProfileNameCtx c = {&l, -1, PROFILE_REGION_PAL};
    profileNameUnique("SNES", takenIn, &c, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SNES", out);                      // a new region: the same name is free
    c.region = PROFILE_REGION_NTSC_J;
    profileNameUnique("SNES", takenIn, &c, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SNES 2", out);                    // Duplicate of SNES / NTSC-J
    c.self = j;
    profileNameUnique("SNES", takenIn, &c, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SNES", out);                      // Rename keeps its own name
}

// /api/profiles answers from a copy taken when the request arrives: a list change between chunks (a delete, the
// active profile switching) cut or spliced the JSON when every chunk re-read the live list
void test_profiles_json_from_a_snapshot_survives_list_changes(void) {
    ProfileList live; live.clear();
    add(live, "Mega Drive"); add(live, "Saturn"); add(live, "SNES");
    live.at(0).region = PROFILE_REGION_PAL;
    live.at(1).modes = 1 | 4;
    live.at(1).basePreset = 5;                                  // the active profile was saved from 1080p
    std::string whole;
    ProfilesSnapshot ref; profilesSnapshotTake(ref, live, 1, true, 1);
    profilesJson(sink, &whole, &ref);
    TEST_ASSERT_EQUAL_STRING("{\"active\":\"p01\",\"modified\":1,\"autoload\":1,\"base\":5,\"profiles\":["
                             "{\"id\":\"p00\",\"name\":\"Mega Drive\",\"region\":\"PAL\",\"fp\":\"\",\"lastused\":0,\"match\":0,\"modes\":[]},"
                             "{\"id\":\"p01\",\"name\":\"Saturn\",\"region\":\"None\",\"fp\":\"\",\"lastused\":0,\"match\":0,"
                             "\"modes\":[\"ntsc\",\"ntsc_480p\"]},"
                             "{\"id\":\"p02\",\"name\":\"SNES\",\"region\":\"None\",\"fp\":\"\",\"lastused\":0,\"match\":0,\"modes\":[]}]}",
                             whole.c_str());

    ProfilesSnapshot snap; profilesSnapshotTake(snap, live, 1, true, 1);
    JsonReplay r = {profilesJson, &snap, 0};
    std::string got;
    char buf[40];
    size_t n = jsonReplayFill(r, buf, sizeof(buf));
    got.append(buf, n);
    live.remove(1); live.remove(2);                    // the loop deletes two profiles before the next ACK
    strcpy(live.at(0).name, "Renamed");
    while ((n = jsonReplayFill(r, buf, sizeof(buf))) > 0) got.append(buf, n);
    TEST_ASSERT_EQUAL_STRING(whole.c_str(), got.c_str());
}

// The web page marks the profiles whose fingerprint matches the signal now ("Match" pill)
void test_profiles_json_match_flag(void) {
    ProfileList l; l.clear();
    add(l, "Mega Drive"); add(l, "Saturn"); add(l, "SNES");
    ProfilesSnapshot s; profilesSnapshotTake(s, l, -1, false, 0, (1ull << 2) | (1ull << 35));
    std::string j;
    profilesJson(sink, &j, &s);
    TEST_ASSERT_TRUE(j.find("\"name\":\"SNES\",\"region\":\"None\",\"fp\":\"\",\"lastused\":0,\"match\":1,") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"name\":\"Saturn\",\"region\":\"None\",\"fp\":\"\",\"lastused\":0,\"match\":0,") != std::string::npos);
    ProfilesSnapshot none; profilesSnapshotTake(none, l, -1, false, 0);   // no signal: no match
    j.clear(); profilesJson(sink, &j, &none);
    TEST_ASSERT_TRUE(j.find("\"match\":1") == std::string::npos);
}

// A save as allocates the entry before its file is written: a GET /api/profiles served meanwhile (the LittleFS
// write yields to the web server) listed it with no name and region None (seen on the device)
// The resolution behind the active profile's Custom: unknown or Custom itself reads as 0 (960p, the fallback)
void test_profiles_json_base_is_zero_when_unknown(void) {
    for (uint8_t b : {(uint8_t)PROFILE_BASE_NONE, (uint8_t)2}) {
        ProfileList live; live.clear();
        add(live, "Mega Drive");
        live.at(0).basePreset = b;
        std::string whole;
        ProfilesSnapshot ref; profilesSnapshotTake(ref, live, 0, false, 0);
        profilesJson(sink, &whole, &ref);
        TEST_ASSERT_TRUE_MESSAGE(whole.find("\"base\":0,") != std::string::npos, whole.c_str());
    }
}

void test_profiles_snapshot_skips_entry_being_written(void) {
    ProfileList l; l.clear();
    add(l, "Mega Drive");
    l.allocate();                                       // the save as in progress: used, no name yet
    ProfilesSnapshot s; profilesSnapshotTake(s, l, -1, false, 0);
    std::string j;
    profilesJson(sink, &j, &s);
    TEST_ASSERT_EQUAL_STRING("{\"active\":\"\",\"modified\":0,\"autoload\":0,\"base\":0,\"profiles\":["
                             "{\"id\":\"p00\",\"name\":\"Mega Drive\",\"region\":\"None\",\"fp\":\"\",\"lastused\":0,\"match\":0,\"modes\":[]}]}",
                             j.c_str());
    TEST_ASSERT_TRUE(l.at(1).used);                     // the live list keeps the slot reserved
}

// POST /api/profiles/saveas name + region (region names as profileRegionName)
void test_saveas_region_param(void) {
    ProfileOpArgs a;
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileSaveAsArgs("Saturn", "NTSC-J", &a));
    TEST_ASSERT_EQUAL_STRING("Saturn", a.name);
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NTSC_J, a.region);
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileSaveAsArgs("  Mega  Drive ", nullptr, &a));   // no region: None
    TEST_ASSERT_EQUAL_STRING("Mega  Drive", a.name);
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, a.region);
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileSaveAsArgs("PS1", "PAL60", &a));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL60, a.region);
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileSaveAsArgs("PS1", "None", &a));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, a.region);
    TEST_ASSERT_EQUAL_UINT8(PARG_REGION, profileSaveAsArgs("PS1", "Europe", &a));
    TEST_ASSERT_EQUAL_UINT8(PARG_REGION, profileSaveAsArgs("PS1", "NTSC", &a));   // old file names are not API names
    TEST_ASSERT_EQUAL_UINT8(PARG_NAME, profileSaveAsArgs(nullptr, "PAL", &a));
    TEST_ASSERT_EQUAL_UINT8(PARG_NAME, profileSaveAsArgs("   ", "PAL", &a));
}

// POST /api/profiles/replace: optional name and region; a region needs a name (without one the profile keeps both)
void test_replace_name_region_params(void) {
    ProfileOpArgs a;
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileReplaceArgs(nullptr, nullptr, &a));
    TEST_ASSERT_EQUAL_STRING("", a.name);
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileReplaceArgs("Saturn", "PAL", &a));
    TEST_ASSERT_EQUAL_STRING("Saturn", a.name);
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL, a.region);
    TEST_ASSERT_EQUAL_UINT8(PARG_OK, profileReplaceArgs("Saturn", nullptr, &a));   // typed name: no region
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, a.region);
    TEST_ASSERT_EQUAL_UINT8(PARG_REGION, profileReplaceArgs(nullptr, "PAL", &a));
    TEST_ASSERT_EQUAL_UINT8(PARG_REGION, profileReplaceArgs("Saturn", "Mars", &a));
    TEST_ASSERT_EQUAL_UINT8(PARG_NAME, profileReplaceArgs("\x01\x02", nullptr, &a));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_allocate_fills_holes_and_limit);
    RUN_TEST(test_find_and_sort);
    RUN_TEST(test_ids);
    RUN_TEST(test_lastused_round_trip);
    RUN_TEST(test_region_defaults_to_none);
    RUN_TEST(test_find_by_name_region);
    RUN_TEST(test_unique_name_per_region);
    RUN_TEST(test_profiles_json_from_a_snapshot_survives_list_changes);
    RUN_TEST(test_profiles_json_match_flag);
    RUN_TEST(test_profiles_json_base_is_zero_when_unknown);
    RUN_TEST(test_profiles_snapshot_skips_entry_being_written);
    RUN_TEST(test_saveas_region_param);
    RUN_TEST(test_replace_name_region_params);
    return UNITY_END();
}
