// WiFi link watch (pure, host-tested): the device pings its gateway (kEchoes echoes, kGapMs apart) every kPeriodMs,
// every kBadPeriodMs while the link is bad, and logs a line only when a check is bad (an echo lost or later than
// kBadMs) and one when it is ok again, with how long it lasted (blackouts of about a minute were seen on 2.4 GHz)
#pragma once
#include <stdint.h>

enum : uint8_t { LW_NONE, LW_BAD, LW_OK_AGAIN };
struct LinkCheck { uint8_t sent, got; uint16_t worstMs; };

class LinkWatch {
public:
    static const uint32_t kPeriodMs = 60000, kBadPeriodMs = 10000;
    static const uint8_t kEchoes = 3;
    static const uint16_t kGapMs = 200, kWaitMs = 1000, kBadMs = 300;   // kWaitMs: after the last echo
    bool due(uint32_t now, bool on, bool busy) const;   // on: connected + Developer toggle; busy: update, OTA, scan, a tune burst
    uint8_t result(const LinkCheck& c, uint32_t now);          // LW_*: what to log
    uint32_t badForMs() const { return badFor_; }              // the bad stretch that LW_OK_AGAIN ended
private:
    uint32_t last_ = 0, badSince_ = 0, badFor_ = 0;
    bool bad_ = false;
};
