// Volume in percent: 100 % = 0 dB (quiet sources need the full range), 80 % = -12 dB
// (the old default step 38), 1 % about -59 dB, 0 % is the PT2257 mute; 1 % = 0.6 dB in between
#include <unity.h>
#include "pt2257-map.h"

void setUp(void) {}
void tearDown(void) {}

void test_ends_and_middle(void) {
    TEST_ASSERT_EQUAL_UINT8(0, pt2257AttenuationDb(100));
    TEST_ASSERT_EQUAL_UINT8(12, pt2257AttenuationDb(80));
    TEST_ASSERT_EQUAL_UINT8(30, pt2257AttenuationDb(50));
    TEST_ASSERT_EQUAL_UINT8(59, pt2257AttenuationDb(1));
    TEST_ASSERT_TRUE(pt2257Silent(0));
    TEST_ASSERT_FALSE(pt2257Silent(1));
    TEST_ASSERT_EQUAL_UINT8(0, pt2257AttenuationDb(250));    // out of range: clamped to 100 %
}

void test_monotonic_and_within_the_chip(void) {
    for (int p = 1; p < 100; p++) {
        TEST_ASSERT_TRUE(pt2257AttenuationDb((uint8_t)p) >= pt2257AttenuationDb((uint8_t)(p + 1)));
        TEST_ASSERT_TRUE(pt2257AttenuationDb((uint8_t)p) <= 79);
    }
}

// The chip takes the tens and the units as two bytes; between them it plays the mix of old and new. The order
// is chosen so that mix is never louder than both ends (-20 -> -19 used to pass through -10 dB: a click)
static uint8_t afterFirst(uint8_t from, uint8_t to, const uint8_t cmd[2]) {
    bool tensFirst = (cmd[0] & 0xF0) == 0xE0;
    return tensFirst ? (uint8_t)((to / 10) * 10 + from % 10) : (uint8_t)((from / 10) * 10 + to % 10);
}

void test_step_order_never_passes_through_a_louder_level(void) {
    for (int from = 0; from <= 79; from++)
        for (int to = 0; to <= 79; to++) {
            uint8_t cmd[2];
            pt2257StepCommands((uint8_t)from, (uint8_t)to, cmd);
            TEST_ASSERT_EQUAL_HEX8(0xD0 + to % 10, (cmd[0] & 0xF0) == 0xD0 ? cmd[0] : cmd[1]);
            TEST_ASSERT_EQUAL_HEX8(0xE0 + to / 10, (cmd[0] & 0xF0) == 0xE0 ? cmd[0] : cmd[1]);
            uint8_t mid = afterFirst((uint8_t)from, (uint8_t)to, cmd);
            TEST_ASSERT_TRUE_MESSAGE(mid >= (from < to ? from : to), "louder than both ends");
        }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ends_and_middle);
    RUN_TEST(test_monotonic_and_within_the_chip);
    RUN_TEST(test_step_order_never_passes_through_a_louder_level);
    return UNITY_END();
}
