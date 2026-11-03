// Input search identification: pure. A sync found on RGB SCART during a scan,
// nothing shown yet: R/G/B content first (listen off); flat → the ADV7280 listens on pin 20 (the termination raises
// the G level only when a console drives the SCART RGB lines; content on B meanwhile = the picture came back after a
// black stretch) → RGB SCART or Composite; then the preset loads once.
#pragma once
#include <stdint.h>

struct IdSample {                 // one pass (every ~300 ms); fields not measured this pass are false / 0
    bool sync;                    // getStatus16SpHsStable(), read before any sampling
    bool rgbMeasured, rgbContent; // listen off: rjRgbContent on tv5725ChannelContentLive
    bool listenSent;              // listen (or the termination) requested
    bool switchesApplied;         // the STATE that changed it went out kSwitchSettleMs ago (time-based)
    bool levelMeasured;           // tv5725ChannelStatsLive G, taken with the switches applied
    uint8_t gLevel;
    bool blueContent;             // with the level: content on B (rjPictureContentListening), the picture is back
    bool reportFresh, lock, freeRun;   // a decoder report newer than the previous pass
    uint32_t sinceListenMs;       // since the listen STATE went out (0 if not yet)
    uint32_t sincePoweredMs;      // since the first POWERED report after it (0 if none)
    bool listenOffConfirmed;      // RELEASING: a report after the listen-off STATE without POWERED (the termination
                                  // goes off before the decoder powers down): G is the console's own
};

enum : uint8_t { ID_IDLE, ID_IDENTIFY, ID_RELEASING, ID_DONE };
enum : uint8_t { IA_NONE, IA_SAMPLE_RGB, IA_LISTEN_ON, IA_SAMPLE_LEVEL, IA_LISTEN_OFF, IA_RELEASE_RGB, IA_MOVE_COMPOSITE,
                 IA_ABANDON };
enum : uint8_t { IW_NONE, IW_PICTURE, IW_RGB_WIRED, IW_LEVEL_AMBIGUOUS, IW_NO_LOCK, IW_DECODER_SILENT, IW_TIMEOUT,
                 IW_COMPOSITE, IW_SYNC_LOST, IW_CHOICE, IW_RESCAN, IW_PAUSED, IW_PICTURE_LISTENING };

class InputIdentify {
public:
    static const uint8_t kRgbWiredMin = 32, kLevelNoise = 8, kSyncLostPasses = 3;
    static const uint32_t kDecoderReadyMs = 1800, kDecoderCapMs = 3000, kIdentifyCapMs = 5000, kReleaseCapMs = 1200,
                          kDecoderInitMs = 800, kSwitchSettleMs = 200,
                          kAbsoluteCapMs = 6000;   // from begin(), whatever the glue's stamps say: never a hang
    static const uint32_t kCompositeMinMs = 4000;   // from begin(): a booting console keeps R/G/B flat (MD 3.4 s)
    // A sync on RGB SCART in a scan: phase IDENTIFY, first action SAMPLE_RGB. decoderListens false = an ADV Controller
    // without ADV_INFO_CAP_LISTEN: the termination alone (no decoder report, Composite needs no lock)
    void begin(uint32_t now, bool decoderListens);
    uint8_t step(const IdSample& s, uint32_t now);   // the next IA_* action for the glue
    void abandon(uint8_t why);                       // CHOICE / RESCAN / PAUSED / SYNC_LOST: back to IDLE, listen off
    void newAcquisition();                           // scan start, choice or path change, source gone: IDLE
    void moveDone() { listen_ = false; }             // the glue switched to Composite: listen may go
    void skip(uint8_t why) { newAcquisition(); phase_ = ID_DONE; why_ = why; }   // no identification this acquisition
    uint8_t phase() const { return phase_; }
    uint8_t why() const { return why_; }
    bool holdsSyncWatcher() const { return phase_ == ID_IDENTIFY || phase_ == ID_RELEASING; }
    bool listenWanted() const { return listen_; }
    bool releaseForced() const { return forced_; }   // released at the cap with G still up: re-detect first
private:
    uint8_t verdictRgb(uint8_t why, uint32_t now);
    uint8_t phase_ = ID_IDLE, why_ = IW_NONE, high_ = 0, low_ = 0, syncLost_ = 0;
    bool listen_ = false, decoderListens_ = true, rgbDone_ = false, haveReport_ = false, lock_ = false, freeRun_ = false,
         forced_ = false;
    uint32_t listenAt_ = 0, releaseFrom_ = 0, beginAt_ = 0;
};
