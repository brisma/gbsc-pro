#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <unity.h>
#include "options.h"
#include "settings-registry.h"
#include "platform-caps.h"
#include "golden.h"

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

// The update channel starts as the build's own (host builds and plain local builds: Beta)
void test_update_channel_defaults_to_the_build_channel(void) {
    int i = settingsFind("system.update_channel");
    TEST_ASSERT_TRUE(i >= 0);
    SettingDef d; settingsDefAt((uint8_t)i, d);
    TEST_ASSERT_EQUAL_UINT16(GBS_FW_CHANNEL, d.def);
    TEST_ASSERT_EQUAL_UINT16(1, d.max);                       // Stable = 0, Beta = 1 (OtaChannel)
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
    TEST_ASSERT_EQUAL_UINT8(1, u.hdmiLimitedRange);   // Auto: compress where the HDMI converter flags limited range
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
    settingsFormat(defOf("output.hdmi_range"), 3, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Limited", buf);
    settingsFormat(defOf("output.hdmi_range"), 1, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Auto", buf);
    settingsFormat(defOf("output.hdmi_range"), 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Full", buf);
    settingsFormat(defOf("adv.format_sv"), 6, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PAL-N 7.5 IRE", buf);
}

// input.sync_dejitter: Auto (as rama's code) / On / Off, per profile, Auto by default (sync-source.h)
void test_sync_dejitter_auto_on_off(void) {
    char buf[32];
    const SettingDef d = defOf("input.sync_dejitter");
    TEST_ASSERT_EQUAL_UINT16(0, d.def);
    settingsFormat(d, 0, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("Auto", buf);
    settingsFormat(d, 1, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("On", buf);
    settingsFormat(d, 2, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("Off", buf);
    uint16_t v;
    TEST_ASSERT_FALSE(settingsParse(d, "3", v));
    TEST_ASSERT_EQUAL_UINT8(1, defOf("input.digital_sync").max);   // Digital sync stays Off / On
}

void test_hdmi_levels_per_profile(void) {
    const SettingDef b = defOf("output.hdmi_black"), w = defOf("output.hdmi_white");
    TEST_ASSERT_EQUAL_UINT16(16, b.def);   // the CEA-861 limited range: black 16, white 235
    TEST_ASSERT_EQUAL_UINT16(0, b.min);
    TEST_ASSERT_EQUAL_UINT16(20, b.max);
    TEST_ASSERT_EQUAL_UINT16(235, w.def);
    TEST_ASSERT_EQUAL_UINT16(215, w.min);
    TEST_ASSERT_EQUAL_UINT16(255, w.max);
    TEST_ASSERT_TRUE(b.scope == SettingScope::Profile);   // with HDMI range: a console's profile carries both
    TEST_ASSERT_TRUE(w.scope == SettingScope::Profile);
    TEST_ASSERT_EQUAL_UINT16(APPLY_COLOR, b.apply);
    TEST_ASSERT_EQUAL_UINT16(APPLY_COLOR, w.apply);
    TEST_ASSERT_EQUAL_UINT8(16, u.hdmiBlackLevel);
    TEST_ASSERT_EQUAL_UINT8(235, u.hdmiWhiteLevel);
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


// ---- Stored values: option ids (lower case, runs of other characters -> one '_') ----
static std::string stored(const char* key, uint16_t v) {
    char b[40]; settingsFormatStored(defOf(key), v, b, sizeof(b)); return std::string(b);
}
static bool parses(const char* key, const char* text, uint16_t want) {
    uint16_t v = 0xFFFF; return settingsParse(defOf(key), text, v) && v == want;
}
static bool rejects(const char* key, const char* text) { uint16_t v; return !settingsParse(defOf(key), text, v); }

void test_id_from_label(void) {
    struct { const char* l; const char* id; } t[] = {{"Auto", "auto"}, {"Force 4:3", "force_4_3"}, {"As 60Hz", "as_60hz"},
        {"RGB SCART", "rgb_scart"}, {"S-Video", "s_video"}, {"NTSC 7.5 IRE", "ntsc_7_5_ire"}, {"3-2 lines", "3_2_lines"},
        {"GBSC PRO", "gbsc_pro"}, {"PAL-Nc 7.5IRE", "pal_nc_7_5ire"}, {" -x- ", "x"}};
    char b[32];
    for (auto& e : t) { settingsIdFromLabel(e.l, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING_MESSAGE(e.id, b, e.l); }
    TEST_ASSERT_EQUAL_UINT(0, settingsIdFromLabel("a label far too long for this small buffer", b, 8));
}

void test_ids_distinct_per_setting(void) {
    for (uint8_t i = 0; i < settingsCount(); i++) {
        SettingDef d; settingsDefAt(i, d);
        if (d.type != SettingType::Enum) continue;
        char key[48]; settingsKeyCopy(d, key, sizeof(key));
        std::vector<std::string> ids;
        char name[48]; uint16_t v;
        for (uint8_t k = 0; settingsEnumAt(d, k, name, sizeof(name), v); k++) {
            TEST_ASSERT_TRUE_MESSAGE(strlen(name) < 40, key);   // the option reader's token buffer is 48 B
            char id[48]; TEST_ASSERT_TRUE_MESSAGE(settingsIdFromLabel(name, id, sizeof(id)) > 0, key);
            for (auto& o : ids) TEST_ASSERT_TRUE_MESSAGE(o != id, key);
            bool digits = true; for (const char* c = id; *c; c++) if (*c < '0' || *c > '9') digits = false;
            if (digits) TEST_ASSERT_EQUAL_UINT16_MESSAGE(v, (uint16_t)atoi(id), key);
            ids.push_back(id);
        }
    }
}

void test_flash_ids_with_suffixes(void) {
    TEST_ASSERT_EQUAL_STRING("s_video", stored("input.source", 5).c_str());      // "@adv"
    TEST_ASSERT_EQUAL_STRING("ntsc_4_43", stored("adv.format_sv", 4).c_str());   // "#a"
    TEST_ASSERT_EQUAL_STRING("bypass", stored("output.preset", 10).c_str());     // "@original"
}

void test_format_stored_ids_and_words(void) {
    TEST_ASSERT_EQUAL_STRING("force_4_3", stored("output.aspect", 1).c_str());
    TEST_ASSERT_EQUAL_STRING("auto", stored("output.hdmi_range", 1).c_str());
    TEST_ASSERT_EQUAL_STRING("off", stored("picture.scanlines", 0).c_str());
    TEST_ASSERT_EQUAL_STRING("on", stored("system.ext_clock", 1).c_str());       // the setting's meaning (INVERT is in uopt)
    TEST_ASSERT_EQUAL_STRING("80", stored("audio.volume", 80).c_str());
    TEST_ASSERT_EQUAL_STRING("2", stored("system.encoder_steps", 2).c_str());
}

void test_format_stored_value_outside_list(void) {
    TEST_ASSERT_EQUAL_STRING("9", stored("output.aspect", 9).c_str());
}

void test_stored_round_trip_every_setting(void) {
    for (uint8_t i = 0; i < settingsCount(); i++) {
        SettingDef d; settingsDefAt(i, d);
        char key[48]; settingsKeyCopy(d, key, sizeof(key));
        std::vector<uint16_t> vals;
        if (d.type == SettingType::Enum) { char n[48]; uint16_t v; for (uint8_t k = 0; settingsEnumAt(d, k, n, sizeof(n), v); k++) vals.push_back(v); }
        else if (d.type == SettingType::Bool) vals = {0, 1};
        else vals = {d.min, d.max};
        for (uint16_t v : vals) {
            char b[40]; settingsFormatStored(d, v, b, sizeof(b));
            uint16_t back = 0xFFFF;
            TEST_ASSERT_TRUE_MESSAGE(settingsParse(d, b, back), key);
            TEST_ASSERT_EQUAL_UINT16_MESSAGE(v, back, key);
        }
    }
}

void test_parse_ids_labels_numbers(void) {
    TEST_ASSERT_TRUE(parses("output.aspect", "force_4_3", 1));
    TEST_ASSERT_TRUE(parses("output.aspect", "Force 4:3", 1));
    TEST_ASSERT_TRUE(parses("output.aspect", "force 4:3", 1));
    TEST_ASSERT_TRUE(parses("output.aspect", "1", 1));
    TEST_ASSERT_TRUE(rejects("output.aspect", "nope"));
    TEST_ASSERT_TRUE(parses("output.hdmi_range", "Limited", 3));
    TEST_ASSERT_TRUE(parses("output.hdmi_range", "limited", 3));
}

void test_bool_words_normalised(void) {
    TEST_ASSERT_TRUE(parses("picture.scanlines", "On", 1));
    TEST_ASSERT_TRUE(parses("picture.scanlines", "OFF", 0));
    TEST_ASSERT_TRUE(parses("picture.scanlines", "on ", 1));
    TEST_ASSERT_TRUE(parses("picture.scanlines", "off\r", 0));
    TEST_ASSERT_TRUE(parses("picture.scanlines", "true", 1));
    TEST_ASSERT_TRUE(parses("picture.scanlines", "0", 0));
    TEST_ASSERT_TRUE(rejects("picture.scanlines", "maybe"));
}

void test_signed_or_marked_numbers_rejected(void) {
    TEST_ASSERT_TRUE(rejects("system.encoder_steps", "-1"));
    TEST_ASSERT_TRUE(rejects("system.encoder_steps", "+1"));
    TEST_ASSERT_TRUE(rejects("system.encoder_steps", "#2"));
    TEST_ASSERT_TRUE(rejects("picture.scanlines", "-1"));
}

void test_format_unchanged_for_menu_and_echo(void) {
    char b[32];
    settingsFormat(defOf("output.aspect"), 1, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("Force 4:3", b);
    settingsFormat(defOf("system.encoder_steps"), 2, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("2", b);
}



// Choices that were numbers with labels only in the menu: registry Enums, same values, same labels in the same order
static std::string labelsOf(const char* key) {
    SettingDef d = defOf(key); std::string out; char n[48]; uint16_t v;
    for (uint8_t k = 0; settingsEnumAt(d, k, n, sizeof(n), v); k++) { if (k) out += "|"; out += n; out += "="; out += std::to_string(v); }
    return out;
}

void test_converted_choices_are_enums(void) {
    const char* keys[] = {"picture.deinterlace", "adv.filter_y", "adv.filter_wy", "adv.filter_c", "adv.comb_luma_ntsc",
                          "adv.comb_luma_pal", "adv.comb_chroma_ntsc", "adv.comb_chroma_pal", "dev.syncwatcher", "input.last"};
    for (const char* k : keys) TEST_ASSERT_TRUE_MESSAGE(defOf(k).type == SettingType::Enum, k);
    TEST_ASSERT_EQUAL_STRING("bob", stored("picture.deinterlace", 1).c_str());
    TEST_ASSERT_EQUAL_STRING("notch", stored("adv.comb_luma_ntsc", 4).c_str());
    TEST_ASSERT_EQUAL_STRING("off", stored("dev.syncwatcher", 2).c_str());
    TEST_ASSERT_EQUAL_STRING("vga", stored("input.last", 3).c_str());
    TEST_ASSERT_EQUAL_STRING("wideband", stored("adv.filter_c", 7).c_str());
    TEST_ASSERT_EQUAL_STRING("Adaptive=0|Bob=1", labelsOf("picture.deinterlace").c_str());
    TEST_ASSERT_EQUAL_STRING("Adaptive=0|Notch=4|2-line upper=5|3-line=6|2-line lower=7", labelsOf("adv.comb_luma_pal").c_str());
    TEST_ASSERT_EQUAL_STRING("Adaptive=0|Off=4|2-line upper=5|3-line=6|2-line lower=7", labelsOf("adv.comb_chroma_ntsc").c_str());
    TEST_ASSERT_EQUAL_STRING("Inherit=0|On=1|Off=2", labelsOf("dev.syncwatcher").c_str());
    TEST_ASSERT_EQUAL_STRING("Auto 1.5MHz=0|Auto 2.2MHz=1|Shape 1=2|Shape 2=3|Shape 3=4|Shape 4=5|Shape 5=6|Wideband=7",
                             labelsOf("adv.filter_c").c_str());
    TEST_ASSERT_EQUAL_STRING("RGB SCART=1|RGB SoG=2|VGA=3|Component=4|S-Video=5|Composite=6", labelsOf("input.last").c_str());
    TEST_ASSERT_EQUAL_UINT8(31, settingsEnumCount(defOf("adv.filter_y")));
    TEST_ASSERT_EQUAL_UINT8(18, settingsEnumCount(defOf("adv.filter_wy")));
}

void test_new_enum_rejects_unlisted_number(void) {
    TEST_ASSERT_TRUE(rejects("adv.comb_luma_ntsc", "2"));
    TEST_ASSERT_TRUE(rejects("picture.deinterlace", "2"));
}



// The ids files hold, frozen: from 3.0.0 on an id may be added, never changed or removed
static std::string idListing() {
    std::string out;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        SettingDef d; settingsDefAt(i, d);
        if (d.type != SettingType::Enum && d.type != SettingType::Bool) continue;
        char key[48]; settingsKeyCopy(d, key, sizeof(key));
        out += key;
        if (d.type == SettingType::Bool) out += " on=1 off=0";
        else { char n[48], id[48]; uint16_t v;
               for (uint8_t k = 0; settingsEnumAt(d, k, n, sizeof(n), v); k++) { settingsIdFromLabel(n, id, sizeof(id)); out += std::string(" ") + id + "=" + std::to_string(v); } }
        out += "\n";
    }
    return out;
}

void test_frozen_ids(void) {
    TEST_ASSERT_TRUE_MESSAGE(goldenCheckFile("settings", "ids", "txt", idListing()),
        "an id was changed or removed: from 3.0.0 on ids may only be added; keep a renamed label's old id");
}


int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_id_from_label);
    RUN_TEST(test_ids_distinct_per_setting);
    RUN_TEST(test_flash_ids_with_suffixes);
    RUN_TEST(test_format_stored_ids_and_words);
    RUN_TEST(test_format_stored_value_outside_list);
    RUN_TEST(test_stored_round_trip_every_setting);
    RUN_TEST(test_parse_ids_labels_numbers);
    RUN_TEST(test_bool_words_normalised);
    RUN_TEST(test_signed_or_marked_numbers_rejected);
    RUN_TEST(test_format_unchanged_for_menu_and_echo);
    RUN_TEST(test_converted_choices_are_enums);
    RUN_TEST(test_new_enum_rejects_unlisted_number);
    RUN_TEST(test_frozen_ids);
    RUN_TEST(test_advanced_mark_stripped_and_long_lists_complete);
    RUN_TEST(test_setting_ids_match_table_order);
    RUN_TEST(test_adv_apply_groups_are_granular);
    RUN_TEST(test_picture_light_apply_group);
    RUN_TEST(test_profiles_startup_setting);
    RUN_TEST(test_no_setup_done_setting);
    RUN_TEST(test_caps_from_key_prefix);
    RUN_TEST(test_update_channel_defaults_to_the_build_channel);
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
    RUN_TEST(test_sync_dejitter_auto_on_off);
    RUN_TEST(test_hdmi_levels_per_profile);
    RUN_TEST(test_option_grammar_with_caps);
    RUN_TEST(test_parse_ignores_caps_marker);
    RUN_TEST(test_input_source_is_a_choice_with_auto);
    RUN_TEST(test_option_availability_follows_caps);
    RUN_TEST(test_setting_availability_follows_caps_and_devmode);
    RUN_TEST(test_auto_update_check_setting);
    RUN_TEST(test_profile_settings);
    return UNITY_END();
}
