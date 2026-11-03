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

// dark: the mode's normal pulse on a dark picture; now: the normal pulse at this moment (>= dark); keep: a limit
// to stay on while the window allows it (0: none)
uint16_t syncPulseLimit(uint16_t dark, uint16_t now, uint16_t keep = 0);
uint16_t syncPulseLimitTop(uint16_t dark);   // 64 % of the dark pulse: under the window's upper edge

// Follows the normal pulse from single samples of the measured width (the vertical interval's half and broad
// pulses and stray readings are left out). update() returns the limit to write, 0 when it stays.
class SyncPulseTrack {
public:
    void reset() { dark_ = now_ = cand_ = prev_ = last_ = keep_ = 0; run_ = 0; mode_ = false; }
    void lost() { dark_ = now_ = cand_ = prev_ = 0; run_ = 0; }   // the pulse is not known for now, the limit to keep is
    uint16_t update(uint16_t hsLow);
    uint16_t limit() const { return dark_ ? syncPulseLimit(dark_, now_, keep_) : 0; }   // 0: no pulse known yet
    uint16_t dark() const { return dark_; }
    uint16_t now() const { return now_; }
    bool takeModeChange() { bool m = mode_; mode_ = false; return m; }   // true once after a new mode was taken
    // One sample that reads like the normal pulse of another video mode (not this mode's dark or bright pulse, not
    // the vertical interval's half or broad pulses, not a stray reading): the first sign of a mode change
    bool otherMode(uint16_t hsLow) const;
private:
    uint16_t dark_ = 0, now_ = 0, cand_ = 0, prev_ = 0, last_ = 0, keep_ = 0;
    uint8_t run_ = 0;
    bool mode_ = false;
};
