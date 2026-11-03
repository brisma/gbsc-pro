// Effective value of a setting while writes are in flight (pure): the newest queued write, else the value a
// queued legacy command will produce (bridge), else nothing
#pragma once
#include <stdint.h>
#include "../settings/settings-request.h"
#include "../settings/settings-bridge.h"

template <typename Queue>
bool pendingValueOf(const Queue& q, const BridgePending& b, uint8_t index, uint16_t current, uint32_t now, uint16_t& out) {
    for (int i = (int)q.size() - 1; i >= 0; i--) {
        const SettingsPair& p = q.at((uint8_t)i);
        if (p.index == index) { out = p.value; return true; }
    }
    return b.peek(index, current, now, out);
}
