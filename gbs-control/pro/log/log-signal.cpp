#include "log-signal.h"
#include <stdio.h>
#include "../input/input-types.h"
#include "../util/pgm.h"

bool operator==(const LogSignalInfo& a, const LogSignalInfo& b) {
    if (a.none != b.none) return false;
    if (a.none) return true;                 // nothing shown, on any input (a scan walks them): the rest is stale
    if (a.input != b.input) return false;
    return a.lines == b.lines && a.srcHz10 == b.srcHz10 && a.outHz10 == b.outHz10 && a.interlaced == b.interlaced &&
           a.sync == b.sync && a.locked == b.locked && a.preset == b.preset;
}

const char* logPresetName(uint8_t id) {
    switch (id) {
        case 0x01: case 0x11: return PSTR("1280x960");
        case 0x02: case 0x12: return PSTR("1280x1024");
        case 0x03: case 0x13: return PSTR("1280x720");
        case 0x05: case 0x15: return PSTR("1920x1080");
        case 0x07: case 0x17: return PSTR("1920x1200");
        case 0x06: case 0x16: return PSTR("downscale");
        case 0x04: return PSTR("720x480");
        case 0x14: return PSTR("768x576");
        default: return PSTR("bypass");
    }
}

size_t logSignalText(const LogSignalInfo& s, char* out, size_t n) {
    int w;
    char preset[12];
    strncpy_P(preset, logPresetName(s.preset), sizeof(preset) - 1);
    preset[sizeof(preset) - 1] = '\0';
    if (s.none) w = snprintf_P(out, n, PSTR("signal: none on %s"), inputLongName(s.input));
    else
        w = snprintf_P(out, n, PSTR("signal: %u lines %s %u.%u Hz, %s, frame lock %s, preset %s, out %u.%u Hz (%s)"),
                     (unsigned)s.lines, s.interlaced ? "interlaced" : "progressive", (unsigned)(s.srcHz10 / 10),
                     (unsigned)(s.srcHz10 % 10), s.sync == 1 ? "CSync" : s.sync == 2 ? "SOG" : "HV",
                     s.locked ? "on" : "off", preset, (unsigned)(s.outHz10 / 10),
                     (unsigned)(s.outHz10 % 10), inputLongName(s.input));
    if (w < 0) return 0;
    return (size_t)w < n ? (size_t)w : n ? n - 1 : 0;
}

bool LogSignalWatch::update(const LogSignalInfo& s, uint32_t now) {
    if (!s.none && (s.srcHz10 < kMinHz10 || s.srcHz10 > kMaxHz10)) {   // a source switching off: no line for it
        haveCand_ = false;
        return false;
    }
    if (!haveCand_ || !(s == cand_)) {
        cand_ = s;
        haveCand_ = true;
        since_ = now;
        return false;
    }
    if (now - since_ < kStableMs || (haveLogged_ && cand_ == logged_)) return false;
    logged_ = cand_;
    haveLogged_ = true;
    return true;
}

uint32_t LogSyncReturn::update(uint16_t noSyncCounter, bool sourceLost, uint32_t nowMs) {
    if (sourceLost) { active_ = false; return 0; }
    if (!active_) {
        if (noSyncCounter >= 2) { active_ = true; since_ = nowMs; }
        return 0;
    }
    if (noSyncCounter != 0) return 0;
    active_ = false;
    uint32_t ms = nowMs - since_;
    return ms ? ms : 1;
}
