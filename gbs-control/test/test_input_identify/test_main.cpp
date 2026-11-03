// Input search identification:
// RGB SCART with a sync, nothing shown yet: R/G/B content, then the G level under listen, decide RGB SCART or Composite
#include <unity.h>
#include "input-identify.h"

void setUp(void) {}
void tearDown(void) {}

static IdSample pass() { IdSample s = {}; s.sync = true; return s; }
static IdSample rgb(bool content) { IdSample s = pass(); s.rgbMeasured = true; s.rgbContent = content; return s; }
static IdSample level(uint8_t g, uint32_t sinceListen, uint32_t sincePowered = 0) {
    IdSample s = pass(); s.listenSent = true; s.switchesApplied = true; s.levelMeasured = true; s.gLevel = g;
    s.sinceListenMs = sinceListen; s.sincePoweredMs = sincePowered; return s;
}
static IdSample report(IdSample s, bool lock, bool freeRun) { s.reportFresh = true; s.lock = lock; s.freeRun = freeRun; return s; }

// begin, then the flat R/G/B pass: listen goes on
static void toListening(InputIdentify& id, bool decoderListens = true) {
    id.begin(0, decoderListens);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_RGB, id.step(pass(), 0));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_ON, id.step(rgb(false), 300));
    TEST_ASSERT_TRUE(id.listenWanted());
}

void test_identify_rgb_picture_never_listens(void) {     // Sonic 2: content at the first pass
    InputIdentify id; id.begin(0, true);
    TEST_ASSERT_TRUE(id.holdsSyncWatcher());
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_RGB, id.step(pass(), 0));
    TEST_ASSERT_EQUAL_UINT8(IA_RELEASE_RGB, id.step(rgb(true), 300));
    TEST_ASSERT_EQUAL_UINT8(IW_PICTURE, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

static void expectRgbWired(InputIdentify& id, uint8_t g) {
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(g, 300, 0), 600));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(g, 600, 0), 900));
    TEST_ASSERT_EQUAL_UINT8(IW_RGB_WIRED, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_RELEASING, id.phase());
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_TRUE(id.holdsSyncWatcher());
}

void test_identify_rgb_wired_on_black(void) {           // Mega Drive black: G 74-80 under listen
    InputIdentify id; toListening(id);
    expectRgbWired(id, 76);
}

void test_identify_rgb_only_cable_on_black(void) {      // SNES csync: G 76-80
    InputIdentify id; toListening(id);
    expectRgbWired(id, 77);
}

void test_identify_composite_only(void) {               // N64: G 0, decoder lock + PAL burst
    InputIdentify id; toListening(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 300, 200), 600));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(report(level(0, 600, 500), true, false), 900));   // a booting console?
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(level(0, 3700, 3600), InputIdentify::kCompositeMinMs));
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_TRUE(id.listenWanted());                 // kept until the glue switched to Composite
    id.moveDone();
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

// Mega Drive powered on during a scan: sync at once, R/G/B and the G level flat for 3.4 s, then the
// SEGA logo; a composite verdict waits kCompositeMinMs from begin(), the picture coming on gives RGB SCART
void test_identify_booting_console_ends_rgb(void) {
    InputIdentify id; toListening(id);
    uint32_t t = 600;
    for (; t <= 3000; t += 300) {
        IdSample s = level(0, t - 300, t - 300);
        if (t == 900) s = report(s, true, false);
        TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(s, t));
    }
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(190, t - 300, t - 300), t));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(192, t, t), t + 300));
    TEST_ASSERT_EQUAL_UINT8(IW_RGB_WIRED, id.why());
}

// Mega Drive, Sonic 2 demo loop: 3.5 s of black, then a scene on a black background (bright parts, the G median
// stays 0). The blue wire carries nothing but an RGB picture: its content during the listen gives RGB SCART
void test_identify_picture_back_on_a_dark_scene_ends_rgb(void) {
    InputIdentify id; toListening(id);
    uint32_t t = 600;
    for (; t <= 3300; t += 300) {
        IdSample s = level(0, t - 300, t - 300);
        if (t == 900) s = report(s, true, false);       // the decoder locks on the cable's CVBS
        TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(s, t));
    }
    IdSample s = level(0, t - 300, t - 300);
    s.blueContent = true;
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, t));
    TEST_ASSERT_EQUAL_UINT8(IW_PICTURE_LISTENING, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_RELEASING, id.phase());
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_TRUE(id.holdsSyncWatcher());
}

void test_identify_blue_content_beats_a_composite_verdict_due(void) {   // the same pass would say Composite
    InputIdentify id; toListening(id);
    id.step(level(0, 300, 200), 600);
    id.step(report(level(0, 600, 500), true, false), 900);
    IdSample s = level(0, 3700, 3600);
    s.blueContent = true;
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, InputIdentify::kCompositeMinMs));
    TEST_ASSERT_EQUAL_UINT8(IW_PICTURE_LISTENING, id.why());
}

void test_identify_blue_content_needs_the_switches_applied(void) {   // a sample not taken reads as no content
    InputIdentify id; toListening(id);
    IdSample s = pass(); s.listenSent = true; s.blueContent = true;   // levelMeasured false: not a listen sample
    TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(s, 600));
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
}

void test_identify_composite_keeps_an_earlier_fresh_report(void) {   // reports every 500 ms, passes every 300 ms
    InputIdentify id; toListening(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(report(level(0, 300, 200), true, false), 600));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 600, 500), 900));
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(level(0, 3700, 3600), InputIdentify::kCompositeMinMs));
}

void test_identify_composite_colour_kill_still_composite(void) {   // lock without burst (a console without colour)
    InputIdentify id; toListening(id);
    id.step(level(0, 300, 200), 600);
    id.step(report(level(0, 600, 500), true, false), 900);
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(level(0, 3700, 3600), InputIdentify::kCompositeMinMs));
}

void test_identify_composite_waits_for_lock(void) {
    InputIdentify id; toListening(id);
    id.step(level(0, 300, 200), 600);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(report(level(0, 600, 900), false, false), 900));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(0, 1500, 1800), 1800));
    TEST_ASSERT_EQUAL_UINT8(IW_NO_LOCK, id.why());
}

void test_identify_lock_with_freerun_is_not_present(void) {
    InputIdentify id; toListening(id);
    id.step(level(0, 300, 200), 600);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(report(level(0, 600, 900), true, true), 900));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(0, 1500, 1800), 1800));
    TEST_ASSERT_EQUAL_UINT8(IW_NO_LOCK, id.why());
}

void test_identify_ambiguous_level_goes_rgb(void) {
    InputIdentify id; toListening(id);
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(20, 300, 0), 600));
    TEST_ASSERT_EQUAL_UINT8(IW_LEVEL_AMBIGUOUS, id.why());
}

void test_identify_one_level_is_not_enough(void) {
    InputIdentify id; toListening(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(76, 300, 0), 600));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(report(level(0, 600, 500), true, false), 900));
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
}

void test_identify_decoder_silent(void) {               // listen sent, no POWERED report
    InputIdentify id; toListening(id);
    IdSample s = level(0, 2999, 0);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(s, 3200));
    s = level(0, 3000, 0);
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, 3300));
    TEST_ASSERT_EQUAL_UINT8(IW_DECODER_SILENT, id.why());
}

void test_identify_cap_releases_rgb(void) {              // alternating levels never decide: the cap does
    InputIdentify id; toListening(id);
    uint32_t t = 600, since = 300;
    uint8_t g = 76;
    for (; since < InputIdentify::kIdentifyCapMs; t += 300, since += 300, g = g ? 0 : 76)
        TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(report(level(g, since, since), true, true), t));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(g, InputIdentify::kIdentifyCapMs, 1000), t));
    TEST_ASSERT_EQUAL_UINT8(IW_TIMEOUT, id.why());
}

void test_identify_releasing_waits_for_listen_off(void) {
    InputIdentify id; toListening(id);
    expectRgbWired(id, 76);                              // LISTEN_OFF at 900; listen-on was at 300: cap from 1100
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(76, 0, 0), 1200));   // the termination still on
    TEST_ASSERT_EQUAL_UINT8(IA_RELEASE_RGB, id.step(level(0, 0, 0), 1500));
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_FALSE(id.releaseForced());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

// Mega Drive powered on: the SEGA logo keeps G at 185 after listen off; the ADV Controller's report
// without POWERED proves the termination is off (switches go before the decoder power-down): no wait, no re-detection
void test_identify_releasing_ends_on_the_decoder_off_report(void) {
    InputIdentify id; toListening(id);
    expectRgbWired(id, 185);                             // LISTEN_OFF at 900
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(187, 0, 0), 1200));  // no report yet: G may be the termination
    IdSample s = level(188, 0, 0);
    s.listenOffConfirmed = true;
    TEST_ASSERT_EQUAL_UINT8(IA_RELEASE_RGB, id.step(s, 1500));
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_FALSE(id.releaseForced());
    TEST_ASSERT_EQUAL_UINT8(IW_RGB_WIRED, id.why());
}

void test_identify_releasing_cap_after_the_decoder_init(void) {
    InputIdentify id; toListening(id);                   // listen-on at 300
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(20, 300, 0), 400));    // LISTEN_OFF at 400: cap from 1100
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(76, 0, 0), 1500));
    TEST_ASSERT_EQUAL_UINT8(IA_RELEASE_RGB, id.step(level(76, 0, 0), 1100 + InputIdentify::kReleaseCapMs));
    TEST_ASSERT_TRUE(id.releaseForced());                // the glue re-detects before showing (green residue)
}

void test_identify_termination_only_composite(void) {   // old ADV Controller image: no decoder report
    InputIdentify id; toListening(id, false);
    id.step(level(0, 300, 0), 600);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 600, 0), 900));
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(level(0, 3700, 0), InputIdentify::kCompositeMinMs));
}

void test_identify_termination_only_rgb(void) {
    InputIdentify id; toListening(id, false);
    expectRgbWired(id, 76);
}

void test_identify_termination_only_never_decoder_silent(void) {
    InputIdentify id; toListening(id, false);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(76, 3100, 0), 3400));
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
}

void test_phase_abandon_sync_lost(void) {
    InputIdentify id; toListening(id);
    IdSample off = pass(); off.sync = false;
    TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(off, 600));
    TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(off, 900));
    TEST_ASSERT_EQUAL_UINT8(IA_ABANDON, id.step(off, 1200));
    TEST_ASSERT_EQUAL_UINT8(IW_SYNC_LOST, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_IDLE, id.phase());
    TEST_ASSERT_FALSE(id.listenWanted()); TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

void test_phase_sync_back_resets_the_count(void) {
    InputIdentify id; toListening(id);
    IdSample off = pass(); off.sync = false;
    id.step(off, 600); id.step(off, 900);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(76, 900, 0), 1200));
    TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(off, 1500));
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
}

static void expectAbandoned(uint8_t why) {
    InputIdentify id; toListening(id);
    id.abandon(why);
    TEST_ASSERT_EQUAL_UINT8(ID_IDLE, id.phase());
    TEST_ASSERT_EQUAL_UINT8(why, id.why());
    TEST_ASSERT_FALSE(id.listenWanted()); TEST_ASSERT_FALSE(id.holdsSyncWatcher());
    TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(level(76, 600, 0), 900));
}
void test_phase_abandon_choice(void) { expectAbandoned(IW_CHOICE); }
void test_phase_abandon_rescan(void) { expectAbandoned(IW_RESCAN); }
void test_phase_abandon_paused(void) { expectAbandoned(IW_PAUSED); }

void test_phase_done_survives_mode_change_and_gaps(void) {   // a shown picture is never identified again
    InputIdentify id; id.begin(0, true);
    id.step(pass(), 0); id.step(rgb(true), 300);
    IdSample off = pass(); off.sync = false;
    for (uint32_t t = 600; t < 3000; t += 300) TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(t % 600 ? off : rgb(false), t));
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

void test_phase_done_resets_on_new_acquisition(void) {
    InputIdentify id; id.begin(0, true);
    id.step(pass(), 0); id.step(rgb(true), 300);
    id.newAcquisition();
    TEST_ASSERT_EQUAL_UINT8(ID_IDLE, id.phase());
    InputIdentify j; toListening(j);
    j.newAcquisition();                                  // during IDENTIFY: abandoned, listen off
    TEST_ASSERT_EQUAL_UINT8(ID_IDLE, j.phase());
    TEST_ASSERT_FALSE(j.listenWanted());
}

// No hang: with the listen STATE never going out (an incompatible ADV Controller) sinceListenMs stays
// 0 and the switches never apply; the machine's own cap from begin() still releases RGB SCART
void test_identify_absolute_cap_without_the_switches(void) {
    InputIdentify id; toListening(id);
    IdSample s = pass(); s.listenSent = true;            // no switchesApplied, no level, no report, sinceListenMs 0
    TEST_ASSERT_EQUAL_UINT8(IA_NONE, id.step(s, InputIdentify::kAbsoluteCapMs - 1));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, InputIdentify::kAbsoluteCapMs));
    TEST_ASSERT_EQUAL_UINT8(IW_TIMEOUT, id.why());
}

void test_identify_absolute_cap_before_listen(void) {   // R/G/B never measured (a flapping sync)
    InputIdentify id; id.begin(0, true);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_RGB, id.step(pass(), InputIdentify::kAbsoluteCapMs - 1));
    TEST_ASSERT_EQUAL_UINT8(IA_RELEASE_RGB, id.step(pass(), InputIdentify::kAbsoluteCapMs));
    TEST_ASSERT_EQUAL_UINT8(IW_TIMEOUT, id.why());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_identify_rgb_picture_never_listens);
    RUN_TEST(test_identify_rgb_wired_on_black);
    RUN_TEST(test_identify_rgb_only_cable_on_black);
    RUN_TEST(test_identify_composite_only);
    RUN_TEST(test_identify_booting_console_ends_rgb);
    RUN_TEST(test_identify_picture_back_on_a_dark_scene_ends_rgb);
    RUN_TEST(test_identify_blue_content_beats_a_composite_verdict_due);
    RUN_TEST(test_identify_blue_content_needs_the_switches_applied);
    RUN_TEST(test_identify_composite_keeps_an_earlier_fresh_report);
    RUN_TEST(test_identify_composite_colour_kill_still_composite);
    RUN_TEST(test_identify_composite_waits_for_lock);
    RUN_TEST(test_identify_lock_with_freerun_is_not_present);
    RUN_TEST(test_identify_ambiguous_level_goes_rgb);
    RUN_TEST(test_identify_one_level_is_not_enough);
    RUN_TEST(test_identify_decoder_silent);
    RUN_TEST(test_identify_cap_releases_rgb);
    RUN_TEST(test_identify_releasing_ends_on_the_decoder_off_report);
    RUN_TEST(test_identify_releasing_waits_for_listen_off);
    RUN_TEST(test_identify_releasing_cap_after_the_decoder_init);
    RUN_TEST(test_identify_termination_only_composite);
    RUN_TEST(test_identify_termination_only_rgb);
    RUN_TEST(test_identify_termination_only_never_decoder_silent);
    RUN_TEST(test_phase_abandon_sync_lost);
    RUN_TEST(test_phase_sync_back_resets_the_count);
    RUN_TEST(test_phase_abandon_choice);
    RUN_TEST(test_phase_abandon_rescan);
    RUN_TEST(test_phase_abandon_paused);
    RUN_TEST(test_phase_done_survives_mode_change_and_gaps);
    RUN_TEST(test_phase_done_resets_on_new_acquisition);
    RUN_TEST(test_identify_absolute_cap_without_the_switches);
    RUN_TEST(test_identify_absolute_cap_before_listen);
    return UNITY_END();
}
