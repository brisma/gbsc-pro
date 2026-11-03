#include "cdc_ring.h"
#include <string.h>

void cdcRingInit(CdcRing* r, uint8_t* mem, size_t size) {
    r->buf = mem; r->size = size; r->head = r->tail = 0; r->dropped = 0;
}

size_t cdcRingUsed(const CdcRing* r) {
    size_t h = r->head, t = r->tail;
    return h >= t ? h - t : r->size - t + h;
}

size_t cdcRingFree(const CdcRing* r) { return r->size - 1 - cdcRingUsed(r); }

size_t cdcRingWrite(CdcRing* r, const void* data, size_t n) {
    const uint8_t* s = (const uint8_t*)data;
    size_t room = cdcRingFree(r), k = n < room ? n : room;
    size_t h = r->head;
    for (size_t i = 0; i < k; i++) { r->buf[h] = s[i]; h = h + 1 == r->size ? 0 : h + 1; }
    r->head = h;                                   /* published after the bytes */
    if (k < n) r->dropped += (uint32_t)(n - k);
    return k;
}

size_t cdcRingPeek(const CdcRing* r, const uint8_t** p, size_t max) {
    size_t h = r->head, t = r->tail;
    size_t n = h >= t ? h - t : r->size - t;       /* contiguous up to the end of the buffer */
    if (n > max) n = max;
    *p = r->buf + t;
    return n;
}

void cdcRingConsume(CdcRing* r, size_t n) {
    size_t t = r->tail + n;
    if (t >= r->size) t -= r->size;
    r->tail = t;
}

size_t cdcRingRead(CdcRing* r, uint8_t* out, size_t max) {
    size_t total = 0;
    while (total < max) {
        const uint8_t* p;
        size_t n = cdcRingPeek(r, &p, max - total);
        if (!n) break;
        memcpy(out + total, p, n);
        cdcRingConsume(r, n);
        total += n;
    }
    return total;
}
