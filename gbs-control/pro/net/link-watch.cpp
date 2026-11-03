#include "link-watch.h"

bool LinkWatch::due(uint32_t now, bool on, bool busy) const {
    return on && !busy && now - last_ >= (bad_ ? kBadPeriodMs : kPeriodMs);
}

uint8_t LinkWatch::result(const LinkCheck& c, uint32_t now) {
    last_ = now;
    bool bad = c.got < c.sent || c.worstMs > kBadMs;
    if (bad) {
        if (!bad_) { bad_ = true; badSince_ = now; }
        return LW_BAD;
    }
    if (!bad_) return LW_NONE;
    bad_ = false;
    badFor_ = now - badSince_;
    return LW_OK_AGAIN;
}
