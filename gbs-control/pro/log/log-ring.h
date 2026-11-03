// Console log core: the RAM ring with the latest lines, the
// formatted line and the JSON record of /api/log. Pure, host-tested (test_log); the device glue is log.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum LogCat : uint8_t { LC_GBS, LC_SYS, LC_INPUT, LC_SCAN, LC_SYNC, LC_PRESET, LC_PROFILE, LC_ADV, LC_LINK,
                        LC_API, LC_WIFI, LC_UPDATE, LC_MENU, LC_COUNT };
enum LogLevel : uint8_t { LL_INFO, LL_DEBUG };   // debug: only with system.devmode on
static const uint8_t LOG_TEXT_MAX = 128;         // longer text is cut (upstream lines are split before, log-lines.h)

const char* logCatName(uint8_t cat);             // "gbs", ...; "?" out of range

struct LogRecord {
    uint16_t seq;
    uint32_t ms;                                 // millis() when written
    uint8_t cat, level, len;
    char text[LOG_TEXT_MAX + 1];
};

// a is a later seq than b (seq wraps 65535 -> 0; the ring holds far fewer than 32768 records)
bool logSeqAfter(uint16_t a, uint16_t b);

// Fixed byte ring: a 9-byte header (seq, ms, cat, level, len) plus the text without NUL per record; the oldest whole
// records make room for a new one. No heap.
class LogRing {
public:
    static const uint16_t kBytes = 2048;
    // Returns the seq: 1..65535, then 1 again (0 is never used: since=0 always means everything)
    uint16_t append(uint32_t ms, uint8_t cat, uint8_t level, const char* text);
    // The oldest record with a seq after `since` (-1 or 0: from the oldest; a since ahead of lastSeq(), the device
    // restarted: from the oldest too). *dropped += the records after `since` already overwritten.
    bool readAfter(int32_t since, LogRecord& out, uint16_t* dropped) const;
    uint16_t lastSeq() const { return next_ == 1 ? 65535 : (uint16_t)(next_ - 1); }   // only when count() > 0
    uint16_t count() const { return count_; }
    // Changes at every start (the device sets it): a client's since from another boot means nothing here
    void setBoot(uint16_t id) { boot_ = id; }
    uint16_t boot() const { return boot_; }
private:
    static const uint8_t kHeader = 9;
    uint8_t byteAt(uint16_t off) const { return buf_[off % kBytes]; }
    void put(uint16_t off, uint8_t v) { buf_[off % kBytes] = v; }
    void readHeader(uint16_t off, LogRecord& r) const;
    void dropOldest();
    uint8_t buf_[kBytes] = {0};
    uint16_t tail_ = 0;      // offset of the oldest record
    uint16_t used_ = 0;      // bytes in use
    uint16_t count_ = 0;
    uint16_t next_ = 1;      // seq of the next record
    uint16_t boot_ = 0;
};

// "[   123.456] [scan]     text\n": seconds since boot, the category tag padded to 10 characters plus one space
size_t logFormatLine(const LogRecord& r, char* out, size_t n);
// {"s":812,"t":123456,"c":"scan","l":"info","m":"text"}; quotes, backslash and control bytes escaped, bytes >= 0x80
// as '?'; 0 when it does not fit (a LOG_TEXT_MAX text always fits in LogJsonCursor::pending)
size_t logRecordJson(const LogRecord& r, char* out, size_t n);

// GET /api/log body, streamed in chunks: {"lines":[...],"next":N,"dropped":D,"boot":B}. The filler runs in the network context
// between loop passes while the loop keeps appending, so one whole record is copied at a time and "next"/"dropped"
// come last. State per response (the device keeps it on the heap for the reply's life).
struct LogJsonCursor {
    int32_t since;         // as asked: -1 everything
    int32_t cursor;        // last seq copied, -1 none yet
    uint16_t dropped;
    uint8_t phase;         // 0 head, 1 lines, 2 tail, 3 done
    char pending[LOG_TEXT_MAX * 6 + 72];   // the worst-case escaped record (test_json_worst_case_fits_pending)
    uint16_t len, off;
};
void logJsonBegin(LogJsonCursor& c, int32_t since);
// since from the client's boot id: another boot (the device restarted) reads everything
void logJsonBegin(LogJsonCursor& c, int32_t since, int32_t boot, const LogRing& ring);
size_t logJsonFill(LogJsonCursor& c, const LogRing& ring, char* buf, size_t max);   // 0: the body is complete
