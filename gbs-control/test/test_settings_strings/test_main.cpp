#include <string.h>
#include <string>
#include <unity.h>
#include "options.h"
#include "platform-caps.h"
#include "settings-registry.h"
#include "settings-store.h"

static userOptions u;
static void sink(const char* t, void* c) { ((std::string*)c)->append(t); }

void setUp(void) { platformSetCaps(0xFF); memset(&u, 0, sizeof(u)); settingsApplyDefaults(u); }
void tearDown(void) {}

static StrSettingDef strDef(const char* key) {
    int i = settingsStrFind(key);
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, key);
    StrSettingDef d; settingsStrDefAt((uint8_t)i, d); return d;
}

void test_put_get_and_limits(void) {
    StrSettingDef d = strDef("system.extra_hosts");
    TEST_ASSERT_EQUAL_UINT8(63, d.maxLen);
    TEST_ASSERT_FALSE(d.flags & SETTING_FLAG_DEVICE_ONLY);   // writable over HTTP since the web redesign
    TEST_ASSERT_TRUE(settingsStrPut(u, d, "gbscontrol.fritz.box,gbs.lan"));
    TEST_ASSERT_EQUAL_STRING("gbscontrol.fritz.box,gbs.lan", settingsStrGet(u, d));
    TEST_ASSERT_FALSE(settingsStrPut(u, d, "bad\"quote"));
    TEST_ASSERT_FALSE(settingsStrPut(u, d, "back\\slash"));
    TEST_ASSERT_FALSE(settingsStrPut(u, d, "new\nline"));
    std::string longStr(64, 'a');
    TEST_ASSERT_FALSE(settingsStrPut(u, d, longStr.c_str()));
    TEST_ASSERT_EQUAL_STRING("gbscontrol.fritz.box,gbs.lan", settingsStrGet(u, d));   // unchanged on failure
}

void test_serialize_and_load_round_trip(void) {
    settingsStrPut(u, strDef("system.extra_hosts"), "gbs.lan");
    std::string out;
    settingsSerialize(u, SettingScope::Device, sink, &out);
    TEST_ASSERT_TRUE(out.find("system.extra_hosts=gbs.lan\n") != std::string::npos);
    userOptions v; memset(&v, 0, sizeof(v)); settingsApplyDefaults(v);
    SettingsLoadReport rep;
    settingsLoadBegin(v, SettingScope::Device, rep);
    settingsLoadLine(v, SettingScope::Device, "system.extra_hosts=gbs.lan", rep);
    TEST_ASSERT_EQUAL_STRING("gbs.lan", v.extraHosts);
    settingsLoadBegin(v, SettingScope::Device, rep);
    TEST_ASSERT_EQUAL_STRING("", v.extraHosts);   // LoadBegin resets strings of the scope
}

// A string value is kept verbatim, spaces at the ends included: an OTA password "abc " came back as "abc" after a
// restart and espota then failed. Only the CR of a CRLF file is dropped.
void test_string_spaces_survive_a_round_trip(void) {
    settingsStrPut(u, strDef("system.ota_password"), " ab c ");
    std::string out;
    settingsSerialize(u, SettingScope::Device, sink, &out);
    size_t at = out.find("system.ota_password=");
    TEST_ASSERT_TRUE(at != std::string::npos);
    std::string line = out.substr(at, out.find('\n', at) - at);
    userOptions v; memset(&v, 0, sizeof(v)); settingsApplyDefaults(v);
    SettingsLoadReport rep;
    settingsLoadBegin(v, SettingScope::Device, rep);
    settingsLoadLine(v, SettingScope::Device, line.c_str(), rep);
    TEST_ASSERT_EQUAL_STRING(" ab c ", v.otaPassword);
    settingsLoadLine(v, SettingScope::Device, (line + "\r").c_str(), rep);
    TEST_ASSERT_EQUAL_STRING(" ab c ", v.otaPassword);
}

void test_values_and_schema_include_strings(void) {
    settingsStrPut(u, strDef("system.extra_hosts"), "gbs.lan");
    std::string values, schema;
    settingsValuesJson(u, false, sink, &values);
    settingsSchemaJson(false, sink, &schema);
    TEST_ASSERT_TRUE(values.find("\"system.extra_hosts\":\"gbs.lan\"") != std::string::npos);
    TEST_ASSERT_TRUE(schema.find("{\"key\":\"system.extra_hosts\",\"type\":\"str\",\"scope\":\"device\",\"max\":63}") != std::string::npos);
}

void test_secret_value_is_masked(void) {
    std::string empty, set;
    settingsValuesJson(u, false, sink, &empty);
    TEST_ASSERT_TRUE(empty.find("\"system.ota_password\":\"\"") != std::string::npos);
    settingsStrPut(u, strDef("system.ota_password"), "pw");
    settingsValuesJson(u, false, sink, &set);
    TEST_ASSERT_TRUE(set.find("\"system.ota_password\":\"***\"") != std::string::npos);
    TEST_ASSERT_TRUE(settingsKeyExcludedFromBackup("system.ota_password"));
}

void test_backup_exclusion(void) {
    TEST_ASSERT_FALSE(settingsKeyExcludedFromBackup("system.extra_hosts"));
    TEST_ASSERT_TRUE(settingsKeyExcludedFromBackup("system.adv_controller_skip"));
    TEST_ASSERT_FALSE(settingsKeyExcludedFromBackup("adv.brightness"));
    TEST_ASSERT_FALSE(settingsKeyExcludedFromBackup("nope"));
}

// Every writer (HTTP, restore, serial H, menu keyboard) passes settingsStrPut: names the Host guard can match only
void test_str_setting_extra_hosts_validated(void) {
    StrSettingDef d = strDef("system.extra_hosts");
    TEST_ASSERT_TRUE(settingsStrPut(u, d, "tv.lan"));
    TEST_ASSERT_FALSE(settingsStrPut(u, d, "a b"));
    TEST_ASSERT_FALSE(settingsStrPut(u, d, "10.0.0.5"));
    TEST_ASSERT_EQUAL_STRING("tv.lan", settingsStrGet(u, d));
    TEST_ASSERT_TRUE(settingsStrPut(u, strDef("system.ota_password"), "a b"));   // other strings keep their rules
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_put_get_and_limits);
    RUN_TEST(test_serialize_and_load_round_trip);
    RUN_TEST(test_string_spaces_survive_a_round_trip);
    RUN_TEST(test_values_and_schema_include_strings);
    RUN_TEST(test_secret_value_is_masked);
    RUN_TEST(test_backup_exclusion);
    RUN_TEST(test_str_setting_extra_hosts_validated);
    return UNITY_END();
}
