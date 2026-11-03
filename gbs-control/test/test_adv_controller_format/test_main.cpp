#include <unity.h>
#include <string.h>
#include <stdio.h>
#include "adv-controller-format.h"

void setUp(void) {}
void tearDown(void) {}

static AdvControllerInfo info(AdvControllerStatus st, const char* fw) {
    AdvControllerInfo i; memset(&i, 0, sizeof(i));
    i.status = st; i.haveInfo = st == ADV_CONTROLLER_OK || st == ADV_CONTROLLER_INCOMPATIBLE;
    strncpy(i.info.fw, fw, sizeof(i.info.fw) - 1);
    i.info.buildId = 0xC3A7B74E; i.info.flags = ADV_INFO_LOCK; i.info.standard = 2; i.info.uptimeS = 77; i.info.badFrames = 3;
    return i;
}

void test_menu_text(void) {
    char t[24];
    advControllerMenuText(info(ADV_CONTROLLER_CHECKING, ""), t, sizeof(t)); TEST_ASSERT_EQUAL_STRING("Checking...", t);
    advControllerMenuText(info(ADV_CONTROLLER_NO_REPLY, ""), t, sizeof(t)); TEST_ASSERT_EQUAL_STRING("No reply", t);
    advControllerMenuText(info(ADV_CONTROLLER_INCOMPATIBLE, "4.0.0"), t, sizeof(t)); TEST_ASSERT_EQUAL_STRING("Incompatible", t);
    advControllerMenuText(info(ADV_CONTROLLER_OK, "3.0.0"), t, sizeof(t)); TEST_ASSERT_EQUAL_STRING("3.0.0", t);
    advControllerMenuText(info(ADV_CONTROLLER_OK, "3.0_x\"<"), t, sizeof(t)); TEST_ASSERT_EQUAL_STRING("3.0 x..", t);   // OSD charset only
}

void test_json(void) {
    char j[200];
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", j, sizeof(j)));
    TEST_ASSERT_EQUAL_STRING("{\"status\":\"ok\",\"version\":\"3.0.0\",\"build\":\"C3A7B74E\",\"lock\":1,"
                             "\"standard\":2,\"uptime\":77,\"bad_frames\":3,\"not_applied\":false,\"update_available\":false,\"update\":\"idle\","
                             "\"caps\":[]}", j);
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_NO_REPLY, ""), false, "idle", j, sizeof(j)));
    TEST_ASSERT_EQUAL_STRING("{\"status\":\"no_reply\"}", j);
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_CHECKING, ""), false, "idle", j, sizeof(j)));
    TEST_ASSERT_EQUAL_STRING("{\"status\":\"checking\"}", j);
}

void test_json_escapes_the_wire_string(void) {
    char j[200];
    advControllerStatusJson(info(ADV_CONTROLLER_OK, "a\"b\\c"), false, "idle", j, sizeof(j));
    TEST_ASSERT_TRUE(strstr(j, "\"version\":\"a\\\"b\\\\c\"") != nullptr);
}

void test_json_reports_truncation(void) {
    char j[20];
    TEST_ASSERT_FALSE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", j, sizeof(j)));
}

void test_json_update_available(void) {
    char j[256];
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), true, "idle", j, sizeof(j)));
    TEST_ASSERT_TRUE(strstr(j, "\"update_available\":true") != nullptr);
}

void test_json_update_state(void) {
    char j[256];
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), true, "failed:crc", j, sizeof(j)));
    TEST_ASSERT_TRUE(strstr(j, "\"update\":\"failed:crc\"") != nullptr);
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_NO_REPLY, ""), false, "running", j, sizeof(j)));
    TEST_ASSERT_EQUAL_STRING("{\"status\":\"no_reply\",\"update\":\"running\"}", j);   // during the copy
}

// v4 signal report: stable names for the web app and a future mobile app
static AdvSignal sig(uint8_t flags, uint8_t standard, uint8_t hz, uint8_t encoder) {
    AdvSignal g; memset(&g, 0, sizeof(g));
    g.gen = 12; g.flags = flags; g.standard = standard; g.hz = hz; g.vidSel = 0x04; g.encoder = encoder;
    return g;
}

void test_json_signal_locked(void) {
    char j[512];
    AdvSignal g = sig(ADV_SIG_POWERED | ADV_SIG_LOCK | ADV_SIG_PRESENT | ADV_SIG_FSC | ADV_SIG_INTERLACED, 4, 50, 2);
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", j, sizeof(j), &g));
    TEST_ASSERT_NOT_NULL(strstr(j, ",\"signal\":{\"gen\":12,\"powered\":true,\"present\":true,\"lock\":true,"
                                   "\"fsc\":true,\"freerun\":false,\"interlaced\":true,\"colorkill\":false,"
                                   "\"nonstd\":false,\"standard\":\"PAL\",\"hz\":50,\"vidsel\":\"0x04\",\"encoder\":\"625p\"}}"));
}

// LL_NSTD as "nonstd"; the encoder is ED only (525p/625p): another value is unknown
void test_json_signal_nonstd_and_unknown_encoder(void) {
    char j[512];
    AdvSignal g = sig(ADV_SIG_POWERED | ADV_SIG_LOCK | ADV_SIG_NONSTD, 0, 60, 3);
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", j, sizeof(j), &g));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"nonstd\":true"));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"encoder\":null"));
}

void test_json_signal_unlocked_has_nulls(void) {
    char j[512];
    AdvSignal g = sig(ADV_SIG_POWERED, ADV_SIG_NO_STANDARD, 0, 0);
    TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", j, sizeof(j), &g));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"standard\":null,\"hz\":null,\"vidsel\":\"0x04\",\"encoder\":null}"));
}

void test_json_signal_standard_names(void) {
    static const char* const want[] = {"NTSC_M", "NTSC_443", "PAL_M", "PAL_60", "PAL", "SECAM", "PAL_N", "SECAM_525"};
    char j[512], w[32];
    for (uint8_t k = 0; k < 8; k++) {
        AdvSignal g = sig(ADV_SIG_LOCK, k, 60, 1);
        TEST_ASSERT_TRUE(advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", j, sizeof(j), &g));
        snprintf(w, sizeof(w), "\"standard\":\"%s\"", want[k]);
        TEST_ASSERT_NOT_NULL(strstr(j, w));
    }
}

// The worst case fits the /api/info buffer (api-web.cpp ADV_CONTROLLER_JSON_MAX)
void test_json_signal_worst_case_fits(void) {
    char j[ADV_CONTROLLER_JSON_MAX];
    AdvControllerInfo h = info(ADV_CONTROLLER_OK, "\"\"\"\"\"\"\"\"\"\"\"");   // 11 quotes: escaped to 22 bytes
    h.info.buildId = 0xFFFFFFFF; h.info.standard = 255; h.info.uptimeS = 65535; h.info.badFrames = 65535; h.info.caps = 0xFF;
    AdvSignal g = sig(0xFF, 7, 60, 1); g.gen = 255; g.vidSel = 0xFF;
    TEST_ASSERT_TRUE(advControllerStatusJson(h, true, "failed:header", j, sizeof(j), &g));
}

void test_json_without_signal_is_unchanged(void) {
    char a[512], b[512];
    advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", a, sizeof(a));
    advControllerStatusJson(info(ADV_CONTROLLER_OK, "3.0.0"), false, "idle", b, sizeof(b), nullptr);
    TEST_ASSERT_EQUAL_STRING(a, b);
    TEST_ASSERT_NULL(strstr(a, "signal"));
}

void test_adv_standard_prefers_a_fresh_signal(void) {
    AdvControllerInfo h = info(ADV_CONTROLLER_OK, "3.0.0");                     // INFO: locked, standard 2
    AdvSignal g = sig(ADV_SIG_LOCK, 4, 50, 2);
    TEST_ASSERT_EQUAL_INT8(4, advControllerStandard(h, &g, true));
    TEST_ASSERT_EQUAL_INT8(2, advControllerStandard(h, &g, false));  // stale signal: INFO
    TEST_ASSERT_EQUAL_INT8(2, advControllerStandard(h, nullptr, true));
    AdvSignal u = sig(ADV_SIG_POWERED, ADV_SIG_NO_STANDARD, 0, 0);
    TEST_ASSERT_EQUAL_INT8(-1, advControllerStandard(h, &u, true));
    h.info.flags = 0;
    TEST_ASSERT_EQUAL_INT8(-1, advControllerStandard(h, nullptr, false));
}

// /api/info adv_controller.caps: what the image answers (the web shows the register read only with reg_read)
void test_json_caps(void) {
    char j[ADV_CONTROLLER_JSON_MAX];
    AdvControllerInfo h = info(ADV_CONTROLLER_OK, "3.0.0");
    h.info.caps = ADV_INFO_CAP_SIGNAL | ADV_INFO_CAP_REG_READ;
    TEST_ASSERT_TRUE(advControllerStatusJson(h, false, "idle", j, sizeof(j)));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"caps\":[\"signal\",\"reg_read\"]}"));
    h.info.caps = ADV_INFO_CAP_SIGNAL;
    TEST_ASSERT_TRUE(advControllerStatusJson(h, false, "idle", j, sizeof(j)));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"caps\":[\"signal\"]}"));
    h.info.caps = ADV_INFO_CAP_SIGNAL | ADV_INFO_CAP_REG_READ | ADV_INFO_CAP_LISTEN;   // the input search's listen
    TEST_ASSERT_TRUE(advControllerStatusJson(h, false, "idle", j, sizeof(j)));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"caps\":[\"signal\",\"reg_read\",\"listen\"]}"));
    h.info.caps = ADV_INFO_CAP_LISTEN;
    TEST_ASSERT_TRUE(advControllerStatusJson(h, false, "idle", j, sizeof(j)));
    TEST_ASSERT_NOT_NULL(strstr(j, "\"caps\":[\"listen\"]}"));
}

// GET /api/adv/read: an announced read not run yet is pending with the announced gen
void test_reg_read_json(void) {
    AdvRegRead r; memset(&r, 0, sizeof(r));
    r.state = ADV_REG_DONE; r.dev = 0x42; r.map = 0x40; r.reg = 0x80; r.count = 3; r.gen = 5;
    r.v[0] = 1; r.v[1] = 0xAB; r.v[2] = 255;
    char j[ADV_REG_READ_JSON_MAX];
    TEST_ASSERT_TRUE(advRegReadJson(r, 5, j, sizeof(j)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"done\",\"dev\":66,\"map\":64,\"reg\":128,\"values\":[1,171,255],\"gen\":5}", j);
    advRegReadJson(r, 6, j, sizeof(j));
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"pending\",\"gen\":6}", j);
    r.state = ADV_REG_UNSUPPORTED;
    advRegReadJson(r, 5, j, sizeof(j));
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"unsupported\",\"dev\":66,\"map\":64,\"reg\":128,\"values\":[],\"gen\":5}", j);
    r.state = ADV_REG_DONE; r.count = 32;
    for (int i = 0; i < 32; i++) r.v[i] = 255;
    TEST_ASSERT_TRUE(advRegReadJson(r, 5, j, sizeof(j)) > 0);       // the widest result fits
    TEST_ASSERT_EQUAL_UINT32(0, advRegReadJson(r, 5, j, 20));         // too small: nothing
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_menu_text);
    RUN_TEST(test_json);
    RUN_TEST(test_json_escapes_the_wire_string);
    RUN_TEST(test_json_reports_truncation);
    RUN_TEST(test_json_update_available);
    RUN_TEST(test_json_update_state);
    RUN_TEST(test_json_signal_locked);
    RUN_TEST(test_json_signal_nonstd_and_unknown_encoder);
    RUN_TEST(test_json_signal_unlocked_has_nulls);
    RUN_TEST(test_json_signal_standard_names);
    RUN_TEST(test_json_signal_worst_case_fits);
    RUN_TEST(test_json_caps);
    RUN_TEST(test_reg_read_json);
    RUN_TEST(test_json_without_signal_is_unchanged);
    RUN_TEST(test_adv_standard_prefers_a_fresh_signal);
    return UNITY_END();
}
