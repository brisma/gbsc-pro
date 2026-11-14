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
static IdSample report(IdSample s, bool lock, bool freeRun, bool burst = true) {
    s.reportFresh = true; s.lock = lock; s.freeRun = freeRun; s.burst = burst; return s;
}
// The answer of a look at the composite picture (the pass after IA_PEEK_COMPOSITE): content there, and what the RGB
// wires read right after it
static IdSample peeked(IdSample s, bool content, uint8_t wires = RW_FLAT) {
    s.peekMeasured = true; s.peekContent = content; s.peekWires = wires; return s;
}
// Passes while R/G/B and the composite are both black, every 300 ms from `t` to `until`: the machine asks for a G
// level sample or for a look, never anything else. Returns the number of looks asked
static uint8_t blackUntil(InputIdentify& id, uint32_t& t, uint32_t until, bool& afterPeek) {
    uint8_t looks = 0;
    for (; t < until; t += 300) {
        IdSample s = level(0, t - 300, t - 300);
        if (afterPeek) s = peeked(s, false);
        uint8_t a = id.step(s, t);
        TEST_ASSERT_TRUE(a == IA_SAMPLE_LEVEL || a == IA_PEEK_COMPOSITE);
        afterPeek = a == IA_PEEK_COMPOSITE;
        if (afterPeek) looks++;
    }
    return looks;
}
// To the first look: listening, two low G levels, the decoder present on pin 20
static void toFirstLook(InputIdentify& id, bool burst = true, bool lastComposite = false) {
    id.begin(0, true, true, lastComposite);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_RGB, id.step(pass(), 0));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_ON, id.step(rgb(false), 300));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 300, 200), 600));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(report(level(0, 600, 500), true, false, burst), 900));
}

// From a look asked at 900 and answered "a picture, wires flat" at 1200: G level samples, then the look that confirms
// it, asked at 2100 (kPeekEveryMs after the first)
static void toSecondLook(InputIdentify& id) {
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true), 1200));   // one look is not enough
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1200, 1100), 1500));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1500, 1400), 1800));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 1800, 1700), 2100));
}

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

// N64 (composite only): G 0, the decoder locked on pin 20, and a picture seen there at two looks in a row with
// the RGB wires flat: Composite, before kCompositeMinMs
void test_identify_composite_only(void) {
    InputIdentify id; toFirstLook(id);
    toSecondLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(peeked(pass(), true), 2400));
    TEST_ASSERT_TRUE(2400 < InputIdentify::kCompositeMinMs);
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_TRUE(id.listenWanted());                 // kept until the glue switched to Composite
    id.moveDone();
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
}

// The picture is what tells a composite source, not its colour: a black-and-white one is Composite too
void test_identify_composite_picture_without_burst(void) {
    InputIdentify id; toFirstLook(id, false);
    toSecondLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(peeked(pass(), true), 2400));
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE, id.why());
}

// Mega Drive, 50/60 Hz switched on a black screen: 0.3 s after the decoder locked, the look read Y 26/24/15 with the
// wires at 0/1/0, a settling that is gone a second later. One look that sees a picture is not a composite source
void test_identify_a_settling_decoder_is_not_a_composite_picture(void) {
    InputIdentify id; toFirstLook(id);
    toSecondLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), false), 2400));  // black now: it was a settling
    uint32_t t = 2700;
    bool afterPeek = false;
    blackUntil(id, t, InputIdentify::kBlackCapMs, afterPeek);
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(0, t, t), InputIdentify::kBlackCapMs));
    TEST_ASSERT_EQUAL_UINT8(IW_BLACK, id.why());
}

// The two looks must follow each other: a look with something on the wires in between starts the count again
void test_identify_composite_needs_two_looks_in_a_row(void) {
    InputIdentify id; toFirstLook(id);
    toSecondLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true, RW_FAINT), 2400));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 2400, 2300), 2700));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 2700, 2600), 3000));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 3000, 2900), 3300));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true), 3600));   // the first of a new pair
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
}

// A picture that comes late (a composite console after a long black start), seen by the last regular look: the look
// that confirms it is still made, after kPeekLastMs, and the black cap waits for it
void test_identify_a_late_picture_gets_its_second_look(void) {
    InputIdentify id; toFirstLook(id);                                                // asked at 900
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), false), 1200));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1200, 1100), 1500));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1500, 1400), 1800));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 1800, 1700), 2100));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), false), 2400));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 2400, 2300), 2700));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 2700, 2600), 3000));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 3000, 2900), 3300));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), false), 3600));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 3600, 3500), 3900));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 3900, 3800), 4100));
    TEST_ASSERT_TRUE(4350 < InputIdentify::kPeekLastMs);
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 4100, 4000), 4350));  // the last regular look
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true), 4700));    // the picture is there, once
    TEST_ASSERT_TRUE(5050 > InputIdentify::kBlackCapMs);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 4700, 4600), 5050));    // no black verdict meanwhile
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 5050, 4950), 5350));  // past kPeekLastMs
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(peeked(pass(), true), 5700));
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE, id.why());
}

// No look before the decoder is present on pin 20, then one every kPeekEveryMs with a G level sample in between,
// and none close to the black cap
void test_identify_looks_are_spaced(void) {
    InputIdentify id; id.begin(0, true);
    id.step(pass(), 0);
    id.step(rgb(false), 300);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 300, 200), 600));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 600, 500), 900));    // two lows, no decoder report yet
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(report(level(0, 900, 800), true, false), 1200));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), false), 1500));  // the answer: black; G not sampled yet
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1500, 1400), 1800));   // sampled, but only 600 ms on
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1800, 1700), 2100));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 2100, 2000), 1200 + InputIdentify::kPeekEveryMs));
    uint32_t t = 2500;
    bool afterPeek = true;
    blackUntil(id, t, InputIdentify::kPeekLastMs, afterPeek);
    for (; t < InputIdentify::kBlackCapMs; t += 300)                                 // too late for another look
        TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(afterPeek ? peeked(level(0, t, t), false) : level(0, t, t), t)), afterPeek = false;
}

// Mega Drive without a cartridge, or its region switched on a black screen: black on the RGB wires and on pin 20
// (its cable carries CVBS) for as long as the identification lasts. Nothing says Composite: RGB SCART
void test_identify_black_on_both_is_rgb(void) {
    InputIdentify id; toFirstLook(id);
    uint32_t t = 1200;
    bool afterPeek = true;
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(2, blackUntil(id, t, InputIdentify::kBlackCapMs, afterPeek));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(0, t, t), InputIdentify::kBlackCapMs));
    TEST_ASSERT_EQUAL_UINT8(IW_BLACK, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_RELEASING, id.phase());
}

// ... unless the last source found on this unit was Composite (a composite-only console on a long black start)
void test_identify_black_on_both_follows_the_last_source(void) {
    InputIdentify id; toFirstLook(id, true, true);
    uint32_t t = 1200;
    bool afterPeek = true;
    blackUntil(id, t, InputIdentify::kBlackCapMs, afterPeek);
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(level(0, t, t), InputIdentify::kBlackCapMs));
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE_LAST, id.why());
    TEST_ASSERT_TRUE(id.listenWanted());
}

// The console's own picture came back while the composite was looked at: content on B right after the look
void test_identify_picture_back_during_a_look_is_rgb(void) {
    InputIdentify id; toFirstLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(peeked(level(0, 900, 800), true, RW_BLUE), 1200));
    TEST_ASSERT_EQUAL_UINT8(IW_PICTURE_LISTENING, id.why());
}

// Mega Drive switched on, its first screen (white, the blue wire uniform) coming up while the composite was looked
// at: Y content 163/184, R/G/B right after 0/129/0. Not a composite-only source: the G level decides next
void test_identify_picture_on_the_wires_during_a_look_is_not_composite(void) {
    InputIdentify id; toFirstLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true, RW_CONTENT), 1200));
    TEST_ASSERT_EQUAL_UINT8(ID_IDENTIFY, id.phase());
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(109, 1200, 1100), 1500));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(104, 1500, 1400), 1800));
    TEST_ASSERT_EQUAL_UINT8(IW_RGB_WIRED, id.why());
}

// Something faint on the wires right after a look decides nothing: Composite needs them flat, at this look or a later one
void test_identify_faint_wires_wait_for_a_flat_look(void) {
    InputIdentify id; toFirstLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true, RW_FAINT), 1200));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1200, 1100), 1500));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 1500, 1400), 1800));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 1800, 1700), 2100));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true), 2400));   // flat now: the first of two
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 2400, 2300), 2700));
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(level(0, 2700, 2600), 3000));
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 3000, 2900), 3300));
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(peeked(pass(), true), 3600));
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE, id.why());
}

// Bright parts on black without blue (G median 0, B flat) for as long as the identification lasts: a look saw content
// on the G wire, so this is the console's own picture whatever the last source was
void test_identify_content_on_the_wires_is_rgb_at_the_cap(void) {
    InputIdentify id; toFirstLook(id, true, true);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(peeked(pass(), true, RW_CONTENT), 1200));
    uint32_t t = 1500;
    bool afterPeek = false;
    blackUntil(id, t, InputIdentify::kBlackCapMs, afterPeek);
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(0, t, t), InputIdentify::kBlackCapMs));
    TEST_ASSERT_EQUAL_UINT8(IW_WIRES, id.why());
}

// Mega Drive powered on during a scan: sync at once, R/G/B, the G level and the composite black for 3.4 s, then the
// SEGA logo: the picture coming on gives RGB SCART
void test_identify_booting_console_ends_rgb(void) {
    InputIdentify id; toFirstLook(id);
    uint32_t t = 1200;
    bool afterPeek = true;
    blackUntil(id, t, 3300, afterPeek);
    IdSample a = level(190, t - 300, t - 300);
    if (afterPeek) a = peeked(a, false);
    TEST_ASSERT_EQUAL_UINT8(IA_SAMPLE_LEVEL, id.step(a, t));
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(level(192, t, t), t + 300));
    TEST_ASSERT_EQUAL_UINT8(IW_RGB_WIRED, id.why());
}

// Mega Drive, Sonic 2 demo loop: 3.5 s of black, then a scene on a black background (bright parts, the G median
// stays 0). The blue wire carries nothing but an RGB picture: its content during the listen gives RGB SCART
void test_identify_picture_back_on_a_dark_scene_ends_rgb(void) {
    InputIdentify id; toFirstLook(id);
    uint32_t t = 1200;
    bool afterPeek = true;
    blackUntil(id, t, 3600, afterPeek);
    IdSample s = level(0, t - 300, t - 300);
    s.blueContent = true;
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, t));
    TEST_ASSERT_EQUAL_UINT8(IW_PICTURE_LISTENING, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_RELEASING, id.phase());
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_TRUE(id.holdsSyncWatcher());
}

void test_identify_blue_content_beats_a_composite_verdict_due(void) {   // the same pass would say Composite
    InputIdentify id; toFirstLook(id);
    IdSample s = peeked(level(0, 900, 800), true);
    s.blueContent = true;
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, 1200));
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
    TEST_ASSERT_EQUAL_UINT8(IA_PEEK_COMPOSITE, id.step(level(0, 600, 500), 900));
    toSecondLook(id);
    TEST_ASSERT_EQUAL_UINT8(IA_MOVE_COMPOSITE, id.step(peeked(pass(), true), 2400));
}

// A lock without a colour burst and no picture on pin 20 is a sync: an RGB cable wired with composite sync (csync),
// its console on a black screen, was shown through the decoder with a jumping picture. RGB SCART at kCompositeMinMs
void test_identify_sync_without_colour_is_not_composite(void) {
    InputIdentify id; toFirstLook(id, false);
    uint32_t t = 1200;
    bool afterPeek = true;
    blackUntil(id, t, InputIdentify::kCompositeMinMs, afterPeek);
    IdSample s = level(0, t, t);
    if (afterPeek) s = peeked(s, false);
    TEST_ASSERT_EQUAL_UINT8(IA_LISTEN_OFF, id.step(s, InputIdentify::kCompositeMinMs));
    TEST_ASSERT_EQUAL_UINT8(IW_NO_BURST, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_RELEASING, id.phase());
}

// Composite left out of Auto search: nothing to tell apart, RGB SCART at once (no decoder, no termination)
void test_identify_composite_not_searched_is_rgb_at_once(void) {
    InputIdentify id; id.begin(0, true, false);
    TEST_ASSERT_EQUAL_UINT8(IA_RELEASE_RGB, id.step(pass(), 0));
    TEST_ASSERT_EQUAL_UINT8(IW_COMPOSITE_OFF, id.why());
    TEST_ASSERT_EQUAL_UINT8(ID_DONE, id.phase());
    TEST_ASSERT_FALSE(id.listenWanted());
    TEST_ASSERT_FALSE(id.holdsSyncWatcher());
    TEST_ASSERT_FALSE(id.releaseForced());
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
    RUN_TEST(test_identify_composite_picture_without_burst);
    RUN_TEST(test_identify_a_settling_decoder_is_not_a_composite_picture);
    RUN_TEST(test_identify_composite_needs_two_looks_in_a_row);
    RUN_TEST(test_identify_a_late_picture_gets_its_second_look);
    RUN_TEST(test_identify_looks_are_spaced);
    RUN_TEST(test_identify_black_on_both_is_rgb);
    RUN_TEST(test_identify_black_on_both_follows_the_last_source);
    RUN_TEST(test_identify_picture_back_during_a_look_is_rgb);
    RUN_TEST(test_identify_picture_on_the_wires_during_a_look_is_not_composite);
    RUN_TEST(test_identify_faint_wires_wait_for_a_flat_look);
    RUN_TEST(test_identify_content_on_the_wires_is_rgb_at_the_cap);
    RUN_TEST(test_identify_booting_console_ends_rgb);
    RUN_TEST(test_identify_picture_back_on_a_dark_scene_ends_rgb);
    RUN_TEST(test_identify_blue_content_beats_a_composite_verdict_due);
    RUN_TEST(test_identify_blue_content_needs_the_switches_applied);
    RUN_TEST(test_identify_composite_keeps_an_earlier_fresh_report);
    RUN_TEST(test_identify_sync_without_colour_is_not_composite);
    RUN_TEST(test_identify_composite_not_searched_is_rgb_at_once);
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
