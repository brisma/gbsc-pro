// The answer of GET /api/regmap (pure, host-tested): 404 when the firmware was built without the map
// (GBSC_NO_REGMAP=1), 304 when the browser's copy is current, 503 when the heap is too low to send it
#pragma once
#include <stdint.h>

static const uint32_t REGMAP_MIN_HEAP = 10u * 1024u;

inline int regmapStatus(uint32_t gzLen, uint32_t freeHeap, bool etagMatch) {
    if (gzLen == 0) return 404;
    if (etagMatch) return 304;
    if (freeHeap <= REGMAP_MIN_HEAP) return 503;
    return 200;
}
