#include <stdint.h>
#include <string.h>
#include <string>
#include <unity.h>
#include "options.h"
#include "settings-registry.h"
#include "settings-store.h"
#include "platform-caps.h"

static userOptions u;
static void appendSink(const char* t, void* ctx) { ((std::string*)ctx)->append(t); }

void setUp(void) { platformSetCaps(0xFF); memset(&u, 0, sizeof(u)); settingsApplyDefaults(u); }
void tearDown(void) {}

static void loadText(SettingScope s, const char* text, SettingsLoadReport& rep) {
    settingsLoadBegin(u, s, rep);
    std::string all(text), line;
    for (char c : all) { if (c == '\n') { settingsLoadLine(u, s, line.c_str(), rep); line.clear(); } else line += c; }
    if (!line.empty()) settingsLoadLine(u, s, line.c_str(), rep);
}

void test_serialize_device_scope_only(void) {
    std::string out;
    u.osdTheme = 2;
    settingsSerialize(u, SettingScope::Device, appendSink, &out);
    TEST_ASSERT_EQUAL_INT(0, (int)out.find("schema=1\n"));
    TEST_ASSERT_TRUE(out.find("osd.theme=Light\n") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("adv.brightness=") == std::string::npos);
}

void test_round_trip_profile(void) {
    u.advBrightness = 42; u.advDnr = 1; u.presetPreference = OutputBypass;
    std::string out;
    settingsSerialize(u, SettingScope::Profile, appendSink, &out);
    userOptions saved = u;
    settingsApplyDefaults(u);
    SettingsLoadReport rep;
    loadText(SettingScope::Profile, out.c_str(), rep);
    TEST_ASSERT_EQUAL_UINT8(42, u.advBrightness);
    TEST_ASSERT_EQUAL_UINT8(1, u.advDnr);
    TEST_ASSERT_EQUAL_UINT8(OutputBypass, u.presetPreference);
    TEST_ASSERT_EQUAL_UINT16(0, rep.invalid);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &u, sizeof(u));
}

void test_unknown_missing_invalid_rules(void) {
    SettingsLoadReport rep;
    loadText(SettingScope::Profile,
             "schema=1\n# comment\n\nfuture.thing=5\nadv.brightness=999\nadv.contrast = 90 \n", rep);
    TEST_ASSERT_EQUAL_UINT16(1, rep.unknown);
    TEST_ASSERT_EQUAL_UINT16(1, rep.invalid);
    TEST_ASSERT_EQUAL_UINT8(50, u.advBrightness);    // invalid → default
    TEST_ASSERT_EQUAL_UINT8(90, u.advContrast);
    TEST_ASSERT_EQUAL_UINT8(50, u.advHue);           // missing → default
    TEST_ASSERT_EQUAL_UINT16(1, rep.schema);
}

// A profile body written before menu v3: picture.peaking is unknown, sharpness keeps its default 22
void test_old_peaking_line_is_skipped(void) {
    SettingsLoadReport rep;
    loadText(SettingScope::Profile, "picture.peaking=1\npicture.scanlines=1\n", rep);
    TEST_ASSERT_EQUAL_UINT16(1, rep.unknown);
    TEST_ASSERT_EQUAL_UINT16(0, rep.invalid);
    TEST_ASSERT_EQUAL_UINT8(22, u.picSharpness);
    TEST_ASSERT_EQUAL_UINT8(1, u.wantScanlines);
}

void test_load_truncated_and_garbage_lines(void) {
    SettingsLoadReport rep;
    loadText(SettingScope::Profile, "adv.hue=20\nadv.bright\n\x01\x02=\xff\nadv.satura", rep);
    TEST_ASSERT_EQUAL_UINT8(20, u.advHue);
    TEST_ASSERT_EQUAL_UINT8(50, u.advSaturation);
}

void test_other_scope_keys_are_ignored(void) {
    SettingsLoadReport rep;
    loadText(SettingScope::Profile, "osd.theme=Light\n", rep);
    TEST_ASSERT_EQUAL_UINT8(0, u.osdTheme);   // the default (GBSC PRO) stays
    TEST_ASSERT_EQUAL_UINT16(1, rep.unknown);
}

void test_values_json(void) {
    std::string out;
    settingsValuesJson(u, true, appendSink, &out);
    TEST_ASSERT_EQUAL_CHAR('{', out.front());
    TEST_ASSERT_EQUAL_CHAR('}', out.back());
    TEST_ASSERT_TRUE(out.find("\"adv.brightness\":50") != std::string::npos);
}

void test_schema_json_has_enum_options(void) {
    std::string out;
    settingsSchemaJson(true, appendSink, &out);
    TEST_ASSERT_TRUE(out.find("\"key\":\"osd.theme\"") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("{\"n\":\"Bypass\",\"v\":10}") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("\"key\":\"adv.hue\",\"type\":\"u8\",\"scope\":\"profile\",\"min\":0,\"max\":100,\"def\":50,\"caps\":1") != std::string::npos);
}

// The web serves the schema one entry at a time (a chunked response): entries compose to the same JSON and
// each one fits the 512-byte chunk buffer (the whole schema does not fit the ESP heap)
void test_schema_entries_compose_to_schema(void) {
    for (int dev = 0; dev < 2; dev++) {
        std::string whole, parts = "[";
        settingsSchemaJson(dev != 0, appendSink, &whole);
        bool first = true;
        size_t longest = 0;
        for (uint16_t n = 0; ; n++) {
            std::string e;
            if (!settingsSchemaEntry(n, dev != 0, appendSink, &e)) break;
            if (e.empty()) continue;
            if (!first) parts += ",";
            first = false;
            parts += e;
            if (e.size() > longest) longest = e.size();
        }
        parts += "]";
        TEST_ASSERT_EQUAL_STRING(whole.c_str(), parts.c_str());
        TEST_ASSERT_TRUE(longest < 512);
    }
}

void test_schema_streamer_chunks(void) {
    std::string whole;
    settingsSchemaJson(true, appendSink, &whole);
    for (size_t chunk : {1u, 7u, 64u, 1460u}) {
        SchemaStreamer st(true);
        std::string out;
        char buf[1460];
        for (int guard = 0; guard < 100000; guard++) {
            size_t n = st.fill(buf, chunk);
            if (!n) break;
            out.append(buf, n);
        }
        TEST_ASSERT_EQUAL_STRING(whole.c_str(), out.c_str());
    }
}

void test_load_replaces_unavailable_option_with_default(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    SettingsLoadReport rep;
    settingsLoadBegin(u, SettingScope::Device, rep);
    settingsLoadLine(u, SettingScope::Device, "input.source=Composite", rep);
    TEST_ASSERT_EQUAL_UINT8(0, u.inputChoice);       // default Auto
    settingsLoadLine(u, SettingScope::Device, "input.source=VGA", rep);
    TEST_ASSERT_EQUAL_UINT8(3, u.inputChoice);
}

void test_load_keeps_value_of_unavailable_setting(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    SettingsLoadReport rep;
    settingsLoadBegin(u, SettingScope::Profile, rep);
    settingsLoadLine(u, SettingScope::Profile, "adv.brightness=75", rep);
    TEST_ASSERT_EQUAL_UINT8(75, u.advBrightness);
}

void test_schema_and_values_filter_by_caps_and_devmode(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    std::string schema, values;
    settingsSchemaJson(false, appendSink, &schema);
    settingsValuesJson(u, false, appendSink, &values);
    TEST_ASSERT_TRUE(schema.find("\"adv.brightness\"") == std::string::npos);
    TEST_ASSERT_TRUE(schema.find("\"dev.syncwatcher\"") == std::string::npos);
    TEST_ASSERT_TRUE(schema.find("{\"n\":\"Composite\"") == std::string::npos);
    TEST_ASSERT_TRUE(schema.find("{\"n\":\"Bypass\",\"v\":10}") != std::string::npos);
    TEST_ASSERT_TRUE(values.find("\"adv.brightness\"") == std::string::npos);
    TEST_ASSERT_TRUE(values.find("\"picture.scanlines\"") != std::string::npos);
    TEST_ASSERT_EQUAL_CHAR('{', values[0]);
    TEST_ASSERT_TRUE(values.find("{,") == std::string::npos);
    std::string dev;
    settingsSchemaJson(true, appendSink, &dev);
    TEST_ASSERT_TRUE(dev.find("\"dev.syncwatcher\"") != std::string::npos);
}

void test_serialize_without_schema_and_scope_crc(void) {
    std::string out;
    settingsSerializeEx(u, SettingScope::Profile, false, appendSink, &out);
    TEST_ASSERT_TRUE(out.find("schema=") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("adv.brightness=") != std::string::npos);
    uint32_t a = settingsScopeCrc(u, SettingScope::Profile);
    u.osdTheme = 2;                                   // Device scope: profile CRC unchanged
    TEST_ASSERT_EQUAL_HEX32(a, settingsScopeCrc(u, SettingScope::Profile));
    u.advBrightness = 99;
    TEST_ASSERT_TRUE(a != settingsScopeCrc(u, SettingScope::Profile));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_device_scope_only);
    RUN_TEST(test_round_trip_profile);
    RUN_TEST(test_unknown_missing_invalid_rules);
    RUN_TEST(test_old_peaking_line_is_skipped);
    RUN_TEST(test_load_truncated_and_garbage_lines);
    RUN_TEST(test_other_scope_keys_are_ignored);
    RUN_TEST(test_values_json);
    RUN_TEST(test_schema_json_has_enum_options);
    RUN_TEST(test_load_replaces_unavailable_option_with_default);
    RUN_TEST(test_load_keeps_value_of_unavailable_setting);
    RUN_TEST(test_schema_and_values_filter_by_caps_and_devmode);
    RUN_TEST(test_serialize_without_schema_and_scope_crc);
    RUN_TEST(test_schema_entries_compose_to_schema);
    RUN_TEST(test_schema_streamer_chunks);
    return UNITY_END();
}
