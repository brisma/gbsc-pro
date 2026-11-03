#include <string.h>
#include <unity.h>
#include "platform-caps.h"

void setUp(void) { platformSetCaps(CAPS_DEFAULT); }
void tearDown(void) {}

static PlatformProbes probes(bool stv, bool pt, bool oled) {
    PlatformProbes p; p.stv = {stv, 1}; p.pt = {pt, 1}; p.oled = {oled, 1}; return p;
}

void test_stv_alone_means_pro(void) {
    TEST_ASSERT_EQUAL_HEX8(CAPS_PRO, platformCapsFrom(probes(true, false, false), BOARD_AUTO));
}
void test_pt2257_alone_means_pro(void) {
    TEST_ASSERT_EQUAL_HEX8(CAPS_PRO, platformCapsFrom(probes(false, true, false), BOARD_AUTO));
}
void test_no_pro_chip_means_original(void) {
    TEST_ASSERT_EQUAL_HEX8(CAP_ORIGINAL | CAP_LED, platformCapsFrom(probes(false, false, false), BOARD_AUTO));
}
void test_oled_adds_oled_and_encoder(void) {
    TEST_ASSERT_EQUAL_HEX8(CAPS_PRO | CAP_OLED | CAP_ENCODER, platformCapsFrom(probes(true, true, true), BOARD_AUTO));
    TEST_ASSERT_EQUAL_HEX8(CAP_ORIGINAL | CAP_LED | CAP_OLED | CAP_ENCODER, platformCapsFrom(probes(false, false, true), BOARD_AUTO));
}
void test_board_setting_overrides_probes(void) {
    // Original forced on a Pro board: GPIO2 is the IR receiver there, so no LED
    TEST_ASSERT_EQUAL_HEX8(CAP_ORIGINAL, platformCapsFrom(probes(true, true, false), BOARD_ORIGINAL));
    TEST_ASSERT_EQUAL_HEX8(CAP_ORIGINAL | CAP_LED, platformCapsFrom(probes(false, false, false), BOARD_ORIGINAL));
    TEST_ASSERT_EQUAL_HEX8(CAPS_PRO, platformCapsFrom(probes(false, false, false), BOARD_PRO));
}
// With no manual recovery for a lost race, the OLED alone does not end the detection rounds
void test_detect_waits_for_pro_chips_when_only_oled_answers(void) {
    for (uint8_t r = 1; r < 5; r++) TEST_ASSERT_FALSE(platformDetectDone(probes(false, false, true), r));
    TEST_ASSERT_TRUE(platformDetectDone(probes(false, false, true), 5));
    TEST_ASSERT_TRUE(platformDetectDone(probes(true, false, false), 1));
    TEST_ASSERT_TRUE(platformDetectDone(probes(false, true, true), 1));
    TEST_ASSERT_FALSE(platformDetectDone(probes(false, false, false), 4));
    TEST_ASSERT_TRUE(platformDetectDone(probes(false, false, false), 5));
}

void test_probes_say_pro(void) {
    TEST_ASSERT_TRUE(platformProbesSayPro(probes(true, false, false)));
    TEST_ASSERT_TRUE(platformProbesSayPro(probes(false, true, false)));
    TEST_ASSERT_FALSE(platformProbesSayPro(probes(false, false, true)));
}

void test_hwhas_requires_all_bits(void) {
    platformSetCaps(CAP_ADV | CAP_OLED);
    TEST_ASSERT_TRUE(hwHas(CAP_ADV));
    TEST_ASSERT_TRUE(hwHas(CAP_NONE));
    TEST_ASSERT_FALSE(hwHas(CAP_ADV | CAP_OSD));
}
void test_cap_names(void) {
    TEST_ASSERT_EQUAL_HEX8(CAP_ADV, platformCapFromName("adv", 3));
    TEST_ASSERT_EQUAL_HEX8(CAP_ORIGINAL, platformCapFromName("original", 8));
    TEST_ASSERT_EQUAL_HEX8(CAP_ENCODER, platformCapFromName("encoder", 7));
    TEST_ASSERT_EQUAL_HEX8(0, platformCapFromName("advx", 4));
    TEST_ASSERT_EQUAL_HEX8(0, platformCapFromName("", 0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stv_alone_means_pro);
    RUN_TEST(test_pt2257_alone_means_pro);
    RUN_TEST(test_no_pro_chip_means_original);
    RUN_TEST(test_oled_adds_oled_and_encoder);
    RUN_TEST(test_board_setting_overrides_probes);
    RUN_TEST(test_detect_waits_for_pro_chips_when_only_oled_answers);
    RUN_TEST(test_probes_say_pro);
    RUN_TEST(test_hwhas_requires_all_bits);
    RUN_TEST(test_cap_names);
    return UNITY_END();
}
