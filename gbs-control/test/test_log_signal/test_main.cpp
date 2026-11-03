// Console log: one line for the shown signal when it changes and stays for a second (log-signal.h)
#include <unity.h>
#include <string.h>
#include "log-signal.h"

void setUp(void) {}
void tearDown(void) {}

static LogSignalInfo md() {   // PAL Mega Drive over RGB SCART
    LogSignalInfo s = {};
    s.none = false; s.lines = 312; s.srcHz10 = 500; s.outHz10 = 500; s.interlaced = false; s.sync = 1;
    s.locked = true; s.preset = 0x12; s.input = 1;
    return s;
}

void test_text_of_a_shown_signal(void) {
    char t[160];
    logSignalText(md(), t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("signal: 312 lines progressive 50.0 Hz, CSync, frame lock on, preset 1280x1024, out 50.0 Hz "
                             "(RGB SCART)", t);
    LogSignalInfo n = md(); n.none = true;
    logSignalText(n, t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("signal: none on RGB SCART", t);
    LogSignalInfo i = md(); i.interlaced = true; i.lines = 525; i.srcHz10 = 599; i.sync = 2; i.locked = false; i.preset = 0x01;
    i.input = 4;
    logSignalText(i, t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("signal: 525 lines interlaced 59.9 Hz, SOG, frame lock off, preset 1280x960, out 50.0 Hz "
                             "(Component)", t);
}

void test_preset_names(void) {
    TEST_ASSERT_EQUAL_STRING("1920x1080", logPresetName(0x15));
    TEST_ASSERT_EQUAL_STRING("768x576", logPresetName(0x14));
    TEST_ASSERT_EQUAL_STRING("bypass", logPresetName(0x21));
}

void test_watch_reports_once_after_a_stable_second(void) {
    LogSignalWatch w;
    LogSignalInfo s = md();
    TEST_ASSERT_FALSE(w.update(s, 0));
    TEST_ASSERT_FALSE(w.update(s, LogSignalWatch::kStableMs - 1));
    TEST_ASSERT_TRUE(w.update(s, LogSignalWatch::kStableMs));
    TEST_ASSERT_FALSE(w.update(s, 5000));                 // unchanged: once
}

void test_watch_ignores_a_short_flap(void) {
    LogSignalWatch w;
    LogSignalInfo s = md();
    w.update(s, 0); w.update(s, 1000);                    // reported
    LogSignalInfo f = s; f.locked = false;
    TEST_ASSERT_FALSE(w.update(f, 2000));
    TEST_ASSERT_FALSE(w.update(s, 2500));                 // back before a second: nothing new
    TEST_ASSERT_FALSE(w.update(s, 4000));
    TEST_ASSERT_FALSE(w.update(f, 5000));
    TEST_ASSERT_TRUE(w.update(f, 6000));                  // stayed: reported
}

// A scan walks the inputs with nothing on them: "none" once, not once per input
// A console switching off showed "interlaced 21.0 Hz" for ~1 s; a rate no console sends is not a
// signal to report (the line comes when the real one, or none, has stayed a second)
void test_watch_skips_a_rate_no_source_sends(void) {
    LogSignalWatch w;
    LogSignalInfo odd = md(); odd.srcHz10 = 210; odd.interlaced = true;
    TEST_ASSERT_FALSE(w.update(odd, 0));
    TEST_ASSERT_FALSE(w.update(odd, 1500));
    TEST_ASSERT_FALSE(w.update(odd, 3000));
    LogSignalInfo high = md(); high.srcHz10 = 1500;
    TEST_ASSERT_FALSE(w.update(high, 4000));
    TEST_ASSERT_FALSE(w.update(high, 6000));
    TEST_ASSERT_FALSE(w.update(md(), 7000));
    TEST_ASSERT_TRUE(w.update(md(), 8100));
}

void test_none_on_another_input_is_the_same(void) {
    LogSignalWatch w;
    LogSignalInfo n = md(); n.none = true;
    w.update(n, 0);
    TEST_ASSERT_TRUE(w.update(n, 1000));
    n.input = 3;
    TEST_ASSERT_FALSE(w.update(n, 2000));
    TEST_ASSERT_FALSE(w.update(n, 4000));
    LogSignalInfo s = md();
    w.update(s, 5000);
    TEST_ASSERT_TRUE(w.update(s, 6000));
}

// A short dropout (a console reset) never declares the source lost, so the signal line stays the same: one line when
// the sync comes back, with how long it was gone
void test_sync_return_reports_a_short_dropout(void) {
    LogSyncReturn r;
    TEST_ASSERT_EQUAL_UINT32(0, r.update(0, false, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, r.update(1, false, 1100));          // one missed check: not a dropout yet
    TEST_ASSERT_EQUAL_UINT32(0, r.update(0, false, 1200));
    TEST_ASSERT_EQUAL_UINT32(0, r.update(2, false, 2000));
    TEST_ASSERT_EQUAL_UINT32(0, r.update(9, false, 4000));
    TEST_ASSERT_EQUAL_UINT32(3200, r.update(0, false, 5200));       // back: gone since the second missed check
    TEST_ASSERT_EQUAL_UINT32(0, r.update(0, false, 5300));          // once
    TEST_ASSERT_EQUAL_UINT32(0, r.update(2, false, 6000));
    TEST_ASSERT_EQUAL_UINT32(0, r.update(40, true, 9000));          // the source was declared lost: logged elsewhere
    TEST_ASSERT_EQUAL_UINT32(0, r.update(0, false, 12000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_text_of_a_shown_signal);
    RUN_TEST(test_preset_names);
    RUN_TEST(test_watch_reports_once_after_a_stable_second);
    RUN_TEST(test_watch_ignores_a_short_flap);
    RUN_TEST(test_watch_skips_a_rate_no_source_sends);
    RUN_TEST(test_none_on_another_input_is_the_same);
    RUN_TEST(test_sync_return_reports_a_short_dropout);
    return UNITY_END();
}
