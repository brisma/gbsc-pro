// Automatic input selection: pure state
// machine, host-testable. The device glue (input-auto-device.cpp) switches paths, decides "signal present" and
// feeds results.
#pragma once
#include <stdint.h>
#include "input-types.h"

enum : uint8_t { IA_MANUAL, IA_SCANNING, IA_FOUND };
// Why a scan started (console log)
enum : uint8_t { IA_WHY_BOOT, IA_WHY_LOST, IA_WHY_CHOSEN, IA_WHY_BOARD, IA_WHY_BACK_TO_AUTO, IA_WHY_RETURN, IA_WHY_FORMAT };

struct InputAutoBoard {
    bool proPaths;      // Pro switch patterns: VGA, RGB SCART, RGB SoG, Component are separate paths
    bool advProbes;     // the ADV Controller announces the signal report: S-Video and Composite can be probed
    uint8_t skip;       // Input > Auto search: paths left out, bit (path - 1); all of them out = none out
    bool operator!=(const InputAutoBoard& o) const { return proPaths != o.proPaths || advProbes != o.advProbes || skip != o.skip; }
};
// The paths the scan uses on this board after Auto search (bit (path - 1)); never empty
uint8_t inputAutoScanned(const InputAutoBoard& b);

// Scan order (best first: VGA, then RGB/Component, S-Video, Composite). The hint (last found path) goes first,
// except S-Video/Composite, which never go ahead of RGB SCART (a SCART cable carries CVBS on pin 20 too). For the
// same reason a Composite find on the Pro is kept only after one more look at RGB SCART finds nothing there; a RGB SoG
// find and a 15 kHz source on the VGA path likewise (the SoG and VGA wires are the SCART RGB wires; a real SoG source,
// an Amiga or an arcade board on VGA stay where they were found).
// Standard board: the RGB header (1: VGA, RGBS and RGsB decided by the TV5725 detection) and YPbPr (4).
uint8_t inputAutoPaths(const InputAutoBoard& b, uint8_t hint, uint8_t* out, uint8_t max);
// The path the hardware starts on: the scan's first one (a S-Video/Composite hint never goes ahead of RGB SCART)
uint8_t inputAutoBootPath(const InputAutoBoard& b, uint8_t hint);

// A TV5725 path shows a source: powered, detected, and not in upstream's sync recovery for kNoSyncLost passes
// (about 1.7 s; upstream keeps recovering for about 50 s before it gives up)
bool inputTvPathPresent(bool boardPower, bool disconnected, uint8_t videoMode, uint16_t noSyncCounter);
// The Pro's VGA wires are the SCART RGB wires: a source on the VGA path counts only with a VGA line rate (above
// ~20 kHz; TV5725 HPERIOD_IF: 216 at 31 kHz, 433 at 15.6 kHz, 511 = no line), else a SCART console looks like VGA
bool inputVgaLineRate(uint16_t hperiodIf);
// Upstream's SOG slicer sweep, one step per detection pass while there is no source: 6, 5, 4, 3, 2, 6, ...
uint8_t inputSogSweepNext(uint8_t level);
// A scan's probe of a TV5725 path is over ("nothing"): the sync slicing paths (RGB SCART, RGB SoG, Component) once
// every SOG level 2..6 was sampled on it (sogSeen: bit per level; a round used to last two sweeps, so Component always
// got the same levels), VGA (H/V sync) after 3 detection passes; never for the decoder paths (the ADV report times them)
bool inputTvPathDone(uint8_t path, uint8_t passes, uint8_t sogSeen);

// A source shown on the active input counts for the machine only when that is the input it wants: right after a new
// scan starts or an input is chosen by hand the hardware is still on the previous input for a few passes, and its
// source would be taken for a find on the probed path (a false VGA) or for "seen" on the chosen one (S-Video
// chosen by hand went back to Auto after input.lost_wait instead of showing the free-run bars)
bool inputAutoSourceCounts(uint8_t state, uint8_t active, uint8_t want);
// The source's rate going 50 <-> 60 Hz (0 = unknown: never a change). The decoder, and RGB SCART with its sync from
// pin 20, can go from one console to another with no gap (a SCART cable carries CVBS on pin 20): a likely new source.
// A new rate counts once it held kHoldMs (a weak signal flickering between the two never does)
class RateWatch {
public:
    static const uint32_t kHoldMs = 1000;
    bool feed(uint8_t hz, uint32_t now);   // true once when a new 50/60 rate has held kHoldMs
    void reset() { ref_ = cand_ = 0; }
private:
    uint8_t ref_ = 0, cand_ = 0;
    uint32_t candAt_ = 0;
};

class InputAuto {
public:
    static const uint32_t kAbsentMs = 5000, kNeverSeenMs = 60000;
    static const uint16_t kNoSyncLost = 60;
    // A found source back after this much absence is searched again (a console swapped inside input.lost_wait).
    // TV5725 paths: absence starts ~1.7 s after the sync went (kNoSyncLost), so 0.3 s more makes a ~2 s gap;
    // S-Video/Composite: the decoder report drops at once, so 2 s. Only after the found path showed the source once
    // (a fallback find switches back from RGB SCART, which takes longer than this)
    static const uint32_t kBackAsNewMs = 300, kBackAsNewDecoderMs = 2000;
    void begin(const InputAutoBoard& b, uint8_t hint, uint32_t now = 0);   // Source starts in Auto, scanning from the hint
    void choose(uint8_t choice, uint32_t now);           // 0 Auto, 1..6 a manual input (temporary)
    void tick(bool signalPresent, uint32_t now);         // every loop: is a source shown on want()?
    void pathResult(uint8_t path, bool found, uint32_t now);   // a probe of want() ended (ignored for another path)
    void redirect(uint8_t path, uint32_t now);           // the RGB SCART judge moved the found source (1 -> 6)
    void sourceChanged(uint32_t now);                    // Auto with a find: a likely new source, search again
    // The VGA path being probed shows a 15 kHz source; true while that find waits for the RGB SCART look (false with
    // RGB SCART left out of Auto search: the source on VGA is a find)
    bool lowRateVga(uint32_t now);
    void setBoard(const InputAutoBoard& b, uint32_t now);   // Auto search changed: a running scan restarts
    void setAbsentMs(uint32_t ms) { absentMs_ = ms; }   // input.lost_wait: absence before a new scan / back to Auto
    uint8_t want() const;                                // the input the hardware should be on (1..6)
    uint8_t state() const { return state_; }
    uint8_t choice() const { return choice_; }            // what Source shows: 0 Auto
    uint8_t found() const { return found_; }              // last found path (the hint), 0 none
    bool takeHintChanged();                              // true once after found() changed: persist input.last
    // Console log
    uint8_t scanReason() const { return why_; }          // IA_WHY_*: why the current (or last) scan started
    uint32_t scanStartedAt() const { return scanAt_; }
    uint8_t scanPaths(uint8_t* out, uint8_t max) const;   // the order of the current scan
    bool takeScanStarted();                              // true once per scan start
    bool takeRoundEnded();                               // true once per scan: every path tried, nothing found
    bool firstRound() const { return !roundSeen_; }      // the scan has not tried every path yet
    bool foundByFallback() const { return fallback_; }   // the find came back after an empty RGB SCART look
private:
    void scanFrom(uint8_t hint, uint8_t why, uint32_t now);
    InputAutoBoard board_ = {true, true};
    uint8_t paths_[6] = {0};
    uint8_t count_ = 0, cursor_ = 0;
    uint8_t state_ = IA_SCANNING, choice_ = 0, found_ = 0, manual_ = 0, hint_ = 0;   // hint_: from begin()
    bool seen_ = false, present_ = false, hintChanged_ = false;
    bool shownSinceFind_ = false;   // IA_FOUND: the found path showed the source (a later absence can be a return)
    uint32_t since_ = 0, absentAt_ = 0, absentMs_ = kAbsentMs;
    bool absent_ = false;
    bool confirm_ = false;  // a find waits for one look at RGB SCART (want() = RGB SCART meanwhile) ...
    uint8_t confirmFrom_ = 0;   // ... and falls back to this path (Composite, or VGA at 15 kHz) when it finds nothing
    uint8_t why_ = IA_WHY_BOOT;
    uint32_t scanAt_ = 0;
    bool started_ = false, roundEnded_ = false, roundSeen_ = false, fallback_ = false;
};
