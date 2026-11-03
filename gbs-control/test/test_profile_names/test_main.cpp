#include <string.h>
#include <set>
#include <string>
#include <unity.h>
#include "profile-names.h"

void setUp(void) {}
void tearDown(void) {}

static bool takenInSet(const char* name, void* ctx) {
    std::set<std::string>* s = (std::set<std::string>*)ctx;
    for (const std::string& t : *s) if (profileNameSame(t.c_str(), name)) return true;
    return false;
}

void test_sanitize(void) {
    char out[32];
    TEST_ASSERT_TRUE(profileNameSanitize("  Mega Drive <NTSC>  ", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("Mega Drive  NTSC", out);
    TEST_ASSERT_FALSE(profileNameSanitize("   ", out, sizeof(out)));
    TEST_ASSERT_FALSE(profileNameSanitize("", out, sizeof(out)));
    TEST_ASSERT_TRUE(profileNameSanitize("abcdefghijklmnopqrstuvwxyz0123", out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(24, strlen(out));
    TEST_ASSERT_TRUE(profileNameSanitize("A-B.C/D:E+F=G'H", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("A-B.C/D:E+F=G'H", out);
}

void test_compare_is_case_insensitive(void) {
    TEST_ASSERT_TRUE(profileNameCompare("amiga", "Mega Drive") < 0);
    TEST_ASSERT_TRUE(profileNameCompare("Mega Drive", "amiga") > 0);
    TEST_ASSERT_EQUAL_INT(0, profileNameCompare("NES", "NES"));
    TEST_ASSERT_TRUE(profileNameSame("snes", "SNES"));
    TEST_ASSERT_FALSE(profileNameSame("snes", "snes 2"));
}

void test_unique_suffix_and_cap(void) {
    std::set<std::string> taken = {"RGBs 240p", "RGBs 240p 2"};
    char out[32];
    profileNameUnique("RGBs 240p", takenInSet, &taken, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("RGBs 240p 3", out);
    profileNameUnique("rgbs 240P", takenInSet, &taken, out, sizeof(out));   // case-insensitive clash
    TEST_ASSERT_EQUAL_STRING("rgbs 240P 3", out);
    profileNameUnique("Neo Geo", takenInSet, &taken, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Neo Geo", out);
    std::set<std::string> long1 = {"abcdefghijklmnopqrstuvwx"};
    profileNameUnique("abcdefghijklmnopqrstuvwx", takenInSet, &long1, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrstuv 2", out);
    TEST_ASSERT_EQUAL_UINT32(24, strlen(out));
}

void test_export_and_generated_names(void) {
    char out[64];
    profileExportFileName("Mega Drive: PAL/50", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Mega_Drive__PAL_50.gbscprofile.txt", out);
    profileGeneratedName(1, 1, false, true, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("RGB SCART 240p", out);
    profileGeneratedName(4, 1, true, true, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Component 480i", out);
    profileGeneratedName(5, 2, true, true, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("S-Video 576i", out);
    profileGeneratedName(3, 14, false, true, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("VGA", out);
    profileGeneratedName(1, 0, false, false, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Profile", out);
}

void test_region_names_round_trip(void) {
    for (uint8_t r = 0; r < PROFILE_REGION_COUNT; r++) TEST_ASSERT_EQUAL_UINT8(r, profileRegionIndex(profileRegionName(r)));
    TEST_ASSERT_EQUAL_STRING("None", profileRegionName(9));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, profileRegionIndex("xx"));
}

void test_region_labels_and_old_aliases(void) {
    TEST_ASSERT_EQUAL_STRING("NTSC-U", profileRegionName(PROFILE_REGION_NTSC_U));
    TEST_ASSERT_EQUAL_STRING("NTSC-J", profileRegionName(PROFILE_REGION_NTSC_J));
    TEST_ASSERT_EQUAL_STRING("PAL", profileRegionName(PROFILE_REGION_PAL));
    TEST_ASSERT_EQUAL_STRING("PAL60", profileRegionName(PROFILE_REGION_PAL60));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NTSC_U, profileRegionIndex("NTSC"));   // files written by older builds
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NTSC_J, profileRegionIndex("JP"));
}

// What a message names: the stored name with its region once (two "Mega Drive" profiles differ only by region;
// the auto-load toast and log said just "Mega Drive")
void test_profile_label(void) {
    char n[25];
    profileLabel("Mega Drive", PROFILE_REGION_PAL, n, sizeof(n));
    TEST_ASSERT_EQUAL_STRING("Mega Drive PAL", n);
    profileLabel("Mega Drive NTSC-U", PROFILE_REGION_NTSC_U, n, sizeof(n));   // already in the name: not twice
    TEST_ASSERT_EQUAL_STRING("Mega Drive NTSC-U", n);
    profileLabel("Saturn JP", PROFILE_REGION_NTSC_J, n, sizeof(n));          // old region word replaced
    TEST_ASSERT_EQUAL_STRING("Saturn NTSC-J", n);
    profileLabel("My setup", PROFILE_REGION_NONE, n, sizeof(n));
    TEST_ASSERT_EQUAL_STRING("My setup", n);
}

void test_name_with_region(void) {
    char n[25];
    profileNameWithRegion("Saturn", PROFILE_REGION_NTSC_J, n, sizeof(n));
    TEST_ASSERT_EQUAL_STRING("Saturn NTSC-J", n);
    profileNameWithRegion("Saturn", PROFILE_REGION_NONE, n, sizeof(n));
    TEST_ASSERT_EQUAL_STRING("Saturn", n);
    profileNameWithRegion("SNES/Super Famicom Long", PROFILE_REGION_NTSC_U, n, sizeof(n));   // suffix kept, within 24
    TEST_ASSERT_EQUAL_STRING("SNES/Super Famico NTSC-U", n);
    TEST_ASSERT_TRUE(strlen(n) <= 24);
}

// Display: a trailing region word is shown as the row's value, so the name drops it (old names included)
void test_name_without_region_suffix(void) {
    char out[32];
    profileNameWithoutRegion("Genesis/Mega Drive NTSC-U", PROFILE_REGION_NTSC_U, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Genesis/Mega Drive", out);
    profileNameWithoutRegion("Saturn JP", PROFILE_REGION_NTSC_J, out, sizeof(out));      // old name
    TEST_ASSERT_EQUAL_STRING("Saturn", out);
    profileNameWithoutRegion("Genesis NTSC", PROFILE_REGION_NTSC_U, out, sizeof(out));   // old name
    TEST_ASSERT_EQUAL_STRING("Genesis", out);
    profileNameWithoutRegion("SNES PAL60", PROFILE_REGION_PAL60, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SNES", out);
    profileNameWithoutRegion("Euro PAL", PROFILE_REGION_NONE, out, sizeof(out));         // no region: kept
    TEST_ASSERT_EQUAL_STRING("Euro PAL", out);
    profileNameWithoutRegion("SNES PAL", PROFILE_REGION_NTSC_U, out, sizeof(out));       // another region: kept
    TEST_ASSERT_EQUAL_STRING("SNES PAL", out);
    profileNameWithoutRegion("My NTSC setup", PROFILE_REGION_NTSC_U, out, sizeof(out));  // not at the end
    TEST_ASSERT_EQUAL_STRING("My NTSC setup", out);
    profileNameWithoutRegion("PAL", PROFILE_REGION_PAL, out, sizeof(out));               // only the region
    TEST_ASSERT_EQUAL_STRING("PAL", out);
}

// The default region of a new profile follows the signal
void test_default_region(void) {
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL, profileDefaultRegion(true, -1));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NTSC_U, profileDefaultRegion(false, -1));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL60, profileDefaultRegion(true, 3));   // decoder reports PAL60
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL60, profileDefaultRegion(false, 3));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL, profileDefaultRegion(true, 4));
}

// Replace with a new name takes that name's region (a typed name has none), no name keeps it
void test_replace_region(void) {
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_PAL, profileReplaceRegion(true, PROFILE_REGION_PAL));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, profileReplaceRegion(true, PROFILE_REGION_NONE));   // typed name
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_COUNT, profileReplaceRegion(false, PROFILE_REGION_PAL));  // keep its own
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NONE, profileReplaceRegion(true, 9));                     // out of range
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sanitize);
    RUN_TEST(test_compare_is_case_insensitive);
    RUN_TEST(test_unique_suffix_and_cap);
    RUN_TEST(test_export_and_generated_names);
    RUN_TEST(test_region_names_round_trip);
    RUN_TEST(test_region_labels_and_old_aliases);
    RUN_TEST(test_name_with_region);
    RUN_TEST(test_name_without_region_suffix);
    RUN_TEST(test_default_region);
    RUN_TEST(test_replace_region);
    RUN_TEST(test_profile_label);
    return UNITY_END();
}
