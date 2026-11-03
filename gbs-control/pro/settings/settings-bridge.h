// Settings whose web writes reuse a legacy command handler, which does the targeted register writes for them.
#pragma once
#include <stdint.h>
#include "settings-types.h"

enum : uint8_t { BRIDGE_SERIAL = 0, BRIDGE_USER = 1 };   // serialCommandBuffer / userCommandBuffer
struct SettingsBridgeHit { uint8_t queue; char ch; };

// Toggle entries ("*c") fire only when target != current (the legacy handler flips the value).
// Value maps ("v:c,v:c") fire for a mapped target, even when unchanged (re-selecting re-applies).
bool settingsBridgeLookup(const SettingDef& d, uint16_t current, uint16_t target, SettingsBridgeHit& hit);

// Queued legacy chars run after the drain (one per loop tick): remember the value they will produce so a
// second write before they ran compares against it, not against the stale uopt value
class BridgePending {
public:
    static const uint8_t kSlots = 4;
    static const uint32_t kTimeoutMs = 3000;   // a char that never ran stops counting after this
    uint16_t effective(uint8_t index, uint16_t current, uint32_t now);
    void set(uint8_t index, uint16_t target, uint32_t now);
    // Pending target for index (not applied yet, not expired) without clearing anything
    bool peek(uint8_t index, uint16_t current, uint32_t now, uint16_t& target) const;
private:
    struct Slot { bool used; uint8_t index; uint16_t target; uint32_t at; };
    Slot s_[kSlots] = {};
};
