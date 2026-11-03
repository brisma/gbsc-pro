#include <stdio.h>
#include "event-ring.h"
#include "log-ring.h"

uint16_t EventRing::add(uint32_t ms, uint8_t type, uint8_t a, uint8_t b) {
    uint16_t seq = next_;
    ev_[head_] = {seq, ms, type, a, b};
    head_ = (uint8_t)((head_ + 1) % kCap);
    if (count_ < kCap) count_++;
    next_ = next_ == 65535 ? 1 : (uint16_t)(next_ + 1);
    return seq;
}

bool EventRing::readAfter(uint16_t since, Event& out) const {
    if (!count_) return false;
    uint16_t last = lastSeq();
    bool fromOldest = since == 0 || logSeqAfter(since, last);
    uint8_t oldest = (uint8_t)((head_ + kCap - count_) % kCap);
    for (uint8_t i = 0; i < count_; i++) {
        const Event& e = ev_[(oldest + i) % kCap];
        if (fromOldest || logSeqAfter(e.seq, since)) { out = e; return true; }
    }
    return false;
}

const char* eventTypeName(uint8_t type) {
    static const char* const kNames[] = {"input_found", "input_lost", "input_changed", "profile_loaded",
                                         "profile_saved", "resolution", "update_offered", "update_done"};
    return type >= EV_INPUT_FOUND && type <= EV_UPDATE_DONE ? kNames[type - 1] : "unknown";
}

size_t eventJson(const Event& e, char* out, size_t cap) {
    int n = snprintf(out, cap, "{\"seq\":%u,\"ms\":%lu,\"type\":\"%s\",\"a\":%u,\"b\":%u}", (unsigned)e.seq,
                     (unsigned long)e.ms, eventTypeName(e.type), (unsigned)e.a, (unsigned)e.b);
    if (n < 0 || (size_t)n >= cap) { if (cap) out[0] = '\0'; return 0; }
    return (size_t)n;
}

size_t eventsJson(const EventRing& r, uint16_t since, uint16_t boot, char* out, size_t cap) {
    int w = snprintf(out, cap, "{\"boot\":%u,\"events\":[", (unsigned)boot);
    if (w < 0 || (size_t)w >= cap) return 0;
    size_t n = (size_t)w;
    Event e;
    bool first = true;
    while (r.readAfter(since, e)) {
        if (!first) { if (n + 1 >= cap) return 0; out[n++] = ','; }
        size_t k = eventJson(e, out + n, cap - n);
        if (!k) return 0;
        n += k;
        since = e.seq;
        first = false;
    }
    if (n + 3 > cap) return 0;
    out[n++] = ']'; out[n++] = '}'; out[n] = '\0';
    return n;
}

bool eventResolutionChanged(uint16_t* last, uint8_t preset, uint8_t input) {
    uint16_t key = (uint16_t)(preset << 8 | input);
    if (*last == key) return false;
    *last = key;
    return true;
}
