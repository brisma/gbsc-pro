// Rotary encoder decoding: edges arrive from the pin ISRs, events are decided in the loop.
// Pure (no Arduino calls): the device glue reads the pins and passes their levels.
#pragma once
#include <stdint.h>

#ifdef ARDUINO
#include <c_types.h>
#define ENC_ISR IRAM_ATTR
#else
#define ENC_ISR
#endif

// Quadrature decoding by quarter steps: bounces cancel out, a step is emitted when the encoder is back
// at its rest position (the pin state seen at attach), so a lost edge realigns at the next detent. No time filter.
// quartersPerStep: 4 = one step per full cycle (default), 2 = two steps per cycle, 1 = every edge.
class EncoderRotary {
public:
    int8_t ENC_ISR edge(uint8_t ab, uint32_t nowMs);   // ab = clk << 1 | data; returns -1 (up), +1 (down), 0
    void seed(uint8_t ab);                             // pin state at rest (attach)
    void configure(bool reversed, uint8_t quartersPerStep);   // cheap: no change unless the values differ
private:
    volatile uint8_t state_ = 3, rest_ = 3, quarters_ = 4;
    volatile int8_t acc_ = 0;
    volatile bool reversed_ = false;
};

// Push button: the ISR stamps edges and counts clean presses; poll() (loop) debounces the level and turns
// it into K_CLICK (released before 600 ms) or K_LONG (once, at 600 ms held, or on a late release)
class EncoderButton {
public:
    void ENC_ISR edge(uint32_t nowMs, bool down);      // pin ISR, CHANGE
    int poll(uint32_t nowMs, bool downNow);             // loop; returns K_CLICK, K_LONG or -1
    static const uint32_t kDebounceMs = 30, kLongMs = 600;
private:
    volatile uint32_t lastEdge_ = 0, pressAt_ = 0;
    volatile uint8_t presses_ = 0;
    uint8_t seen_ = 0;
    bool held_ = false, longFired_ = false;
    uint32_t downAt_ = 0;
};
