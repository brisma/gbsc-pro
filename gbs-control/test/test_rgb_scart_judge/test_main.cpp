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
    RUN_TEST(test_content_order_does_not_matter);
    RUN_TEST(test_bus_level_is_eight_bits);
    RUN_TEST(test_judge_r_only_is_not_rgb);
    RUN_TEST(test_judge_samples_only_a_scaled_path);
    RUN_TEST(test_hint_after_20s_of_black_rgb);
    RUN_TEST(test_hint_not_for_an_rgb_picture);
    RUN_TEST(test_hint_idle_until_armed);
    RUN_TEST(test_hint_once_per_acquisition);
    return UNITY_END();
}
