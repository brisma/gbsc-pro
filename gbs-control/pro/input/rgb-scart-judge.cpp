#include <algorithm>
#include "rgb-scart-judge.h"

uint16_t rjContent(uint16_t* s, uint16_t n) {
    if (n < 8) return 0;
    std::sort(s, s + n);
    uint16_t hi = s[(uint32_t)n * 95 / 100], lo = s[(uint32_t)n * 15 / 100];
    uint16_t range = hi > lo ? (uint16_t)(hi - lo) : 0;
    // thin bright parts on a dark picture are fewer than 5 % of the samples: the third highest stands for them
    // (one or two could be a glitch), when it rises clearly above the median
    uint16_t top = s[n - 3], mid = s[n / 2];
    uint16_t peak = top > mid ? (uint16_t)(top - mid) : 0;
    return peak >= RgbScartJudge::kSparkStep && peak > range ? peak : range;
}

uint16_t rjBusLevel(uint16_t raw) {
    return (uint16_t)((raw & 0x7fff) >> 7);
}

bool rjRgbContent(const uint16_t content[3]) {
    // G or B decide: R alone is the chroma of S-Video over SCART (pin 15), not an RGB picture
    return content[1] >= RgbScartJudge::kRgbMin || content[2] >= RgbScartJudge::kRgbMin;
}

bool rjPictureContentListening(const uint16_t content[3]) {
    return content[2] >= RgbScartJudge::kRgbMin || content[1] >= RgbScartJudge::kGListenMin;
}

uint8_t rjWiresAfterLook(const uint16_t content[3]) {
    if (rjPictureContentListening(content)) return RW_BLUE;
    if (content[1] >= RgbScartJudge::kRgbMin) return RW_CONTENT;
    if (content[1] >= RgbScartJudge::kWiresFlatMax || content[2] >= RgbScartJudge::kWiresFlatMax) return RW_FAINT;
    return RW_FLAT;                                      // R is not asked: the chroma of S-Video over SCART
}

bool rjHintSeesPicture(const uint16_t content[3]) {
    return content[1] >= RgbScartJudge::kWiresFlatMax || content[2] >= RgbScartJudge::kWiresFlatMax;
}

bool rjPathMeasurable(uint8_t videoStandardInput, bool hdBypass) {
    return videoStandardInput != 0 && videoStandardInput != 15 && !hdBypass;
}
