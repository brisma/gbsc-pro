// 2X (I2P) inhibit: progressive 50 Hz sources the TV5725 cannot lock with I2P on (313-line fields -> 626 lines)
#include <unity.h>
#include "i2p-guard.h"

void setUp(void) {}
void tearDown(void) {}

static I2pGuardIn md() {   // PAL Mega Drive on composite, 2X on, the TV5725 shows nothing
    I2pGuardIn in;
    in.i2p = true; in.input = 6; in.decoder = true; in.locked = true; in.progressive = true; in.hz50 = true;
    in.shown = false;
    return in;
}

void test_inhibits_after_the_wait(void) {
    I2pGuard g;
    I2pGuardIn in = md();
    TEST_ASSERT_FALSE(g.update(in, 1000));
    TEST_ASSERT_FALSE(g.update(in, 1000 + I2pGuard::kEngageMs - 1));
    TEST_ASSERT_TRUE(g.update(in, 1000 + I2pGuard::kEngageMs));
    in.shown = true;                                      // the TV5725 locks once I2P is off
    TEST_ASSERT_TRUE(g.update(in, 20000));
}

void test_a_shown_picture_never_inhibits(void) {
    I2pGuard g;
    I2pGuardIn in = md();
    in.shown = true;                                      // N64 PAL: 624-line frames lock
    TEST_ASSERT_FALSE(g.update(in, 1000));
    TEST_ASSERT_FALSE(g.update(in, 60000));
}

void test_short_loss_restarts_the_wait(void) {
    I2pGuard g;
    I2pGuardIn in = md();
    in.shown = true; g.update(in, 1000);
    in.shown = false; g.update(in, 2000);                 // a mode change: the TV5725 re-detects
    in.shown = true; g.update(in, 4000);
    in.shown = false;
    TEST_ASSERT_FALSE(g.update(in, 4000 + I2pGuard::kEngageMs - 1));
}

void test_only_progressive_50hz_decoder_sources(void) {
    I2pGuard g;
    I2pGuardIn in = md(); in.progressive = false;         // 576i: the I2P is made for it
    TEST_ASSERT_FALSE(g.update(in, 1000)); TEST_ASSERT_FALSE(g.update(in, 30000));
    in = md(); in.hz50 = false;                           // 240p: 524-line frames work
    TEST_ASSERT_FALSE(g.update(in, 40000)); TEST_ASSERT_FALSE(g.update(in, 70000));
    in = md(); in.decoder = false;
    TEST_ASSERT_FALSE(g.update(in, 80000)); TEST_ASSERT_FALSE(g.update(in, 110000));
    in = md(); in.i2p = false;                            // 2X off: nothing to inhibit
    TEST_ASSERT_FALSE(g.update(in, 120000)); TEST_ASSERT_FALSE(g.update(in, 150000));
}

void test_released_when_the_source_changes(void) {
    I2pGuard g;
    I2pGuardIn in = md();
    g.update(in, 1000);
    TEST_ASSERT_TRUE(g.update(in, 1000 + I2pGuard::kEngageMs));
    in.locked = false;                                    // console off or reset
    TEST_ASSERT_FALSE(g.update(in, 20000));
    in = md();
    TEST_ASSERT_FALSE(g.update(in, 21000));               // a new source gets 2X first
}

void test_released_on_input_or_setting_change(void) {
    I2pGuard g;
    I2pGuardIn in = md();
    g.update(in, 1000); g.update(in, 1000 + I2pGuard::kEngageMs);
    in.input = 5;                                         // S-Video: another source
    TEST_ASSERT_FALSE(g.update(in, 20000));
    in = md(); g.update(in, 30000); g.update(in, 30000 + I2pGuard::kEngageMs);
    in.i2p = false;                                       // the user turns 2X off and on: try again
    TEST_ASSERT_FALSE(g.update(in, 40000));
    in.i2p = true;
    TEST_ASSERT_FALSE(g.update(in, 40001));
}

// Console log: the wait before 2X goes off is visible
void test_waiting_is_reported(void) {
    I2pGuard g;
    I2pGuardIn in = md();
    TEST_ASSERT_FALSE(g.waiting());
    g.update(in, 1000);
    TEST_ASSERT_TRUE(g.waiting());
    in.shown = true; g.update(in, 2000);                  // the picture came: no wait
    TEST_ASSERT_FALSE(g.waiting());
    in.shown = false; g.update(in, 3000);
    g.update(in, 3000 + I2pGuard::kEngageMs);             // engaged: no longer waiting
    TEST_ASSERT_FALSE(g.waiting());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_inhibits_after_the_wait);
    RUN_TEST(test_a_shown_picture_never_inhibits);
    RUN_TEST(test_short_loss_restarts_the_wait);
    RUN_TEST(test_only_progressive_50hz_decoder_sources);
    RUN_TEST(test_released_when_the_source_changes);
    RUN_TEST(test_released_on_input_or_setting_change);
    RUN_TEST(test_waiting_is_reported);
    return UNITY_END();
}
