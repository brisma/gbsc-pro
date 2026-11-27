// Short-pulse limit of the TV5725 sync processor (SP_H_PULSE_IGNOR, in the counts of the measured sync pulse).
// Pulses under it are dropped: that keeps the half-width equalizing pulses of a composite sync out of the line
// counter. Measured window: the limit works from the width of the equalizing pulses up to 66 % of the normal pulse;
// under it the scaler counts about ten lines too many per field, over it too few, and the picture shakes.
// A bright picture moves the slicing point of the sync, so every pulse gets wider by the same amount: the
// equalizing pulses grow from 50 % to about 55 % of the dark normal pulse and pass a limit set just above half
// (upstream's formula gives about 53 %) for as long as the bright picture lasts. A console that changes its
// horizontal mode changes its pulse width, and with it the window.
// The limit here is half of the mode's dark pulse, plus the widening of the moment, plus a margin: low on a dark
// picture, higher only while the picture is bright. Across a mode change the limit stays where it was when that
// is still inside the new mode's window (two modes of one console share part of it), so the change itself needs no
// correction in either direction. Pure and host-tested (test_sync_pulse).
#pragma once
#include <stdint.h>

static const uint16_t kSyncIgnoreMax = 0x8F;   // one under upstream's cap: upstream recomputes a register at 0x90 from scratch
static const uint8_t kSyncLimitMargin = 6;     // counts above the equalizing pulses
static const uint8_t kSyncTrackRun = 4;        // agreeing samples that make a new mode

// A source without equalizing pulses (a SNES) has no lower edge: any limit from a fifth of the pulse up works on a
// clean picture. What it can have is a disturbance that now and then splits a normal pulse in two halves (measured:
// a SNES 1CHIP with a SuperFX game): over the right limit both halves are dropped and the field counts a line fewer,
// under it both count and the field has a line more. The right limit is where one half counts, a point that depends
// on the console and the cable, so it is not a constant here: the limit starts a little under half and follows the
// sign of the line count error. A source with equalizing pulses keeps half + margin: its edge is the video standard's.
static const uint16_t kSyncEqWindow = 800;     // width reads per decision
static const uint8_t kSyncEqSeen = 4;          // half-width reads of one same width in a window that mean equalizing pulses
                                               // (measured: 2.4..3 % of the reads on a Mega Drive, all within a count;
                                               // a SNES with a SuperFX game: a stray read now and then, never two alike)
static const int8_t kSyncNoEqStart = -3;       // counts from half of the pulse, before any correction
static const int8_t kSyncNoEqTrimMax = (int8_t)kSyncLimitMargin;   // never over the limit of a source with equalizing pulses
static const uint16_t kSyncLineWindow = 100;   // fields per correction
static const uint8_t kSyncLineErrors = 5;      // fields off by a line or two in a window (5 %) that move the limit one count
static const uint8_t kSyncLineGapMs = 21;      // between two line count reads: longer than a field, so each read is a new field

// dark: the mode's normal pulse on a dark picture; now: the normal pulse at this moment (>= dark); keep: a limit
// to stay on while the window allows it (0: none)
uint16_t syncPulseLimit(uint16_t dark, uint16_t now, uint16_t keep = 0);
uint16_t syncPulseLimitTop(uint16_t dark);   // 64 % of the dark pulse: under the window's upper edge
// A limit set by hand (input.sync_limit, in counts): that value on a dark picture, plus the widening of the moment
// like the automatic one, within 2..kSyncIgnoreMax. For a source whose sync carries pulses the automatic limit
// misjudges (a split normal pulse counts twice under the right value and not at all over it).
uint16_t syncPulseManualLimit(uint16_t manual, uint16_t dark, uint16_t now);
// A source without equalizing pulses: half of the dark pulse + trim (kSyncNoEqStart, then corrected), plus the
// widening of the moment; from a quarter of the pulse up to the limit with equalizing pulses
uint16_t syncPulseNoEqLimit(uint16_t dark, uint16_t now, int8_t trim);

// The measured width is two register bytes, read one after the other while the chip keeps measuring: a read that
// falls across the vertical interval mixes a broad pulse with a normal one and gives a width no pulse has. Two
// reads in a row that agree (2 %) are one real pulse width.
bool syncPulseReadsAgree(uint16_t first, uint16_t second);

// Follows the normal pulse from single samples of the measured width (the vertical interval's half and broad
// pulses and stray readings are left out). update() returns the limit to write, 0 when it stays.
class SyncPulseTrack {
public:
    void reset() { dark_ = now_ = cand_ = prev_ = last_ = keep_ = 0; run_ = 0; mode_ = false; sourceReset(); }
    // the pulse is not known for now, the limit to keep is; the kind of source has to show again (another console may
    // be on the same cable): until then the limit every source takes
    void lost() { dark_ = now_ = cand_ = prev_ = 0; run_ = 0; kindUnknown(); }
    uint16_t update(uint16_t hsLow);
    // 0: no pulse known yet
    uint16_t limit() const {
        if (!dark_) return 0;
        if (manual_) return syncPulseManualLimit(manual_, dark_, now_);
        return eq_ == EQ_NONE ? syncPulseNoEqLimit(dark_, now_, trim_) : syncPulseLimit(dark_, now_, keep_);
    }
    // The line count of one field (STATUS_SYNC_PROC_VTOTAL; the caller leaves kSyncLineGapMs between two reads) while
    // the picture is locked: on a source without equalizing pulses it corrects the limit. Returns the limit to
    // write, 0 when it stays.
    uint16_t lineCount(uint16_t lines);
    bool wantsLineCount() const { return dark_ && !manual_ && eq_ == EQ_NONE; }
    enum : uint8_t { EQ_UNKNOWN, EQ_SEEN, EQ_NONE };   // unknown is treated as seen: the limit every source takes
    uint8_t equalizing() const { return eq_; }
    bool takeEqChange() { bool c = eqChanged_; eqChanged_ = false; return c; }   // true once after the kind was decided or changed
    int8_t trim() const { return trim_; }
    void setManual(uint16_t counts) { manual_ = counts; last_ = 0; lineReset(); }   // 0: automatic; survives reset() and lost()
    uint16_t manual() const { return manual_; }
    uint16_t dark() const { return dark_; }
    uint16_t now() const { return now_; }
    bool takeModeChange() { bool m = mode_; mode_ = false; return m; }   // true once after a new mode was taken
    // One sample that reads like the normal pulse of another video mode (not this mode's dark or bright pulse, not
    // the vertical interval's half or broad pulses, not a stray reading): the first sign of a mode change
    bool otherMode(uint16_t hsLow) const;
private:
    void sourceReset() { kindUnknown(); eqChanged_ = false; trim_ = kSyncNoEqStart; }
    void kindUnknown() {
        if (eq_ == EQ_NONE) last_ = 0;                  // another formula: the next limit is written whatever it is
        if (eq_ != EQ_UNKNOWN) { eq_ = EQ_UNKNOWN; eqChanged_ = true; }
        reads_ = halves_ = halfRef_ = 0;
        lineReset();
    }
    void lineReset() { usual_ = lineCand_ = 0; lineRun_ = 0; lineReads_ = more_ = fewer_ = 0; }
    void countRead(uint16_t hsLow);
    uint16_t dark_ = 0, now_ = 0, cand_ = 0, prev_ = 0, last_ = 0, keep_ = 0, manual_ = 0;
    uint16_t reads_ = 0, halves_ = 0;          // this window's width reads, and the half-width ones of one same width
    uint16_t halfRef_ = 0;                     // that width
    uint16_t usual_ = 0, lineCand_ = 0, lineReads_ = 0, more_ = 0, fewer_ = 0;
    uint8_t lineRun_ = 0;
    uint8_t eq_ = EQ_UNKNOWN;
    bool eqChanged_ = false;
    int8_t trim_ = kSyncNoEqStart;
    uint8_t run_ = 0;
    bool mode_ = false;
};
