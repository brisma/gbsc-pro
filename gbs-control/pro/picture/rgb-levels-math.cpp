#include "rgb-levels-math.h"
#include "../input/input-types.h"
#include <math.h>
#include <stdlib.h>

static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

float rgbLevelsScreenY(const uint8_t* counts, uint16_t used) {
    if (used < 30) return 0;
    int m = 1;
    for (int i = 2; i < 256; i++)
        if (counts[i] > counts[m]) m = i;
    uint32_t s = 0, w = 0;
    for (int j = m - 2; j <= m + 2; j++) {
        if (j < 1 || j > 255) continue;
        s += counts[j];
        w += (uint32_t)counts[j] * (uint32_t)j;
    }
    return s ? (float)w / (float)s : 0;
}

RgbLevelsVerdict rgbLevelsBlackStep(float y, uint8_t round, int16_t& off) {
    if (y > 0 && fabsf(y - 16) < 0.8f) return RL_DONE;
    if (y > 40) return RL_BAD_PICTURE;                       // not a black screen (a dark grey would crush black)
    if (round >= RL_MAX_ROUNDS) return RL_NO_CONVERGE;
    off += y <= 0 ? -8 : (int16_t)lroundf(y - 16);            // nothing above the blanking level: raise black a lot
    if (abs(off) > RL_MAX_OFFSET) return RL_OUT_OF_RANGE;
    return RL_CONTINUE;
}

RgbLevelsVerdict rgbLevelsWhiteStep(float y, uint8_t round, RgbLevelsWhite& s) {
    if (y < 180) return RL_BAD_PICTURE;                      // not a white screen
    if (y < 254.5f && fabsf(y - 235) < 1.0f) return RL_DONE;
    if (round >= RL_MAX_ROUNDS) return RL_NO_CONVERGE;
    int next;
    if (y >= 254.5f) {
        const int big = (0xF0 - s.gain) / 2;                  // clipped: less gain, a big step (half the way to 0xF0
        next = s.gain + (big > 16 ? big : 16);                // from a low gain, 16 near the top)
    } else if (s.lastGain >= 0 && s.lastY > 0 && s.lastY < 254.5f && s.lastGain != s.gain && fabsf(y - s.lastY) > 0.2f) {
        const float perStep = (y - s.lastY) / (float)(s.gain - s.lastGain);   // codes per step (negative)
        next = s.gain + (int)lroundf((235 - y) / perStep);
    } else {
        next = s.gain + (int)lroundf((y - 235) / 0.5f);        // ~0.5 code of white per step near the top
    }
    if (next == s.gain) next += y > 235 ? 1 : -1;
    s.lastY = y;
    s.lastGain = s.gain;
    s.gain = (int16_t)clampInt(next, 0, 255);
    if (round > 1 && (s.gain == 255 || s.gain == 0)) return RL_OUT_OF_RANGE;   // the source is outside the ADC's range
    return RL_CONTINUE;
}

bool rgbLevelsInput(uint8_t t) { return t == InputTypeRGBs || t == InputTypeRGsB; }

uint8_t rgbLevelsSettingFromSteps(int16_t steps) { return (uint8_t)clampInt(128 + steps, 0, 255); }
