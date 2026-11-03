#include <stdint.h>
#include <string.h>
#include <unity.h>
#include "options.h"
#include "settings-registry.h"
#include "platform-caps.h"

static userOptions u;

void setUp(void) { platformSetCaps(CAPS_DEFAULT); memset(&u, 0xAB, sizeof(u)); settingsApplyDefaults(u); }
void tearDown(void) {}

static SettingDef defOf(const char* key) {
    int i = settingsFind(key);
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, key);
    SettingDef d; settingsDefAt((uint8_t)i, d); return d;
}

void test_changed_apply_mask_has_only_changed_groups(void) {
    static userOptions before;
    before = u;
    TEST_ASSERT_EQUAL_UINT16(0, settingsChangedApply(before, u, SettingScope::Profile));   // same values: nothing
    settingsPut(u, defOf("audio.volume"), 60);
    TEST_ASSERT_EQUAL_UINT16(APPLY_AUDIO, settingsChangedApply(before, u, SettingScope::Profile));
    settingsPut(u, defOf("input.sync_stripper"), 0);
    TEST_ASSERT_EQUAL_UINT16(APPLY_AUDIO | APPLY_SYNC, settingsChangedApply(before, u, SettingScope::Profile));
    before = u;
    settingsPut(u, defOf("osd.theme"), 2);                                  // a Device setting: not this scope
    TEST_ASSERT_EQUAL_UINT16(0, settingsChangedApply(before, u, SettingScope::Profile));
}

void test_wifi_watch_is_a_developer_toggle_off_by_default(void) {
    SettingDef d = defOf("dev.wifi_watch");
    TEST_ASSERT_TRUE(settingsIsDevKey(d));                          // listed only with Developer mode on
    TEST_ASSERT_EQUAL_UINT16(0, settingsGet(u, d));
    TEST_ASSERT_TRUE(d.scope == SettingScope::Device);
}

void test_keys_are_unique_and_defaults_valid(void) {
    char a[48], b[48];
    for (uint8_t i = 0; i < settingsCount(); i++) {
        SettingDef d; settingsDefAt(i, d);
        settingsKeyCopy(d, a, sizeof(a));
        TEST_ASSERT_TRUE_MESSAGE(settingsValid(d, d.def), a);
        for (uint8_t j = i + 1; j < settingsCount(); j++) {
            SettingDef e; settingsDefAt(j, e); settingsKeyCopy(e, b, sizeof(b));
            TEST_ASSERT_TRUE_MESSAGE(strcmp(a, b) != 0, a);
        }
    }
}

void test_defaults_match_legacy_defaults(void) {
    TEST_ASSERT_EQUAL_UINT8(80, u.volume);   // percent (80 % = -12 dB = the old default 38)
    TEST_ASSERT_EQUAL_UINT8(1, u.advSyncStripper);
    TEST_ASSERT_EQUAL_UINT8(50, u.advBrightness);    // menu v3: decoder levels 0..100
    TEST_ASSERT_EQUAL_UINT8(19, u.advFilterWYShaping);
    TEST_ASSERT_EQUAL_UINT8(1, u.hdmiLimitedRange);
}

void test_comb_defaults_are_set(void) {   // legacy loadDefaultUserOptions() forgot these
    TEST_ASSERT_EQUAL_UINT8(2, u.advCombChromaTapsNTSC);
    TEST_ASSERT_EQUAL_UINT8(3, u.advCombChromaTapsPAL);
}

void test_get_put_u16_and_invert(void) {
    // no U16 setting left since the per-standard geometry went (issue #45): the inverted Bool path only
    SettingDef c = defOf("system.ext_clock");          // stored inverted in disableExternalClockGenerator
    settingsPut(u, c, 0);
    TEST_ASSERT_EQUAL_UINT8(1, u.disableExternalClockGenerator);
    TEST_ASSERT_EQUAL_UINT16(0, settingsGet(u, c));
}

void test_validation_ranges(void) {
    SettingDef d = defOf("adv.brightness");
    TEST_ASSERT_TRUE(settingsValid(d, 100));
    TEST_ASSERT_FALSE(settingsValid(d, 101));
    SettingDef w = defOf("adv.filter_wy");
    TEST_ASSERT_FALSE(settingsValid(w, 1));
}

void test_parse_bool_number_and_enum(void) {
    uint16_t v;
    TEST_ASSERT_TRUE(settingsParse(defOf("picture.scanlines"), "true", v)); TEST_ASSERT_EQUAL_UINT16(1, v);
    TEST_ASSERT_TRUE(settingsParse(defOf("adv.hue"), " 80 ", v)); TEST_ASSERT_EQUAL_UINT16(80, v);
    TEST_ASSERT_TRUE(settingsParse(defOf("osd.theme"), "Retro", v)); TEST_ASSERT_EQUAL_UINT16(3, v);
    TEST_ASSERT_TRUE(settingsParse(defOf("output.preset"), "Bypass", v)); TEST_ASSERT_EQUAL_UINT16(10, v);
    TEST_ASSERT_TRUE(settingsParse(defOf("osd.theme"), "GBSC PRO", v)); TEST_ASSERT_EQUAL_UINT16(0, v);
    TEST_ASSERT_EQUAL_UINT16(0, defOf("osd.theme").def);   // GBSC PRO, black and yellow, is the default
    TEST_ASSERT_FALSE(settingsParse(defOf("osd.theme"), "Pink", v));
    TEST_ASSERT_FALSE(settingsParse(defOf("osd.theme"), "Dark", v));   // removed: too close to GBSC PRO
    TEST_ASSERT_FALSE(settingsParse(defOf("adv.hue"), "12x", v));
}

void test_parse_enum_accepts_number(void) {
    uint16_t v;
    TEST_ASSERT_TRUE(settingsParse(defOf("osd.theme"), "2", v)); TEST_ASSERT_EQUAL_UINT16(2, v);
    TEST_ASSERT_FALSE(settingsParse(defOf("output.preset"), "8", v));   // 8 is not an option value (7 = 1200p)
}

void test_format_enum_by_name(void) {
    char buf[32];
    settingsFormat(defOf("output.hdmi_range"), 2, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Limited SD", buf);
    settingsFormat(defOf("output.hdmi_range"), 1, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Auto", buf);
    settingsFormat(defOf("output.hdmi_range"), 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Full", buf);
    settingsFormat(defOf("adv.format_sv"), 6, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PAL-N 7.5 IRE", buf);
}

void test_caps_from_key_prefix(void) {
    TEST_ASSERT_EQUAL_UINT8(CAP_ADV, settingsCaps(defOf("adv.brightness")));
    TEST_ASSERT_EQUAL_UINT8(CAP_ADV, settingsCaps(defOf("input.sync_stripper")));
    TEST_ASSERT_EQUAL_UINT8(CAP_OSD, settingsCaps(defOf("osd.theme")));
    TEST_ASSERT_EQUAL_UINT8(CAP_AUDIO, settingsCaps(defOf("audio.volume")));
    TEST_ASSERT_EQUAL_UINT8(CAP_NONE, settingsCaps(defOf("picture.scanlines")));
}

// Startup = Last used (today) or Defaults, a Device choice
void test_profiles_startup_setting(void) {
    SettingDef d = defOf("profiles.startup");
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SettingScope::Device, (uint8_t)d.scope);
    TEST_ASSERT_EQUAL_UINT16(0, d.def);
    char name[24]; uint16_t v;
    TEST_ASSERT_TRUE(settingsEnumAt(d, 0, name, sizeof(name), v)); TEST_ASSERT_EQUAL_STRING("Last used", name);
    TEST_ASSERT_TRUE(settingsEnumAt(d, 1, name, sizeof(name), v)); TEST_ASSERT_EQUAL_STRING("Defaults", name);
}
// Quick setup removed (the quick page covers it): no setup_done setting
void test_no_setup_done_setting(void) { TEST_ASSERT_TRUE(settingsFind("system.setup_done") < 0); }

// Sharpness and scanline strength are light writes (no preset reload)
void test_picture_light_apply_group(void) {
    TEST_ASSERT_EQUAL_UINT16(APPLY_PICTURE, defOf("picture.sharpness").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_PICTURE, defOf("picture.scanline_strength").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_COLOR, defOf("output.hdmi_range").apply);   // the HDMI step lives in pictureColor()
}

// One web slider step must touch only its own decoder block (I2P resets the ADV Controller decoder)
void test_adv_apply_groups_are_granular(void) {
    TEST_ASSERT_EQUAL_UINT16(APPLY_ADV_BCSH, defOf("adv.brightness").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_ADV_BCSH, defOf("adv.hue").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_ADV_I2P, defOf("adv.i2p").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_ADV_I2P, defOf("adv.smooth").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_ADV_ACE, defOf("adv.ace").apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_ADV_ACE, defOf("adv.ace_luma_gain").apply);
}

void test_option_grammar_with_caps(void) {
    SettingDef d = defOf("input.source");
    char name[32]; uint16_t v; uint8_t caps;
    TEST_ASSERT_TRUE(settingsEnumAtEx(d, 5, name, sizeof(name), v, caps));
    TEST_ASSERT_EQUAL_STRING("S-Video", name);
    TEST_ASSERT_EQUAL_UINT16(5, v);
    TEST_ASSERT_EQUAL_HEX8(CAP_ADV, caps);
    TEST_ASSERT_TRUE(settingsEnumAtEx(d, 1, name, sizeof(name), v, caps));
    TEST_ASSERT_EQUAL_STRING("RGB SCART", name);
    TEST_ASSERT_EQUAL_HEX8(0, caps);
    TEST_ASSERT_TRUE(settingsEnumAtEx(d, 0, name, sizeof(name), v, caps));
    TEST_ASSERT_EQUAL_STRING("Auto", name);                        // the default choice
    TEST_ASSERT_EQUAL_UINT16(0, v);
    SettingDef b = defOf("system.board");
    TEST_ASSERT_TRUE(settingsEnumAtEx(b, 1, name, sizeof(name), v, caps));
    TEST_ASSERT_EQUAL_STRING("Standard", name);                     // the original gbs-control
    TEST_ASSERT_EQUAL_UINT16(2, v);
    TEST_ASSERT_TRUE(settingsEnumAtEx(b, 2, name, sizeof(name), v, caps));
    TEST_ASSERT_EQUAL_STRING("Pro", name);
    TEST_ASSERT_EQUAL_UINT16(1, v);
}

void test_parse_ignores_caps_marker(void) {
    uint16_t v;
    TEST_ASSERT_TRUE(settingsParse(defOf("input.source"), "Composite", v));
    TEST_ASSERT_EQUAL_UINT16(6, v);
    TEST_ASSERT_FALSE(settingsParse(defOf("input.source"), "Composite@adv", v));
}

// Source is the user's choice (Auto by default); the found path is a read-only hint
void test_input_source_is_a_choice_with_auto(void) {
    SettingDef d = defOf("input.source");
    TEST_ASSERT_EQUAL_UINT16(0, d.def);
    TEST_ASSERT_TRUE(settingsValid(d, 0));
    SettingDef l = defOf("input.last");
    TEST_ASSERT_EQUAL_UINT16(1, l.def);
    TEST_ASSERT_TRUE(settingsValid(l, 6));
    TEST_ASSERT_FALSE(settingsValid(l, 0));
    TEST_ASSERT_TRUE(l.flags & SETTING_FLAG_DEVICE_ONLY);           // not writable over HTTP, not in backups
}

void test_option_availability_follows_caps(void) {
    SettingDef d = defOf("input.source");
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    TEST_ASSERT_FALSE(settingsOptionAvailable(d, 6));
    TEST_ASSERT_TRUE(settingsOptionAvailable(d, 1));
    TEST_ASSERT_TRUE(settingsOptionAvailable(defOf("output.preset"), 10));
    platformSetCaps(CAPS_PRO);
    TEST_ASSERT_TRUE(settingsOptionAvailable(d, 6));
    TEST_ASSERT_FALSE(settingsOptionAvailable(defOf("output.preset"), 10));
    TEST_ASSERT_TRUE(settingsOptionAvailable(defOf("adv.brightness"), 200));   // not an enum
}

void test_setting_availability_follows_caps_and_devmode(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    TEST_ASSERT_FALSE(settingsAvailable(defOf("adv.brightness"), true));
    TEST_ASSERT_FALSE(settingsAvailable(defOf("osd.theme"), true));
    TEST_ASSERT_TRUE(settingsAvailable(defOf("picture.scanlines"), false));
    platformSetCaps(CAPS_DEFAULT);
    TEST_ASSERT_FALSE(settingsAvailable(defOf("dev.syncwatcher"), false));
    TEST_ASSERT_TRUE(settingsAvailable(defOf("dev.syncwatcher"), true));
    TEST_ASSERT_TRUE(settingsAvailable(defOf("system.devmode"), false));
}

void test_auto_update_check_setting(void) {
    uint16_t v;
    TEST_ASSERT_TRUE(settingsParse(defOf("system.auto_update_check"), "Daily", v));
    TEST_ASSERT_EQUAL_UINT16(1, v);
    TEST_ASSERT_EQUAL_UINT16(1, defOf("system.auto_update_check").def);   // Daily: a found update is only reported
}

void test_profile_settings(void) {
    // which profile the working settings belong to travels with them in a backup; over HTTP both stay read-only
    TEST_ASSERT_FALSE(settingsKeyExcludedFromBackup("profile.active"));
    TEST_ASSERT_FALSE(settingsKeyExcludedFromBackup("profile.hash"));
    StrSettingDef act; settingsStrDefAt((uint8_t)settingsStrFind("profile.active"), act);
    TEST_ASSERT_TRUE(act.flags & SETTING_FLAG_DEVICE_ONLY);
    TEST_ASSERT_TRUE(settingsKeyExcludedFromBackup("input.last"));                   // the unit's own
    TEST_ASSERT_TRUE(settingsKeyExcludedFromBackup("system.adv_controller_skip"));
    TEST_ASSERT_TRUE(settingsStrFind("profile.active") >= 0);
    TEST_ASSERT_EQUAL_INT(-1, settingsFind("legacy.slot"));
    TEST_ASSERT_EQUAL_UINT16(1, defOf("profiles.autoload").def);   // on: a saved profile comes back with its signal
}

void test_setting_ids_match_table_order(void) {
    TEST_ASSERT_EQUAL_INT(settingsFind("output.preset"), SID_output_preset);
    TEST_ASSERT_EQUAL_INT(settingsFind("dev.syncwatcher"), SID_dev_syncwatcher);
    TEST_ASSERT_EQUAL_INT(settingsCount(), SID_COUNT);
}

// Volume belongs to the profile (each console has its own clean level); mute stays global
void test_volume_is_per_profile_mute_is_global(void) {
    SettingDef d;
    settingsDefAt(SID_audio_volume, d);
    TEST_ASSERT_TRUE(d.scope == SettingScope::Profile);
    settingsDefAt(SID_audio_mute, d);
    TEST_ASSERT_TRUE(d.scope == SettingScope::Device);
}

// The '#a' mark (advanced-only option) is stripped from labels; long lists keep every option
void test_advanced_mark_stripped_and_long_lists_complete(void) {
    SettingDef d; settingsDefAt(SID_adv_format_sv, d);
    TEST_ASSERT_EQUAL_UINT8(13, settingsEnumCount(d));
    char name[32]; uint16_t v; uint8_t caps;
    bool secam = false;
    for (uint8_t i = 0; settingsEnumAtEx(d, i, name, sizeof(name), v, caps); i++) {
        TEST_ASSERT_NULL(strchr(name, '#'));
        if (v == 11) secam = true;
    }
    TEST_ASSERT_TRUE(secam);
    TEST_ASSERT_TRUE(settingsValid(d, 11));
    TEST_ASSERT_TRUE(settingsOptionAdvanced(d, 11));
    TEST_ASSERT_FALSE(settingsOptionAdvanced(d, 12));
    settingsDefAt(SID_input_source, d);                  // '@cap' still parsed
    for (uint8_t i = 0; settingsEnumAtEx(d, i, name, sizeof(name), v, caps); i++)
        if (v == 5) TEST_ASSERT_EQUAL_UINT8(CAP_ADV, caps);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_advanced_mark_stripped_and_long_lists_complete);
    RUN_TEST(test_setting_ids_match_table_order);
    RUN_TEST(test_adv_apply_groups_are_granular);
    RUN_TEST(test_picture_light_apply_group);
    RUN_TEST(test_profiles_startup_setting);
    RUN_TEST(test_no_setup_done_setting);
    RUN_TEST(test_caps_from_key_prefix);
    RUN_TEST(test_keys_are_unique_and_defaults_valid);
    RUN_TEST(test_wifi_watch_is_a_developer_toggle_off_by_default);
    RUN_TEST(test_changed_apply_mask_has_only_changed_groups);
    RUN_TEST(test_defaults_match_legacy_defaults);
    RUN_TEST(test_volume_is_per_profile_mute_is_global);
    RUN_TEST(test_comb_defaults_are_set);
    RUN_TEST(test_get_put_u16_and_invert);
    RUN_TEST(test_validation_ranges);
    RUN_TEST(test_parse_bool_number_and_enum);
    RUN_TEST(test_parse_enum_accepts_number);
    RUN_TEST(test_format_enum_by_name);
    RUN_TEST(test_option_grammar_with_caps);
    RUN_TEST(test_parse_ignores_caps_marker);
    RUN_TEST(test_input_source_is_a_choice_with_auto);
    RUN_TEST(test_option_availability_follows_caps);
    RUN_TEST(test_setting_availability_follows_caps_and_devmode);
    RUN_TEST(test_auto_update_check_setting);
    RUN_TEST(test_profile_settings);
    return UNITY_END();
}
