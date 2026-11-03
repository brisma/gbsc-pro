#include <unity.h>
#include "api-live.h"
#include "menu-types.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include "api-json.h"

void setUp(void) {}
void tearDown(void) {}

void test_escape(void) {
    char out[32];
    jsonEscape("My \"Wi\\Fi\"\n", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("My \\\"Wi\\\\Fi\\\"\\u000a", out);
}

void test_truncates_without_splitting_escapes(void) {
    char out[6];
    jsonEscape("ab\"cd", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ab\\\"c", out);
    jsonEscape("abcd\"", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("abcd", out);   // no room for the 2-char escape
}

// Chunked JSON by replay: each fill re-runs the builder and keeps only its window, so a response costs no buffer
static void build3(void (*sink)(const char*, void*), void* ctx, const void* state) {
    int n = *(const int*)state;
    sink("{", ctx);
    for (int i = 0; i < n; i++) { char t[24]; snprintf(t, sizeof(t), "%s\"k%d\":%d", i ? "," : "", i, i * 7); sink(t, ctx); }
    sink("}", ctx);
}

void test_replay_fill_concatenates_to_the_whole(void) {
    int n = 40;
    std::string whole;
    JsonReplay all = {build3, &n, 0};
    char big[2048];
    size_t len = jsonReplayFill(all, big, sizeof(big));
    whole.assign(big, len);
    TEST_ASSERT_TRUE(whole.size() > 300 && whole[0] == '{' && whole.back() == '}');
    const size_t sizes[] = {1, 7, 64, 333};
    for (size_t sz : sizes) {
        JsonReplay r = {build3, &n, 0};
        std::string got;
        char buf[400];
        for (int guard = 0; guard < 2000; guard++) {
            size_t k = jsonReplayFill(r, buf, sz);
            if (!k) break;
            got.append(buf, k);
        }
        TEST_ASSERT_EQUAL_STRING(whole.c_str(), got.c_str());
    }
}

// GET /api/dev/regs: 256 register values as 512 lowercase hex characters
void test_regs_hex_format(void) {
    uint8_t v[256];
    for (int i = 0; i < 256; i++) v[i] = (uint8_t)i;
    char out[513];
    TEST_ASSERT_EQUAL_UINT32(512, regsHexFormat(v, 256, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(512, strlen(out));
    TEST_ASSERT_EQUAL_STRING_LEN("000102", out, 6);
    TEST_ASSERT_EQUAL_STRING("fdfeff", out + 506);
    TEST_ASSERT_EQUAL_UINT32(0, regsHexFormat(v, 256, out, 512));   // no room for the NUL: nothing
    TEST_ASSERT_EQUAL_STRING("", out);
}

// GET /api/wifi body, built off the stack of the network context
void test_wifi_json(void) {
    char out[WIFI_JSON_MAX];
    TEST_ASSERT_TRUE(wifiJson(true, "gbscontrol", "192.168.4.1", "gbscontrol.local", "tv.lan", "qqqqqqqq", out, sizeof(out)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"mode\":\"AP\",\"ssid\":\"gbscontrol\",\"ip\":\"192.168.4.1\",\"hostname\":\"gbscontrol.local\","
                             "\"extra_hosts\":\"tv.lan\",\"ap_password\":\"qqqqqqqq\"}", out);
    wifiJson(false, "My \"Net\"", "10.0.0.9", "gbscontrol.local", "", "ignored", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"mode\":\"STA\",\"ssid\":\"My \\\"Net\\\"\",\"ip\":\"10.0.0.9\",\"hostname\":\"gbscontrol.local\","
                             "\"extra_hosts\":\"\",\"ap_password\":null}", out);
    std::string ssid(32, '"'), hosts(63, 'a');                                  // the widest values fit
    TEST_ASSERT_TRUE(wifiJson(true, ssid.c_str(), "255.255.255.255", "gbscontrol.local", hosts.c_str(), ssid.c_str(), out, sizeof(out)) > 0);
}

// /api/info signal: the menu predicates' inputs the page needs
void test_info_signal_json(void) {
    InfoSignal sg = {false, 2, 240, 599, 600, false, true, false, 17};
    char out[200];
    size_t n = infoSignalJson(sg, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"none\":0,\"std\":2,\"lines\":240,\"src_hz10\":599,\"out_hz10\":600,\"interlaced\":0,"
                             "\"scanlines_ok\":1,\"adv_ntsc\":0,\"preset\":17}", out);
    TEST_ASSERT_EQUAL_UINT32(strlen(out), n);
    TEST_ASSERT_EQUAL_UINT32(0, infoSignalJson(sg, out, 20));
}

// GET /api/live: the menu's readbacks for the web rows, names = MenuLive without LV_, lowercase;
// pairs (position, size, borders) as [h,v]; values not read yet are absent
void test_live_json(void) {
    int32_t v[LV_COUNT] = {0};
    v[LV_ADC_GAIN] = 123;
    v[LV_POSITION] = (40 << 16) | 7;
    v[LV_SOG] = -1;
    char out[LIVE_JSON_MAX];
    size_t n = liveJson(v, (1u << LV_ADC_GAIN) | (1u << LV_POSITION), 5000, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"at\":5000,\"values\":{\"adc_gain\":123,\"position\":[40,7]}}", out);
    TEST_ASSERT_EQUAL_UINT32(strlen(out), n);
    liveJson(v, 1u << LV_SOG, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"at\":1,\"values\":{\"sog\":-1}}", out);
    liveJson(v, 0, 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"at\":2,\"values\":{}}", out);
    TEST_ASSERT_EQUAL_UINT32(0, liveJson(v, (1u << LV_ADC_GAIN) | (1u << LV_POSITION), 5000, out, 30));   // does not fit
    for (uint8_t i = 0; i < LV_COUNT; i++) v[i] = i == LV_POSITION || i == LV_SIZE || i == LV_BORDERS || i == LV_BORDER_POS ? (int32_t)0x7fff7fff : INT32_MIN;
    TEST_ASSERT_TRUE(liveJson(v, 0xffffffffu, 0xffffffffu, out, sizeof(out)) > 0);   // every value at its widest fits
    TEST_ASSERT_TRUE(strstr(out, "\"none\"") == nullptr);                          // LV_NONE is never sent
    TEST_ASSERT_TRUE(strstr(out, "\"ota\":") != nullptr);
    TEST_ASSERT_TRUE(strstr(out, "\"border_pos\":[32767,32767]") != nullptr);   // the mask's left / top edge, a pair
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_escape);
    RUN_TEST(test_truncates_without_splitting_escapes);
    RUN_TEST(test_replay_fill_concatenates_to_the_whole);
    RUN_TEST(test_regs_hex_format);
    RUN_TEST(test_wifi_json);
    RUN_TEST(test_info_signal_json);
    RUN_TEST(test_live_json);
    return UNITY_END();
}
