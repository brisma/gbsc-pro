#include "log-ring.h"
#include <stdio.h>
#include <string.h>
#include "../util/pgm.h"

static const char* const kCatNames[LC_COUNT] = {"gbs", "sys", "input", "scan", "sync", "preset", "profile", "adv",
                                                "link", "api", "wifi", "update", "menu"};

const char* logCatName(uint8_t cat) { return cat < LC_COUNT ? kCatNames[cat] : "?"; }

bool logSeqAfter(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b) > 0; }

void LogRing::readHeader(uint16_t off, LogRecord& r) const {
    r.seq = (uint16_t)(byteAt(off) | byteAt((uint16_t)(off + 1)) << 8);
    r.ms = 0;
    for (uint8_t i = 0; i < 4; i++) r.ms |= (uint32_t)byteAt((uint16_t)(off + 2 + i)) << (8 * i);
    r.cat = byteAt((uint16_t)(off + 6));
    r.level = byteAt((uint16_t)(off + 7));
    r.len = byteAt((uint16_t)(off + 8));
}

void LogRing::dropOldest() {
    uint16_t size = (uint16_t)(kHeader + byteAt((uint16_t)(tail_ + 8)));
    tail_ = (uint16_t)((tail_ + size) % kBytes);
    used_ = (uint16_t)(used_ - size);
    count_--;
}

uint16_t LogRing::append(uint32_t ms, uint8_t cat, uint8_t level, const char* text) {
    size_t len = strlen(text);
    if (len > LOG_TEXT_MAX) len = LOG_TEXT_MAX;
    uint16_t size = (uint16_t)(kHeader + len);
    while (count_ && kBytes - used_ < size) dropOldest();
    uint16_t off = (uint16_t)((tail_ + used_) % kBytes);
    uint16_t seq = next_;
    next_ = next_ == 65535 ? 1 : (uint16_t)(next_ + 1);
    put(off, (uint8_t)seq);
    put((uint16_t)(off + 1), (uint8_t)(seq >> 8));
    for (uint8_t i = 0; i < 4; i++) put((uint16_t)(off + 2 + i), (uint8_t)(ms >> (8 * i)));
    put((uint16_t)(off + 6), cat);
    put((uint16_t)(off + 7), level);
    put((uint16_t)(off + 8), (uint8_t)len);
    for (size_t i = 0; i < len; i++) put((uint16_t)(off + kHeader + i), (uint8_t)text[i]);
    used_ = (uint16_t)(used_ + size);
    count_++;
    return seq;
}

bool LogRing::readAfter(int32_t since, LogRecord& out, uint16_t* dropped) const {
    if (!count_) return false;
    LogRecord h;
    readHeader(tail_, h);
    uint16_t oldest = h.seq, last = lastSeq();
    bool fromOldest = since <= 0 || logSeqAfter((uint16_t)since, last);
    uint16_t s = (uint16_t)since;
    if (!fromOldest && !logSeqAfter(last, s)) return false;            // nothing newer
    uint16_t off = tail_;
    if (!fromOldest && logSeqAfter(oldest, (uint16_t)(s + 1))) {        // lines after since already overwritten
        uint16_t lost = (uint16_t)(oldest - s - 1);
        if (oldest < s && lost) lost--;                                  // the range crossed the unused seq 0
        if (dropped) *dropped = (uint16_t)(*dropped + lost);
        fromOldest = true;
    }
    if (!fromOldest) {
        for (uint16_t i = 0; i < count_; i++) {                           // walk to the first record after since
            readHeader(off, h);
            if (logSeqAfter(h.seq, s)) break;
            off = (uint16_t)((off + kHeader + h.len) % kBytes);
        }
    }
    readHeader(off, out);
    for (uint8_t i = 0; i < out.len; i++) out.text[i] = (char)byteAt((uint16_t)(off + kHeader + i));
    out.text[out.len] = '\0';
    return true;
}

size_t logFormatLine(const LogRecord& r, char* out, size_t n) {
    char tag[16];
    snprintf(tag, sizeof(tag), "[%s]", logCatName(r.cat));
    int w = snprintf_P(out, n, PSTR("[%6lu.%03lu] %-10s %s\n"), (unsigned long)(r.ms / 1000), (unsigned long)(r.ms % 1000), tag,
                     r.text);
    if (w < 0) return 0;
    return (size_t)w < n ? (size_t)w : n ? n - 1 : 0;
}

size_t logRecordJson(const LogRecord& r, char* out, size_t n) {
    int w = snprintf_P(out, n, PSTR("{\"s\":%u,\"t\":%lu,\"c\":\"%s\",\"l\":\"%s\",\"m\":\""), (unsigned)r.seq,
                     (unsigned long)r.ms, logCatName(r.cat), r.level == LL_DEBUG ? "debug" : "info");
    if (w < 0 || (size_t)w >= n) return 0;
    size_t k = (size_t)w;
    for (uint8_t i = 0; i < r.len; i++) {
        uint8_t c = (uint8_t)r.text[i];
        char esc[8];
        if (c == '"' || c == '\\') { esc[0] = '\\'; esc[1] = (char)c; esc[2] = '\0'; }
        else if (c < 0x20) snprintf(esc, sizeof(esc), "\\u%04x", c);
        else { esc[0] = c >= 0x80 ? '?' : (char)c; esc[1] = '\0'; }
        size_t e = strlen(esc);
        if (k + e >= n) return 0;
        memcpy(out + k, esc, e);
        k += e;
    }
    if (k + 3 > n) return 0;
    out[k++] = '"';
    out[k++] = '}';
    out[k] = '\0';
    return k;
}

void logJsonBegin(LogJsonCursor& c, int32_t since, int32_t boot, const LogRing& ring) {
    logJsonBegin(c, boot >= 0 && (uint16_t)boot != ring.boot() ? -1 : since);
}

void logJsonBegin(LogJsonCursor& c, int32_t since) {
    c.since = since;
    c.cursor = -1;
    c.dropped = 0;
    c.phase = 0;
    c.len = c.off = 0;
}

// Next piece of text into c.pending; false when the body is complete
static bool logJsonNext(LogJsonCursor& c, const LogRing& ring) {
    c.off = 0;
    if (c.phase == 0) {
        c.len = (uint16_t)snprintf(c.pending, sizeof(c.pending), "{\"lines\":[");
        c.phase = 1;
        return true;
    }
    if (c.phase == 1) {
        LogRecord r;
        int32_t from = c.cursor >= 0 ? c.cursor : c.since;
        if (ring.readAfter(from, r, &c.dropped)) {
            size_t k = 0;
            if (c.cursor >= 0) c.pending[k++] = ',';
            size_t w = logRecordJson(r, c.pending + k, sizeof(c.pending) - k);
            c.len = (uint16_t)(w ? k + w : 0);
            c.cursor = r.seq;
            if (c.len) return true;
        }
        c.phase = 2;
    }
    if (c.phase == 2) {
        long next = c.cursor >= 0 ? c.cursor : c.since >= 0 ? c.since : ring.count() ? ring.lastSeq() : 0;
        c.len = (uint16_t)snprintf_P(c.pending, sizeof(c.pending), PSTR("],\"next\":%ld,\"dropped\":%u,\"boot\":%u}"),
                                     next, (unsigned)c.dropped, (unsigned)ring.boot());
        c.phase = 3;
        return true;
    }
    c.len = 0;   // done: nothing more on later fills
    return false;
}

size_t logJsonFill(LogJsonCursor& c, const LogRing& ring, char* buf, size_t max) {
    size_t k = 0;
    while (k < max) {
        if (c.off >= c.len && !logJsonNext(c, ring)) break;
        size_t n = c.len - c.off;
        if (n > max - k) n = max - k;
        memcpy(buf + k, c.pending + c.off, n);
        c.off = (uint16_t)(c.off + n);
        k += n;
    }
    return k;
}
