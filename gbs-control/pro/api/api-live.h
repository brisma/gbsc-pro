// GET /api/live (pure, host-tested): the menu's MenuLive readbacks for the web rows (gain, SOG, position, the
// Developer toggles), read by the loop into a cache while a page asks (api-device.cpp apiLiveRefresh)
#pragma once
#include <stdint.h>
#include <stddef.h>

static const size_t LIVE_JSON_MAX = 640;   // every value at its widest (test_live_json)

// {"at":<ms>,"values":{"<name>":<value>,...}}; v indexed by MenuLive (LV_COUNT entries), bit i of validMask = v[i]
// read; names = MenuLive without LV_, lowercase; position/size/borders ((h << 16) | v) as [h,v]; LV_NONE never.
// Returns the length, 0 when it does not fit (out then holds "")
size_t liveJson(const int32_t* v, uint32_t validMask, uint32_t ms, char* out, size_t cap);
