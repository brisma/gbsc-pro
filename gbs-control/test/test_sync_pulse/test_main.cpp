// Short-pulse limit of the sync processor: inside the working window in every video mode, on dark and bright
// pictures, and across a mode change
#include <unity.h>
#include "sync-pulse.h"

void setUp(void) {}
void tearDown(void) {}

// Measured on a Mega Drive (RGB SCART): the limit works from the equalizing pulses' width up to 66 % of the dark
// normal pulse; a bright picture widens every pulse by the same amount (229 -> 241, 189 -> 204)
static void inWindow(uint16_t dark, uint16_t now) {
    uint16_t t = syncPulseLimit(dark, now);
    uint16_t equalizing = dark / 2 + (now - dark);
    TEST_ASSERT_TRUE_MESSAGE(t >= equalizing + 3, "too close to the equalizing pulses");
    TEST_ASSERT_TRUE_MESSAGE(t * 100u <= dark * 64u, "too close to the upper edge");
}

void test_limit_sits_inside_the_window_on_dark_and_bright_pictures(void) {
    for (uint16_t dark = 100; dark <= 240; dark++)
        for (uint16_t now = dark; now <= dark + dark * 8 / 100; now++) inWindow(dark, now);
}

void test_measured_values(void) {
    TEST_ASSERT_EQUAL_UINT16(120, syncPulseLimit(229, 229));
    TEST_ASSERT_EQUAL_UINT16(132, syncPulseLimit(229, 241));
    TEST_ASSERT_EQUAL_UINT16(100, syncPulseLimit(189, 189));
    TEST_ASSERT_EQUAL_UINT16(115, syncPulseLimit(189, 204));
    TEST_ASSERT_EQUAL_UINT16(120, syncPulseLimit(229, 220));   // never under the dark value
}

// The limit of the wide mode on a dark picture also works in the narrow mode (measured there: fine up to 124),
// so the first lines after a mode change are counted right
void test_dark_limit_of_one_mode_works_in_the_other(void) {
    TEST_ASSERT_TRUE(syncPulseLimit(229, 229) <= 124);          // the wide mode's limit in the narrow mode
    TEST_ASSERT_EQUAL_UINT16(120, syncPulseLimit(189, 189, 120));   // kept there
    TEST_ASSERT_EQUAL_UINT16(121, syncPulseLimit(189, 189, 140));   // never over the window
}

void test_cap(void) {
    TEST_ASSERT_EQUAL_UINT16(kSyncIgnoreMax, syncPulseLimit(400, 400));
    TEST_ASSERT_TRUE(kSyncIgnoreMax < 0x90);
}

static uint16_t feed(SyncPulseTrack& t, uint16_t width, int count) {
    uint16_t last = 0;
    for (int i = 0; i < count; i++) { uint16_t r = t.update(width); if (r) last = r; }
    return last;
}

void test_tracker_learns_the_pulse_and_then_keeps_quiet(void) {
    SyncPulseTrack t;
    TEST_ASSERT_EQUAL_UINT16(0, t.limit());
    TEST_ASSERT_EQUAL_UINT16(0, feed(t, 229, kSyncTrackRun - 1));
    TEST_ASSERT_EQUAL_UINT16(120, t.update(229));
    TEST_ASSERT_TRUE(t.takeModeChange());
    TEST_ASSERT_FALSE(t.takeModeChange());
    for (int i = 0; i < 600; i++) {
        uint16_t s = i % 15 == 3 ? 112 : i % 60 == 7 ? 1105 : i % 97 == 5 ? 81 : 229 + i % 2;   // half, broad, stray
        TEST_ASSERT_EQUAL_UINT16(0, t.update(s));
    }
    TEST_ASSERT_UINT16_WITHIN(1, 120, t.limit());   // one count of jitter moves it, without a write
}

void test_tracker_follows_a_bright_picture_up_and_down(void) {
    SyncPulseTrack t;
    feed(t, 229, 10);
    uint16_t top = 0;
    for (uint16_t w = 229; w <= 241; w++) {
        feed(t, w, 6);
        TEST_ASSERT_TRUE(t.limit() >= 229 / 2 + (w - 229) + 3);     // always above the equalizing pulses of the moment
        top = t.limit();
    }
    TEST_ASSERT_EQUAL_UINT16(132, top);
    for (uint16_t w = 241; w >= 229; w--) feed(t, w, 6);
    TEST_ASSERT_EQUAL_UINT16(120, t.limit());
    TEST_ASSERT_EQUAL_UINT16(229, t.dark());
}

void test_tracker_takes_a_sudden_white_screen_in_two_samples(void) {
    SyncPulseTrack t;
    feed(t, 229, 10);
    TEST_ASSERT_EQUAL_UINT16(0, t.update(241));
    TEST_ASSERT_EQUAL_UINT16(132, t.update(241));
}

// Entering a narrower mode on a white flash, as recorded: 229, then 193 rising to 204, then 189. The limit of the
// wide mode (120) is inside the narrow mode's window (up to 124 measured): it stays, nothing to correct
void test_limit_stays_across_a_mode_change_both_ways(void) {
    SyncPulseTrack t;
    feed(t, 229, 10);
    t.takeModeChange();
    feed(t, 193, kSyncTrackRun - 1);
    TEST_ASSERT_EQUAL_UINT16(120, t.limit());
    TEST_ASSERT_EQUAL_UINT16(120, t.update(194));
    TEST_ASSERT_TRUE(t.takeModeChange());
    for (uint16_t w = 194; w <= 204; w++) {
        feed(t, w, 5);
        TEST_ASSERT_TRUE(t.limit() >= 189 / 2 + (w - 189) + 3);      // the real equalizing pulses (dark pulse 189)
        TEST_ASSERT_TRUE(t.limit() <= 124);
    }
    for (uint16_t w = 204; w >= 189; w--) feed(t, w, 5);
    TEST_ASSERT_EQUAL_UINT16(189, t.dark());
    TEST_ASSERT_EQUAL_UINT16(120, t.limit());                        // also right for the wide mode it returns to
    TEST_ASSERT_EQUAL_UINT16(120, feed(t, 229, kSyncTrackRun));
    TEST_ASSERT_TRUE(t.takeModeChange());
    feed(t, 241, 6);
    TEST_ASSERT_EQUAL_UINT16(132, t.limit());                        // the bright picture still raises it
    feed(t, 229, 6);
    TEST_ASSERT_EQUAL_UINT16(120, t.limit());
}

// A console that starts in its narrow mode: the first change to the wide one sets the shared limit
void test_limit_from_the_narrow_mode_is_raised_once(void) {
    SyncPulseTrack t;
    TEST_ASSERT_EQUAL_UINT16(100, feed(t, 189, 10));
    TEST_ASSERT_EQUAL_UINT16(120, feed(t, 229, kSyncTrackRun));
    TEST_ASSERT_EQUAL_UINT16(120, feed(t, 189, kSyncTrackRun));
    TEST_ASSERT_EQUAL_UINT16(120, t.limit());
}

void test_a_limit_outside_the_new_window_is_dropped(void) {
    SyncPulseTrack t;
    feed(t, 280, 10);                                                // limit 146 -> capped at 143
    TEST_ASSERT_EQUAL_UINT16(143, t.limit());
    TEST_ASSERT_EQUAL_UINT16(100, feed(t, 189, kSyncTrackRun));      // 143 is over 64 % of 189
}

// A sync loss forgets the pulse, not the limit both modes share
void test_limit_survives_a_sync_loss(void) {
    SyncPulseTrack t;
    feed(t, 229, 10);
    t.lost();
    TEST_ASSERT_EQUAL_UINT16(0, t.limit());
    TEST_ASSERT_EQUAL_UINT16(120, feed(t, 189, kSyncTrackRun));
}

void test_tracker_is_not_fooled_by_the_mess_of_a_switch(void) {
    SyncPulseTrack t;
    feed(t, 229, 10);
    static const uint16_t mess[] = {96, 382, 229, 77, 169, 8, 1, 229, 1442, 54, 229, 230, 112, 229, 112, 112, 112, 112, 112, 229};
    for (uint16_t s : mess) TEST_ASSERT_EQUAL_UINT16(0, t.update(s));
    TEST_ASSERT_UINT16_WITHIN(1, 120, t.limit());
}

void test_tracker_forgets_when_reset(void) {
    SyncPulseTrack t;
    feed(t, 229, 10);
    t.reset();
    TEST_ASSERT_EQUAL_UINT16(0, t.limit());
    TEST_ASSERT_EQUAL_UINT16(100, feed(t, 189, 10));
}

// The first pulse of the other mode is told from everything a steady source shows (values as recorded)
void test_other_mode_is_seen_on_the_first_sample(void) {
    SyncPulseTrack t;
    TEST_ASSERT_FALSE(t.otherMode(193));                 // nothing known yet
    feed(t, 229, 10);
    static const uint16_t same[] = {229, 228, 231, 241, 242, 112, 113, 125, 81, 82, 1105, 1136, 1148, 54, 8, 1, 0, 382};
    for (uint16_t s : same) TEST_ASSERT_FALSE(t.otherMode(s));
    TEST_ASSERT_TRUE(t.otherMode(193));
    TEST_ASSERT_TRUE(t.otherMode(204));
    SyncPulseTrack n;
    feed(n, 189, 10);
    static const uint16_t sameN[] = {189, 190, 204, 92, 96, 108, 54, 52, 88, 1078, 1116};
    for (uint16_t s : sameN) TEST_ASSERT_FALSE(n.otherMode(s));
    TEST_ASSERT_TRUE(n.otherMode(229));
    TEST_ASSERT_TRUE(n.otherMode(241));
}

// The width is two register bytes read one after the other: a read across the vertical interval mixes a broad
// pulse with a normal one (SNES, recorded: 173 normal, 2164 broad, 116 and 2222 mixed). Only a second read that
// agrees makes a mode change.
void test_a_mixed_read_is_not_confirmed_by_the_next_one(void) {
    TEST_ASSERT_FALSE(syncPulseReadsAgree(116, 173));
    TEST_ASSERT_FALSE(syncPulseReadsAgree(116, 2164));
    TEST_ASSERT_FALSE(syncPulseReadsAgree(116, 0));
    TEST_ASSERT_TRUE(syncPulseReadsAgree(193, 193));     // the other mode's pulse, twice
    TEST_ASSERT_TRUE(syncPulseReadsAgree(193, 195));
    TEST_ASSERT_TRUE(syncPulseReadsAgree(229, 226));
}

// input.sync_limit: a limit set by hand, for a source the automatic one misjudges (measured: a SNES 1CHIP with a
// SuperFX game counts 311 lines at 90, loses lines at the automatic 100 and counts extra ones under 88)
void test_manual_limit_is_the_value_plus_the_widening(void) {
    TEST_ASSERT_EQUAL_UINT16(90, syncPulseManualLimit(90, 189, 189));
    TEST_ASSERT_EQUAL_UINT16(105, syncPulseManualLimit(90, 189, 204));   // a bright picture widens every pulse
    TEST_ASSERT_EQUAL_UINT16(90, syncPulseManualLimit(90, 189, 180));    // never under the value
    TEST_ASSERT_EQUAL_UINT16(kSyncIgnoreMax, syncPulseManualLimit(143, 229, 241));
    TEST_ASSERT_EQUAL_UINT16(2, syncPulseManualLimit(1, 189, 189));
}

void test_tracker_uses_the_manual_limit_and_goes_back_to_automatic(void) {
    SyncPulseTrack t;
    for (int i = 0; i < 4; i++) t.update(189);
    TEST_ASSERT_EQUAL_UINT16(100, t.limit());
    t.setManual(90);
    TEST_ASSERT_EQUAL_UINT16(90, t.limit());          // at once, no new sample needed
    t.update(204); 
    TEST_ASSERT_EQUAL_UINT16(105, t.update(204));     // the brightness is still followed
    t.setManual(0);
    TEST_ASSERT_EQUAL_UINT16(115, t.limit());
}

void test_manual_limit_survives_a_reset_and_waits_for_the_pulse(void) {
    SyncPulseTrack t;
    t.setManual(90);
    TEST_ASSERT_EQUAL_UINT16(0, t.limit());           // no pulse known: upstream's formula for now
    t.reset();
    TEST_ASSERT_EQUAL_UINT16(90, t.manual());
    uint16_t l = 0;
    for (int i = 0; i < 4; i++) l = t.update(229);
    TEST_ASSERT_EQUAL_UINT16(90, l);                  // written with the new mode
    t.lost();
    TEST_ASSERT_EQUAL_UINT16(90, t.manual());
}

// ---- the kind of source and the limit it gets ----

static uint32_t gRnd = 12345;
static uint32_t rnd(uint32_t n) { gRnd = gRnd * 1103515245u + 12345u; return (gRnd >> 16) % n; }

// Width reads as measured on a Mega Drive: 2.4..3 % are equalizing pulses (half, within a count), about 1 % broad pulses
static uint16_t megaDriveRead(uint16_t pulse) { uint32_t r = rnd(1000); return r < 27 ? pulse / 2 - 2 + rnd(2) : r < 37 ? pulse * 5 : pulse + rnd(2); }
// A SNES with a SuperFX game: a stray read now and then, none repeating at half
static uint16_t snesRead(uint16_t pulse) { uint32_t r = rnd(2000); return r < 2 ? pulse * 43 / 100 + rnd(20) : r < 12 ? pulse * 12 : pulse + rnd(2); }

void test_equalizing_pulses_keep_the_limit_every_source_takes(void) {
    SyncPulseTrack t;
    for (int i = 0; i < 4; i++) t.update(229);
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_UNKNOWN, t.equalizing());
    TEST_ASSERT_EQUAL_UINT16(120, t.limit());
    for (int i = 0; i < 6000; i++) { t.update(megaDriveRead(229)); TEST_ASSERT_NOT_EQUAL(SyncPulseTrack::EQ_NONE, t.equalizing()); }
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_SEEN, t.equalizing());
    TEST_ASSERT_UINT16_WITHIN(1, 120, t.limit());
    for (int i = 0; i < 3000; i++) TEST_ASSERT_EQUAL_UINT16(0, t.lineCount(i % 3 ? 259 : 258));   // the line count is not used
    TEST_ASSERT_UINT16_WITHIN(1, 120, t.limit());
}

void test_no_equalizing_pulses_start_a_little_under_half(void) {
    SyncPulseTrack t;
    for (int i = 0; i < 4; i++) t.update(189);
    TEST_ASSERT_EQUAL_UINT16(100, t.limit());                       // not decided yet: as before
    int n = 0;
    while (t.equalizing() == SyncPulseTrack::EQ_UNKNOWN && n < 2000) { t.update(snesRead(189)); n++; }
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_NONE, t.equalizing());
    TEST_ASSERT_TRUE(n >= kSyncEqWindow);                           // only after a whole window without them
    TEST_ASSERT_TRUE(t.takeEqChange());
    TEST_ASSERT_FALSE(t.takeEqChange());
    TEST_ASSERT_UINT16_WITHIN(1, 189 / 2 + kSyncNoEqStart, t.limit());  // 91, or 92 while the pulse reads a count wider
    TEST_ASSERT_EQUAL_UINT16(189 / 2 + kSyncNoEqStart + 15, syncPulseNoEqLimit(189, 204, kSyncNoEqStart));   // the widening
    TEST_ASSERT_EQUAL_UINT16(189 / 4, syncPulseNoEqLimit(189, 189, -100));     // a floor
    TEST_ASSERT_EQUAL_UINT16(100, syncPulseNoEqLimit(189, 189, 100));          // never over the usual limit
    for (int i = 0; i < 40; i++) t.update(189 / 2);                 // equalizing pulses after all: back at once
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_SEEN, t.equalizing());
    TEST_ASSERT_UINT16_WITHIN(1, 100, t.limit());
}

// The line count of a field at a given limit when a disturbance splits pulses around `balance`: more lines under it,
// fewer over it, more often with the distance (the shape measured on a SNES 1CHIP in Yoshi's Island, balance 92)
static uint16_t splitLines(uint16_t usual, int limit, int balance) {
    int d = limit - balance;
    uint32_t pm = d >= -1 ? 0 : d >= -6 ? 80 : d >= -12 ? 150 : 600;      // per mille
    uint32_t pf = d <= 1 ? 0 : d <= 12 ? 250 : 650;
    uint32_t r = rnd(1000);
    return r < pm ? usual + 1 : r < pm + pf ? usual - 1 : usual;
}

static SyncPulseTrack noEqTracker(uint16_t pulse) {
    SyncPulseTrack t;
    for (int i = 0; i < 4; i++) t.update(pulse);
    for (int i = 0; i < kSyncEqWindow + 10; i++) t.update(pulse);
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_NONE, t.equalizing());
    return t;
}

void test_line_count_brings_the_limit_to_the_balance_point_and_holds_it(void) {
    const int balances[] = {92, 84, 97};                             // this console; another one lower, another higher
    for (int balance : balances) {
        SyncPulseTrack t = noEqTracker(189);
        int limit = t.limit();
        for (int i = 0; i < 20000; i++) { uint16_t l = t.lineCount(splitLines(311, limit, balance)); if (l) limit = l; }
        TEST_ASSERT_INT_WITHIN_MESSAGE(1, balance, limit, "settled where the fields count right");
        for (int i = 0; i < 20000; i++) {                            // and stays there
            uint16_t l = t.lineCount(splitLines(311, limit, balance)); if (l) limit = l;
            TEST_ASSERT_INT_WITHIN(2, balance, limit);
        }
    }
}

void test_a_clean_source_keeps_the_start_value(void) {
    SyncPulseTrack t = noEqTracker(190);
    for (int i = 0; i < 20000; i++) TEST_ASSERT_EQUAL_UINT16(0, t.lineCount(i % 997 ? 261 : 231));   // a torn read now and then
    TEST_ASSERT_EQUAL_UINT16(190 / 2 + kSyncNoEqStart, t.limit());
}

void test_alternating_fields_are_not_an_error(void) {
    SyncPulseTrack t = noEqTracker(189);                             // an interlaced source: 312, 313, 312, ...
    for (int i = 0; i < 20000; i++) TEST_ASSERT_EQUAL_UINT16(0, t.lineCount(i % 2 ? 313 : 312));
    TEST_ASSERT_EQUAL_INT8(kSyncNoEqStart, t.trim());
}

void test_the_correction_stays_inside_its_bounds(void) {
    SyncPulseTrack t = noEqTracker(189);
    int limit = t.limit();
    for (int i = 0; i < 40000; i++) { uint16_t l = t.lineCount(rnd(5) ? 311 : 312); if (l) limit = l; }   // always "more lines"
    TEST_ASSERT_EQUAL_INT(100, limit);                               // up to the limit of a source with equalizing pulses
    for (int i = 0; i < 200000; i++) { uint16_t l = t.lineCount(rnd(5) ? 311 : 310); if (l) limit = l; } // always "fewer"
    TEST_ASSERT_EQUAL_INT(189 / 4, limit);                           // down to a quarter of the pulse
}

void test_a_sync_loss_makes_the_kind_unknown_again(void) {
    SyncPulseTrack t = noEqTracker(189);                             // a SNES, then a Mega Drive on the same cable
    t.takeEqChange();
    t.lost();
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_UNKNOWN, t.equalizing());
    for (int i = 0; i < 4; i++) t.update(189);
    TEST_ASSERT_UINT16_WITHIN(1, 100, t.limit());                    // the limit every source takes, until the kind shows
}

void test_stray_half_reads_are_not_equalizing_pulses(void) {
    SyncPulseTrack t = noEqTracker(189);
    for (int i = 0; i < 50000; i++) {                                // strays anywhere in the half band, twice the rate measured
        t.update(rnd(500) ? 189 : (uint16_t)(84 + rnd(44)));
        TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_NONE, t.equalizing());
    }
}

void test_equalizing_pulses_are_seen_on_a_bright_picture(void) {
    SyncPulseTrack t;
    for (int i = 0; i < 4; i++) t.update(229);
    for (int i = 0; i < 6000; i++) {                                 // every pulse 26 counts wider: halves at 61 % of the dark pulse
        t.update(rnd(1000) < 27 ? 114 + 26 : 229 + 26);
        TEST_ASSERT_NOT_EQUAL(SyncPulseTrack::EQ_NONE, t.equalizing());
    }
    TEST_ASSERT_EQUAL_UINT8(SyncPulseTrack::EQ_SEEN, t.equalizing());
}

void test_manual_limit_wins_over_the_source_kind(void) {
    SyncPulseTrack t = noEqTracker(189);
    t.setManual(110);
    TEST_ASSERT_EQUAL_UINT16(110, t.limit());
    for (int i = 0; i < 3000; i++) TEST_ASSERT_EQUAL_UINT16(0, t.lineCount(rnd(3) ? 311 : 310));
    TEST_ASSERT_EQUAL_UINT16(110, t.limit());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_limit_sits_inside_the_window_on_dark_and_bright_pictures);
    RUN_TEST(test_measured_values);
    RUN_TEST(test_dark_limit_of_one_mode_works_in_the_other);
    RUN_TEST(test_cap);
    RUN_TEST(test_tracker_learns_the_pulse_and_then_keeps_quiet);
    RUN_TEST(test_tracker_follows_a_bright_picture_up_and_down);
    RUN_TEST(test_tracker_takes_a_sudden_white_screen_in_two_samples);
    RUN_TEST(test_limit_stays_across_a_mode_change_both_ways);
    RUN_TEST(test_limit_from_the_narrow_mode_is_raised_once);
    RUN_TEST(test_a_limit_outside_the_new_window_is_dropped);
    RUN_TEST(test_limit_survives_a_sync_loss);
    RUN_TEST(test_tracker_is_not_fooled_by_the_mess_of_a_switch);
    RUN_TEST(test_tracker_forgets_when_reset);
    RUN_TEST(test_other_mode_is_seen_on_the_first_sample);
    RUN_TEST(test_a_mixed_read_is_not_confirmed_by_the_next_one);
    RUN_TEST(test_manual_limit_is_the_value_plus_the_widening);
    RUN_TEST(test_tracker_uses_the_manual_limit_and_goes_back_to_automatic);
    RUN_TEST(test_manual_limit_survives_a_reset_and_waits_for_the_pulse);
    RUN_TEST(test_equalizing_pulses_keep_the_limit_every_source_takes);
    RUN_TEST(test_no_equalizing_pulses_start_a_little_under_half);
    RUN_TEST(test_line_count_brings_the_limit_to_the_balance_point_and_holds_it);
    RUN_TEST(test_a_clean_source_keeps_the_start_value);
    RUN_TEST(test_alternating_fields_are_not_an_error);
    RUN_TEST(test_the_correction_stays_inside_its_bounds);
    RUN_TEST(test_a_sync_loss_makes_the_kind_unknown_again);
    RUN_TEST(test_stray_half_reads_are_not_equalizing_pulses);
    RUN_TEST(test_equalizing_pulses_are_seen_on_a_bright_picture);
    RUN_TEST(test_manual_limit_wins_over_the_source_kind);
    return UNITY_END();
}
