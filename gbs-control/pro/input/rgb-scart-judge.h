// RGB SCART content helpers: pure. CVBS on SCART pin 20 (or in the AV jack, the same net) also feeds the LM1881, so the RGB
// SCART path shows a stable sync even with a composite-only cable; input-identify.* decides from these contents and
// the G level under listen before any preset.
#pragma once
#include <stdint.h>

struct RgbScartJudge {
    // 8-bit levels (rjBusLevel). Measured: black RGB and floating R/G/B 0-1, a Sonic 2 screen 94-255 per
    // channel, N64 PAL composite Y 61-76
    static const uint16_t kRgbMin = 24, kYMin = 24;
};

// One TV5725 test-bus word (0_2E low, 0_2F high) as an 8-bit level: bit 15 toggles by itself and the level sits in
// bits 14..7 (full scale 0x7FFF = 255); the bits below it are not picture
uint16_t rjBusLevel(uint16_t raw);

// Content of one channel: 95th minus 15th percentile of the samples (the sync tip and blanking fall below the 15th,
// so a black picture reads about 0 whatever the clamp). Sorts `s` in place.
uint16_t rjContent(uint16_t* s, uint16_t n);
// R, G, B contents: any channel with content, except R alone (chroma of S-Video over SCART lands on pin 15)
bool rjRgbContent(const uint16_t content[3]);
// The same while the decoder listens on pin 20: B alone decides (G takes what the termination couples in from pin 20,
// R the chroma of S-Video over SCART). A scene of bright parts on black has content and a median level of 0.
bool rjPictureContentListening(const uint16_t content[3]);
// The test bus carries the picture only on a scaled path: RGBHV bypass (mode 15, the first step when the LM1881
// gives CSync and VSync) and HD bypass read flat, and mode 0 has no preset yet
bool rjPathMeasurable(uint8_t videoStandardInput, bool hdBypass);
// An RGB SCART fallback verdict of the identification (no lock, decoder silent, ambiguous level, timeout) on a black
// picture: sampled in place every kStepMs; still black after kShowMs gives one "try Composite" hint, a picture on RGB
// ends it silently
enum : uint8_t { RH_WAIT, RH_SHOW, RH_DONE };
struct RjHint {
    static const uint32_t kStepMs = 5000, kShowMs = 20000;
    void arm(uint32_t now) { if (shown_) return; armed_ = true; at_ = now; next_ = now + kStepMs; }   // once per acquisition
    void reset() { armed_ = false; shown_ = false; }    // a new acquisition
    bool sampleDue(uint32_t now) const { return armed_ && (int32_t)(now - next_) >= 0; }
    uint8_t sampled(uint32_t now, bool rgbContent) {
        if (!armed_) return RH_DONE;
        next_ = now + kStepMs;
        if (rgbContent) { armed_ = false; return RH_DONE; }
        if (now - at_ >= kShowMs) { armed_ = false; shown_ = true; return RH_SHOW; }
        return RH_WAIT;
    }
private:
    bool armed_ = false, shown_ = false;
    uint32_t at_ = 0, next_ = 0;
};

