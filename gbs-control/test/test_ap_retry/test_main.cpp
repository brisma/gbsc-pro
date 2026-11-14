// Access point fallback: the saved network is tried again every minute of access point time, never while a phone or
// a computer is joined to the access point or an update runs
#include <unity.h>
#include "ap-retry.h"

void setUp(void) {}
void tearDown(void) {}

void test_never_outside_the_access_point(void) {
    ApRetry r;
    TEST_ASSERT_FALSE(r.due(0, false, false, false));
    TEST_ASSERT_FALSE(r.due(10 * ApRetry::kEveryMs, false, false, false));
}

void test_a_minute_after_the_access_point_came_up(void) {
    ApRetry r;
    uint32_t t = 45000;                                              // the 45 s first connect gave up
    TEST_ASSERT_FALSE(r.due(t, true, false, false));
    TEST_ASSERT_FALSE(r.due(t + ApRetry::kEveryMs - 1, true, false, false));
    TEST_ASSERT_TRUE(r.due(t + ApRetry::kEveryMs, true, false, false));
    TEST_ASSERT_FALSE(r.due(t + ApRetry::kEveryMs + 1, true, false, false));   // one try, then a minute again
}

void test_not_while_someone_is_on_the_access_point_or_an_update_runs(void) {
    ApRetry r;
    TEST_ASSERT_FALSE(r.due(1000, true, false, false));
    TEST_ASSERT_FALSE(r.due(1000 + ApRetry::kEveryMs, true, true, false));    // a phone joined
    TEST_ASSERT_FALSE(r.due(1000 + ApRetry::kEveryMs, true, false, true));    // update
    TEST_ASSERT_TRUE(r.due(2000 + ApRetry::kEveryMs, true, false, false));    // as soon as they are gone
}

void test_the_clock_restarts_when_the_access_point_comes_back(void) {
    ApRetry r;
    TEST_ASSERT_FALSE(r.due(1000, true, false, false));
    TEST_ASSERT_TRUE(r.due(1000 + ApRetry::kEveryMs, true, false, false));
    uint32_t back = 1000 + ApRetry::kEveryMs + ApRetry::kTryS * 1000;  // the try failed: access point again
    TEST_ASSERT_FALSE(r.due(back - 500, false, false, false));
    TEST_ASSERT_FALSE(r.due(back, true, false, false));
    TEST_ASSERT_FALSE(r.due(back + ApRetry::kEveryMs - 1, true, false, false));
    TEST_ASSERT_TRUE(r.due(back + ApRetry::kEveryMs, true, false, false));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_never_outside_the_access_point);
    RUN_TEST(test_a_minute_after_the_access_point_came_up);
    RUN_TEST(test_not_while_someone_is_on_the_access_point_or_an_update_runs);
    RUN_TEST(test_the_clock_restarts_when_the_access_point_comes_back);
    return UNITY_END();
}
