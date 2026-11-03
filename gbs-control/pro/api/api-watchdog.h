// Detects an upload whose client went away: no chunk and no finish for kTimeoutMs (pure, header-only)
#pragma once
#include <stdint.h>

struct UploadWatchdog {
    static const uint32_t kTimeoutMs = 30000;   // only a vanished client: a disturbed link stalls TCP >10 s
    bool active = false;
    uint32_t last = 0;
    void touch(uint32_t now) { active = true; last = now; }
    void stop() { active = false; }
    bool expired(uint32_t now) const { return active && (uint32_t)(now - last) >= kTimeoutMs; }
};
