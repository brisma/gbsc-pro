#include <stdlib.h>
#include <string.h>
#include "profile-autoload.h"
#include "profile-modes.h"
#include "../input/input-types.h"

bool autoloadStageBusy(uint8_t stage) { return stage == 1 || stage == 10; }

void StableLatch::reset() {
    counting_ = false; fired_ = false; count_ = 0; oddCount_ = 0; parityRun_ = 0; parityRearmed_ = false;
}

bool StableLatch::parityDisagrees(const AutoloadSample& s) const {
    return fired_ && profileIsInterlaced(s.mode, s.lines) != interlaced(s.mode);
}

bool StableLatch::feed(const AutoloadSample& s) {
    parityRearmed_ = false;
    if (s.signal && s.stageBusy) return false;   // preset being applied: ignore
    if (!s.signal) { reset(); return false; }
    uint16_t lo = s.lines < minLines_ ? s.lines : minLines_;
    uint16_t hi = s.lines > maxLines_ ? s.lines : maxLines_;
    bool same = counting_ && s.input == anchor_.input && s.mode == anchor_.mode && hi - lo <= kLinesSpread;
    if (same && fired_) {                         // a clean 240p <-> 480i switch keeps the window: watch the parity
        parityRun_ = parityDisagrees(s) ? (uint8_t)(parityRun_ + 1) : 0;
        if (parityRun_ >= kParityRun) { same = false; parityRearmed_ = true; }
    }
    if (same) {
        minLines_ = lo;
        maxLines_ = hi;
    } else {
        anchor_ = s;
        minLines_ = maxLines_ = s.lines;
        counting_ = true;
        fired_ = false;
        count_ = 0;
        oddCount_ = 0;
        parityRun_ = 0;
    }
    if (fired_) return false;
    if (count_ < kSamples) {
        count_++;
        if (s.lines & 1) oddCount_++;
    }
    if (count_ >= kSamples) { fired_ = true; return true; }
    return false;
}

bool StableLatch::decidedFor(const AutoloadSample& s) const {
    if (!fired_ || s.input != anchor_.input || s.mode != anchor_.mode || parityDisagrees(s)) return false;
    uint16_t lo = s.lines < minLines_ ? s.lines : minLines_;
    uint16_t hi = s.lines > maxLines_ ? s.lines : maxLines_;
    return hi - lo <= kLinesSpread;
}

bool StableLatch::interlaced(uint8_t gbsMode) const {
    if (gbsMode != 1 && gbsMode != 2) return false;
    // odd VPERIOD_IF = progressive in modes 1/2 (profileIsInterlaced); majority vote over the window
    return oddCount_ * 2 < count_;
}

static bool ranksBefore(const ProfileEntry& a, uint8_t ai, const ProfileEntry& b, uint8_t bi, bool interlaced) {
    bool aInter = (a.fpInterlaced != 0) == interlaced, bInter = (b.fpInterlaced != 0) == interlaced;
    if (aInter != bInter) return aInter;
    if (a.lastused != b.lastused) return a.lastused > b.lastused;
    return ai < bi;
}

uint8_t profileMatches(const ProfileList& list, uint8_t input, uint8_t mode, uint16_t lines, bool interlaced,
                       uint8_t* out, uint8_t max, uint32_t milliHz) {
    uint8_t all[PROFILE_MAX], n = 0;
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {
        const ProfileEntry& e = list.at(i);
        if (!e.used || !e.hasFp || e.fpInput != input || e.fpMode != mode) continue;
        if (abs((int)e.fpLines - (int)lines) > 4) continue;
        if (!profileRateFits(input, mode, e.fpLines, e.fpInterlaced != 0, e.fpMilliHz, lines, interlaced, milliHz)) continue;
        uint8_t at = n++;                                   // insertion sort by rank
        while (at > 0 && ranksBefore(e, i, list.at(all[at - 1]), all[at - 1], interlaced)) { all[at] = all[at - 1]; at--; }
        all[at] = i;
    }
    if (n > max) n = max;
    for (uint8_t i = 0; i < n; i++) out[i] = all[i];
    return n;
}

AutoloadDecision profileAutoloadMatch(const ProfileList& list, uint8_t input, uint8_t mode, uint16_t lines,
                                      bool interlaced, int activeIndex, bool activeModified, int* candidate,
                                      uint8_t regMode, uint32_t milliHz) {
    uint8_t first;
    int best = profileMatches(list, input, mode, lines, interlaced, &first, 1, milliHz) ? first : -1;
    if (candidate) *candidate = best;
    if (best < 0) {
        bool foreign = regMode != 0xFF && activeIndex >= 0 &&
                       !profileKeepsWithoutMatch(list.at((uint8_t)activeIndex), input, mode, lines, interlaced, milliHz);
        return foreign ? AL_DEFAULTS : AL_NONE;
    }
    (void)activeModified;   // another profile's match loads over unsaved changes too
    if (best == activeIndex) return AL_ALREADY_ACTIVE;
    return AL_LOAD;
}

bool profileKeepsWithoutMatch(const ProfileEntry& active, uint8_t input, uint8_t mode, uint16_t lines, bool interlaced,
                              uint32_t milliHz) {
    if (!active.hasFp || !inputTypeValid(active.fpInput)) return true;   // general, or a fingerprint shown as none
    if (active.fpInput != input) return false;
    // The same input: the same console changing mode (50/60 Hz, 480i/p, VGA scaling) keeps its profile. In the mode
    // the profile was saved in, a line rate that is not that console's is another console
    return active.fpMode != mode ||
           profileRateFits(input, mode, active.fpLines, active.fpInterlaced != 0, active.fpMilliHz, lines, interlaced, milliHz);
}

static void noticeCopy(char* out, size_t n, const char* in) {
    strncpy(out, in ? in : "", n - 1);
    out[n - 1] = '\0';
}

void AutoloadNotice::start(uint16_t seq, const char* label, const char* discarded, uint32_t now) {
    on_ = true;
    seq_ = seq;
    at_ = now;
    noticeCopy(label_, sizeof(label_), label);
    noticeCopy(discarded_, sizeof(discarded_), discarded);
    result_[0] = '\0';
}

AutoloadNotice::Outcome AutoloadNotice::poll(bool found, const char* result, uint32_t now) {
    if (!on_) return AN_NONE;
    if (!found) {
        if (now - at_ < kGiveUpMs) return AN_WAIT;
        on_ = false;
        return AN_TIMEOUT;
    }
    on_ = false;
    noticeCopy(result_, sizeof(result_), result);
    return result_[0] ? AN_FAILED : AN_LOADED;
}
