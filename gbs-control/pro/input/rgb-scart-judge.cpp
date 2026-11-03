#include <algorithm>
#include "rgb-scart-judge.h"

uint16_t rjContent(uint16_t* s, uint16_t n) {
    if (n < 8) return 0;
    std::sort(s, s + n);
    uint16_t hi = s[(uint32_t)n * 95 / 100], lo = s[(uint32_t)n * 15 / 100];
    return hi > lo ? (uint16_t)(hi - lo) : 0;
}

uint16_t rjBusLevel(uint16_t raw) {
    return (uint16_t)((raw & 0x7fff) >> 7);
}

bool rjRgbContent(const uint16_t content[3]) {
    // G or B decide: R alone is the chroma of S-Video over SCART (pin 15), not an RGB picture
    return content[1] >= RgbScartJudge::kRgbMin || content[2] >= RgbScartJudge::kRgbMin;
}

bool rjPictureContentListening(const uint16_t content[3]) {
    return content[2] >= RgbScartJudge::kRgbMin;
}

bool rjPathMeasurable(uint8_t videoStandardInput, bool hdBypass) {
    return videoStandardInput != 0 && videoStandardInput != 15 && !hdBypass;
}
