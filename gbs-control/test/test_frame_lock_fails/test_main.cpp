// Frame lock runs that fail in a row: 16 are let through, the 17th resets the frame lock, and the count starts again
#include <unity.h>
#include "../../pro/clock/frame-lock-fails.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t failsUntilReset(uint8_t& left) {
    for (uint8_t n = 1; n < 255; n++)
        if (frameLockFailed(left)) return n;
    return 255;
}

void test_the_17th_failure_in_a_row_resets(void) {
    uint8_t left = kFrameLockFailsAllowed;
    TEST_ASSERT_EQUAL_UINT8(kFrameLockFailsAllowed + 1, failsUntilReset(left));
}

void test_after_a_reset_the_next_comes_as_soon(void) {
    uint8_t left = kFrameLockFailsAllowed;
    failsUntilReset(left);
    TEST_ASSERT_EQUAL_UINT8(kFrameLockFailsAllowed, left);                        // not wrapped to 255
    TEST_ASSERT_EQUAL_UINT8(kFrameLockFailsAllowed + 1, failsUntilReset(left));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_17th_failure_in_a_row_resets);
    RUN_TEST(test_after_a_reset_the_next_comes_as_soon);
    return UNITY_END();
}
