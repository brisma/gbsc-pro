// Picture maths: settings -> TV5725 VDS colour and peaking registers, no register access
#pragma once
#include <stdint.h>

struct ColorIn {
    bool yuvInput;          // YPbPr, S-Video or Composite (the decoder path delivers YUV)
    bool componentOut;      // analog output in YPbPr
    bool customPreset;      // registers came from a profile dump (never read back: the bases are constants)
    uint8_t r, g, b;        // picture.red/green/blue, 128 = neutral
    uint8_t brightness, contrast, saturation;   // 0..100, 50 = neutral
    uint8_t hdmiRange;      // output.hdmi_range: 0 Off, 1 HD modes, 2 SD/PC modes, 3 all
    uint8_t presetId;       // rto->presetID of the running output mode
    bool hdBypass;          // HD bypass: no HDMI compression
};

struct ColorRegs {
    uint8_t yGain, uGain, vGain;
    int8_t yOff, uOff, vOff;
};

ColorRegs pictureColor(const ColorIn& in);

struct SharpRegs { bool bypass; uint8_t lb, lh; };   // VDS_PK_Y_H_BYPS, VDS_PK_LB_GAIN, VDS_PK_LH_GAIN

// picture.sharpness 0..31: 0 bypasses the peaking; LH follows LB scaled from the preset's base (1080p: 0x0A, else 0x18)
SharpRegs pictureSharpness(uint8_t value, uint8_t presetId);

// Input gain up/down: one step of the ADC range register (ADC_xGCTRL), held at 0 and 255 instead of wrapping
uint8_t adcGainStep(uint8_t gain, int8_t delta);
