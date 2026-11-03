// WiFi optimization: burst statistics and the per-session memory clock choice
#include <unity.h>
#include "wifi-tune.h"

void setUp(void) {}
void tearDown(void) {}

static WifiBurst burst(uint16_t p90ms, uint8_t got = kWifiBurstCount) {
    WifiBurst b = {};
    b.sent = kWifiBurstCount; b.got = got;
    b.p50x10 = b.p90x10 = b.maxx10 = (uint16_t)(p90ms * 10);
    return b;
}

static void expect(WifiTune& t, uint8_t kind, uint8_t clock) {
    WifiTuneAction a = t.next();
    TEST_ASSERT_EQUAL_UINT8(kind, a.kind);
    if (kind != WT_NONE) TEST_ASSERT_EQUAL_UINT8(clock, a.clock);
}

static void ready(WifiTune& t, uint8_t presetClock = 2, uint8_t channel = 8) {
    t.setEnabled(true);
    t.setLink(true, channel);
    t.setPicture(true, presetClock);
}

// preset 2 slow (140), 144 good (9): W = 144 at once, 162 not tried
static void clearWinner(WifiTune& t) {
    ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(9));
    expect(t, WT_NONE, 0);                                        // the burst left the hardware on 144
}

void test_burst_stats_counts_lost_as_250ms(void) {
    uint32_t rtt[20];
    for (int i = 0; i < 20; i++) rtt[i] = 5000;
    rtt[3] = rtt[9] = rtt[15] = 0;
    WifiBurst b = wifiBurstStats(rtt, 20);
    TEST_ASSERT_EQUAL_UINT8(20, b.sent);
    TEST_ASSERT_EQUAL_UINT8(17, b.got);
    TEST_ASSERT_EQUAL_UINT16(2500, b.p90x10);
    TEST_ASSERT_EQUAL_UINT16(50, b.p50x10);
}

void test_measures_nothing_until_ready(void) {
    WifiTune t;
    t.setLink(true, 8); t.setPicture(true, 2);
    expect(t, WT_NONE, 0);                                        // disabled
    t.setEnabled(true); t.setLink(false, 0);
    expect(t, WT_NONE, 0);                                        // AP mode / not connected
    t.setLink(true, 8); t.setPicture(false, 2);
    expect(t, WT_NONE, 0);                                        // picture not stable
    t.setPicture(true, 2); t.setBusy(true);
    expect(t, WT_NONE, 0);                                        // an update runs
    t.setBusy(false);
    expect(t, WT_MEASURE, 2);
}

void test_boot_good_preset_clock_stops(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);
    t.burstDone(burst(12));
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT8(2, t.clockFor(2));
    TEST_ASSERT_FALSE(t.slow());
}

void test_good_144_skips_the_162_burst(void) {   // 162 disturbs most radios: no burst there when 144 is good
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(250));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(9));
    expect(t, WT_NONE, 0);                                        // already on 144
    TEST_ASSERT_EQUAL_UINT8(4, t.clockFor(2));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, t.p90x10(3));
    TEST_ASSERT_TRUE(t.roundComplete());
}

void test_better_but_not_good_144_still_tries_162(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(30));      // half of 140 but over 25 ms
    expect(t, WT_SET_MEASURE, 3);   t.burstDone(burst(12));
    expect(t, WT_NONE, 0);                                        // the burst left the hardware on 162
    TEST_ASSERT_EQUAL_UINT8(3, t.clockFor(2));
}

void test_boot_slow_preset_clock_clear_winner(void) {
    WifiTune t; clearWinner(t);
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT8(4, t.clockFor(2));
    TEST_ASSERT_EQUAL_UINT8(4, t.clockFor(7));
    TEST_ASSERT_FALSE(t.slow());
}

void test_no_clear_winner_back_to_preset_clock(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(76));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(62));
    expect(t, WT_SET_MEASURE, 3);   t.burstDone(burst(71));
    expect(t, WT_APPLY, 2);
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT8(2, t.clockFor(2));
    TEST_ASSERT_TRUE(t.slow());                                   // 76 ms > 40
}

void test_router_ignores_ping_gives_up(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(250, 0));
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(250, 0));   // the one retry, same clock
    expect(t, WT_NONE, 0);
    TEST_ASSERT_TRUE(t.gaveUp());
    TEST_ASSERT_EQUAL_UINT8(2, t.clockFor(2));
}

void test_lossy_burst_is_not_a_silent_router(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(250, 5));
    expect(t, WT_SET_MEASURE, 4);
    TEST_ASSERT_FALSE(t.gaveUp());
}

void test_new_preset_clock_measured_silently(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(12));
    t.setPicture(true, 7);                                        // a load brings 129.6 MHz, never measured
    expect(t, WT_MEASURE, 7);       t.burstDone(burst(120));      // as it is: no switch
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(9));        // good: 162 not tried
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT8(4, t.clockFor(7));
    TEST_ASSERT_EQUAL_UINT8(4, t.clockFor(2));                    // the winner holds for every preset
}

void test_candidate_equal_to_preset_clock_skipped(void) {
    WifiTune t; ready(t, 4);                                      // a profile captured at 144 MHz
    expect(t, WT_MEASURE, 4);       t.burstDone(burst(80));
    expect(t, WT_SET_MEASURE, 3);   t.burstDone(burst(30));
    expect(t, WT_NONE, 0);                                        // 162 wins and the hardware is already there
    TEST_ASSERT_EQUAL_UINT8(3, t.clockFor(4));
    TEST_ASSERT_EQUAL_UINT8(3, t.appliedClock());
}

void test_reconnect_same_channel_keeps_results(void) {
    WifiTune t; clearWinner(t);
    t.setLink(false, 0);
    t.setLink(true, 8);
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT16(90, t.p90x10(4));
}

void test_silent_candidate_counts_as_slow(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(250, 0));   // no retry off the first burst
    expect(t, WT_SET_MEASURE, 3);
    TEST_ASSERT_FALSE(t.gaveUp());
}

void test_capture_keeps_manual_clock(void) {
    TEST_ASSERT_EQUAL_HEX8(0x7C, wifiTuneCaptureS040(0x7C, 2, 2));   // 129.6 set by hand, the tuner wrote nothing
}

void test_fixed_winner_skips_new_preset_clock(void) {
    WifiTune t; clearWinner(t);
    t.setPicture(true, 7);
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT8(4, t.clockFor(7));
}

void test_channel_change_remeasures(void) {
    WifiTune t; clearWinner(t);
    t.setLink(true, 1);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, t.p90x10(4));
    TEST_ASSERT_EQUAL_UINT8(1, t.channel());
    expect(t, WT_SET_MEASURE, 2);                                 // back to the preset clock first
}

void test_disable_restores_preset_clock(void) {
    WifiTune t; clearWinner(t);
    t.setEnabled(false);
    t.setPicture(false, 2);
    expect(t, WT_APPLY, 2);                                       // not gated by the picture
    expect(t, WT_NONE, 0);
    TEST_ASSERT_EQUAL_UINT8(2, t.clockFor(2));
    t.setPicture(true, 2);
    t.setEnabled(true);
    expect(t, WT_MEASURE, 2);                                     // a new round
}

void test_burst_aborted_resumes(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(30));       // better, not good: 162 is next
    expect(t, WT_SET_MEASURE, 3);
    t.burstAborted();
    t.setPicture(false, 2);
    expect(t, WT_APPLY, 2);                                       // no winner yet: back to the preset clock
    expect(t, WT_NONE, 0);
    t.setPicture(true, 2);
    expect(t, WT_SET_MEASURE, 3);
}

void test_capture_stores_preset_clock(void) {
    TEST_ASSERT_EQUAL_HEX8(0x2C, wifiTuneCaptureS040(0x4C, 2, 4));
    TEST_ASSERT_EQUAL_HEX8(0x2C, wifiTuneCaptureS040(0x2C, 2, 2));
}

// a burst dropped again and again (a busy loop every time) must not switch the clock forever: after 3 drops in a row
// the round ends on the preset clock
void test_repeated_aborts_end_the_round(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    for (int i = 0; i < 3; i++) {
        expect(t, WT_SET_MEASURE, 4);
        t.burstAborted();
        expect(t, WT_APPLY, 2);                                   // back to the preset clock each time
    }
    expect(t, WT_NONE, 0);
    TEST_ASSERT_TRUE(t.stalled());
    TEST_ASSERT_EQUAL_UINT8(2, t.clockFor(2));
    t.setLink(true, 1);                                           // a new round may try again
    TEST_ASSERT_FALSE(t.stalled());
}

// The input search can drop the first bursts (its sampling stalls the loop before any preset): the preset that
// loads next is a new chance, or the whole session would stay on the preset clock
void test_a_preset_load_lets_a_stalled_round_try_again(void) {
    WifiTune t; ready(t);
    for (int i = 0; i < 3; i++) {
        expect(t, WT_MEASURE, 2);
        t.burstAborted();
    }
    expect(t, WT_NONE, 0);
    TEST_ASSERT_TRUE(t.stalled());
    t.presetLoaded(2);
    TEST_ASSERT_FALSE(t.stalled());
    expect(t, WT_NONE, 0);                                        // not stable yet
    t.setPicture(true, 2);
    expect(t, WT_MEASURE, 2);
}

// Every clock slow on this channel: the user is told to try another router channel, for the picture that was
// measured only (a new preset is judged again)
void test_slow_channel_advice_follows_the_picture(void) {
    WifiTune t; ready(t);
    TEST_ASSERT_FALSE(t.slowChannel());
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(130));
    TEST_ASSERT_FALSE(t.slowChannel());                           // still measuring
    expect(t, WT_SET_MEASURE, 3);   t.burstDone(burst(150));
    expect(t, WT_APPLY, 2);                                       // nothing better: back on the preset clock
    TEST_ASSERT_TRUE(t.slowChannel());
    t.presetLoaded(7);                                            // another picture, its clock not measured yet
    TEST_ASSERT_FALSE(t.slowChannel());
    t.setPicture(true, 7);
    expect(t, WT_MEASURE, 7);       t.burstDone(burst(9));
    expect(t, WT_NONE, 0);
    TEST_ASSERT_FALSE(t.slowChannel());                           // this picture leaves the channel alone
    t.setEnabled(false);
    TEST_ASSERT_FALSE(t.slowChannel());
}

// HD bypass and RGBHV bypass set PLL_MS themselves: no measuring and no APPLY at all, even turning the option off
void test_bypass_freezes_everything(void) {
    WifiTune t; ready(t); t.setBypass(true);
    expect(t, WT_NONE, 0);
    WifiTune w; clearWinner(w);
    w.setBypass(true);
    w.setEnabled(false);
    expect(w, WT_NONE, 0);
}

// No signal (the scan's resets): no picture to protect, so the quiet clock instead of upstream's FB — the session's
// winner, else 144 MHz; FB with the option off (measured: p90 3.1 s at FB, 0.4 s at 144 without a signal)
void test_idle_clock_without_a_signal(void) {
    WifiTune t;
    TEST_ASSERT_EQUAL_UINT8(2, t.idleClock(false));
    TEST_ASSERT_EQUAL_UINT8(kWifiClock144, t.idleClock(true));
    WifiTune w; ready(w);
    expect(w, WT_MEASURE, 2);       w.burstDone(burst(140));
    expect(w, WT_SET_MEASURE, 4);   w.burstDone(burst(90));
    expect(w, WT_SET_MEASURE, 3);   w.burstDone(burst(20));
    expect(w, WT_NONE, 0);                                        // 162 wins, the hardware is already there
    TEST_ASSERT_EQUAL_UINT8(3, w.idleClock(true));
    TEST_ASSERT_EQUAL_UINT8(2, w.idleClock(false));
}

// The decision line is written once the round is over, not when it only waits (busy, WiFi lost) halfway
void test_round_complete_only_at_the_end(void) {
    WifiTune t; ready(t);
    TEST_ASSERT_FALSE(t.roundComplete());
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    t.setBusy(true);
    expect(t, WT_NONE, 0);
    TEST_ASSERT_FALSE(t.roundComplete());                         // waiting, 144 and 162 still to measure
    t.setBusy(false);
    expect(t, WT_SET_MEASURE, 4);   t.burstDone(burst(9));
    expect(t, WT_NONE, 0);                                        // 144 good: the round ends there
    TEST_ASSERT_TRUE(t.roundComplete());
    WifiTune g; ready(g);
    expect(g, WT_MEASURE, 2);       g.burstDone(burst(12));
    TEST_ASSERT_TRUE(g.roundComplete());                          // the preset clock was fine
}

// A preset load during a burst on 144/162 that brings the same preset clock: the load already wrote clockFor(),
// so no extra APPLY (and no extra SDRAM reset) follows
void test_preset_load_mid_burst_no_extra_apply(void) {
    WifiTune t; ready(t);
    expect(t, WT_MEASURE, 2);       t.burstDone(burst(140));
    expect(t, WT_SET_MEASURE, 4);
    t.burstAborted();
    t.presetLoaded(2);
    TEST_ASSERT_EQUAL_UINT8(2, t.appliedClock());
    t.setPicture(false, 2);
    expect(t, WT_NONE, 0);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_burst_stats_counts_lost_as_250ms);
    RUN_TEST(test_measures_nothing_until_ready);
    RUN_TEST(test_boot_good_preset_clock_stops);
    RUN_TEST(test_boot_slow_preset_clock_clear_winner);
    RUN_TEST(test_good_144_skips_the_162_burst);
    RUN_TEST(test_better_but_not_good_144_still_tries_162);
    RUN_TEST(test_no_clear_winner_back_to_preset_clock);
    RUN_TEST(test_router_ignores_ping_gives_up);
    RUN_TEST(test_lossy_burst_is_not_a_silent_router);
    RUN_TEST(test_new_preset_clock_measured_silently);
    RUN_TEST(test_candidate_equal_to_preset_clock_skipped);
    RUN_TEST(test_reconnect_same_channel_keeps_results);
    RUN_TEST(test_silent_candidate_counts_as_slow);
    RUN_TEST(test_capture_keeps_manual_clock);
    RUN_TEST(test_fixed_winner_skips_new_preset_clock);
    RUN_TEST(test_channel_change_remeasures);
    RUN_TEST(test_disable_restores_preset_clock);
    RUN_TEST(test_burst_aborted_resumes);
    RUN_TEST(test_capture_stores_preset_clock);
    RUN_TEST(test_repeated_aborts_end_the_round);
    RUN_TEST(test_a_preset_load_lets_a_stalled_round_try_again);
    RUN_TEST(test_slow_channel_advice_follows_the_picture);
    RUN_TEST(test_bypass_freezes_everything);
    RUN_TEST(test_idle_clock_without_a_signal);
    RUN_TEST(test_round_complete_only_at_the_end);
    RUN_TEST(test_preset_load_mid_burst_no_extra_apply);
    return UNITY_END();
}
