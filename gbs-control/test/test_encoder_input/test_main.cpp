#include <unity.h>
#include <stdlib.h>
#include "encoder-input.h"
#include "menu-types.h"

void setUp(void) {}
void tearDown(void) {}

// One detent: 00 -> 01 -> 11 -> 10 -> 00 (clk<<1 | data), edges `gap` ms apart; the encoder rests at 00 here
// (seed(0)); a full quadrature cycle per detent is the default
static int detent(EncoderRotary& r, uint32_t& t, bool cw, uint32_t gap = 2) {
    static const uint8_t fw[4] = {1, 3, 2, 0}, bw[4] = {2, 3, 1, 0};
    int sum = 0;
    for (int i = 0; i < 4; i++) { t += gap; sum += r.edge(cw ? fw[i] : bw[i], t); }
    return sum;
}

static EncoderRotary seeded(uint8_t rest = 0) { EncoderRotary r; r.seed(rest); return r; }

void test_rotation_one_step_per_detent(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    TEST_ASSERT_EQUAL_INT(1, abs(detent(r, t, true)));
    TEST_ASSERT_EQUAL_INT(1, abs(detent(r, t, true)));
}

void test_rotation_directions_are_opposite(void) {
    EncoderRotary a = seeded(), b = seeded(); uint32_t ta = 1000, tb = 1000;
    int fw = detent(a, ta, true), bw = detent(b, tb, false);
    TEST_ASSERT_TRUE(fw != 0 && bw != 0 && fw == -bw);
}

void test_clockwise_is_down(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    TEST_ASSERT_EQUAL_INT(+1, detent(r, t, true));          // the Pro's direction (v2 REVERSE = 1)
}

void test_step_comes_at_the_rest_position(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    TEST_ASSERT_EQUAL_INT(0, r.edge(1, ++t));
    TEST_ASSERT_EQUAL_INT(0, r.edge(3, ++t));
    TEST_ASSERT_EQUAL_INT(0, r.edge(2, ++t));
    TEST_ASSERT_EQUAL_INT(+1, r.edge(0, ++t));
}

void test_bounce_cancels(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    static const uint8_t seq[6] = {1, 0, 1, 3, 2, 0};       // contact bounce on the first quarter
    int sum = 0;
    for (uint8_t ab : seq) sum += r.edge(ab, ++t);
    TEST_ASSERT_EQUAL_INT(+1, sum);
}

void test_reverse_flips(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    r.configure(true, 4);
    TEST_ASSERT_EQUAL_INT(-1, detent(r, t, true));
}

void test_two_and_four_steps_per_detent(void) {
    EncoderRotary a = seeded(), b = seeded(); uint32_t ta = 1000, tb = 1000;
    a.configure(false, 2);
    b.configure(false, 1);
    TEST_ASSERT_EQUAL_INT(2, detent(a, ta, true));
    TEST_ASSERT_EQUAL_INT(4, detent(b, tb, true));
}

void test_fast_turn_keeps_every_step(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    int sum = 0;
    for (int i = 0; i < 10; i++) sum += detent(r, t, true, 1);   // the v2 100 ms filter is gone
    TEST_ASSERT_EQUAL_INT(10, sum);
}

void test_seed_from_pins(void) {
    EncoderRotary r = seeded(3); uint32_t t = 1000;           // an encoder resting at 11
    static const uint8_t cw[4] = {2, 0, 1, 3};
    int sum = 0;
    for (uint8_t ab : cw) sum += r.edge(ab, ++t);
    TEST_ASSERT_EQUAL_INT(+1, sum);
}

void test_reversal_loses_no_detent(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    TEST_ASSERT_EQUAL_INT(+1, detent(r, t, true));
    TEST_ASSERT_EQUAL_INT(-1, detent(r, t, false));
    TEST_ASSERT_EQUAL_INT(+1, detent(r, t, true));
}

void test_missed_edge_realigns(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    static const uint8_t seq[3] = {1, 2, 0};                  // the 11 edge was lost: 01 -> 10 is a jump
    int sum = 0;
    for (uint8_t ab : seq) sum += r.edge(ab, ++t);
    TEST_ASSERT_EQUAL_INT(+1, sum);
    TEST_ASSERT_EQUAL_INT(+1, detent(r, t, true));          // the next detent is whole again
}

void test_configure_keeps_the_count(void) {
    EncoderRotary r = seeded(); uint32_t t = 1000;
    r.edge(1, ++t); r.edge(3, ++t);
    r.configure(false, 4);                                   // the loop re-applies settings every pass
    r.edge(2, ++t);
    TEST_ASSERT_EQUAL_INT(+1, r.edge(0, ++t));
}

void test_short_press_is_click(void) {
    EncoderButton b;
    b.edge(1000, true);
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1100, true));
    b.edge(1200, false);
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1201, false));          // release not settled yet
    TEST_ASSERT_EQUAL_INT(K_CLICK, b.poll(1230, false));
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1300, false));
}

void test_press_bounce_is_ignored(void) {
    EncoderButton b;
    b.edge(1000, true); b.edge(1005, false); b.edge(1010, true);   // contact bounce on press
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1050, true));
    b.edge(1200, false);
    TEST_ASSERT_EQUAL_INT(K_CLICK, b.poll(1240, false));
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1300, false));
}

void test_release_bounce_longer_than_debounce(void) {
    EncoderButton b;
    b.edge(1000, true);
    b.poll(1001, true);
    b.edge(1200, false); b.edge(1231, true); b.edge(1233, false);
    int clicks = 0, longs = 0;
    for (uint32_t t = 1202; t < 2500; t += 3) {
        int k = b.poll(t, t < 1231 || t >= 1233 ? false : true);
        if (k == K_CLICK) clicks++;
        if (k == K_LONG) longs++;
    }
    TEST_ASSERT_EQUAL_INT(1, clicks);
    TEST_ASSERT_EQUAL_INT(0, longs);
}

void test_tiny_tap_gives_nothing(void) {
    EncoderButton b;
    b.edge(1000, true); b.edge(1010, false);
    int any = 0;
    for (uint32_t t = 1011; t < 2500; t += 5) if (b.poll(t, false) >= 0) any++;
    TEST_ASSERT_EQUAL_INT(0, any);
}

void test_long_press_fires_once_while_held(void) {
    EncoderButton b;
    b.edge(1000, true);
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1599, true));
    TEST_ASSERT_EQUAL_INT(K_LONG, b.poll(1600, true));
    TEST_ASSERT_EQUAL_INT(-1, b.poll(2000, true));
    b.edge(2500, false);
    TEST_ASSERT_EQUAL_INT(-1, b.poll(2501, false));
    TEST_ASSERT_EQUAL_INT(-1, b.poll(2600, false));          // release after a long press is swallowed
}

void test_late_loop_still_tells_long_from_click(void) {
    EncoderButton b;
    b.edge(1000, true);
    b.edge(1650, false);                          // released after 650 ms, the loop never polled while held
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1651, false));
    TEST_ASSERT_EQUAL_INT(K_LONG, b.poll(1681, false));
    TEST_ASSERT_EQUAL_INT(-1, b.poll(1700, false));
}

void test_two_presses_between_polls(void) {
    EncoderButton b;
    b.edge(1000, true);
    b.poll(1001, true);
    b.edge(1100, false); b.edge(1200, true);      // the loop missed the first release
    TEST_ASSERT_EQUAL_INT(K_CLICK, b.poll(1210, true));
    b.edge(1300, false);
    TEST_ASSERT_EQUAL_INT(K_CLICK, b.poll(1340, false));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rotation_one_step_per_detent);
    RUN_TEST(test_rotation_directions_are_opposite);
    RUN_TEST(test_clockwise_is_down);
    RUN_TEST(test_step_comes_at_the_rest_position);
    RUN_TEST(test_bounce_cancels);
    RUN_TEST(test_reverse_flips);
    RUN_TEST(test_two_and_four_steps_per_detent);
    RUN_TEST(test_fast_turn_keeps_every_step);
    RUN_TEST(test_seed_from_pins);
    RUN_TEST(test_reversal_loses_no_detent);
    RUN_TEST(test_missed_edge_realigns);
    RUN_TEST(test_configure_keeps_the_count);
    RUN_TEST(test_short_press_is_click);
    RUN_TEST(test_press_bounce_is_ignored);
    RUN_TEST(test_release_bounce_longer_than_debounce);
    RUN_TEST(test_tiny_tap_gives_nothing);
    RUN_TEST(test_long_press_fires_once_while_held);
    RUN_TEST(test_late_loop_still_tells_long_from_click);
    RUN_TEST(test_two_presses_between_polls);
    return UNITY_END();
}
