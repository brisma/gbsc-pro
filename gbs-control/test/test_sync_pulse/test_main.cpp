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
    return UNITY_END();
}
