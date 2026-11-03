// Console log: one line for the signal the scaler shows, written when it changed and stayed for kStableMs (a sync
// flap is not a new signal). Pure, host-tested (test_log_signal); the device fills it from the menu's signal snapshot.
#pragma once
#include <stdint.h>
#include <stddef.h>

struct LogSignalInfo {
    bool none;              // no source shown
    uint16_t lines;         // source lines per frame or field
    uint16_t srcHz10, outHz10;
    bool interlaced;
    uint8_t sync;           // 0 HV, 1 CSync, 2 SOG
    bool locked;            // frame lock
    uint8_t preset;         // rto->presetID
    uint8_t input;          // active input 1..6
};
bool operator==(const LogSignalInfo& a, const LogSignalInfo& b);

const char* logPresetName(uint8_t presetId);   // upstream's "preset applied" names
// "signal: 312 lines progressive 50.0 Hz, CSync, frame lock on, preset 1280x1024, out 50.0 Hz (RGB SCART)"
size_t logSignalText(const LogSignalInfo& s, char* out, size_t n);

class LogSignalWatch {
public:
    static const uint32_t kStableMs = 1000;
    static const uint16_t kMinHz10 = 230, kMaxHz10 = 1000;   // rates a source sends (24p .. 100 Hz); others are transients
    bool update(const LogSignalInfo& s, uint32_t nowMs);   // true once when a new value has stayed kStableMs
private:
    LogSignalInfo cand_ = {}, logged_ = {};
    bool haveCand_ = false, haveLogged_ = false;
    uint32_t since_ = 0;
};

// A sync dropout the scaler recovers from without declaring the source lost (a console reset): how long it lasted,
// once, when the sync comes back (0 otherwise). noSyncCounter as upstream counts it; a dropout starts at 2 (one
// missed check is noise)
class LogSyncReturn {
public:
    uint32_t update(uint16_t noSyncCounter, bool sourceLost, uint32_t nowMs);
private:
    bool active_ = false;
    uint32_t since_ = 0;
};
