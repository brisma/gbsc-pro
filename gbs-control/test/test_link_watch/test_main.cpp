// WiFi link watch: 3 echoes to the gateway every minute (every 10 s while bad), a line only when it goes bad and
// one when it is ok again
#include <unity.h>
#include "link-watch.h"

void setUp(void) {}
void tearDown(void) {}

static LinkCheck check(uint8_t got, uint16_t worstMs) { return {LinkWatch::kEchoes, got, worstMs}; }

void test_checks_every_minute_when_connected(void) {
    LinkWatch w;
    TEST_ASSERT_FALSE(w.due(1000, true, false));                     // the first one a minute after the start
    TEST_ASSERT_TRUE(w.due(LinkWatch::kPeriodMs, true, false));
    TEST_ASSERT_FALSE(w.due(LinkWatch::kPeriodMs, false, false));    // not connected
    TEST_ASSERT_FALSE(w.due(LinkWatch::kPeriodMs, true, true));      // update, OTA, WiFi scan or the tune
    TEST_ASSERT_EQUAL_UINT8(LW_NONE, w.result(check(3, 12), LinkWatch::kPeriodMs));
    TEST_ASSERT_FALSE(w.due(LinkWatch::kPeriodMs + 59000, true, false));
    TEST_ASSERT_TRUE(w.due(2 * LinkWatch::kPeriodMs, true, false));
}

void test_bad_then_ok_again_with_its_length(void) {
    LinkWatch w;
    uint32_t t = LinkWatch::kPeriodMs;
    TEST_ASSERT_EQUAL_UINT8(LW_BAD, w.result(check(0, 0), t));        // all lost
    TEST_ASSERT_TRUE(w.due(t + LinkWatch::kBadPeriodMs, true, false));  // sooner while bad
    TEST_ASSERT_EQUAL_UINT8(LW_BAD, w.result(check(3, 2410), t + 10000));   // all back, one very late
    TEST_ASSERT_EQUAL_UINT8(LW_OK_AGAIN, w.result(check(3, 15), t + 20000));
    TEST_ASSERT_EQUAL_UINT32(20000, w.badForMs());
    TEST_ASSERT_FALSE(w.due(t + 30000, true, false));                  // back to once a minute
    TEST_ASSERT_EQUAL_UINT8(LW_NONE, w.result(check(3, 15), t + 80000));
}

void test_late_threshold(void) {
    LinkWatch w;
    TEST_ASSERT_EQUAL_UINT8(LW_NONE, w.result(check(3, LinkWatch::kBadMs), 60000));
    TEST_ASSERT_EQUAL_UINT8(LW_BAD, w.result(check(3, LinkWatch::kBadMs + 1), 120000));
    TEST_ASSERT_EQUAL_UINT8(LW_BAD, w.result(check(2, 10), 130000));      // one lost
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_checks_every_minute_when_connected);
    RUN_TEST(test_bad_then_ok_again_with_its_length);
    RUN_TEST(test_late_threshold);
    return UNITY_END();
}
