#pragma once
#include <stdint.h>
#include "settings-types.h"

// Holds that keep the debounced save from running (independent reasons, flush only when none is set)
enum : uint8_t { SAVER_HOLD_UPDATE = 1, SAVER_HOLD_RESTORE = 2, SAVER_HOLD_MENU = 4 };

// Debounced save scheduler: due 2000 ms after the last change
class SettingsSaver {
public:
    void hold(uint8_t reason, bool on) { holds_ = on ? (uint8_t)(holds_ | reason) : (uint8_t)(holds_ & ~reason); }
    bool held() const { return holds_ != 0; }
    static const uint32_t kDelayMs = 2000;
    void markDirty(SettingScope s, uint32_t now) {
        mask_ |= (s == SettingScope::Device) ? 0x01 : 0x02;
        last_ = now;
    }
    bool dirty() const { return mask_ != 0; }
    bool pending() const { return mask_ && !holds_; }   // a save would write something now
    void hurry() { if (mask_) hurry_ = true; }           // the pending save runs at once, not at the end of the window
    bool due(uint32_t now) const { return mask_ && !holds_ && (hurry_ || (uint32_t)(now - last_) >= kDelayMs); }
    uint8_t take() { uint8_t m = mask_; mask_ = 0; hurry_ = false; return m; }
private:
    bool hurry_ = false;
    uint8_t mask_ = 0;
    uint8_t holds_ = 0;
    uint32_t last_ = 0;
};
