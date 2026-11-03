// Picture colour maths: one pure function from settings to the six VDS colour registers
#include <unity.h>
#include "picture-color.h"

void setUp(void) {}
void tearDown(void) {}

static ColorIn neutral(bool yuv, bool component) {
    ColorIn in = {};
    in.yuvInput = yuv;
    in.componentOut = component;
    in.r = in.g = in.b = 128;
    in.brightness = in.contrast = in.saturation = 50;
    in.hdmiRange = 0;
    in.presetId = 0x01;
    return in;
}

static void expectRegs(const ColorRegs& r, uint8_t yg, uint8_t ug, uint8_t vg, int8_t yo, int8_t uo, int8_t vo) {
    TEST_ASSERT_EQUAL_HEX8(yg, r.yGain);
    TEST_ASSERT_EQUAL_HEX8(ug, r.uGain);
    TEST_ASSERT_EQUAL_HEX8(vg, r.vGain);
    TEST_ASSERT_EQUAL_INT8(yo, r.yOff);
    TEST_ASSERT_EQUAL_INT8(uo, r.uOff);
    TEST_ASSERT_EQUAL_INT8(vo, r.vOff);
}

// Today: the patches write the gains, then applyRGBtoYUVConversion() overwrites the three offsets from R/G/B
// (YUV inputs default to 129/123/132 -> -2/+3/+2 after truncation; the component offsets are overwritten too)
void test_defaults_match_today(void) {
    expectRegs(pictureColor(neutral(false, false)), 0x80, 0x1C, 0x29, 0, 0, 0);
    expectRegs(pictureColor(neutral(true, false)), 0x80, 0x1C, 0x29, -2, 3, 2);
    expectRegs(pictureColor(neutral(false, true)), 0x64, 0x19, 0x19, 0, 0, 0);
    expectRegs(pictureColor(neutral(true, true)), 0x64, 0x19, 0x19, -2, 3, 2);
}

void test_hdmi_last(void) {
    ColorIn in = neutral(false, false);
    in.hdmiRange = 1;                       // Auto: HD modes (720p, 1080p)
    in.presetId = 0x05;
    ColorRegs r = pictureColor(in);
    TEST_ASSERT_EQUAL_HEX8(0x80 * 110 / 128, r.yGain);
    TEST_ASSERT_EQUAL_INT8(16, r.yOff);
    in.presetId = 0x01;                     // 960p is not an HD mode
    TEST_ASSERT_EQUAL_HEX8(0x80, pictureColor(in).yGain);
    in.hdmiRange = 2;                       // SD and PC modes
    TEST_ASSERT_EQUAL_HEX8(110, pictureColor(in).yGain);
    in.hdmiRange = 3;                       // every mode
    in.presetId = 0x13;
    TEST_ASSERT_EQUAL_HEX8(110, pictureColor(in).yGain);
    in.hdBypass = true;
    TEST_ASSERT_EQUAL_HEX8(0x80, pictureColor(in).yGain);
    ColorIn y = neutral(true, false);       // the compression scales the existing offset, then lifts by 16
    y.hdmiRange = 3;
    TEST_ASSERT_EQUAL_INT8((-2 * 110) / 128 + 16, pictureColor(y).yOff);
}

void test_custom_preset_not_double_compressed(void) {
    ColorIn a = neutral(true, false), b = a;
    a.hdmiRange = b.hdmiRange = 3;
    b.customPreset = true;
    ColorRegs ra = pictureColor(a), rb = pictureColor(b);
    TEST_ASSERT_EQUAL_HEX8(ra.yGain, rb.yGain);
    TEST_ASSERT_EQUAL_INT8(ra.yOff, rb.yOff);
}

void test_brightness_contrast_saturation(void) {
    ColorIn in = neutral(false, false);
    in.brightness = 100;
    TEST_ASSERT_EQUAL_INT8(62, pictureColor(in).yOff);
    in.brightness = 0;
    TEST_ASSERT_EQUAL_INT8(-62, pictureColor(in).yOff);
    in.brightness = 50;
    in.contrast = 100;
    TEST_ASSERT_EQUAL_HEX8(0x80 + 50, pictureColor(in).yGain);
    in.contrast = 0;
    TEST_ASSERT_EQUAL_HEX8(0x80 - 50, pictureColor(in).yGain);
    in.contrast = 50;
    in.saturation = 100;
    TEST_ASSERT_EQUAL_HEX8(0x1C * 2, pictureColor(in).uGain);
    TEST_ASSERT_EQUAL_HEX8(0x29 * 2, pictureColor(in).vGain);
    in.saturation = 0;
    TEST_ASSERT_EQUAL_HEX8(0, pictureColor(in).uGain);
    in.saturation = 50;
    TEST_ASSERT_EQUAL_HEX8(0x1C, pictureColor(in).uGain);
    ColorIn y = neutral(true, false);       // offsets clamp to the signed register range
    y.r = 255; y.g = 255; y.b = 255; y.brightness = 100;
    TEST_ASSERT_EQUAL_INT8(127, pictureColor(y).yOff);
}

void test_rgb_matrix(void) {
    ColorIn in = neutral(false, false);
    in.r = 255;
    ColorRegs r = pictureColor(in);
    TEST_ASSERT_EQUAL_INT8((int)(0.299f * 127), r.yOff);
    TEST_ASSERT_EQUAL_INT8((int)(-0.14713f * 127), r.uOff);
    TEST_ASSERT_EQUAL_INT8((int)(0.615f * 127), r.vOff);
}

// Sharpness 0..31: 0 = peaking bypass, LB = value, LH scaled from the preset's own base
// (0x18, or 0x0A at 1080p, today's doPostPresetLoadSteps rule); the gains are 6-bit
void test_sharpness(void) {
    SharpRegs s = pictureSharpness(0, 0x01);
    TEST_ASSERT_TRUE(s.bypass);
    s = pictureSharpness(22, 0x01);
    TEST_ASSERT_FALSE(s.bypass);
    TEST_ASSERT_EQUAL_HEX8(22, s.lb);
    TEST_ASSERT_EQUAL_HEX8(0x18, s.lh);
    s = pictureSharpness(22, 0x05);
    TEST_ASSERT_EQUAL_HEX8(0x0A, s.lh);
    s = pictureSharpness(22, 0x15);
    TEST_ASSERT_EQUAL_HEX8(0x0A, s.lh);
    s = pictureSharpness(31, 0x01);
    TEST_ASSERT_EQUAL_HEX8(31, s.lb);
    TEST_ASSERT_EQUAL_HEX8(31, s.lh);                      // round(31*24/22) = 34, clamped to the 6-bit field
    s = pictureSharpness(31, 0x05);
    TEST_ASSERT_EQUAL_HEX8(14, s.lh);                      // round(31*10/22)
    s = pictureSharpness(40, 0x01);                        // out of range values clamp
    TEST_ASSERT_EQUAL_HEX8(31, s.lb);
}

// Input gain up/down step the ADC range register and stop at its ends (0 and 255 used to wrap to the other end,
// a jump from the brightest to the darkest picture; from warheat1990/gbsc-pro 13fb07c)
void test_adc_gain_step_stops_at_the_ends(void) {
    TEST_ASSERT_EQUAL_UINT8(0x7F, adcGainStep(0x80, -1));
    TEST_ASSERT_EQUAL_UINT8(0x81, adcGainStep(0x80, 1));
    TEST_ASSERT_EQUAL_UINT8(0, adcGainStep(0, -1));
    TEST_ASSERT_EQUAL_UINT8(255, adcGainStep(255, 1));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_match_today);
    RUN_TEST(test_hdmi_last);
    RUN_TEST(test_custom_preset_not_double_compressed);
    RUN_TEST(test_brightness_contrast_saturation);
    RUN_TEST(test_rgb_matrix);
    RUN_TEST(test_sharpness);
    RUN_TEST(test_adc_gain_step_stops_at_the_ends);
    return UNITY_END();
}
