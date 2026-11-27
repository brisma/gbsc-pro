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

uint16_t syncPulseManualLimit(uint16_t manual, uint16_t dark, uint16_t now) {
    uint32_t limit = (uint32_t)manual + (now > dark ? now - dark : 0);
    if (limit < 2) limit = 2;
    return (uint16_t)(limit > kSyncIgnoreMax ? kSyncIgnoreMax : limit);
}

uint16_t syncPulseNoEqLimit(uint16_t dark, uint16_t now, int8_t trim) {
    if (now < dark) now = dark;
    int32_t limit = (int32_t)(dark / 2u) + trim;
    int32_t low = dark / 4u;
    if (limit < low) limit = low;
    limit += now - dark;
    uint16_t top = syncPulseLimit(dark, now);
    return (uint16_t)(limit > top ? top : limit);
}

static bool within(uint16_t a, uint16_t b, uint8_t percent) {
    uint32_t d = a > b ? a - b : b - a;
    return d * 100u <= (uint32_t)b * percent;
}

bool syncPulseReadsAgree(uint16_t first, uint16_t second) {
    return first && second && within(first, second, 2);
}

// Equalizing pulses show as half-width reads of one same width, again and again; a disturbance gives a stray read
// of some other width now and then. Seen: at once. Not seen: only after a whole window without them.
void SyncPulseTrack::countRead(uint16_t hsLow) {
    if (!dark_) return;
    uint32_t h100 = (uint32_t)hsLow * 100u;
    // a bright picture widens the equalizing pulses by as many counts as the normal ones: up to two thirds of the dark pulse
    bool half = h100 >= (uint32_t)dark_ * 44u && h100 <= (uint32_t)dark_ * 68u;
    bool normal = h100 >= (uint32_t)dark_ * 97u && h100 <= (uint32_t)dark_ * 112u;
    if (!half && !normal) return;                       // broad pulses, stray and mixed reads say nothing
    reads_++;
    if (half) {
        uint16_t d = hsLow > halfRef_ ? hsLow - halfRef_ : halfRef_ - hsLow;
        if (halfRef_ && d <= 3) { halves_++; halfRef_ = hsLow; }   // follows a picture that gets brighter
        else { halfRef_ = hsLow; halves_ = 1; }
    }
    if (halves_ >= kSyncEqSeen && eq_ != EQ_SEEN) {
        if (eq_ == EQ_NONE) last_ = 0;                  // another formula: the next limit is written whatever it is
        eq_ = EQ_SEEN; eqChanged_ = true;
    }
    if (reads_ >= kSyncEqWindow) {
        if (halves_ < kSyncEqSeen && eq_ != EQ_NONE) { eq_ = EQ_NONE; eqChanged_ = true; last_ = 0; lineReset(); }
        reads_ = halves_ = halfRef_ = 0;
    }
}

uint16_t SyncPulseTrack::lineCount(uint16_t lines) {
    if (!lines || !wantsLineCount()) return 0;
    if (!usual_) {                                      // the field's line count: the same value five fields in a row
        if (lines == lineCand_) { if (++lineRun_ >= 5) usual_ = lines; }
        else { lineCand_ = lines; lineRun_ = 1; }
        return 0;
    }
    lineReads_++;
    if (lines > usual_ && lines <= usual_ + 2) more_++;          // a line or two off: a split pulse. Further off: another
    else if (lines < usual_ && lines + 2 >= usual_) fewer_++;    // mode, a torn read, a lost sync: not this
    if (lineReads_ < kSyncLineWindow) return 0;
    int8_t step = 0;
    // most reads off: an interlaced source's alternating fields or a new mode, not an error to correct
    if (more_ + fewer_ <= kSyncLineWindow * 2u / 5u) {
        if (more_ >= kSyncLineErrors && more_ > fewer_ * 2u) step = 1;          // short pulses counted: raise
        else if (fewer_ >= kSyncLineErrors && fewer_ > more_ * 2u) step = -1;   // real pulses dropped: lower
    } else {
        usual_ = 0; lineRun_ = 0;                        // learn the count again
    }
    lineReads_ = more_ = fewer_ = 0;
    if (!step) return 0;
    int16_t t = (int16_t)trim_ + step;
    if (t > kSyncNoEqTrimMax || (int32_t)(dark_ / 2u) + t < (int32_t)(dark_ / 4u)) return 0;
    trim_ = (int8_t)t;
    uint16_t l = limit();
    if (l == last_) return 0;
    last_ = l;
    return l;
}

uint16_t SyncPulseTrack::update(uint16_t hsLow) {
    if (!hsLow) return 0;
    countRead(hsLow);
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
        lineReset();                                    // the trim stays: it is counted from half of the pulse
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
