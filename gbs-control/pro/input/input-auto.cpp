#include "input-auto.h"

static uint8_t pathBit(uint8_t path) { return (uint8_t)(1 << (path - 1)); }

uint8_t inputAutoScanned(const InputAutoBoard& b) {
    uint8_t all = b.proPaths ? (uint8_t)(pathBit(1) | pathBit(2) | pathBit(3) | pathBit(4) |
                                         (b.advProbes ? pathBit(5) | pathBit(6) : 0))
                             : (uint8_t)(pathBit(1) | pathBit(4));
    uint8_t left = (uint8_t)(all & ~b.skip);
    return left ? left : all;                        // everything left out: Auto looks at every path
}

uint8_t inputAutoPaths(const InputAutoBoard& b, uint8_t hint, uint8_t* out, uint8_t max) {
    uint8_t order[6], n = 0;
    uint8_t scanned = inputAutoScanned(b);
    if (hint >= 1 && hint <= 6 && !(scanned & pathBit(hint))) hint = 0;   // also a decoder hint without probes
    if (b.proPaths) {
        static const uint8_t kPro[6] = {3, 1, 2, 4, 5, 6};
        if (hint) {
            if (inputIsDecoder(hint) && (scanned & pathBit(1))) order[n++] = 1;   // RGB SCART first: it also sees the CVBS of a SCART cable
            order[n++] = hint;
        }
        for (uint8_t i = 0; i < 6; i++) {
            uint8_t p = kPro[i];
            if (!(scanned & pathBit(p))) continue;
            bool dup = false;
            for (uint8_t j = 0; j < n; j++) dup |= order[j] == p;
            if (!dup) order[n++] = p;
        }
    } else {
        if (hint == 4) order[n++] = 4;
        if (scanned & pathBit(1)) order[n++] = 1;
        if (hint != 4 && (scanned & pathBit(4))) order[n++] = 4;
    }
    if (n > max) n = max;
    for (uint8_t i = 0; i < n; i++) out[i] = order[i];
    return n;
}

uint8_t inputAutoBootPath(const InputAutoBoard& b, uint8_t hint) {
    uint8_t p[6];
    return inputAutoPaths(b, hint, p, 6) ? p[0] : InputTypeRGBs;
}

bool inputTvPathPresent(bool boardPower, bool disconnected, uint8_t videoMode, uint16_t noSyncCounter) {
    return boardPower && !disconnected && videoMode != 0 && noSyncCounter < InputAuto::kNoSyncLost;
}

bool inputVgaLineRate(uint16_t hperiodIf) { return hperiodIf != 0 && hperiodIf <= 330; }

static const uint8_t kSogSweepTop = 6, kSogSweepBottom = 2;

uint8_t inputSogSweepNext(uint8_t level) { return level > kSogSweepBottom && level <= kSogSweepTop ? level - 1 : kSogSweepTop; }

bool inputTvPathDone(uint8_t path, uint8_t passes, uint8_t sogSeen) {
    const uint8_t all = (uint8_t)(((1 << (kSogSweepTop + 1)) - 1) & ~((1 << kSogSweepBottom) - 1));
    switch (path) {
        case InputTypeRGBs: case InputTypeRGsB: case InputTypeYUV: return (sogSeen & all) == all;
        case InputTypeVGA: return passes >= 3;
        default: return false;
    }
}

void InputAuto::begin(const InputAutoBoard& b, uint8_t hint, uint32_t now) {
    board_ = b;
    choice_ = 0;
    found_ = 0;
    hint_ = hint;
    hintChanged_ = false;
    present_ = false;
    scanFrom(hint, IA_WHY_BOOT, now);
}

void InputAuto::scanFrom(uint8_t hint, uint8_t why, uint32_t now) {
    count_ = inputAutoPaths(board_, hint, paths_, 6);
    cursor_ = 0;
    state_ = IA_SCANNING;
    absent_ = false;
    confirm_ = false;
    why_ = why;
    scanAt_ = now;
    started_ = true;
    roundEnded_ = roundSeen_ = false;
}

uint8_t InputAuto::scanPaths(uint8_t* out, uint8_t max) const {
    uint8_t n = count_ < max ? count_ : max;
    for (uint8_t i = 0; i < n; i++) out[i] = paths_[i];
    return n;
}

bool InputAuto::takeScanStarted() { bool s = started_; started_ = false; return s; }
bool InputAuto::takeRoundEnded() { bool r = roundEnded_; roundEnded_ = false; return r; }

uint8_t InputAuto::want() const {
    if (state_ == IA_MANUAL) return manual_;
    if (state_ == IA_FOUND) return found_;
    if (confirm_) return InputTypeRGBs;
    return count_ ? paths_[cursor_] : 1;
}

bool inputAutoSourceCounts(uint8_t state, uint8_t active, uint8_t want) {
    return state == IA_FOUND || active == want;
}

void InputAuto::choose(uint8_t choice, uint32_t now) {
    if (choice >= 1 && choice <= 6) {
        uint8_t was = want();
        choice_ = manual_ = choice;
        state_ = IA_MANUAL;
        confirm_ = false;
        seen_ = present_ && was == choice;          // the same input already shows a source
        since_ = now;
        absent_ = false;
        return;
    }
    if (state_ != IA_MANUAL) return;                 // already Auto
    choice_ = 0;
    scanFrom(0, IA_WHY_CHOSEN, now);                 // Auto chosen on purpose: the best source, by priority
}

void InputAuto::tick(bool signalPresent, uint32_t now) {
    present_ = signalPresent;
    switch (state_) {
        case IA_MANUAL:
            if (signalPresent) {                     // a manual input with a source is the new hint too
                seen_ = true;
                absent_ = false;
                if (found_ != manual_) { found_ = manual_; hintChanged_ = true; }
                return;
            }
            if (!seen_) {
                if (now - since_ >= kNeverSeenMs) { choice_ = 0; scanFrom(found_ ? found_ : hint_, IA_WHY_BACK_TO_AUTO, now); }
                return;
            }
            if (!absent_) { absent_ = true; absentAt_ = now; }
            if (now - absentAt_ >= absentMs_) { choice_ = 0; scanFrom(found_ ? found_ : hint_, IA_WHY_BACK_TO_AUTO, now); }
            return;
        case IA_FOUND:
            if (signalPresent) {
                uint32_t gap = inputIsDecoder(found_) ? kBackAsNewDecoderMs : kBackAsNewMs;
                bool back = absent_ && shownSinceFind_ && now - absentAt_ >= gap;   // maybe another console
                absent_ = false;
                shownSinceFind_ = true;
                if (back) scanFrom(found_, IA_WHY_RETURN, now);
                return;
            }
            if (!absent_) { absent_ = true; absentAt_ = now; }
            if (now - absentAt_ >= absentMs_) scanFrom(found_, IA_WHY_LOST, now);
            return;
        default:                                     // scanning: a source on the probed path is a find
            if (signalPresent) pathResult(want(), true, now);
            return;
    }
}

bool RateWatch::feed(uint8_t hz, uint32_t now) {
    if (hz != 50 && hz != 60) { cand_ = 0; return false; }   // unknown: a candidate must hold again
    if (!ref_ || hz == ref_) { ref_ = hz; cand_ = 0; return false; }
    if (hz != cand_) { cand_ = hz; candAt_ = now; return false; }
    if (now - candAt_ < kHoldMs) return false;
    ref_ = hz;
    cand_ = 0;
    return true;
}

void InputAuto::sourceChanged(uint32_t now) {
    if (state_ == IA_FOUND && choice_ == 0) scanFrom(found_, IA_WHY_FORMAT, now);
}

void InputAuto::pathResult(uint8_t path, bool found, uint32_t now) {
    (void)now;
    if (state_ != IA_SCANNING || path != want()) return;
    bool fallback = false;
    if (confirm_) {                                  // the RGB SCART look after a Composite or a 15 kHz VGA find
        confirm_ = false;
        if (!found) { path = confirmFrom_; fallback = true; }
        found = true;
    } else if (found && (path == InputTypeAV || path == InputTypeRGsB) && board_.proPaths &&
               (inputAutoScanned(board_) & pathBit(1))) {
        // A SCART cable carries CVBS on pin 20, and RGB SoG shares the RGB wires (a SCART console powering up while
        // SoG is probed locked there, unstable): RGB SCART first
        confirm_ = true;
        confirmFrom_ = path;
        return;
    }
    if (found) {
        if (found_ != path) hintChanged_ = true;
        found_ = path;
        state_ = IA_FOUND;
        shownSinceFind_ = false;
        absent_ = false;
        fallback_ = fallback;
        return;
    }
    if (count_) cursor_ = (uint8_t)((cursor_ + 1) % count_);
    if (cursor_ == 0 && !roundSeen_) roundEnded_ = roundSeen_ = true;
}

void InputAuto::redirect(uint8_t path, uint32_t now) {
    (void)now;
    if (state_ == IA_MANUAL || path < 1 || path > 6) return;
    confirm_ = false;
    if (found_ != path) hintChanged_ = true;
    found_ = path;
    state_ = IA_FOUND;
    shownSinceFind_ = false;
    absent_ = false;
    fallback_ = false;
}

bool InputAuto::lowRateVga(uint32_t now) {
    (void)now;
    if (state_ != IA_SCANNING || !board_.proPaths || !(inputAutoScanned(board_) & pathBit(1)))
        return false;                                // RGB SCART left out of Auto search: VGA as it is
    if (confirm_) return confirmFrom_ == InputTypeVGA;
    if (want() != InputTypeVGA) return false;
    confirm_ = true;
    confirmFrom_ = InputTypeVGA;
    return true;
}

void InputAuto::setBoard(const InputAutoBoard& b, uint32_t now) {
    if (!(b != board_)) return;
    board_ = b;
    if (state_ == IA_SCANNING) scanFrom(found_ ? found_ : hint_, IA_WHY_BOARD, now);
}

bool InputAuto::takeHintChanged() {
    bool c = hintChanged_;
    hintChanged_ = false;
    return c;
}
