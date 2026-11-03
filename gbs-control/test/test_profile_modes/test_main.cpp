#include <string.h>
#include <unity.h>
#include "profile-modes.h"
#include "profile-file.h"

void setUp(void) {}
void tearDown(void) {}

void test_mode_table(void) {
    const uint8_t modes[] = {1, 2, 3, 4, 8, 14};
    const char* names[] = {"ntsc", "pal", "ntsc_480p", "pal_576p", "medium_res", "vga_upscale"};
    for (uint8_t i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_INT8(i, profileModeIndex(modes[i]));
        TEST_ASSERT_EQUAL_STRING(names[i], profileModeName(i));
        TEST_ASSERT_EQUAL_INT8(i, profileModeIndexByName(names[i]));
        TEST_ASSERT_EQUAL_UINT8(modes[i], profileModeAt(i));
    }
    const uint8_t none[] = {0, 5, 6, 7, 9, 13, 15};
    for (uint8_t m : none) TEST_ASSERT_EQUAL_INT8(-1, profileModeIndex(m));
    TEST_ASSERT_EQUAL_INT8(-1, profileModeIndexByName("ntsc_1080p"));
}

void test_reg_mode_undoes_pal_force60_only(void) {
    TEST_ASSERT_EQUAL_UINT8(2, profileRegModeOf(1, true));
    TEST_ASSERT_EQUAL_UINT8(4, profileRegModeOf(3, true));
    TEST_ASSERT_EQUAL_UINT8(1, profileRegModeOf(1, false));
    TEST_ASSERT_EQUAL_UINT8(8, profileRegModeOf(8, true));
}

// Output > Force 60Hz changed during a PAL picture applies at once (upstream waited for the next
// preset load); it reloads the preset only for the modes it remaps
void test_force60_change_reloads_only_pal_modes(void) {
    TEST_ASSERT_TRUE(profileForce60Reloads(2));                     // PAL 288p/576i
    TEST_ASSERT_TRUE(profileForce60Reloads(4));                     // PAL 576p
    TEST_ASSERT_TRUE(profileForce60Reloads(profileRegModeOf(1, true)));   // running forced: turning it off reloads
    TEST_ASSERT_FALSE(profileForce60Reloads(1));
    TEST_ASSERT_FALSE(profileForce60Reloads(3));
    TEST_ASSERT_FALSE(profileForce60Reloads(8));
    TEST_ASSERT_FALSE(profileForce60Reloads(14));
    TEST_ASSERT_FALSE(profileForce60Reloads(0));                    // no signal
}

// PAL Mega Drive under Force 60Hz at 1080p: upstream's fixed IF_VB_SP 4 for preset 0x05 started the
// capture ~36 lines early (picture low); the preset's own stop + 56 (every other output's rule) centred it
void test_force60_custom_registers_keep_their_mode_and_run_at_60(void) {
    TEST_ASSERT_TRUE(profileForce60Active(true, 2));
    TEST_ASSERT_TRUE(profileForce60Active(true, 4));
    TEST_ASSERT_FALSE(profileForce60Active(true, 1));
    TEST_ASSERT_FALSE(profileForce60Active(true, 3));
    TEST_ASSERT_FALSE(profileForce60Active(false, 2));
    TEST_ASSERT_EQUAL_UINT8(1, profileForce60PresetMode(true, false, 2));   // base preset: the NTSC twin
    TEST_ASSERT_EQUAL_UINT8(3, profileForce60PresetMode(true, false, 4));
    TEST_ASSERT_EQUAL_UINT8(2, profileForce60PresetMode(true, true, 2));    // registers captured at 60 Hz: their own mode
    TEST_ASSERT_EQUAL_UINT8(4, profileForce60PresetMode(true, true, 4));
    TEST_ASSERT_EQUAL_UINT8(2, profileForce60PresetMode(false, false, 2));
    TEST_ASSERT_EQUAL_UINT8(1, profileForce60PresetMode(true, false, 1));
    TEST_ASSERT_EQUAL_UINT8(8, profileForce60PresetMode(true, false, 8));
}

void test_force60_vertical_shift_is_the_same_for_every_output(void) {
    TEST_ASSERT_EQUAL_UINT16(66, profileForce60VbStop(10));          // ntsc_1920x1080: s1_1E = 0x0A
    TEST_ASSERT_EQUAL_UINT16(56 + 0x22, profileForce60VbStop(0x22));
    TEST_ASSERT_EQUAL_UINT16(0x7FF, profileForce60VbStop(0x7F0));    // 11-bit register
}

void test_source_mode_undoes_i2p_on_decoder_inputs(void) {
    TEST_ASSERT_EQUAL_UINT8(1, profileSourceModeOf(3, 6, true));    // Composite + I2P
    TEST_ASSERT_EQUAL_UINT8(2, profileSourceModeOf(4, 5, true));    // S-Video + I2P, PAL
    TEST_ASSERT_EQUAL_UINT8(3, profileSourceModeOf(3, 1, true));    // RGBs: real 480p
    TEST_ASSERT_EQUAL_UINT8(3, profileSourceModeOf(3, 6, false));
}

void test_preset_status_char(void) {
    const uint8_t hd = 0x21, rgbhv = 0x22;
    TEST_ASSERT_EQUAL_CHAR('9', profilePresetStatusChar(0x01, true, hd, rgbhv));
    TEST_ASSERT_EQUAL_CHAR('1', profilePresetStatusChar(0x11, false, hd, rgbhv));
    TEST_ASSERT_EQUAL_CHAR('6', profilePresetStatusChar(0x06, false, hd, rgbhv));
    TEST_ASSERT_EQUAL_CHAR('7', profilePresetStatusChar(0x07, false, hd, rgbhv));   // 1200p (issue #35)
    TEST_ASSERT_EQUAL_CHAR('7', profilePresetStatusChar(0x17, false, hd, rgbhv));
    TEST_ASSERT_EQUAL_CHAR('8', profilePresetStatusChar(hd, false, hd, rgbhv));
    TEST_ASSERT_EQUAL_CHAR('8', profilePresetStatusChar(rgbhv, false, hd, rgbhv));
    TEST_ASSERT_EQUAL_CHAR('0', profilePresetStatusChar(0x00, false, hd, rgbhv));
}

void test_interlace_follows_the_firmware_rule(void) {
    // GBS counts odd VPERIOD_IF for progressive 240p/288p, even for 480i/576i
    TEST_ASSERT_FALSE(profileIsInterlaced(1, 523));
    TEST_ASSERT_TRUE(profileIsInterlaced(1, 524));
    TEST_ASSERT_FALSE(profileIsInterlaced(2, 625));
    TEST_ASSERT_TRUE(profileIsInterlaced(2, 624));
    TEST_ASSERT_FALSE(profileIsInterlaced(3, 524));   // only modes 1/2 can be interlaced
}

void test_preset_for_load(void) {
    const uint8_t custom = 2;
    TEST_ASSERT_EQUAL_UINT8(custom, profilePresetForLoad(custom, true, 3));
    TEST_ASSERT_EQUAL_UINT8(3, profilePresetForLoad(custom, false, 3));
    TEST_ASSERT_EQUAL_UINT8(0, profilePresetForLoad(custom, false, 0xFF));   // unknown base: 960p
    TEST_ASSERT_EQUAL_UINT8(0, profilePresetForLoad(custom, false, 10));     // bypass is not a preset table
    TEST_ASSERT_EQUAL_UINT8(0, profilePresetForLoad(custom, false, custom));
    TEST_ASSERT_EQUAL_UINT8(5, profilePresetForLoad(5, false, 3));           // not custom: unchanged
}

// Defaults (by hand, or Startup = Defaults at power-on) keep the output resolution: factory picture
// at the resolution in use; Custom (a profile's scaling) becomes the standard resolution that profile was made from
void test_defaults_keep_the_output_resolution(void) {
    const uint8_t custom = 2;
    TEST_ASSERT_EQUAL_UINT8(5, profileDefaultsPreset(5, 0xFF));         // 1080p stays 1080p
    TEST_ASSERT_EQUAL_UINT8(3, profileDefaultsPreset(3, 5));            // a base only matters for Custom
    TEST_ASSERT_EQUAL_UINT8(10, profileDefaultsPreset(10, 0xFF));       // Bypass (standard board) stays
    TEST_ASSERT_EQUAL_UINT8(5, profileDefaultsPreset(custom, 5));       // Custom made from 1080p: 1080p
    TEST_ASSERT_EQUAL_UINT8(0, profileDefaultsPreset(custom, 0xFF));    // base unknown: 960p
    TEST_ASSERT_EQUAL_UINT8(0, profileDefaultsPreset(custom, custom));
}

// Issue #22: loading a profile by hand also switches to the input it was saved on
void test_load_switches_input(void) {
    TEST_ASSERT_TRUE(profileLoadSwitchesInput(true, true, 5, 1));
    TEST_ASSERT_FALSE(profileLoadSwitchesInput(false, true, 5, 1));   // option off
    TEST_ASSERT_FALSE(profileLoadSwitchesInput(true, false, 5, 1));   // no fingerprint: input unknown
    TEST_ASSERT_FALSE(profileLoadSwitchesInput(true, true, 1, 1));    // already there (auto-load)
    TEST_ASSERT_FALSE(profileLoadSwitchesInput(true, true, 0, 1));
    TEST_ASSERT_FALSE(profileLoadSwitchesInput(true, true, 7, 1));
}

// Pure rules behind the device-only profile store paths
void test_profile_store_rules(void) {
    TEST_ASSERT_FALSE(profileModifiedRule(true, 3, false));      // no active profile: never Unsaved
    TEST_ASSERT_TRUE(profileModifiedRule(true, 0, true));        // settings differ
    TEST_ASSERT_TRUE(profileModifiedRule(false, 1, true));       // a register action since the last save/load
    TEST_ASSERT_FALSE(profileModifiedRule(false, 0, true));
    TEST_ASSERT_TRUE(profileSaveKeepsFp(false, true));           // Save changes on an existing profile: untouched
    TEST_ASSERT_FALSE(profileSaveKeepsFp(false, false));         // a new profile captures the current signal
    TEST_ASSERT_FALSE(profileSaveKeepsFp(true, true));           // Replace with current moves it
    TEST_ASSERT_TRUE(profileStartupLoadsCurrent(0));             // Last used
    TEST_ASSERT_FALSE(profileStartupLoadsCurrent(1));            // Defaults
}

// Always 60Hz on a profile: PAL-mode registers keep the 1_2C bit 2 (GBS_OPTION_PALFORCED60_ENABLED)
// they were captured with; registers captured at the other rate are not used (the base preset plus upstream's remap
// gives the chosen rate) until Save changes captures them again
void test_force60_must_match_the_captured_pal_registers(void) {
    uint8_t regs[PROFILE_REG_BYTES] = {0};
    TEST_ASSERT_EQUAL_UINT16(92, PROFILE_REG_FORCE60_OFFSET);   // seg 0 0x40-0x5F, 0x90-0x9F, then seg 1 from 0x00
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 2, false));     // captured at 50 Hz, Always 60Hz off
    TEST_ASSERT_FALSE(profileRegsFitForce60(regs, 2, true));     // turned on: these 50 Hz registers do not fit
    TEST_ASSERT_FALSE(profileRegsFitForce60(regs, 4, true));
    regs[PROFILE_REG_FORCE60_OFFSET] = 0x04;                     // captured at 60 Hz
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 2, true));
    TEST_ASSERT_FALSE(profileRegsFitForce60(regs, 2, false));
    regs[PROFILE_REG_FORCE60_OFFSET] = 0x00;
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 1, true));      // NTSC and the other modes: not concerned
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 3, true));
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 14, true));
    // A Bypass capture keeps passthrough whatever Always 60Hz says (the rate follows the source there)
    regs[PROFILE_REG_PRESET_ID_OFFSET] = 0x21;                   // PresetHdBypass in 1_2B
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 2, true));
    regs[PROFILE_REG_PRESET_ID_OFFSET] = 0x22 | 0x80;            // PresetBypassRGBHV, custom mark (bit 7) set
    TEST_ASSERT_TRUE(profileRegsFitForce60(regs, 2, true));
    regs[PROFILE_REG_PRESET_ID_OFFSET] = 0x81;                   // a scaled preset with the custom mark
    TEST_ASSERT_FALSE(profileRegsFitForce60(regs, 2, true));
}


// "MD NTSC-U" with Watcher Off, then "MD PAL" (Inherit) kept it off, so the Mega Drive's region switch was never
// seen. Inherit turns it back on when a profile's Off turned it off; a hand toggle stays
void test_sync_watcher_off_lives_only_in_its_profile(void) {
    bool byProfile = false;
    TEST_ASSERT_FALSE(profileSyncWatcherNext(2, true, &byProfile));    // Off
    TEST_ASSERT_TRUE(byProfile);
    TEST_ASSERT_TRUE(profileSyncWatcherNext(0, false, &byProfile));    // Inherit after a profile's Off: on again
    TEST_ASSERT_FALSE(byProfile);
    TEST_ASSERT_FALSE(profileSyncWatcherNext(0, false, &byProfile));   // off by hand (Developer): stays off
    TEST_ASSERT_TRUE(profileSyncWatcherNext(0, true, &byProfile));     // Inherit leaves an on watcher on
    TEST_ASSERT_TRUE(profileSyncWatcherNext(1, false, &byProfile));    // On
    TEST_ASSERT_FALSE(byProfile);
    profileSyncWatcherNext(2, true, &byProfile);
    TEST_ASSERT_FALSE(profileSyncWatcherNext(2, true, &byProfile));    // Off again after an input change forced it on
    TEST_ASSERT_TRUE(profileSyncWatcherNext(1, false, &byProfile));
    TEST_ASSERT_FALSE(byProfile);
}

// The line rate is the console's own: its clock and its line length. Field rates measured on the unit: SNES 60.0999 Hz
// in 240p (262 lines) and 50.0065 / 49.9260 Hz in 288p / 576i; a PAL Mega Drive at 60 Hz 59.3755 Hz
void test_rate_tells_consoles_of_one_standard_apart(void) {
    const uint8_t rgb = 1, ntsc = 1, pal = 2;
    TEST_ASSERT_TRUE(profileRateFits(rgb, ntsc, 523, false, 60100, 523, false, 60099));    // the same console again
    TEST_ASSERT_FALSE(profileRateFits(rgb, ntsc, 523, false, 59376, 523, false, 60100));   // Mega Drive profile, SNES
    TEST_ASSERT_FALSE(profileRateFits(rgb, ntsc, 523, false, 59923, 523, false, 60100));   // an NTSC Mega Drive
    TEST_ASSERT_FALSE(profileRateFits(rgb, ntsc, 525, false, 59826, 523, false, 60099));   // PlayStation (263 lines)
    // 240p <-> 480i and 288p <-> 576i of one console: another field rate, the same line rate
    TEST_ASSERT_TRUE(profileRateFits(rgb, pal, 623, false, 50007, 624, true, 49926));
    TEST_ASSERT_TRUE(profileRateFits(rgb, pal, 624, true, 49926, 623, false, 50007));
    TEST_ASSERT_TRUE(profileRateFits(rgb, ntsc, 525, false, 59826, 524, true, 59940));     // PlayStation 240p / 480i
    TEST_ASSERT_FALSE(profileRateFits(rgb, ntsc, 523, false, 60100, 524, true, 59940));    // SNES profile, PlayStation 480i
    // one console whose games pick 262 or 263 lines: the field rate moves, the line rate does not
    TEST_ASSERT_TRUE(profileRateFits(rgb, ntsc, 523, false, 60054, 525, false, 59826));
}

void test_rate_is_not_judged_where_it_cannot_be(void) {
    TEST_ASSERT_TRUE(profileRateFits(1, 1, 523, false, 0, 523, false, 60100));       // a profile saved without it
    TEST_ASSERT_TRUE(profileRateFits(1, 1, 523, false, 59376, 523, false, 0));       // not measured now
    TEST_ASSERT_TRUE(profileRateFits(6, 1, 523, false, 59376, 523, false, 60100));   // Composite: through the decoder
    TEST_ASSERT_TRUE(profileRateFits(5, 1, 523, false, 59376, 523, false, 60100));   // S-Video
    TEST_ASSERT_TRUE(profileRateFits(1, 3, 525, false, 59940, 525, false, 60100));   // 480p and up: not judged
    TEST_ASSERT_TRUE(profileRateFits(1, 1, 524, false, 59376, 523, false, 60100));   // a progressive count that is not exact
}

// A reading taken while the signal settles can be twice or half the field rate, twice in a row: out of the range the
// scaler handles (47 to 86 Hz) it is no rate
void test_rate_out_of_range_is_no_rate(void) {
    TEST_ASSERT_EQUAL_UINT32(60100, profileRateMilliHz(60.0999f));
    TEST_ASSERT_EQUAL_UINT32(49926, profileRateMilliHz(49.92602f));
    TEST_ASSERT_EQUAL_UINT32(85000, profileRateMilliHz(85.0f));
    TEST_ASSERT_EQUAL_UINT32(0, profileRateMilliHz(120.199f));     // seen on the unit while two consoles were swapped
    TEST_ASSERT_EQUAL_UINT32(0, profileRateMilliHz(30.05f));
    TEST_ASSERT_EQUAL_UINT32(0, profileRateMilliHz(0.0f));
    TEST_ASSERT_EQUAL_UINT32(0, profileRateMilliHz(46.9f));
    TEST_ASSERT_EQUAL_UINT32(0, profileRateMilliHz(86.1f));
}

// Two measurements that agree make a rate; one stray reading is left out, three different ones are no rate
void test_rate_pick(void) {
    TEST_ASSERT_EQUAL_UINT32(60100, profileRatePick(60099, 60101, 0));
    TEST_ASSERT_EQUAL_UINT32(60100, profileRatePick(59509, 60099, 60101));
    TEST_ASSERT_EQUAL_UINT32(60100, profileRatePick(60099, 59509, 60101));
    TEST_ASSERT_EQUAL_UINT32(0, profileRatePick(60099, 59509, 49926));
    TEST_ASSERT_EQUAL_UINT32(0, profileRatePick(60099, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(0, profileRatePick(0, 0, 0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_profile_store_rules);
    RUN_TEST(test_mode_table);
    RUN_TEST(test_reg_mode_undoes_pal_force60_only);
    RUN_TEST(test_source_mode_undoes_i2p_on_decoder_inputs);
    RUN_TEST(test_preset_status_char);
    RUN_TEST(test_interlace_follows_the_firmware_rule);
    RUN_TEST(test_preset_for_load);
    RUN_TEST(test_load_switches_input);
    RUN_TEST(test_force60_must_match_the_captured_pal_registers);
    RUN_TEST(test_force60_change_reloads_only_pal_modes);
    RUN_TEST(test_force60_vertical_shift_is_the_same_for_every_output);
    RUN_TEST(test_force60_custom_registers_keep_their_mode_and_run_at_60);
    RUN_TEST(test_sync_watcher_off_lives_only_in_its_profile);
    RUN_TEST(test_defaults_keep_the_output_resolution);
    RUN_TEST(test_rate_tells_consoles_of_one_standard_apart);
    RUN_TEST(test_rate_is_not_judged_where_it_cannot_be);
    RUN_TEST(test_rate_out_of_range_is_no_rate);
    RUN_TEST(test_rate_pick);
    return UNITY_END();
}
