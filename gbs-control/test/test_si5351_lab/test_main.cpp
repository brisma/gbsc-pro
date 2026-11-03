// Si5351 evaluation helpers: divider and VCO as the library computes them, the nx rule, status decoding
#include <unity.h>
#include "../../pro/clock/si5351-lab.h"

void setUp(void) {}
void tearDown(void) {}

void test_library_divider_and_vco(void) {
    uint8_t r = 0;
    TEST_ASSERT_EQUAL_UINT32(10, si5351OutDivider(81000000, false, &r));
    TEST_ASSERT_EQUAL_UINT8(1, r);
    TEST_ASSERT_EQUAL_UINT32(810000000, si5351VcoHz(81000000, false));
    TEST_ASSERT_EQUAL_UINT32(864000000, si5351VcoHz(108000000, false));
    TEST_ASSERT_EQUAL_UINT32(648000000, si5351VcoHz(162000000, false));
    TEST_ASSERT_EQUAL_UINT32(777600000, si5351VcoHz(129600000, false));
    TEST_ASSERT_EQUAL_UINT32(864000000, si5351VcoHz(54000000, false));
}

void test_nx_rule_moves_81_and_108_to_648(void) {
    TEST_ASSERT_EQUAL_UINT32(8, si5351OutDivider(81000000, true, nullptr));
    TEST_ASSERT_EQUAL_UINT32(648000000, si5351VcoHz(81000000, true));
    TEST_ASSERT_EQUAL_UINT32(6, si5351OutDivider(108000000, true, nullptr));
    TEST_ASSERT_EQUAL_UINT32(648000000, si5351VcoHz(108000000, true));
    TEST_ASSERT_EQUAL_UINT32(648000000, si5351VcoHz(162000000, true));       // unchanged
    TEST_ASSERT_EQUAL_UINT32(864000000, si5351VcoHz(54000000, true));        // outside nx's ranges
}

void test_low_frequency_uses_r_divider(void) {
    uint8_t r = 0;
    uint32_t d = si5351OutDivider(500000, false, &r);     // 900 MHz / 0.5 MHz = 1800 > 900
    TEST_ASSERT_EQUAL_UINT8(2, r);
    TEST_ASSERT_EQUAL_UINT32(900, d);
}

void test_status_bits(void) {
    // AN619 register 0: SYS_INIT 7, LOL_B 6, LOL_A 5, LOS_CLKIN 4 (Si5351C only), LOS_XTAL 3, REVID 1:0
    TEST_ASSERT_FALSE(si5351DecodeStatus(0x10).los);       // LOS_CLKIN: no CLKIN on a Si5351A
    Si5351Status s = si5351DecodeStatus(0xA9);             // SYS_INIT, LOL_A, LOS_XTAL, rev 1
    TEST_ASSERT_TRUE(s.sysInit);
    TEST_ASSERT_TRUE(s.lolA);
    TEST_ASSERT_FALSE(s.lolB);
    TEST_ASSERT_TRUE(s.los);
    TEST_ASSERT_EQUAL_UINT8(1, s.revId);
    Si5351Status ok = si5351DecodeStatus(0x00);
    TEST_ASSERT_FALSE(ok.sysInit || ok.lolA || ok.lolB || ok.los);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_library_divider_and_vco);
    RUN_TEST(test_nx_rule_moves_81_and_108_to_648);
    RUN_TEST(test_low_frequency_uses_r_divider);
    RUN_TEST(test_status_bits);
    return UNITY_END();
}
