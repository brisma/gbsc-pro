// Scrolling text for texts longer than their space: pure, shared by the OSD (units = character cells) and the
// OLED (units = pixels).
#pragma once
#include <stdint.h>

struct MarqueeTiming { uint16_t holdMs, stepMs; };   // pause at both ends, time per step

// Scroll state of one text: a hash tells when the text changed (no copy of it in RAM)
struct MarqueeState { uint32_t hash; uint32_t at; };
inline uint32_t marqueeHash(const char* s) {                 // FNV-1a
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}
// Milliseconds since the text appeared (restarts when it changes)
inline uint32_t marqueeSince(MarqueeState& st, const char* text, uint32_t now) {
    uint32_t h = marqueeHash(text);
    if (h != st.hash) { st.hash = h; st.at = now; }
    return now - st.at;
}

// Steps from the start to the end position (0 when the text fits)
inline uint16_t marqueeSteps(uint16_t textLen, uint16_t visible, uint8_t step = 1) {
    if (textLen <= visible || !step) return 0;
    return (uint16_t)((textLen - visible + step - 1) / step);
}

// One cycle: start hold, the steps, end hold (0 when the text fits)
inline uint32_t marqueeCycleMs(uint16_t textLen, uint16_t visible, const MarqueeTiming& t, uint8_t step = 1) {
    uint16_t n = marqueeSteps(textLen, visible, step);
    return n ? 2UL * t.holdMs + (uint32_t)n * t.stepMs : 0;
}

// Offset of the first visible unit elapsedMs after the text appeared; the cycle repeats
inline uint16_t marqueeOffset(uint16_t textLen, uint16_t visible, uint32_t elapsedMs, const MarqueeTiming& t,
                              uint8_t step = 1) {
    uint16_t n = marqueeSteps(textLen, visible, step);
    if (!n || !t.stepMs) return 0;
    uint32_t c = elapsedMs % marqueeCycleMs(textLen, visible, t, step);
    if (c < t.holdMs) return 0;
    uint32_t k = (c - t.holdMs) / t.stepMs + 1;
    if (k > n) k = n;
    uint32_t off = k * step, max = (uint32_t)(textLen - visible);
    return (uint16_t)(off > max ? max : off);
}
