// 2X (ADV7280 I2P) inhibit, pure: a progressive 50 Hz source with fields of 313 lines (PAL Mega Drive) doubles to
// 626-line frames; the ADV7391 625p counter cuts them, no vertical sync reaches the TV5725 and it never locks.
// 312-line fields (N64 PAL, 624 lines) and 240p (524 lines) work. When the decoder is locked on such a source and the
// TV5725 shows nothing for kEngageMs, I2P goes off (the TV5725 doubles the 288p itself); a new source,
// another input or a 2X change tries I2P again.
#pragma once
#include <stdint.h>

struct I2pGuardIn {
    bool i2p;           // the 2X setting
    uint8_t input;      // active input
    bool decoder;       // S-Video or Composite
    bool locked;        // decoder locked, not free-run
    bool progressive;   // decoder: fields not interlaced
    bool hz50;          // decoder: 50 Hz
    bool shown;         // the TV5725 shows a source
};

class I2pGuard {
public:
    static const uint32_t kEngageMs = 3500;   // a working 576p shows ~1-2 s after the decoder locks
    bool update(const I2pGuardIn& in, uint32_t nowMs);   // true: send I2P off
    bool waiting() const { return waiting_ && !inhibit_; }   // console log: counting down to I2P off
private:
    bool inhibit_ = false, waiting_ = false;
    uint8_t input_ = 0;
    uint32_t since_ = 0;
};
