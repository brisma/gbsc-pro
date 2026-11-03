#include "picture-color.h"

static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static bool presetIn(uint8_t id, const uint8_t* set, uint8_t n) {
    for (uint8_t i = 0; i < n; i++)
        if (set[i] == id) return true;
    return false;
}

uint8_t adcGainStep(uint8_t gain, int8_t delta) { return (uint8_t)clampInt(gain + delta, 0, 255); }

SharpRegs pictureSharpness(uint8_t value, uint8_t presetId) {
    SharpRegs s;
    if (value > 31) value = 31;
    s.bypass = value == 0;
    s.lb = value;
    int base = (presetId == 0x05 || presetId == 0x15) ? 0x0A : 0x18;
    s.lh = (uint8_t)clampInt((value * base + 11) / 22, 0, 31);   // 22 = the default LB: Normal keeps the preset's LH
    return s;
}

ColorRegs pictureColor(const ColorIn& in) {
    // Bases: the gains applyRGBPatches/applyYuvPatches/applyComponentColorMixing write, and the offsets the
    // R/G/B matrix leaves after them (YUV inputs: 129/123/132 -> -2/+3/+2). Never read from registers.
    int yGain = in.componentOut ? 0x64 : 0x80;
    int uGain = in.componentOut ? 0x19 : 0x1C;
    int vGain = in.componentOut ? 0x19 : 0x29;
    int yOff = in.yuvInput ? -2 : 0, uOff = in.yuvInput ? 3 : 0, vOff = in.yuvInput ? 2 : 0;

    // BT.601 matrix of the R/G/B offsets (picture.red/green/blue), truncated toward zero
    int r = in.r - 128, g = in.g - 128, b = in.b - 128;
    yOff += (int)(0.299f * r + 0.587f * g + 0.114f * b);
    uOff += (int)(-0.14713f * r - 0.28886f * g + 0.436f * b);
    vOff += (int)(0.615f * r - 0.51499f * g - 0.10001f * b);

    yOff += (in.brightness - 50) * 5 / 4;
    yGain += in.contrast - 50;
    uGain = (uGain * in.saturation + 25) / 50;
    vGain = (vGain * in.saturation + 25) / 50;

    yGain = clampInt(yGain, 0, 255);
    uGain = clampInt(uGain, 0, 255);
    vGain = clampInt(vGain, 0, 255);
    yOff = clampInt(yOff, -128, 127);
    uOff = clampInt(uOff, -128, 127);
    vOff = clampInt(vOff, -128, 127);

    // HDMI range last, on the computed values: pre-compress full range to 16-235 for the MS9288
    static const uint8_t kHd[] = {0x03, 0x13, 0x05, 0x15};
    static const uint8_t kSd[] = {0x01, 0x11, 0x02, 0x12, 0x07, 0x17, 0x04, 0x14};
    bool hd = presetIn(in.presetId, kHd, sizeof(kHd)), sd = presetIn(in.presetId, kSd, sizeof(kSd));
    bool compress = !in.hdBypass && ((in.hdmiRange == 1 && hd) || (in.hdmiRange == 2 && sd) || (in.hdmiRange == 3 && (hd || sd)));
    if (compress) {
        yGain = yGain * 110 / 128;
        yOff = clampInt(yOff * 110 / 128 + 16, -128, 127);
    }

    ColorRegs out;
    out.yGain = (uint8_t)yGain;
    out.uGain = (uint8_t)uGain;
    out.vGain = (uint8_t)vGain;
    out.yOff = (int8_t)yOff;
    out.uOff = (int8_t)uOff;
    out.vOff = (int8_t)vOff;
    return out;
}
