// RGB source levels maths (rgb-levels-math.h): the screen reading, the black and white steps, the setting's encoding.
// The numbers are measurements from a GBSC PRO (2026-11-19): an N64 over its RGB cable read black as Y 2.6 and
// clipped white from level ~205.
#include <unity.h>
#include <string.h>
#include <math.h>
#include "rgb-levels-math.h"
#include "input-types.h"

void setUp(void) {}
void tearDown(void) {}

// ---- the screen reading ----

void test_screen_y_is_the_most_common_value(void) {
    uint8_t h[256];
    memset(h, 0, sizeof(h));
    h[16] = 60; h[17] = 60; h[200] = 10;   // a black screen, a little of the border
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 16.5f, rgbLevelsScreenY(h, 130));
}

void test_screen_y_needs_samples_above_blanking(void) {
    uint8_t h[256];
    memset(h, 0, sizeof(h));
    h[16] = 20;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rgbLevelsScreenY(h, 20));
}

// ---- RGB source levels ----

void test_rgb_black_from_the_n64(void) {
    int16_t off = 0;
    TEST_ASSERT_EQUAL(RL_CONTINUE, rgbLevelsBlackStep(2.6f, 1, off));
    TEST_ASSERT_EQUAL_INT16(-13, off);   // the value the board settled on
    TEST_ASSERT_EQUAL(RL_DONE, rgbLevelsBlackStep(15.4f + 0.3f, 2, off));
}

void test_rgb_black_below_the_blanking_level_raises_a_lot(void) {
    int16_t off = 0;
    TEST_ASSERT_EQUAL(RL_CONTINUE, rgbLevelsBlackStep(0.0f, 1, off));
    TEST_ASSERT_EQUAL_INT16(-8, off);
}

void test_rgb_black_rejects_a_bright_screen(void) {
    int16_t off = 0;
    TEST_ASSERT_EQUAL(RL_BAD_PICTURE, rgbLevelsBlackStep(120.0f, 1, off));
}

// The N64's white against the gain register: clipped at the preset's 0x7B, then ~0.53 code per step (measured)
static float n64White(int16_t g) {
    const float y = 243.8f - 0.53f * (float)(g - 234);
    return y > 255.0f ? 255.0f : y;
}

void test_rgb_white_converges_from_a_clipped_start(void) {
    RgbLevelsWhite s = {0x7B, -1, -1.0f};
    RgbLevelsVerdict v = RL_CONTINUE;
    uint8_t round = 0;
    while (v == RL_CONTINUE) v = rgbLevelsWhiteStep(n64White(s.gain), ++round, s);
    TEST_ASSERT_EQUAL(RL_DONE, v);
    TEST_ASSERT_INT16_WITHIN(2, 250, s.gain);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 235.0f, n64White(s.gain));
}

void test_rgb_white_rejects_a_dark_screen(void) {
    RgbLevelsWhite s = {0x7B, -1, -1.0f};
    TEST_ASSERT_EQUAL(RL_BAD_PICTURE, rgbLevelsWhiteStep(90.0f, 1, s));
}

void test_rgb_white_out_of_the_adc_range(void) {
    RgbLevelsWhite s = {250, 240, 255.0f};
    TEST_ASSERT_EQUAL(RL_OUT_OF_RANGE, rgbLevelsWhiteStep(255.0f, 3, s));   // clipped even at the end of the range
    TEST_ASSERT_EQUAL_INT16(255, s.gain);
}

void test_rgb_inputs(void) {
    TEST_ASSERT_TRUE(rgbLevelsInput(InputTypeRGBs));
    TEST_ASSERT_TRUE(rgbLevelsInput(InputTypeRGsB));
    TEST_ASSERT_FALSE(rgbLevelsInput(InputTypeVGA));
    TEST_ASSERT_FALSE(rgbLevelsInput(InputTypeYUV));
    TEST_ASSERT_FALSE(rgbLevelsInput(InputTypeAV));
}

void test_rgb_black_stays_in_the_setting_range(void) {
    int16_t off = -58;
    TEST_ASSERT_EQUAL(RL_OUT_OF_RANGE, rgbLevelsBlackStep(5.0f, 2, off));   // -69: past 128 - 60
}

void test_rgb_black_from_above(void) {
    int16_t off = 0;
    TEST_ASSERT_EQUAL(RL_CONTINUE, rgbLevelsBlackStep(30.0f, 1, off));   // a raised black: less offset
    TEST_ASSERT_EQUAL_INT16(14, off);
}

void test_rgb_black_refuses_a_dark_grey(void) {
    int16_t off = 0;
    TEST_ASSERT_EQUAL(RL_CONTINUE, rgbLevelsBlackStep(40.0f, 1, off));
    off = 0;
    TEST_ASSERT_EQUAL(RL_BAD_PICTURE, rgbLevelsBlackStep(41.0f, 1, off));
    TEST_ASSERT_EQUAL_INT16(0, off);
}

void test_rgb_black_gives_up_after_the_last_round(void) {
    int16_t off = 0;
    TEST_ASSERT_EQUAL(RL_NO_CONVERGE, rgbLevelsBlackStep(20.0f, RL_MAX_ROUNDS, off));
}

void test_rgb_white_secant_step(void) {
    RgbLevelsWhite s = {200, 190, 250.0f};   // 10 steps more gain register gave 10 codes less white
    TEST_ASSERT_EQUAL(RL_CONTINUE, rgbLevelsWhiteStep(240.0f, 2, s));
    TEST_ASSERT_EQUAL_INT16(205, s.gain);    // 5 codes to go at 1 code per step
}

void test_rgb_white_from_a_low_start_gain(void) {
    RgbLevelsWhite s = {0x30, -1, -1.0f};    // after auto gain or a profile: a hot source clips for long
    RgbLevelsVerdict v = RL_CONTINUE;
    uint8_t round = 0;
    while (v == RL_CONTINUE) v = rgbLevelsWhiteStep(n64White(s.gain), ++round, s);
    TEST_ASSERT_EQUAL(RL_DONE, v);
    TEST_ASSERT_INT16_WITHIN(2, 250, s.gain);
}

void test_rgb_white_gives_up_after_the_last_round(void) {
    RgbLevelsWhite s = {200, -1, -1.0f};
    TEST_ASSERT_EQUAL(RL_NO_CONVERGE, rgbLevelsWhiteStep(245.0f, RL_MAX_ROUNDS, s));
}

void test_screen_y_at_the_ends_of_the_range(void) {
    uint8_t h[256];
    memset(h, 0, sizeof(h));
    h[1] = 40; h[2] = 10;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2f, rgbLevelsScreenY(h, 50));   // blanking (0) is never counted
    memset(h, 0, sizeof(h));
    h[255] = 50; h[254] = 50;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 254.5f, rgbLevelsScreenY(h, 100));
}

void test_settings_encoding(void) {
    TEST_ASSERT_EQUAL_UINT8(128, rgbLevelsSettingFromSteps(0));
    TEST_ASSERT_EQUAL_UINT8(115, rgbLevelsSettingFromSteps(-13));
    TEST_ASSERT_EQUAL_INT16(-16, rgbLevelsStepsFromSetting(112));
    TEST_ASSERT_EQUAL_UINT8(0, rgbLevelsSettingFromSteps(-200));
    TEST_ASSERT_EQUAL_UINT8(255, rgbLevelsSettingFromSteps(200));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_screen_y_is_the_most_common_value);
    RUN_TEST(test_screen_y_needs_samples_above_blanking);
    RUN_TEST(test_rgb_black_from_the_n64);
    RUN_TEST(test_rgb_black_below_the_blanking_level_raises_a_lot);
    RUN_TEST(test_rgb_black_rejects_a_bright_screen);
    RUN_TEST(test_rgb_black_stays_in_the_setting_range);
    RUN_TEST(test_rgb_white_converges_from_a_clipped_start);
    RUN_TEST(test_rgb_white_rejects_a_dark_screen);
    RUN_TEST(test_rgb_white_out_of_the_adc_range);
    RUN_TEST(test_rgb_inputs);
    RUN_TEST(test_rgb_black_from_above);
    RUN_TEST(test_rgb_black_refuses_a_dark_grey);
    RUN_TEST(test_rgb_black_gives_up_after_the_last_round);
    RUN_TEST(test_rgb_white_secant_step);
    RUN_TEST(test_rgb_white_from_a_low_start_gain);
    RUN_TEST(test_rgb_white_gives_up_after_the_last_round);
    RUN_TEST(test_screen_y_at_the_ends_of_the_range);
    RUN_TEST(test_settings_encoding);
    return UNITY_END();
}
