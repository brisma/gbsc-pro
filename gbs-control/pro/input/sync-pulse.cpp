#include "sync-pulse.h"

uint16_t syncPulseLimitTop(uint16_t dark) {
    uint32_t top = (uint32_t)dark * 41u / 64u;
    return (uint16_t)(top > kSyncIgnoreMax ? kSyncIgnoreMax : top);
}

uint16_t syncPulseLimit(uint16_t dark, uint16_t now, uint16_t keep) {
    if (now < dark) now = dark;
    uint32_t limit = dark / 2u + (now - dark) + kSyncLimitMargin;
    if (keep > limit) limit = keep;
    uint16_t top = syncPulseLimitTop(dark);
    return (uint16_t)(limit > top ? top : limit);
}

static bool within(uint16_t a, uint16_t b, uint8_t percent) {
    uint32_t d = a > b ? a - b : b - a;
    return d * 100u <= (uint32_t)b * percent;
}

uint16_t SyncPulseTrack::update(uint16_t hsLow) {
    if (!hsLow) return 0;
    uint16_t before = prev_;
    prev_ = hsLow;
    if (cand_ && within(hsLow, cand_, 4)) { if (run_ < 255) run_++; }
    else { cand_ = hsLow; run_ = 1; }

    bool newMode = false;
    if (!dark_) {
        newMode = run_ >= kSyncTrackRun;
    } else if (run_ >= kSyncTrackRun && !(cand_ * 100u >= dark_ * 97u && cand_ * 100u <= dark_ * 112u)) {
        // a sampling rhythm that keeps landing in the vertical interval reads its half or broad pulses in a row
        uint32_t c100 = (uint32_t)cand_ * 100u;
        bool half = c100 >= (uint32_t)dark_ * 44u && c100 <= (uint32_t)dark_ * 60u;
        newMode = !half && cand_ <= dark_ * 3u;
    }
    if (newMode) {
        dark_ = now_ = cand_;
        mode_ = true;
        // the limit of the mode before stays when it is inside this mode's window, else this mode's own
        uint16_t own = syncPulseLimit(dark_, dark_);
        if (keep_ < own || keep_ > syncPulseLimitTop(dark_)) keep_ = own;
    } else if (dark_ && before && within(hsLow, before, 2) && hsLow * 100u >= dark_ * 97u && hsLow * 100u <= dark_ * 112u) {
        now_ = hsLow;                                   // two samples agree: the picture got brighter or darker
        if (now_ < dark_) dark_ = now_;
    } else {
        return 0;
    }
    uint16_t l = limit();
    if (!newMode && (l > last_ ? l - last_ : last_ - l) < 2) return 0;
    last_ = l;
    return l;
}

bool SyncPulseTrack::otherMode(uint16_t hsLow) const {
    if (!dark_ || !hsLow) return false;
    uint32_t h100 = (uint32_t)hsLow * 100u;
    if (h100 < (uint32_t)dark_ * 62u || h100 > (uint32_t)dark_ * 160u) return false;   // half, broad, stray
    return h100 < (uint32_t)dark_ * 97u || h100 > (uint32_t)dark_ * 112u;
}
