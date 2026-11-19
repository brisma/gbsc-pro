// RGB source levels maths, no register access (host-tested; the job around it is rgb-levels.cpp)
//
// The console shows a full black, then a full white screen; the scaler's input-stage Y is read on its test bus
// (TEST_BUS_SEL 0x00 + IF_TEST_SEL 8: 0x2F = Y). The three ADC offsets move together until black reads Y 16, the three
// gains together until white reads Y 235. Register senses: ADC offset +1 step = -1 code; ADC gain (ADC_xGCTRL) higher
// = less gain.
#pragma once
#include <stdint.h>

// The most common Y of a look (counts per value, blanking at 0 already left out), refined to the mean of +-2 codes.
// 0: fewer than 30 samples above the blanking level.
float rgbLevelsScreenY(const uint8_t* counts, uint16_t used);

enum RgbLevelsVerdict : uint8_t {
    RL_CONTINUE,       // the working value was moved: measure again
    RL_DONE,           // within tolerance: save it
    RL_BAD_PICTURE,    // not the expected screen
    RL_OUT_OF_RANGE,   // the correction would leave the registers' useful range
    RL_NO_CONVERGE,    // too many rounds
};

static const uint8_t RL_MAX_ROUNDS = 12;
static const int16_t RL_MAX_OFFSET = 60;   // input.rgb_black's range: 128 +- 60

// Black screen: y measured with offset steps off (y <= 0: nothing above the blanking level). RL_CONTINUE moves off.
RgbLevelsVerdict rgbLevelsBlackStep(float y, uint8_t round, int16_t& off);

// White screen: gain is the register in use, lastGain / lastY the previous round's (lastGain < 0: none yet).
// Clipped (y >= 254.5): half the way to 0xF0 less gain, at least 16 steps; else a secant step on the last two readings, or ~0.5 code per step.
struct RgbLevelsWhite {
    int16_t gain;
    int16_t lastGain;
    float lastY;
};
RgbLevelsVerdict rgbLevelsWhiteStep(float y, uint8_t round, RgbLevelsWhite& s);

// RGBs / RGsB: the inputs the RGB source levels apply to
bool rgbLevelsInput(uint8_t inputType);

// input.rgb_black holds signed offset steps as U8 with 128 = none (as picture.red/green/blue)
inline int16_t rgbLevelsStepsFromSetting(uint8_t v) { return (int16_t)v - 128; }
uint8_t rgbLevelsSettingFromSteps(int16_t steps);
