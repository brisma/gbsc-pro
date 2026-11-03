// Loop watch: a loop() pass slower than kSlowMs is reported with its slowest stretch, at most once every kQuietMs
#include <unity.h>
#include "loop-watch.h"

void setUp(void) {}
void tearDown(void) {}

void test_fast_passes_are_not_reported(void) {
    LoopWatch w;
    LoopWatch::Report r;
    TEST_ASSERT_FALSE(w.begin(0, r));                      // no previous pass
    w.mark(1, 10);
    w.mark(2, 20);
    TEST_ASSERT_FALSE(w.begin(LoopWatch::kSlowMs - 1, r));
}

void test_slow_pass_names_its_slowest_stretch(void) {
    LoopWatch w;
    LoopWatch::Report r;
    w.begin(1000, r);
    w.mark(1, 1010);                                        // 10 ms
    w.mark(2, 1410);                                        // 400 ms: the stall
    w.mark(3, 1450);                                        // 40 ms
    TEST_ASSERT_TRUE(w.begin(1460, r));                     // 10 ms after the last mark
    TEST_ASSERT_EQUAL_UINT32(460, r.passMs);
    TEST_ASSERT_EQUAL_UINT8(2, r.stretch);
    TEST_ASSERT_EQUAL_UINT32(400, r.stretchMs);
}

void test_time_after_the_last_mark_is_the_rest(void) {   // early returns and the code after the last mark
    LoopWatch w;
    LoopWatch::Report r;
    w.begin(0, r);
    w.mark(1, 5);
    TEST_ASSERT_TRUE(w.begin(2005, r));
    TEST_ASSERT_EQUAL_UINT8(LoopWatch::kRest, r.stretch);
    TEST_ASSERT_EQUAL_UINT32(2000, r.stretchMs);
}

void test_reports_are_spaced(void) {
    LoopWatch w;
    LoopWatch::Report r;
    w.begin(0, r);
    TEST_ASSERT_TRUE(w.begin(500, r));                      // slow pass 0..500
    TEST_ASSERT_FALSE(w.begin(1000, r));                    // slow again, too soon after the report
    TEST_ASSERT_FALSE(w.begin(500 + LoopWatch::kQuietMs - 1, r));
    TEST_ASSERT_TRUE(w.begin(500 + LoopWatch::kQuietMs + 400, r));
}

void test_counts_slow_passes_since_the_last_report(void) {   // how often it stalls, not just once
    LoopWatch w;
    LoopWatch::Report r;
    w.begin(0, r);
    TEST_ASSERT_TRUE(w.begin(400, r));
    TEST_ASSERT_EQUAL_UINT16(1, r.slowPasses);
    uint32_t t = 400;
    for (int i = 0; i < 5; i++) { t += 2000; w.begin(t, r); }   // 5 more slow passes, the last one reported
    TEST_ASSERT_EQUAL_UINT16(3, r.slowPasses);              // 2400, 4400 quiet; 6400 reported (3); 8400, 10400 quiet
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fast_passes_are_not_reported);
    RUN_TEST(test_slow_pass_names_its_slowest_stretch);
    RUN_TEST(test_time_after_the_last_mark_is_the_rest);
    RUN_TEST(test_reports_are_spaced);
    RUN_TEST(test_counts_slow_passes_since_the_last_report);
    return UNITY_END();
}
