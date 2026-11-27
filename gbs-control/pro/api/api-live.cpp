#include <stdio.h>
#include "../util/pgm.h"
#include <string.h>
#include "api-live.h"
#include "../ui/menu-types.h"

// Index = MenuLive; must follow the enum (static_assert below)
#define LIVE_NAMES "none\0adc_gain\0position\0size\0borders\0htotal\0pll_div\0sdram\0osr\0adc_filter\0" \
    "debug_view\0freeze\0if_offset\0sog\0out_rate\0syncwatcher\0osd_always_on\0wifi_rssi\0osd_test\0" \
    "print_info\0ir_log\0sync_polarity\0ota\0border_pos\0sync_limit"
static const char kLiveNames[] PROGMEM = LIVE_NAMES;
static_assert(PGM_LIST_COUNT(LIVE_NAMES) == LV_COUNT, "kLiveNames follows MenuLive");
static_assert(LV_COUNT <= 32, "validMask holds one bit per MenuLive");

static bool isPair(uint8_t lv) { return lv == LV_POSITION || lv == LV_SIZE || lv == LV_BORDERS || lv == LV_BORDER_POS; }

size_t liveJson(const int32_t* v, uint32_t validMask, uint32_t ms, char* out, size_t cap) {
    if (!cap) return 0;
    size_t n = 0;
    auto put = [&](int w) { if (w < 0 || n + (size_t)w >= cap) { n = cap; return; } n += (size_t)w; };
    put(snprintf_P(out, cap, PSTR("{\"at\":%lu,\"values\":{"), (unsigned long)ms));
    bool first = true;
    for (uint8_t i = LV_NONE + 1; i < LV_COUNT && n < cap; i++) {
        if (!(validMask & (1u << i))) continue;
        const char* sep = first ? "" : ",";
        first = false;
        char name[16];
        static_assert(PGM_LIST_MAXLEN(LIVE_NAMES) < sizeof(name), "name holds the longest name");
        pgmListCopy(name, sizeof(name), kLiveNames, i, LV_COUNT);
        if (isPair(i)) put(snprintf_P(out + n, cap - n, PSTR("%s\"%s\":[%ld,%ld]"), sep, name, (long)((uint32_t)v[i] >> 16),
                                    (long)(v[i] & 0xffff)));
        else put(snprintf_P(out + n, cap - n, PSTR("%s\"%s\":%ld"), sep, name, (long)v[i]));
    }
    if (n < cap) put(snprintf_P(out + n, cap - n, PSTR("}}")));
    if (n >= cap) { out[0] = '\0'; return 0; }
    return n;
}
