// Auto-load: stable-signal latch and fingerprint matcher (pure, host-testable)
#pragma once
#include <stdint.h>
#include "profile-list.h"

struct AutoloadSample {
    uint8_t input;
    uint8_t mode;        // source mode (profileSourceModeOf)
    uint16_t lines;      // VPERIOD_IF
    bool signal;
    bool stageBusy;      // applyPresetDoneStage != 0: the preset is being (re)applied
};

// Fires once per stable signal identity: input and mode equal, lines spread (max - min) at most 4, i.e. within
// ±2 of the middle of the range (interlaced VPERIOD_IF alternates 522/524/526), 100 samples.
// Busy-stage samples are ignored (a preset re-apply never re-fires); signal loss or a change re-arms. After firing,
// a VPERIOD_IF parity (modes 1/2: odd = progressive) against the fired vote for kParityRun samples in a row is a
// change too: 240p (523) and 480i (524/526) share the input, the mode and the line window.
class StableLatch {
public:
    static const uint8_t kSamples = 100;
    static const uint8_t kLinesSpread = 4;
    static const uint8_t kParityRun = 25;     // 0.5 s at the 20 ms sample period
    void reset();
    bool feed(const AutoloadSample& s);
    uint16_t anchorLines() const { return (uint16_t)((minLines_ + maxLines_) / 2); }
    bool interlaced(uint8_t gbsMode) const;   // parity vote over the counted samples
    bool fired() const { return fired_; }     // the current anchor is complete (false again after a re-arm)
    bool decidedFor(const AutoloadSample& s) const;   // fired, and s is the same signal (feed() would keep the anchor)
    bool parityRearmed() const { return parityRearmed_; }   // the last feed() re-armed on a 240p <-> 480i switch
private:
    AutoloadSample anchor_ = {};
    uint16_t minLines_ = 0, maxLines_ = 0;
    uint8_t count_ = 0;
    uint8_t oddCount_ = 0;
    uint8_t parityRun_ = 0;                   // consecutive samples whose parity disagrees with the fired vote
    bool counting_ = false;
    bool parityDisagrees(const AutoloadSample& s) const;
    bool fired_ = false;
    bool parityRearmed_ = false;
};

// applyPresetDoneStage values where a preset is still being applied (11 is a finished state: bypass or a
// skipped re-apply, and stays until the next applyPresets)
bool autoloadStageBusy(uint8_t stage);

// Every candidate of the rule below for the current signal, ranked as auto-load ranks them (equal interlace,
// then higher lastused, then lower index); returns how many were written (at most max)
uint8_t profileMatches(const ProfileList& list, uint8_t input, uint8_t mode, uint16_t lines, bool interlaced,
                       uint8_t* out, uint8_t max, uint32_t milliHz = 0);

enum AutoloadDecision : uint8_t { AL_NONE, AL_LOAD, AL_ALREADY_ACTIVE, AL_DEFAULTS };

// Candidates: fingerprint with the same input and mode, lines within ±4; prefer equal interlace, then the
// highest lastused, then the lowest index. The active profile matching is never reloaded (its unsaved changes stay);
// another one loads even over unsaved changes (profiles of two consoles are never mixed)
// No match: AL_DEFAULTS when the signal is on another input than the active profile's (see below),
// else AL_NONE. regMode: the register mode running (profileRegModeOf); 0xFF skips the Defaults rule
AutoloadDecision profileAutoloadMatch(const ProfileList& list, uint8_t input, uint8_t mode, uint16_t lines,
                                      bool interlaced, int activeIndex, bool activeModified, int* candidate,
                                      uint8_t regMode = 0xFF, uint32_t milliHz = 0);
// The active profile stays on a signal no profile matches: it has no (valid) fingerprint, a general one loaded by
// hand, or the signal is on its fingerprint's input (any mode: else 50/60 Hz, 480i/p and VGA scaling switches of
// one console drop to Defaults, the VGA one loops)
bool profileKeepsWithoutMatch(const ProfileEntry& active, uint8_t input, uint8_t mode = 0, uint16_t lines = 0,
                              bool interlaced = false, uint32_t milliHz = 0);

// Auto-load's announcement (web status and log line) waits for its operation's result: "loaded" and whose unsaved
// changes went are said once the load ran, a failed load says why and dropped nothing
class AutoloadNotice {
public:
    enum Outcome : uint8_t { AN_NONE, AN_WAIT, AN_LOADED, AN_FAILED, AN_TIMEOUT };
    static const uint32_t kGiveUpMs = 10000;
    void start(uint16_t seq, const char* label, const char* discarded, uint32_t now);
    // found: the result of operation seq is known; result "" = done, else the failure text
    Outcome poll(bool found, const char* result, uint32_t now);
    void hold(uint32_t now) { if (on_) at_ = now; }   // the profile queue is held (a backup): the wait restarts
    bool waiting() const { return on_; }
    uint16_t seq() const { return seq_; }
    const char* label() const { return label_; }
    const char* discarded() const { return discarded_; }
    const char* result() const { return result_; }
private:
    bool on_ = false;
    uint16_t seq_ = 0;
    uint32_t at_ = 0;
    char label_[PROFILE_LABEL_BUF] = {0};
    char discarded_[PROFILE_LABEL_BUF] = {0};
    char result_[24] = {0};
};
