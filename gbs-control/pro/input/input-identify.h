// Input search identification: pure. A sync found on RGB SCART during a scan,
// nothing shown yet: R/G/B content first (listen off); flat → the ADV7280 listens on pin 20 (the termination raises
// the G level only when a console drives the SCART RGB lines; content on B meanwhile = the picture came back after a
// black stretch) → RGB SCART or Composite; then the preset loads once.
// Composite is decided on what pin 20 shows, not on time: every SCART RGB cable carries composite video or composite
// sync there, so an RGB console on a black screen locks the decoder too. With the G level low and the decoder present
// the composite picture is looked at (the encoder already drives the TV5725's YPbPr input while the decoder listens:
// one ADC input change, no ADV switch): a picture there with nothing on the G and B wires right after is a
// composite-only source; content on B is the console's own picture coming up meanwhile; content on G alone (a white
// screen: the blue uniform) or something faint decides nothing at that look, and the G level goes on. Black on both
// decides nothing either: looked at again every kPeekEveryMs. At the end a sync without colour burst is RGB SCART
// (an RGB cable wired with composite sync) at kCompositeMinMs; at kBlackCapMs a look that saw content on the wires
// gives RGB SCART, anything else what the last source found here was. With Composite left out of Auto search there
// is nothing to tell apart: RGB SCART at once.
#pragma once
#include <stdint.h>
#include "rgb-scart-judge.h"

struct IdSample {                 // one pass (every ~300 ms); fields not measured this pass are false / 0
    bool sync;                    // getStatus16SpHsStable(), read before any sampling
    bool rgbMeasured, rgbContent; // listen off: rjRgbContent on tv5725ChannelContentLive
    bool listenSent;              // listen (or the termination) requested
    bool switchesApplied;         // the STATE that changed it went out kSwitchSettleMs ago (time-based)
    bool levelMeasured;           // tv5725ChannelStatsLive G, taken with the switches applied
    uint8_t gLevel;
    bool blueContent;             // with the level: content on B (rjPictureContentListening), the picture is back
    bool reportFresh, lock, freeRun, burst;   // a decoder report newer than the previous pass (burst: FSC lock)
    uint32_t sinceListenMs;       // since the listen STATE went out (0 if not yet)
    uint32_t sincePoweredMs;      // since the first POWERED report after it (0 if none)
    bool listenOffConfirmed;      // RELEASING: a report after the listen-off STATE without POWERED (the termination
                                  // goes off before the decoder powers down): G is the console's own
    bool peekMeasured;            // the look asked by IA_PEEK_COMPOSITE was made
    bool peekContent;             // ... a picture on the composite (Y content before and after the R/G/B sample)
    uint8_t peekWires;            // ... RW_*: what the RGB wires read in between (rjWiresAfterLook)
};

enum : uint8_t { ID_IDLE, ID_IDENTIFY, ID_RELEASING, ID_DONE };
enum : uint8_t { IA_NONE, IA_SAMPLE_RGB, IA_LISTEN_ON, IA_SAMPLE_LEVEL, IA_LISTEN_OFF, IA_RELEASE_RGB, IA_MOVE_COMPOSITE,
                 IA_ABANDON, IA_PEEK_COMPOSITE };
enum : uint8_t { IW_NONE, IW_PICTURE, IW_RGB_WIRED, IW_LEVEL_AMBIGUOUS, IW_NO_LOCK, IW_DECODER_SILENT, IW_TIMEOUT,
                 IW_COMPOSITE, IW_SYNC_LOST, IW_CHOICE, IW_RESCAN, IW_PAUSED, IW_PICTURE_LISTENING, IW_NO_BURST,
                 IW_COMPOSITE_OFF, IW_BLACK, IW_COMPOSITE_LAST, IW_WIRES };

class InputIdentify {
public:
    static const uint8_t kRgbWiredMin = 32, kLevelNoise = 8, kSyncLostPasses = 3;
    static const uint32_t kDecoderReadyMs = 1800, kDecoderCapMs = 3000, kIdentifyCapMs = 5000, kReleaseCapMs = 1200,
                          kDecoderInitMs = 800, kSwitchSettleMs = 200,
                          kAbsoluteCapMs = 6000;   // from begin(), whatever the glue's stamps say: never a hang
    // From begin(). kCompositeMinMs: a sync without colour and no picture is RGB SCART (and the legacy time rule of
    // an ADV Controller that cannot listen: a booting console keeps R/G/B flat, MD 3.4 s). Looks kPeekEveryMs apart,
    // none after kPeekLastMs; still black on both at kBlackCapMs: the last source decides
    static const uint32_t kCompositeMinMs = 4000, kPeekEveryMs = 1000, kPeekLastMs = 4400, kBlackCapMs = 5000;
    // A sync on RGB SCART in a scan: phase IDENTIFY, first action SAMPLE_RGB. decoderListens false = an ADV Controller
    // without ADV_INFO_CAP_LISTEN: the termination alone (no decoder report, Composite needs no lock).
    // compositeSearched false = Composite is not among the inputs Auto searches: RGB SCART, no listening.
    // lastComposite = the last source found on this unit was Composite (black on both to the end)
    void begin(uint32_t now, bool decoderListens, bool compositeSearched = true, bool lastComposite = false);
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
         forced_ = false, burst_ = false, compositeSearched_ = true, lastComposite_ = false, peeked_ = false,
         levelSincePeek_ = false, wires_ = false;
    uint32_t listenAt_ = 0, releaseFrom_ = 0, beginAt_ = 0, peekAt_ = 0;
};
