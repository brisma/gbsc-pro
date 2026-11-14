// RGB SCART content helpers and the "try Composite" hint
#include <unity.h>
#include "rgb-scart-judge.h"

void setUp(void) {}
void tearDown(void) {}

// A line of video: 7 % sync tip, 10 % blanking at black, the rest picture at `level`
static void line(uint16_t* s, uint16_t n, uint16_t black, uint16_t level) {
    for (uint16_t i = 0; i < n; i++) {
        uint16_t k = (uint16_t)(i * 100 / n);
        s[i] = k < 7 ? (uint16_t)(black > 40 ? black - 40 : 0) : k < 17 ? black : level;
    }
}

void test_content_ignores_sync_and_blanking(void) {
    uint16_t s[256];
    line(s, 256, 60, 60);                                   // black picture with sync on the channel
    TEST_ASSERT_TRUE(rjContent(s, 256) < RgbScartJudge::kYMin);
    line(s, 256, 60, 200);                                  // a bright picture
    TEST_ASSERT_TRUE(rjContent(s, 256) >= RgbScartJudge::kYMin);
}

// Thin bright parts on black (the text screens of the 240p Test Suite on a Mega Drive): of 256 samples of the G wire
// 10 to 13 read 190 and more and the rest 0, so p95 - p15 says 1 and the wires looked flat. Measured next to it:
// empty wires beside a bright composite picture on pin 20 (N64) reach 16 at most, a black screen reads 0 everywhere
void test_content_sees_thin_bright_parts(void) {
    uint16_t s[256];
    for (uint16_t i = 0; i < 256; i++) s[i] = i % 25 == 0 ? 196 : 0;        // G: 11 bright samples
    TEST_ASSERT_TRUE(rjContent(s, 256) >= RgbScartJudge::kRgbMin);
    for (uint16_t i = 0; i < 256; i++) s[i] = i % 64 == 0 ? 198 : 0;        // B: 4
    TEST_ASSERT_TRUE(rjContent(s, 256) >= RgbScartJudge::kRgbMin);
    for (uint16_t i = 0; i < 256; i++) s[i] = i % 128 == 0 ? 198 : 0;       // 2: not told from a glitch
    TEST_ASSERT_TRUE(rjContent(s, 256) < RgbScartJudge::kWiresFlatMax);
    for (uint16_t i = 0; i < 256; i++) s[i] = i == 7 ? 16 : i % 60 == 0 ? 10 : i % 40 == 0 ? 6 : 0;   // empty wires
    TEST_ASSERT_TRUE(rjContent(s, 256) < RgbScartJudge::kWiresFlatMax);
    for (uint16_t i = 0; i < 256; i++) s[i] = 0;                            // a black screen
    TEST_ASSERT_EQUAL_UINT16(0, rjContent(s, 256));
    // the composite picture of a black screen, read through the decoder: p15 17, p50 21, p99 25, max 31
    for (uint16_t i = 0; i < 256; i++) s[i] = i < 2 ? 31 : i < 12 ? 25 : i < 128 ? 21 : 17;
    TEST_ASSERT_TRUE(rjContent(s, 256) < RgbScartJudge::kPeekYMin);
}

void test_content_order_does_not_matter(void) {
    uint16_t a[256], b[256];
    line(a, 256, 50, 180);
    for (uint16_t i = 0; i < 256; i++) b[i] = a[(i * 97) % 256];   // the same samples, shuffled
    TEST_ASSERT_EQUAL_UINT16(rjContent(a, 256), rjContent(b, 256));
}

// Raw test-bus words measured on the device: bit 15 toggles by itself, the level is 8 bits in bits 14..7.
// Floating R/G/B (composite-only cable) and a black RGB screen read B as 0x4040 / 0xC0C0: one step of noise, which
// the 15-bit spread counted as 128 and judged "RGB picture"; a Sonic 2 screen reaches full scale
void test_bus_level_is_eight_bits(void) {
    TEST_ASSERT_EQUAL_UINT16(128, rjBusLevel(0x4040));
    TEST_ASSERT_EQUAL_UINT16(129, rjBusLevel(0xC0C0));      // bit 15 dropped, bit 7 is below the level
    TEST_ASSERT_EQUAL_UINT16(0, rjBusLevel(0x8000));
    TEST_ASSERT_EQUAL_UINT16(255, rjBusLevel(0xFFFF));
    uint16_t flat[256], game[256];
    for (uint16_t i = 0; i < 256; i++) {
        flat[i] = rjBusLevel(i & 1 ? 0xC0C0 : 0x4040);      // black / floating B
        game[i] = rjBusLevel(i < 40 ? 0x0008 : i < 128 ? 0x1500 : 0x7F9C);   // MD game R: p15 8, p50 5376, p95 32668
    }
    TEST_ASSERT_TRUE(rjContent(flat, 256) < RgbScartJudge::kRgbMin);
    TEST_ASSERT_TRUE(rjContent(game, 256) >= RgbScartJudge::kRgbMin);
}

void test_judge_r_only_is_not_rgb(void) {
    uint16_t p[3] = {RgbScartJudge::kRgbMin + 10, 0, 0};    // S-Video over SCART: chroma on pin 15
    TEST_ASSERT_FALSE(rjRgbContent(p));
    uint16_t q[3] = {0, RgbScartJudge::kRgbMin, 0};
    TEST_ASSERT_TRUE(rjRgbContent(q));
}

// The wires right after a look at the composite picture
void test_judge_wires_after_a_look(void) {
    uint16_t n64[3] = {0, 0, 0};                             // composite only, a picture on pin 20
    TEST_ASSERT_EQUAL_UINT8(RW_FLAT, rjWiresAfterLook(n64));
    uint16_t noise[3] = {7, 7, 7};
    TEST_ASSERT_EQUAL_UINT8(RW_FLAT, rjWiresAfterLook(noise));
    uint16_t white[3] = {0, 129, 0};                         // Mega Drive, a white first screen: blue uniform
    TEST_ASSERT_EQUAL_UINT8(RW_BLUE, rjWiresAfterLook(white));  // far above what pin 20 couples into G (16 at most)
    uint16_t text[3] = {58, 196, 0};                         // thin text on black: G alone
    TEST_ASSERT_EQUAL_UINT8(RW_BLUE, rjWiresAfterLook(text));
    TEST_ASSERT_TRUE(rjPictureContentListening(text));
    uint16_t some[3] = {0, 30, 0};                           // G over kRgbMin, not clear of the coupling: no verdict
    TEST_ASSERT_EQUAL_UINT8(RW_CONTENT, rjWiresAfterLook(some));
    TEST_ASSERT_FALSE(rjPictureContentListening(some));
    uint16_t coupled[3] = {6, 16, 4};                        // empty wires beside a bright composite picture (N64)
    TEST_ASSERT_EQUAL_UINT8(RW_FAINT, rjWiresAfterLook(coupled));
    TEST_ASSERT_FALSE(rjPictureContentListening(coupled));
    uint16_t snes[3] = {0, 14, 63};
    TEST_ASSERT_EQUAL_UINT8(RW_BLUE, rjWiresAfterLook(snes));
    uint16_t faint[3] = {0, RgbScartJudge::kWiresFlatMax, 0};
    TEST_ASSERT_EQUAL_UINT8(RW_FAINT, rjWiresAfterLook(faint));
    uint16_t chroma[3] = {RgbScartJudge::kRgbMin + 10, 0, 0};   // R alone: S-Video over SCART
    TEST_ASSERT_EQUAL_UINT8(RW_FLAT, rjWiresAfterLook(chroma));
    uint16_t dim[3] = {0, 0, RgbScartJudge::kWiresFlatMax};
    TEST_ASSERT_EQUAL_UINT8(RW_FAINT, rjWiresAfterLook(dim));
}

// The "try Composite" hint asks whether the screen is black, not whether it is a rich picture: a dim one (the
// 240p Test Suite menu reads 22/23/27 before a preset, a little less while it runs) is a picture
void test_hint_takes_a_dim_picture_for_a_picture(void) {
    uint16_t black[3] = {0, 1, 0};
    TEST_ASSERT_FALSE(rjHintSeesPicture(black));
    uint16_t dim[3] = {20, 21, 23};                          // under kRgbMin on every channel
    TEST_ASSERT_FALSE(rjRgbContent(dim));
    TEST_ASSERT_TRUE(rjHintSeesPicture(dim));
    uint16_t edge[3] = {0, RgbScartJudge::kWiresFlatMax, 0};
    TEST_ASSERT_TRUE(rjHintSeesPicture(edge));
    uint16_t chroma[3] = {RgbScartJudge::kRgbMin + 10, 0, 0};   // R alone: S-Video over SCART, Composite is the advice
    TEST_ASSERT_FALSE(rjHintSeesPicture(chroma));
}

void test_judge_samples_only_a_scaled_path(void) {
    TEST_ASSERT_FALSE(rjPathMeasurable(15, false));          // RGBHV bypass (first step of RGB + CSync + VSync)
    TEST_ASSERT_TRUE(rjPathMeasurable(14, false));           // RGBHV scaled
    TEST_ASSERT_TRUE(rjPathMeasurable(1, false));            // 240p/480i scaled
    TEST_ASSERT_FALSE(rjPathMeasurable(1, true));            // HD bypass
    TEST_ASSERT_FALSE(rjPathMeasurable(0, false));           // no mode yet
}

// Undecided with R/G/B still black after 20 s (sampled in place every 5 s, no path switch): one hint "try
// Composite" for a composite-only cable; a picture appearing on RGB ends it without a hint
void test_hint_after_20s_of_black_rgb(void) {
    RjHint h;
    h.arm(1000);
    TEST_ASSERT_FALSE(h.sampleDue(5999));
    TEST_ASSERT_TRUE(h.sampleDue(6000));
    TEST_ASSERT_EQUAL_UINT8(RH_WAIT, h.sampled(6000, false));
    TEST_ASSERT_EQUAL_UINT8(RH_WAIT, h.sampled(11000, false));
    TEST_ASSERT_EQUAL_UINT8(RH_WAIT, h.sampled(16000, false));
    TEST_ASSERT_EQUAL_UINT8(RH_SHOW, h.sampled(21000, false));
    TEST_ASSERT_FALSE(h.sampleDue(60000));             // once
}

void test_hint_not_for_an_rgb_picture(void) {
    RjHint h;
    h.arm(0);
    TEST_ASSERT_EQUAL_UINT8(RH_WAIT, h.sampled(5000, false));
    TEST_ASSERT_EQUAL_UINT8(RH_DONE, h.sampled(10000, true));   // the logo came on RGB: right input, no hint
    TEST_ASSERT_FALSE(h.sampleDue(20000));
}

void test_hint_idle_until_armed(void) {
    RjHint h;
    TEST_ASSERT_FALSE(h.sampleDue(100000));
    h.arm(0);
    h.reset();
    TEST_ASSERT_FALSE(h.sampleDue(100000));
}

// One hint per acquisition: a second arm after the toast shows nothing; a new acquisition allows one again
void test_hint_once_per_acquisition(void) {
    RjHint h;
    h.arm(0);
    for (uint32_t t = 5000; t <= 20000; t += 5000) h.sampled(t, false);
    h.arm(30000);                                      // armed again in the same acquisition
    TEST_ASSERT_FALSE(h.sampleDue(60000));             // already shown in this acquisition
    h.reset();                                         // a new acquisition
    h.arm(70000);
    TEST_ASSERT_TRUE(h.sampleDue(75000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_content_ignores_sync_and_blanking);
    RUN_TEST(test_content_sees_thin_bright_parts);
    RUN_TEST(test_content_order_does_not_matter);
    RUN_TEST(test_bus_level_is_eight_bits);
    RUN_TEST(test_judge_r_only_is_not_rgb);
    RUN_TEST(test_judge_samples_only_a_scaled_path);
    RUN_TEST(test_hint_after_20s_of_black_rgb);
    RUN_TEST(test_judge_wires_after_a_look);
    RUN_TEST(test_hint_takes_a_dim_picture_for_a_picture);
    RUN_TEST(test_hint_not_for_an_rgb_picture);
    RUN_TEST(test_hint_idle_until_armed);
    RUN_TEST(test_hint_once_per_acquisition);
    return UNITY_END();
}
