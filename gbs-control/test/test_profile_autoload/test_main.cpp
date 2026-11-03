#include <string.h>
#include <unity.h>
#include "profile-autoload.h"
#include "profile-modes.h"

void setUp(void) {}
void tearDown(void) {}

static AutoloadSample S(uint16_t lines, bool signal = true, bool stageBusy = false) {
    AutoloadSample s; s.input = 1; s.mode = 1; s.lines = lines; s.signal = signal; s.stageBusy = stageBusy; return s;
}

static int feedN(StableLatch& l, AutoloadSample s, int n) {
    int fires = 0;
    for (int i = 0; i < n; i++) if (l.feed(s)) fires++;
    return fires;
}

void test_latch_fires_once_per_stable_signal(void) {
    StableLatch l; l.reset();
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(523), 99));
    TEST_ASSERT_EQUAL_INT(1, feedN(l, S(523), 1));
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(523), 500));
    TEST_ASSERT_EQUAL_UINT16(523, l.anchorLines());
    TEST_ASSERT_FALSE(l.interlaced(1));
}

void test_latch_tolerates_jitter_and_rearms_on_jump(void) {
    StableLatch l; l.reset();
    int fires = 0;
    for (int i = 0; i < 100; i++) if (l.feed(S(i & 1 ? 525 : 524))) fires++;
    TEST_ASSERT_EQUAL_INT(1, fires);
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(530), 99));   // jump of 6: re-armed, counting again
    TEST_ASSERT_EQUAL_INT(1, feedN(l, S(530), 1));
}

void test_latch_tolerates_interlaced_field_jitter(void) {
    StableLatch l; l.reset();
    static const uint16_t seq[3] = {522, 526, 524};   // 480i VPERIOD_IF alternation
    int fires = 0;
    for (int i = 0; i < 100; i++) if (l.feed(S(seq[i % 3]))) fires++;
    TEST_ASSERT_EQUAL_INT(1, fires);
    TEST_ASSERT_EQUAL_UINT16(524, l.anchorLines());   // middle of the observed range
}

void test_latch_rearms_when_spread_exceeds_window(void) {
    StableLatch l; l.reset();
    feedN(l, S(522), 50);
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(527), 99));   // spread 5: re-armed at 527
    TEST_ASSERT_EQUAL_INT(1, feedN(l, S(527), 1));
}

void test_stage_busy_only_while_applying(void) {
    TEST_ASSERT_FALSE(autoloadStageBusy(0));
    TEST_ASSERT_TRUE(autoloadStageBusy(1));
    TEST_ASSERT_TRUE(autoloadStageBusy(10));
    TEST_ASSERT_FALSE(autoloadStageBusy(11));   // finished state (bypass / skipped re-apply)
}

void test_interlace_vote(void) {
    StableLatch l; l.reset();
    for (int i = 0; i < 100; i++) l.feed(S(i % 3 ? 524 : 526));   // even field lengths: interlaced
    TEST_ASSERT_TRUE(l.interlaced(1));
    TEST_ASSERT_FALSE(l.interlaced(3));                          // only modes 1/2 can be interlaced
}

void test_stage_busy_samples_are_ignored(void) {
    StableLatch l; l.reset();
    feedN(l, S(523), 50);
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(523, true, true), 300));   // preset re-apply: neither counts nor re-arms
    TEST_ASSERT_EQUAL_INT(1, feedN(l, S(523), 50));
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(523, true, true), 300));   // after firing: no second fire
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(523), 200));
}

void test_signal_loss_rearms(void) {
    StableLatch l; l.reset();
    feedN(l, S(523), 100);
    TEST_ASSERT_EQUAL_INT(0, feedN(l, S(0, false), 10));
    TEST_ASSERT_EQUAL_INT(1, feedN(l, S(523), 100));
}

static ProfileList makeList() {
    ProfileList l; l.clear();
    int a = l.allocate(); strcpy(l.at(a).name, "A"); l.at(a).hasFp = true; l.at(a).fpInput = 1; l.at(a).fpMode = 1;
    l.at(a).fpLines = 523; l.at(a).fpInterlaced = 0; l.at(a).lastused = 5;
    int b = l.allocate(); strcpy(l.at(b).name, "B"); l.at(b).hasFp = true; l.at(b).fpInput = 1; l.at(b).fpMode = 1;
    l.at(b).fpLines = 525; l.at(b).fpInterlaced = 0; l.at(b).lastused = 9;
    int c = l.allocate(); strcpy(l.at(c).name, "C"); l.at(c).hasFp = false;   // no fingerprint
    return l;
}

void test_matcher(void) {
    ProfileList l = makeList();
    int cand = -1;
    TEST_ASSERT_EQUAL_UINT8(AL_LOAD, profileAutoloadMatch(l, 1, 1, 524, false, -1, false, &cand));
    TEST_ASSERT_EQUAL_INT(1, cand);                                   // most recently used
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 2, 1, 524, false, -1, false, &cand));
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 2, 524, false, -1, false, &cand));
    TEST_ASSERT_EQUAL_UINT8(AL_LOAD, profileAutoloadMatch(l, 1, 1, 519, false, -1, false, &cand));   // 523-4
    TEST_ASSERT_EQUAL_INT(0, cand);
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 1, 518, false, -1, false, &cand));   // 523-5
    TEST_ASSERT_EQUAL_UINT8(AL_ALREADY_ACTIVE, profileAutoloadMatch(l, 1, 1, 524, false, 1, false, &cand));
    // Another console's profile is loaded even over unsaved changes (they are discarded and the
    // pop-up says so); the matching profile already active is never reloaded over its own unsaved changes
    TEST_ASSERT_EQUAL_UINT8(AL_LOAD, profileAutoloadMatch(l, 1, 1, 524, false, 0, true, &cand));
    TEST_ASSERT_EQUAL_INT(1, cand);
    TEST_ASSERT_EQUAL_UINT8(AL_ALREADY_ACTIVE, profileAutoloadMatch(l, 1, 1, 524, false, 1, true, &cand));
}

// An N64 on Composite with no profile kept the Mega Drive profile. No match: Defaults only when the signal is on
// another input than the active profile's fingerprint. The same input keeps it whatever the mode (PAL 50 <-> 60, 480i
// <-> 480p, VGA RGBHV scaling 14 <-> 15 of one console must not drop to Defaults, and the VGA case looped profile <->
// Defaults)
void test_no_match_gives_defaults_only_off_the_profile_input(void) {
    ProfileList l = makeList();                    // A, B: RGB SCART (1), mode 1; C: no fingerprint
    int cand = -1;
    TEST_ASSERT_EQUAL_UINT8(AL_DEFAULTS, profileAutoloadMatch(l, 6, 2, 624, true, 0, false, &cand, 2));   // Composite
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 1, 500, true, 0, false, &cand, 1));  // 240p <-> 480i
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 2, 625, false, 0, false, &cand, 2)); // 60 <-> 50 Hz
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 3, 525, false, 0, false, &cand, 3)); // 480p
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 6, 2, 624, true, 2, false, &cand, 2));  // no fingerprint
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 6, 2, 624, true, -1, false, &cand, 2)); // Defaults already
    l.at(0).fpInput = 0;                           // an imported file with a broken fingerprint: shown as none, kept
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 6, 2, 624, true, 0, false, &cand, 2));
}

void test_matcher_prefers_interlace_then_ties_by_index(void) {
    ProfileList l = makeList();
    l.at(0).fpInterlaced = 1;
    int cand = -1;
    profileAutoloadMatch(l, 1, 1, 524, true, -1, false, &cand);
    TEST_ASSERT_EQUAL_INT(0, cand);                                   // equal interlace beats lastused
    l.at(0).fpInterlaced = 0; l.at(0).lastused = 9;
    profileAutoloadMatch(l, 1, 1, 524, false, -1, false, &cand);
    TEST_ASSERT_EQUAL_INT(0, cand);                                   // full tie: lowest index
    l.remove(0);
    profileAutoloadMatch(l, 1, 1, 524, false, -1, false, &cand);
    TEST_ASSERT_EQUAL_INT(1, cand);                                   // unused entries ignored
}

// fired() tells the device glue the anchor is complete: a jump re-arms it, so a pending match waiting for a
// closed menu is dropped instead of running on a half-counted anchor
// 240p (odd VPERIOD_IF, 523) and 480i (even, 524/526) are the same input, mode and line window: a clean switch (PS2
// menu at 480i -> a 240p game) must still be a new signal, else its own profile never loads.
// The parity has to disagree with the fired vote for kParityRun samples in a row
void test_latch_rearms_on_a_240p_480i_switch(void) {
    StableLatch l; l.reset();
    TEST_ASSERT_EQUAL_INT(1, feedN(l, S(523), StableLatch::kSamples));
    int fires = 0;
    for (int i = 0; i < 200; i++) if (l.feed(S(i % 2 ? 526 : 524))) fires++;
    TEST_ASSERT_EQUAL_INT(1, fires);
    TEST_ASSERT_TRUE(l.interlaced(1));
    fires = 0;
    for (int i = 0; i < 200; i++) if (l.feed(S(523))) fires++;   // and back to 240p
    TEST_ASSERT_EQUAL_INT(1, fires);
    TEST_ASSERT_FALSE(l.interlaced(1));
}

// The device logs a parity re-arm: the flag is set by the feed that re-armed and
// only by it; a line jump re-arms without it
void test_latch_reports_a_parity_rearm(void) {
    StableLatch l; l.reset();
    feedN(l, S(523), StableLatch::kSamples);
    for (int i = 1; i < StableLatch::kParityRun; i++) {
        l.feed(S(524));
        TEST_ASSERT_FALSE(l.parityRearmed());
    }
    l.feed(S(524));
    TEST_ASSERT_TRUE(l.parityRearmed());
    l.feed(S(524));
    TEST_ASSERT_FALSE(l.parityRearmed());
    feedN(l, S(524), StableLatch::kSamples);
    l.feed(S(312));
    TEST_ASSERT_FALSE(l.parityRearmed());
}

void test_latch_parity_jitter_does_not_rearm(void) {
    StableLatch l; l.reset();
    static const uint16_t seq[3] = {522, 526, 524};
    int fires = 0;
    for (int i = 0; i < 100; i++) if (l.feed(S(seq[i % 3]))) fires++;
    for (int i = 0; i < 300; i++) if (l.feed(S(i % 20 ? seq[i % 3] : 525))) fires++;   // an odd reading now and then
    TEST_ASSERT_EQUAL_INT(1, fires);
}

void test_latch_not_decided_while_parity_disagrees(void) {
    StableLatch l; l.reset();
    feedN(l, S(523), StableLatch::kSamples);
    TEST_ASSERT_TRUE(l.decidedFor(S(523)));
    TEST_ASSERT_FALSE(l.decidedFor(S(524)));           // the pop-up waits for the new signal's decision
}

void test_latch_fired_state_follows_rearm(void) {
    StableLatch l;
    feedN(l, S(262), StableLatch::kSamples);
    TEST_ASSERT_TRUE(l.fired());
    l.feed(S(312));                                              // another signal while the menu is open
    TEST_ASSERT_FALSE(l.fired());
}

// The status pop-up waits until the decision is about the signal shown now, not the last one (a format change is
// seen by the pop-up gate before the 20 ms auto-load sample re-arms the latch)
void test_latch_decided_for_the_current_signal_only(void) {
    StableLatch l;
    TEST_ASSERT_FALSE(l.decidedFor(S(262)));                     // nothing counted yet
    feedN(l, S(262), StableLatch::kSamples - 1);
    TEST_ASSERT_FALSE(l.decidedFor(S(262)));                     // still counting
    l.feed(S(262));
    TEST_ASSERT_TRUE(l.decidedFor(S(262)));
    TEST_ASSERT_TRUE(l.decidedFor(S(263)));                      // field jitter: the same signal
    TEST_ASSERT_FALSE(l.decidedFor(S(312)));                     // new format, latch not re-armed yet
    AutoloadSample other = S(262); other.input = 2;
    TEST_ASSERT_FALSE(l.decidedFor(other));
    AutoloadSample mode = S(262); mode.mode = 3;
    TEST_ASSERT_FALSE(l.decidedFor(mode));
    StableLatch w;                                               // the window widened by jitter after the start
    for (int i = 0; i < StableLatch::kSamples; i++) w.feed(S(i & 1 ? 526 : 522));
    TEST_ASSERT_TRUE(w.decidedFor(S(524)));
    TEST_ASSERT_FALSE(w.decidedFor(S(527)));                     // feed() would re-arm on it
    TEST_ASSERT_FALSE(w.decidedFor(S(521)));
}

static int fp(ProfileList& l, const char* name, uint16_t lines, bool inter, uint32_t lastused) {
    int i = l.allocate();
    ProfileEntry& e = l.at((uint8_t)i);
    strcpy(e.name, name); e.hasFp = true; e.fpInput = 1; e.fpMode = 1; e.fpLines = lines;
    e.fpInterlaced = inter ? 1 : 0; e.lastused = lastused;
    return i;
}

// Every candidate of the auto-load rule, ranked as auto-load ranks them
void test_matches_order(void) {
    ProfileList l; l.clear();
    int lo = fp(l, "Lo", 524, false, 5), hi = fp(l, "Hi", 523, false, 9), in = fp(l, "In", 525, true, 1);
    fp(l, "Far", 529, false, 20);                                // lines +5: not a candidate
    uint8_t out[8];
    TEST_ASSERT_EQUAL_UINT8(3, profileMatches(l, 1, 1, 524, true, out, 8));
    TEST_ASSERT_EQUAL_UINT8(in, out[0]);                         // interlace match first
    TEST_ASSERT_EQUAL_UINT8(hi, out[1]);                         // then the most recently used
    TEST_ASSERT_EQUAL_UINT8(lo, out[2]);
    TEST_ASSERT_EQUAL_UINT8(1, profileMatches(l, 1, 1, 524, true, out, 1));
    TEST_ASSERT_EQUAL_UINT8(0, profileMatches(l, 2, 1, 524, true, out, 8));
}


// A Mega Drive and a SNES in 240p on one input: the same input, standard and line count, another line rate
void test_matches_keep_consoles_apart_by_rate(void) {
    ProfileList l; l.clear();
    int md = fp(l, "Mega Drive", 523, false, 9), snes = fp(l, "SNES", 523, false, 5), old = fp(l, "Old", 523, false, 1);
    l.at((uint8_t)md).fpMilliHz = 59376;
    l.at((uint8_t)snes).fpMilliHz = 60100;                       // "Old" was saved without a rate: it fits any
    uint8_t out[8];
    TEST_ASSERT_EQUAL_UINT8(2, profileMatches(l, 1, 1, 523, false, out, 8, 60099));
    TEST_ASSERT_EQUAL_UINT8(snes, out[0]);                       // not the Mega Drive's, used last
    TEST_ASSERT_EQUAL_UINT8(old, out[1]);
    TEST_ASSERT_EQUAL_UINT8(3, profileMatches(l, 1, 1, 523, false, out, 8));   // rate not measured: as before
    int cand = -1;
    TEST_ASSERT_EQUAL_UINT8(AL_LOAD, profileAutoloadMatch(l, 1, 1, 523, false, md, false, &cand, 1, 60099));
    TEST_ASSERT_EQUAL_INT(snes, cand);
}

// Another console on the active profile's input, with no profile of its own: Defaults, not that profile
void test_another_console_without_a_profile_gets_defaults(void) {
    ProfileList l; l.clear();
    int md = fp(l, "Mega Drive", 523, false, 9);
    l.at((uint8_t)md).fpMilliHz = 59376;
    int cand = -1;
    TEST_ASSERT_EQUAL_UINT8(AL_DEFAULTS, profileAutoloadMatch(l, 1, 1, 523, false, md, false, &cand, 1, 60099));   // a SNES
    TEST_ASSERT_EQUAL_UINT8(AL_ALREADY_ACTIVE, profileAutoloadMatch(l, 1, 1, 523, false, md, false, &cand, 1, 59377));
    TEST_ASSERT_EQUAL_UINT8(AL_ALREADY_ACTIVE, profileAutoloadMatch(l, 1, 1, 524, true, md, false, &cand, 1, 59263));   // its 480i
    // the same console in another standard keeps its profile, as before (50 Hz: 313 lines, the same line rate or not)
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 2, 625, false, md, false, &cand, 2, 49701));
    TEST_ASSERT_EQUAL_UINT8(AL_NONE, profileAutoloadMatch(l, 1, 2, 623, false, md, false, &cand, 2, 50007));
    TEST_ASSERT_EQUAL_UINT8(AL_DEFAULTS, profileAutoloadMatch(l, 1, 1, 523, false, md, false, &cand, 1, 60099));
    l.at((uint8_t)md).fpMilliHz = 0;               // a profile saved without a rate: it loads for the signal, as before
    TEST_ASSERT_EQUAL_UINT8(AL_ALREADY_ACTIVE, profileAutoloadMatch(l, 1, 1, 523, false, md, false, &cand, 1, 60099));
}

// The web and the log said "loaded" (and "unsaved changes of X discarded") when the load was only
// queued; a failed load drops nothing, and a web Save queued before it runs first
void test_notice_waits_for_the_result(void) {
    AutoloadNotice n;
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_NONE, n.poll(false, "", 0));
    n.start(7, "Mega Drive PAL", "N64 PAL", 1000);
    TEST_ASSERT_TRUE(n.waiting());
    TEST_ASSERT_EQUAL_UINT16(7, n.seq());
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_WAIT, n.poll(false, "", 1500));
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_LOADED, n.poll(true, "", 1600));
    TEST_ASSERT_EQUAL_STRING("Mega Drive PAL", n.label());
    TEST_ASSERT_EQUAL_STRING("N64 PAL", n.discarded());
    TEST_ASSERT_FALSE(n.waiting());
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_NONE, n.poll(true, "", 1700));   // once
}

void test_notice_failed_load_discards_nothing(void) {
    AutoloadNotice n;
    n.start(3, "Defaults", "SNES", 0);
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_FAILED, n.poll(true, "io", 100));
    TEST_ASSERT_EQUAL_STRING("io", n.result());
    TEST_ASSERT_FALSE(n.waiting());
}

void test_notice_gives_up(void) {
    AutoloadNotice n;
    n.start(3, "Saturn", nullptr, 0);
    TEST_ASSERT_EQUAL_STRING("", n.discarded());
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_WAIT, n.poll(false, "", AutoloadNotice::kGiveUpMs - 1));
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_TIMEOUT, n.poll(false, "", AutoloadNotice::kGiveUpMs));
    TEST_ASSERT_FALSE(n.waiting());
}

// A backup holds the profile queue: the wait for the result restarts after it, else a slow
// download made the notice give up and the load ran with no "auto-load: X" line and no web toast
void test_notice_waits_out_a_backup(void) {
    AutoloadNotice n;
    n.start(4, "Saturn", nullptr, 0);
    n.hold(9000);                                                      // the backup still runs at 9 s
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_WAIT, n.poll(false, "", 15000));
    TEST_ASSERT_EQUAL_UINT8(AutoloadNotice::AN_LOADED, n.poll(true, "", 16000));
    AutoloadNotice idle;
    idle.hold(5000);                                                   // nothing waiting: nothing starts
    TEST_ASSERT_FALSE(idle.waiting());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_latch_fires_once_per_stable_signal);
    RUN_TEST(test_latch_fired_state_follows_rearm);
    RUN_TEST(test_latch_decided_for_the_current_signal_only);
    RUN_TEST(test_latch_tolerates_jitter_and_rearms_on_jump);
    RUN_TEST(test_latch_tolerates_interlaced_field_jitter);
    RUN_TEST(test_latch_rearms_when_spread_exceeds_window);
    RUN_TEST(test_latch_rearms_on_a_240p_480i_switch);
    RUN_TEST(test_latch_reports_a_parity_rearm);
    RUN_TEST(test_latch_parity_jitter_does_not_rearm);
    RUN_TEST(test_latch_not_decided_while_parity_disagrees);
    RUN_TEST(test_stage_busy_only_while_applying);
    RUN_TEST(test_interlace_vote);
    RUN_TEST(test_stage_busy_samples_are_ignored);
    RUN_TEST(test_signal_loss_rearms);
    RUN_TEST(test_matcher);
    RUN_TEST(test_matcher_prefers_interlace_then_ties_by_index);
    RUN_TEST(test_matches_order);
    RUN_TEST(test_matches_keep_consoles_apart_by_rate);
    RUN_TEST(test_another_console_without_a_profile_gets_defaults);
    RUN_TEST(test_no_match_gives_defaults_only_off_the_profile_input);
    RUN_TEST(test_notice_waits_for_the_result);
    RUN_TEST(test_notice_failed_load_discards_nothing);
    RUN_TEST(test_notice_gives_up);
    RUN_TEST(test_notice_waits_out_a_backup);
    return UNITY_END();
}
