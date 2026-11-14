// Frame lock runs that fail in a row (pure, host-tested): kFrameLockFailsAllowed are let through, the next one
// resets the frame lock and the count starts again. A post-decrement test (`left-- == 0`) wrapped 0 to 255, so after
// the first reset the next one came 255 failures later
#pragma once
#include <stdint.h>

static const uint8_t kFrameLockFailsAllowed = 16;

// One failed run; true = reset the frame lock now
inline bool frameLockFailed(uint8_t& left) {
    if (left > 0) {
        left--;
        return false;
    }
    left = kFrameLockFailsAllowed;
    return true;
}
