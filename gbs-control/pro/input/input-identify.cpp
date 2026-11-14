#include "input-identify.h"

void InputIdentify::begin(uint32_t now, bool decoderListens, bool compositeSearched, bool lastComposite) {
    newAcquisition();
    phase_ = ID_IDENTIFY;
    decoderListens_ = decoderListens;
    compositeSearched_ = compositeSearched;
    lastComposite_ = lastComposite;
    listenAt_ = beginAt_ = now;
}

void InputIdentify::abandon(uint8_t why) {
    if (phase_ != ID_IDENTIFY && phase_ != ID_RELEASING) return;
    phase_ = ID_IDLE;
    why_ = why;
    listen_ = false;
}

void InputIdentify::newAcquisition() {
    phase_ = ID_IDLE;
    why_ = IW_NONE;
    high_ = low_ = syncLost_ = 0;
    listen_ = rgbDone_ = haveReport_ = lock_ = freeRun_ = forced_ = burst_ = peeked_ = levelSincePeek_ = wires_ = seen_ = false;
}

// RGB SCART after listen: listen goes off first; the syncwatcher stays held until the termination is off (RELEASING)
uint8_t InputIdentify::verdictRgb(uint8_t why, uint32_t now) {
    why_ = why;
    listen_ = false;
    phase_ = ID_RELEASING;
    uint32_t afterInit = listenAt_ + kDecoderInitMs;   // a STATE sent during the decoder init applies after it
    releaseFrom_ = (int32_t)(afterInit - now) > 0 ? afterInit : now;
    return IA_LISTEN_OFF;
}

uint8_t InputIdentify::step(const IdSample& s, uint32_t now) {
    if (phase_ != ID_IDENTIFY && phase_ != ID_RELEASING) return IA_NONE;
    if (!s.sync) {
        if (++syncLost_ < kSyncLostPasses) return IA_NONE;
        abandon(IW_SYNC_LOST);
        return IA_ABANDON;
    }
    syncLost_ = 0;
    if (s.reportFresh) { haveReport_ = true; lock_ = s.lock; freeRun_ = s.freeRun; burst_ = s.burst; }   // the last fresh report counts
    if (phase_ == ID_IDENTIFY && !compositeSearched_) {      // nothing to tell apart
        why_ = IW_COMPOSITE_OFF;
        phase_ = ID_DONE;
        return IA_RELEASE_RGB;
    }
    if (phase_ == ID_IDENTIFY && now - beginAt_ >= kAbsoluteCapMs) {   // a STATE that never goes out, a flapping sync
        if (listen_) return verdictRgb(IW_TIMEOUT, now);
        why_ = IW_TIMEOUT;
        phase_ = ID_DONE;
        return IA_RELEASE_RGB;
    }

    if (phase_ == ID_RELEASING) {
        bool started = (int32_t)(now - releaseFrom_) >= 0;
        if (s.listenOffConfirmed || (started && s.levelMeasured && s.gLevel < kLevelNoise)) {
            phase_ = ID_DONE;
            return IA_RELEASE_RGB;
        }
        if (started && now - releaseFrom_ >= kReleaseCapMs) { phase_ = ID_DONE; forced_ = true; return IA_RELEASE_RGB; }
        return IA_SAMPLE_LEVEL;
    }

    if (!rgbDone_) {
        if (!s.rgbMeasured) return IA_SAMPLE_RGB;
        if (s.rgbContent) { why_ = IW_PICTURE; phase_ = ID_DONE; return IA_RELEASE_RGB; }
        rgbDone_ = true;
        listen_ = true;
        listenAt_ = now;
        return IA_LISTEN_ON;
    }

    // a black stretch ended, the picture is back: in a G level sample, or right after a look at the composite
    if ((s.levelMeasured && s.blueContent) || (s.peekMeasured && s.peekWires == RW_BLUE)) return verdictRgb(IW_PICTURE_LISTENING, now);
    if (s.peekMeasured) {
        if (s.peekWires == RW_CONTENT) wires_ = true;    // the console's own picture, its blue uniform or absent
        bool only = s.peekContent && s.peekWires == RW_FLAT;   // a picture on pin 20, nothing on R/G/B
        if (only && seen_) {                             // at two looks in a row: composite only
            why_ = IW_COMPOSITE;
            phase_ = ID_DONE;                            // listen_ stays on: the decoder keeps its lock into Composite
            return IA_MOVE_COMPOSITE;
        }
        seen_ = only;                                    // once: a decoder that just locked reads like a dim picture
    }
    if (s.levelMeasured) {
        levelSincePeek_ = true;
        if (s.gLevel >= kRgbWiredMin) { high_++; low_ = 0; }
        else if (s.gLevel < kLevelNoise) { low_++; high_ = 0; }
        else return verdictRgb(IW_LEVEL_AMBIGUOUS, now);
    }
    if (high_ >= 2) return verdictRgb(IW_RGB_WIRED, now);
    if (low_ >= 2) {
        uint32_t since = now - beginAt_;
        if (!decoderListens_) {                          // an ADV Controller that cannot listen: the time rule
            if (since < kCompositeMinMs) return IA_SAMPLE_LEVEL;   // a booting console: its picture may come
            why_ = IW_COMPOSITE;
            phase_ = ID_DONE;
            return IA_MOVE_COMPOSITE;
        }
        if (haveReport_ && lock_ && !freeRun_) {         // the decoder present on pin 20, black on both so far
            // a look that saw a picture waits for the one that confirms it; else: a sync, not a picture
            if (!seen_ && !burst_ && since >= kCompositeMinMs) return verdictRgb(IW_NO_BURST, now);
            if (!seen_ && since >= kBlackCapMs) {
                if (wires_) return verdictRgb(IW_WIRES, now);
                if (!lastComposite_) return verdictRgb(IW_BLACK, now);
                why_ = IW_COMPOSITE_LAST;
                phase_ = ID_DONE;
                return IA_MOVE_COMPOSITE;
            }
            if ((since < kPeekLastMs || seen_) && (!peeked_ || (levelSincePeek_ && now - peekAt_ >= kPeekEveryMs))) {
                peeked_ = true;
                levelSincePeek_ = false;
                peekAt_ = now;
                return IA_PEEK_COMPOSITE;
            }
            return IA_SAMPLE_LEVEL;
        }
        if (s.sincePoweredMs >= kDecoderReadyMs) return verdictRgb(IW_NO_LOCK, now);
    }
    if (decoderListens_ && s.sincePoweredMs == 0 && s.sinceListenMs >= kDecoderCapMs)
        return verdictRgb(IW_DECODER_SILENT, now);
    if (s.sinceListenMs >= kIdentifyCapMs) return verdictRgb(IW_TIMEOUT, now);
    return s.switchesApplied ? IA_SAMPLE_LEVEL : IA_NONE;
}
