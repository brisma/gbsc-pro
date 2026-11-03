#include <unity.h>
#include "marquee.h"

void setUp(void) {}
void tearDown(void) {}

static const MarqueeTiming kT = {1500, 300};

void test_short_text_never_moves(void) {
    TEST_ASSERT_EQUAL_UINT16(0, marqueeOffset(20, 28, 0, kT));
    TEST_ASSERT_EQUAL_UINT16(0, marqueeOffset(28, 28, 99999, kT));
}

void test_hold_then_steps_then_hold_then_restart(void) {
    // 40 characters in 28 cells: 12 steps
    TEST_ASSERT_EQUAL_UINT16(0, marqueeOffset(40, 28, 0, kT));
    TEST_ASSERT_EQUAL_UINT16(0, marqueeOffset(40, 28, 1499, kT));     // start hold
    TEST_ASSERT_EQUAL_UINT16(1, marqueeOffset(40, 28, 1500, kT));
    TEST_ASSERT_EQUAL_UINT16(1, marqueeOffset(40, 28, 1799, kT));
    TEST_ASSERT_EQUAL_UINT16(2, marqueeOffset(40, 28, 1800, kT));
    uint32_t end = 1500 + 11 * 300;                                    // offset 12 reached
    TEST_ASSERT_EQUAL_UINT16(12, marqueeOffset(40, 28, end, kT));
    TEST_ASSERT_EQUAL_UINT16(12, marqueeOffset(40, 28, end + 1499 + 300, kT));   // end hold
    uint32_t cycle = marqueeCycleMs(40, 28, kT);
    TEST_ASSERT_EQUAL_UINT32(1500 + 12 * 300 + 1500, cycle);
    TEST_ASSERT_EQUAL_UINT16(0, marqueeOffset(40, 28, cycle, kT));   // restarts
    TEST_ASSERT_EQUAL_UINT16(1, marqueeOffset(40, 28, cycle + 1500, kT));
}

void test_pixel_units(void) {
    MarqueeTiming px = {1200, 100};
    TEST_ASSERT_EQUAL_UINT16(0, marqueeOffset(150, 120, 1100, px));
    TEST_ASSERT_EQUAL_UINT16(3, marqueeOffset(150, 120, 1200, px, 3));   // 3 px per step
    TEST_ASSERT_EQUAL_UINT16(30, marqueeOffset(150, 120, 1200 + 20 * 100, px, 3));   // clamped at the end
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_short_text_never_moves);
    RUN_TEST(test_hold_then_steps_then_hold_then_restart);
    RUN_TEST(test_pixel_units);
    return UNITY_END();
}
