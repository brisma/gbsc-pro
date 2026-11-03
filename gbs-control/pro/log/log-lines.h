// Console log: upstream prints (gbs-control.ino through SerialM) arrive as bytes; the splitter turns them into lines
// with a category and level, and the sync watcher's marks (. * ` digits) become counters summarised in one line.
// Pure, host-tested (test_log_lines).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "log-ring.h"

struct LogTag { uint8_t cat, level; bool ring; };   // ring false: WebSocket/USB only (the printInfo stream)
typedef void (*LogLineOut)(const LogTag& tag, const char* text, void* ctx);

class LogLineSplitter {
public:
    static const uint32_t kPartialMs = 1000;
    // The next non-empty line gets this tag; pending text without a newline goes out first with its own (a debug tag
    // must not hide it), and upstream prints that start with "\n" just end an empty line
    void tagNext(const LogTag& t, LogLineOut out, void* ctx);
    // '\n', "\r\n" and a lone '\r' (ArduinoOTA progress) end a line; empty lines are dropped
    void feed(const char* data, size_t n, uint32_t now, LogLineOut out, void* ctx);
    void tick(uint32_t now, LogLineOut out, void* ctx);   // text without a newline for kPartialMs goes out as a line
    void flush(LogLineOut out, void* ctx);                // pending text goes out now
private:
    void emit(LogLineOut out, void* ctx, bool lineEnds);
    static LogTag defaultTag() { return {LC_GBS, LL_INFO, true}; }
    char buf_[LOG_TEXT_MAX + 1] = {0};
    uint8_t len_ = 0;
    LogTag tag_ = defaultTag();      // tag of the line in progress
    LogTag armedTag_ = defaultTag();
    bool armed_ = false, cr_ = false;   // cr_: a '\r' waits to see whether '\n' follows
    uint32_t lastAt_ = 0;
};

enum : uint8_t { LM_SYNC_DROP, LM_SP_REFRESH, LM_SOG_BAD, LM_MODE_WATCH, LM_ACTIVITY_LOST, LM_COUNT };

// Upstream detection's "Activity detected" ... " lost.." cycle: one that starts within this time of the last loss is a
// repeat (a SCART source on a VGA path loops about once a second) and becomes an LM_ACTIVITY_LOST count
static const uint32_t LOG_ACTIVITY_REPEAT_MS = 2000;
inline bool logActivityRepeat(bool lostBefore, uint32_t now, uint32_t lostAt) {
    return lostBefore && now - lostAt < LOG_ACTIVITY_REPEAT_MS;
}

class LogMarks {
public:
    static const uint32_t kQuietMs = 1000;
    void mark(uint8_t kind, uint32_t now);
    bool pending() const;
    bool due(uint32_t now) const { return pending() && now - lastAt_ >= kQuietMs; }
    // "no sync x12, SP refresh x2, SOG bad x3, mode watch x5" (only the non-zero counts, in this order); resets
    size_t take(char* out, size_t n);
private:
    uint16_t counts_[LM_COUNT] = {0};
    uint32_t lastAt_ = 0;
};
