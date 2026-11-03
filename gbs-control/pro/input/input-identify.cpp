#include "input-identify.h"

void InputIdentify::begin(uint32_t now, bool decoderListens) {
    newAcquisition();
    phase_ = ID_IDENTIFY;
    decoderListens_ = decoderListens;
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
    listen_ = rgbDone_ = haveReport_ = lock_ = freeRun_ = forced_ = false;
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
    if (s.reportFresh) { haveReport_ = true; lock_ = s.lock; freeRun_ = s.freeRun; }   // the last fresh report counts
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

    if (s.levelMeasured) {
        if (s.blueContent) return verdictRgb(IW_PICTURE_LISTENING, now);   // a black stretch ended: the picture is back
        if (s.gLevel >= kRgbWiredMin) { high_++; low_ = 0; }
        else if (s.gLevel < kLevelNoise) { low_++; high_ = 0; }
        else return verdictRgb(IW_LEVEL_AMBIGUOUS, now);
    }
    if (high_ >= 2) return verdictRgb(IW_RGB_WIRED, now);
    if (low_ >= 2) {
        if (!decoderListens_ || (haveReport_ && lock_ && !freeRun_)) {
            if (now - beginAt_ < kCompositeMinMs) return IA_SAMPLE_LEVEL;   // a booting console: its picture may come
            why_ = IW_COMPOSITE;
            phase_ = ID_DONE;                            // listen_ stays on: the decoder keeps its lock into Composite
            return IA_MOVE_COMPOSITE;
        }
        if (s.sincePoweredMs >= kDecoderReadyMs) return verdictRgb(IW_NO_LOCK, now);
    }
    if (decoderListens_ && s.sincePoweredMs == 0 && s.sinceListenMs >= kDecoderCapMs)
        return verdictRgb(IW_DECODER_SILENT, now);
    if (s.sinceListenMs >= kIdentifyCapMs) return verdictRgb(IW_TIMEOUT, now);
    return s.switchesApplied ? IA_SAMPLE_LEVEL : IA_NONE;
}
