#include <stdint.h>
#include <string.h>
#include <string>
#include <unity.h>
#include "options.h"
#include "platform-caps.h"
#include "settings-registry.h"
#include "settings-request.h"

static userOptions u;
static void sink(const char* t, void* c) { ((std::string*)c)->append(t); }

void setUp(void) { platformSetCaps(CAPS_DEFAULT); memset(&u, 0, sizeof(u)); settingsApplyDefaults(u); }
void tearDown(void) {}

void test_validate_returns_pairs_without_writing(void) {
    const char* k[] = {"adv.brightness", "osd.theme"};
    const char* v[] = {"70", "Light"};
    std::string err; SettingsPair out[4];
    SettingsValidateResult r = settingsValidateRequest(k, v, 2, false, sink, &err, out, 4, nullptr);
    TEST_ASSERT_EQUAL_UINT8(2, r.okCount);
    TEST_ASSERT_EQUAL_UINT8(0, r.errCount);
    TEST_ASSERT_EQUAL_INT(settingsFind("adv.brightness"), out[0].index);
    TEST_ASSERT_EQUAL_UINT16(70, out[0].value);
    TEST_ASSERT_EQUAL_UINT16(2, out[1].value);
    TEST_ASSERT_EQUAL_UINT8(50, u.advBrightness);    // nothing written
}

void test_validate_reports_each_error_kind(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    const char* k[] = {"adv.brightness", "input.source", "nope.key", "picture.red", "dev.syncwatcher", "profile.active"};
    const char* v[] = {"70", "Composite", "1", "999", "5", "x"};
    std::string err; SettingsPair out[8];
    SettingsValidateResult r = settingsValidateRequest(k, v, 6, false, sink, &err, out, 8, nullptr);
    TEST_ASSERT_EQUAL_UINT8(0, r.okCount);
    TEST_ASSERT_EQUAL_UINT8(6, r.errCount);
    TEST_ASSERT_TRUE(err.find("\"adv.brightness\":\"unavailable\"") != std::string::npos);
    TEST_ASSERT_TRUE(err.find("\"input.source\":\"unavailable\"") != std::string::npos);
    TEST_ASSERT_TRUE(err.find("\"nope.key\":\"unknown\"") != std::string::npos);
    TEST_ASSERT_TRUE(err.find("\"picture.red\":\"invalid\"") != std::string::npos);
    TEST_ASSERT_TRUE(err.find("\"dev.syncwatcher\":\"unavailable\"") != std::string::npos);
    TEST_ASSERT_TRUE(err.find("\"profile.active\":\"readonly\"") != std::string::npos);
}

void test_validate_keeps_valid_pairs_next_to_errors(void) {
    const char* k[] = {"adv.brightness", "nope"};
    const char* v[] = {"60", "1"};
    std::string err; SettingsPair out[4];
    SettingsValidateResult r = settingsValidateRequest(k, v, 2, false, sink, &err, out, 4, nullptr);
    TEST_ASSERT_EQUAL_UINT8(1, r.okCount);
    TEST_ASSERT_EQUAL_UINT8(1, r.errCount);
}

void test_devmode_unlocks_dev_keys(void) {
    const char* k[] = {"dev.syncwatcher"};
    const char* v[] = {"1"};
    std::string err; SettingsPair out[2];
    TEST_ASSERT_EQUAL_UINT8(1, settingsValidateRequest(k, v, 1, true, sink, &err, out, 2, nullptr).okCount);
}

void test_secret_string_write_is_validated_and_returned(void) {
    const char* k[] = {"system.ota_password"};
    const char* v[] = {"s3cret"};
    std::string err; SettingsPair out[2]; SettingsStrWrite sw; sw.index = -1;
    SettingsValidateResult r = settingsValidateRequest(k, v, 1, false, sink, &err, out, 2, &sw);
    TEST_ASSERT_EQUAL_UINT8(1, r.okCount);
    TEST_ASSERT_EQUAL_INT(settingsStrFind("system.ota_password"), sw.index);
    TEST_ASSERT_EQUAL_STRING("s3cret", sw.value);
    const char* bad[] = {"has\"quote"};
    sw.index = -1; err.clear();
    r = settingsValidateRequest(k, bad, 1, false, sink, &err, out, 2, &sw);
    TEST_ASSERT_EQUAL_UINT8(1, r.errCount);
    TEST_ASSERT_EQUAL_INT(-1, sw.index);
}

void test_device_only_is_readonly_over_http(void) {
    const char* k[] = {"system.allow_unsigned", "profile.active"};   // the first is gone (no signatures)
    const char* v[] = {"1", "01"};
    std::string err; SettingsPair out[2]; SettingsStrWrite sw; sw.index = -1;
    SettingsValidateResult r = settingsValidateRequest(k, v, 2, true, sink, &err, out, 2, &sw);
    TEST_ASSERT_EQUAL_UINT8(2, r.errCount);
    TEST_ASSERT_TRUE(err.find("\"system.allow_unsigned\":\"unknown\"") != std::string::npos);
    TEST_ASSERT_TRUE(err.find("\"profile.active\":\"readonly\"") != std::string::npos);
}

// Extra hosts are writable over HTTP (web redesign); the shared validator refuses names the guard cannot match
void test_extra_hosts_writable_and_validated(void) {
    const char* k[] = {"system.extra_hosts"};
    const char* good[] = {"tv.lan,gbs.local"};
    const char* bad[] = {"a b"};
    std::string err; SettingsPair out[1]; SettingsStrWrite sw; sw.index = -1;
    SettingsValidateResult r = settingsValidateRequest(k, good, 1, false, sink, &err, out, 1, &sw);
    TEST_ASSERT_EQUAL_UINT8(1, r.okCount);
    TEST_ASSERT_EQUAL_STRING("tv.lan,gbs.local", sw.value);
    sw.index = -1; err.clear();
    r = settingsValidateRequest(k, bad, 1, false, sink, &err, out, 1, &sw);
    TEST_ASSERT_EQUAL_UINT8(1, r.errCount);
    TEST_ASSERT_TRUE(err.find("\"system.extra_hosts\":\"invalid\"") != std::string::npos);
}

void test_string_and_numeric_in_one_request(void) {
    const char* k[] = {"system.ota_password", "adv.brightness"};
    const char* v[] = {"x", "70"};
    std::string err; SettingsPair out[4]; SettingsStrWrite sw; sw.index = -1;
    SettingsValidateResult r = settingsValidateRequest(k, v, 2, false, sink, &err, out, 4, &sw);
    TEST_ASSERT_EQUAL_UINT8(2, r.okCount);
    TEST_ASSERT_EQUAL_UINT8(1, r.pairCount);
    TEST_ASSERT_EQUAL_INT(settingsFind("adv.brightness"), out[0].index);   // numeric pairs start at out[0]
    TEST_ASSERT_EQUAL_UINT16(70, out[0].value);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_validate_returns_pairs_without_writing);
    RUN_TEST(test_validate_reports_each_error_kind);
    RUN_TEST(test_validate_keeps_valid_pairs_next_to_errors);
    RUN_TEST(test_devmode_unlocks_dev_keys);
    RUN_TEST(test_secret_string_write_is_validated_and_returned);
    RUN_TEST(test_device_only_is_readonly_over_http);
    RUN_TEST(test_extra_hosts_writable_and_validated);
    RUN_TEST(test_string_and_numeric_in_one_request);
    return UNITY_END();
}
