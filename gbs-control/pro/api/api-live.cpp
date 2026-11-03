#include <stdio.h>
#include <string.h>
#include "api-live.h"
#include "../ui/menu-types.h"

// Index = MenuLive; must follow the enum (static_assert below)
static const char* const kLiveNames[] = {
    "none", "adc_gain", "position", "size", "borders", "htotal", "pll_div", "sdram", "osr", "adc_filter",
    "debug_view", "freeze", "if_offset", "sog", "out_rate", "syncwatcher", "osd_always_on", "wifi_rssi", "osd_test",
    "print_info", "ir_log", "sync_polarity", "ota", "border_pos"};
static_assert(sizeof(kLiveNames) / sizeof(kLiveNames[0]) == LV_COUNT, "kLiveNames follows MenuLive");
static_assert(LV_COUNT <= 32, "validMask holds one bit per MenuLive");

static bool isPair(uint8_t lv) { return lv == LV_POSITION || lv == LV_SIZE || lv == LV_BORDERS || lv == LV_BORDER_POS; }

size_t liveJson(const int32_t* v, uint32_t validMask, uint32_t ms, char* out, size_t cap) {
    if (!cap) return 0;
    size_t n = 0;
    auto put = [&](int w) { if (w < 0 || n + (size_t)w >= cap) { n = cap; return; } n += (size_t)w; };
    put(snprintf(out, cap, "{\"at\":%lu,\"values\":{", (unsigned long)ms));
    bool first = true;
    for (uint8_t i = LV_NONE + 1; i < LV_COUNT && n < cap; i++) {
        if (!(validMask & (1u << i))) continue;
        const char* sep = first ? "" : ",";
        first = false;
        if (isPair(i)) put(snprintf(out + n, cap - n, "%s\"%s\":[%ld,%ld]", sep, kLiveNames[i], (long)((uint32_t)v[i] >> 16),
                                    (long)(v[i] & 0xffff)));
        else put(snprintf(out + n, cap - n, "%s\"%s\":%ld", sep, kLiveNames[i], (long)v[i]));
    }
    if (n < cap) put(snprintf(out + n, cap - n, "}}"));
    if (n >= cap) { out[0] = '\0'; return 0; }
    return n;
}
