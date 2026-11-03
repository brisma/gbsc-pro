// Issue #27: Screen > Border position moves the masked window (both blanking edges together)
#include <unity.h>
#include "screen-borders.h"

void setUp(void) {}
void tearDown(void) {}

void test_moves_both_edges_and_keeps_the_width(void) {
    uint16_t st, sp;
    TEST_ASSERT_TRUE(screenBorderMove(1800, 200, 2000, 4, 4, st, sp));
    TEST_ASSERT_EQUAL_UINT16(1804, st); TEST_ASSERT_EQUAL_UINT16(204, sp);
    TEST_ASSERT_TRUE(screenBorderMove(1800, 200, 2000, -4, 4, st, sp));
    TEST_ASSERT_EQUAL_UINT16(1796, st); TEST_ASSERT_EQUAL_UINT16(196, sp);
}

void test_stops_at_the_edges(void) {
    uint16_t st = 0, sp = 0;
    TEST_ASSERT_FALSE(screenBorderMove(1998, 200, 2000, 4, 4, st, sp));   // the blank start would pass total
    TEST_ASSERT_FALSE(screenBorderMove(1800, 6, 2000, -4, 4, st, sp));     // the blank stop would go under the margin
    TEST_ASSERT_TRUE(screenBorderMove(1800, 8, 2000, -4, 4, st, sp));
    TEST_ASSERT_EQUAL_UINT16(4, sp);
}

// Most presets have the vertical blank start at VTOTAL; only the edge moving towards its limit is checked
void test_window_already_at_the_end_still_moves_back(void) {
    uint16_t st, sp;
    TEST_ASSERT_TRUE(screenBorderMove(1000, 40, 1000, -2, 6, st, sp));     // VB_ST == VTOTAL: up works
    TEST_ASSERT_EQUAL_UINT16(998, st); TEST_ASSERT_EQUAL_UINT16(38, sp);
    TEST_ASSERT_FALSE(screenBorderMove(1000, 40, 1000, 2, 6, st, sp));     // down: no room
    TEST_ASSERT_TRUE(screenBorderMove(998, 38, 1000, 2, 6, st, sp));       // and back to where the preset started
    TEST_ASSERT_EQUAL_UINT16(1000, st);
    TEST_ASSERT_TRUE(screenBorderMove(1233, 33, 1235, 2, 6, st, sp));      // 1200p: down by 2 still fits
    TEST_ASSERT_TRUE(screenBorderMove(1116, 100, 1116, -4, 4, st, sp));    // ofw presets: HB_ST == HTOTAL
}

void test_refuses_a_wrapped_or_empty_window(void) {
    uint16_t st, sp;
    TEST_ASSERT_FALSE(screenBorderMove(100, 1800, 2000, 4, 4, st, sp));    // blank start before blank stop
    TEST_ASSERT_FALSE(screenBorderMove(500, 500, 2000, 4, 4, st, sp));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_moves_both_edges_and_keeps_the_width);
    RUN_TEST(test_stops_at_the_edges);
    RUN_TEST(test_window_already_at_the_end_still_moves_back);
    RUN_TEST(test_refuses_a_wrapped_or_empty_window);
    return UNITY_END();
}
