#include "encoder-input.h"
#include "menu-types.h"

// Clockwise (00 -> 01 -> 11 -> 10 -> 00) is +1 (down): the Pro's direction; the standard board turns the opposite
// way, the glue passes `reversed` for it
static int8_t ENC_ISR quarter(uint8_t from, uint8_t to) {
    switch (from) {
        case 0b00: return to == 0b01 ? +1 : to == 0b10 ? -1 : 0;
        case 0b01: return to == 0b11 ? +1 : to == 0b00 ? -1 : 0;
        case 0b11: return to == 0b10 ? +1 : to == 0b01 ? -1 : 0;
        default:   return to == 0b00 ? +1 : to == 0b11 ? -1 : 0;   // 0b10
    }
}

void EncoderRotary::seed(uint8_t ab) { state_ = rest_ = ab & 3; acc_ = 0; }

void EncoderRotary::configure(bool reversed, uint8_t quartersPerStep) {
    if (quartersPerStep != 1 && quartersPerStep != 2) quartersPerStep = 4;
    if (reversed_ == reversed && quarters_ == quartersPerStep) return;
    reversed_ = reversed;
    quarters_ = quartersPerStep;
    acc_ = 0;
}

int8_t ENC_ISR EncoderRotary::edge(uint8_t ab, uint32_t) {
    ab &= 3;
    int8_t q = quarter(state_, ab);
    state_ = ab;
    if (!q) return 0;                                   // no move, or a two-bit jump (lost edge)
    acc_ = (int8_t)(acc_ + q);
    int8_t step = 0;
    if (quarters_ == 1) step = q;
    else if (quarters_ == 2) { if (ab == rest_ || ab == (rest_ ^ 3)) step = acc_ >= 1 ? 1 : acc_ <= -1 ? -1 : 0; else return 0; }
    else { if (ab == rest_) step = acc_ >= 2 ? 1 : acc_ <= -2 ? -1 : 0; else return 0; }
    acc_ = 0;
    return reversed_ ? (int8_t)-step : step;
}

void ENC_ISR EncoderButton::edge(uint32_t now, bool down) {
    if (down && now - lastEdge_ >= kDebounceMs) { pressAt_ = now; presses_++; }   // a press after a quiet pin
    lastEdge_ = now;
}

int EncoderButton::poll(uint32_t now, bool downNow) {
    int ev = -1;
    if (presses_ != seen_) {                            // a new clean press (the previous one ended unseen)
        seen_ = presses_;
        if (held_ && !longFired_) ev = K_CLICK;
        held_ = true;
        longFired_ = false;
        downAt_ = pressAt_;
        if (ev >= 0) return ev;
    }
    if (!held_) return -1;
    bool settled = now - lastEdge_ >= kDebounceMs;
    if (!downNow && settled) {                          // released
        held_ = false;
        if (longFired_) return -1;
        uint32_t len = lastEdge_ - downAt_;
        if (len < kDebounceMs) return -1;               // a tap shorter than the debounce is noise
        return len >= kLongMs ? K_LONG : K_CLICK;
    }
    if (downNow && !longFired_ && now - downAt_ >= kLongMs) { longFired_ = true; return K_LONG; }
    return -1;
}
