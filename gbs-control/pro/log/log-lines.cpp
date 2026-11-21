#include "log-lines.h"
#include "../util/pgm.h"
#include <stdio.h>

void LogLineSplitter::emit(LogLineOut out, void* ctx, bool lineEnds) {
    if (len_) {
        buf_[len_] = '\0';
        out(tag_, buf_, ctx);
        len_ = 0;
    }
    if (lineEnds) { tag_ = defaultTag(); cr_ = false; }   // a piece of a long line keeps its tag for the rest
}

void LogLineSplitter::tagNext(const LogTag& t, LogLineOut out, void* ctx) {
    if (len_) emit(out, ctx, true);
    armed_ = true;
    armedTag_ = t;
}

void LogLineSplitter::feed(const char* data, size_t n, uint32_t now, LogLineOut out, void* ctx) {
    for (size_t i = 0; i < n; i++) {
        char c = data[i];
        if (cr_) {                                    // the byte after a '\r': "\r\n" is one end, else the '\r' was one
            cr_ = false;
            emit(out, ctx, true);
            if (c == '\n') continue;
        }
        if (c == '\r') { cr_ = true; lastAt_ = now; continue; }
        if (c == '\n') { emit(out, ctx, true); continue; }
        if (armed_) { tag_ = armedTag_; armed_ = false; }
        buf_[len_++] = c;
        lastAt_ = now;
        if (len_ >= LOG_TEXT_MAX) emit(out, ctx, false);
    }
}

void LogLineSplitter::tick(uint32_t now, LogLineOut out, void* ctx) {
    if (len_ && now - lastAt_ >= kPartialMs) emit(out, ctx, true);
}

void LogLineSplitter::flush(LogLineOut out, void* ctx) { emit(out, ctx, true); }

void LogMarks::mark(uint8_t kind, uint32_t now) {
    if (kind >= LM_COUNT) return;
    if (counts_[kind] < 0xFFFF) counts_[kind]++;
    lastAt_ = now;
}

bool LogMarks::pending() const {
    for (uint8_t i = 0; i < LM_COUNT; i++) if (counts_[i]) return true;
    return false;
}

size_t LogMarks::take(char* out, size_t n) {
#define LM_NAMES "no sync\0SP refresh\0SOG bad\0mode watch\0activity lost"
    static const char kNames[] PROGMEM = LM_NAMES;
    static_assert(PGM_LIST_COUNT(LM_NAMES) == LM_COUNT, "kNames follows the marks");
    size_t k = 0;
    if (n) out[0] = '\0';
    for (uint8_t i = 0; i < LM_COUNT; i++) {
        if (!counts_[i]) continue;
        char name[16];
        pgmListCopy(name, sizeof(name), kNames, i, LM_COUNT);
        int w = snprintf_P(out + k, n > k ? n - k : 0, PSTR("%s%s x%u"), k ? ", " : "", name, (unsigned)counts_[i]);
        if (w > 0) k = k + (size_t)w < n ? k + (size_t)w : (n ? n - 1 : 0);
        counts_[i] = 0;
    }
    return k;
}
