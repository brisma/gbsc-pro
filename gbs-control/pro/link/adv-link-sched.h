// When the ESP sends the ADV state to the ADV Controller (link v3): pure, the caller builds the states
#pragma once
#include <stdint.h>
#include "adv-link.h"

class AdvLinkSched {
public:
    // Every loop pass with the state built from uopt; true when a STATE frame must go out now
    bool tick(const AdvState& s, uint32_t nowMs);
    void kick() { kicked_ = true; }               // send at the next tick (input switch)
    void kickResend() { kicked_ = true; resendQuiet_ = true; }   // resend after an INFO hash mismatch
    // false for kInputQuietMs after a STATE whose plan re-initialises the input, kDecoderQuietMs when the decoder is
    // on after it (S-Video, Composite, RGB SCART listening: the whole decoder init, nothing answered for ~750 ms)
    bool requestAllowed(uint32_t nowMs) const;
    uint8_t nextSeq() { return seq_++; }
    static const uint32_t kCoalesceMs = 20, kMaxLatencyMs = 100, kKeepAliveMs = 5000, kInputQuietMs = 400,
                          kDecoderQuietMs = 1000;

private:
    AdvState last_ = {};
    bool sent_ = false, pending_ = false, kicked_ = false, quiet_ = false, resendQuiet_ = false;
    uint32_t lastSend_ = 0, firstChange_ = 0, lastChange_ = 0, inputSentAt_ = 0, quietMs_ = kInputQuietMs;
    AdvState candidate_ = {};
    uint8_t seq_ = 0;
};
