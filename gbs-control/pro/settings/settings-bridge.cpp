#include <string.h>
#include "settings-pgm.h"
#include "settings-registry.h"
#include "settings-bridge.h"

#define SETTINGS_BRIDGE_TABLE(B) \
    B(auto_gain,   "input.auto_gain",          BRIDGE_SERIAL, "*T") \
    B(step,        "picture.step_response",    BRIDGE_SERIAL, "*V") \
    B(match,       "output.match_preset",      BRIDGE_SERIAL, "*Z") \
    B(scanlines,   "picture.scanlines",        BRIDGE_USER,   "*7") \
    B(line_filter, "picture.line_filter",      BRIDGE_USER,   "*m") \
    B(full_height, "screen.full_height",       BRIDGE_USER,   "*v") \
    B(force60,     "output.force_60hz",        BRIDGE_USER,   "*0") \
    B(ext_clock,   "system.ext_clock",         BRIDGE_USER,   "*X") \
    B(adc_cal,     "system.adc_calibration",   BRIDGE_USER,   "*w") \
    B(ftl,         "output.frame_lock",        BRIDGE_USER,   "*5") \
    B(ftl_method,  "output.frame_lock_method", BRIDGE_USER,   "*i") \
    B(deint,       "picture.deinterlace",      BRIDGE_USER,   "0:r,1:q") \
    B(preset,      "output.preset",            BRIDGE_USER,   "5:s,4:p,0:f,3:g,1:h")

#define BRIDGE_STR(id, key, q, map) \
    static const char kBKey_##id[] SET_PROGMEM = key; static const char kBMap_##id[] SET_PROGMEM = map;
SETTINGS_BRIDGE_TABLE(BRIDGE_STR)
#undef BRIDGE_STR

struct BridgeRow { const char* key; uint8_t queue; const char* map; };
#define BRIDGE_ROW(id, key, q, map) { kBKey_##id, q, kBMap_##id },
static const BridgeRow kBridge[] SET_PROGMEM = { SETTINGS_BRIDGE_TABLE(BRIDGE_ROW) };
#undef BRIDGE_ROW

bool settingsBridgeLookup(const SettingDef& d, uint16_t current, uint16_t target, SettingsBridgeHit& hit) {
    char key[48];
    settingsKeyCopy(d, key, sizeof(key));
    for (size_t i = 0; i < sizeof(kBridge) / sizeof(kBridge[0]); i++) {
        BridgeRow r; memcpy_P(&r, &kBridge[i], sizeof(r));
        if (strcmp_P(key, r.key) != 0) continue;
        char map[40];
        strncpy_P(map, r.map, sizeof(map) - 1);
        map[sizeof(map) - 1] = '\0';
        hit.queue = r.queue;
        if (map[0] == '*') {
            if (current == target) return false;
            hit.ch = map[1];
            return true;
        }
        for (const char* p = map; *p;) {
            uint16_t v = 0;
            while (*p >= '0' && *p <= '9') v = (uint16_t)(v * 10 + (*p++ - '0'));
            if (*p != ':' || !p[1]) return false;
            char ch = p[1];
            p += 2;
            if (v == target) { hit.ch = ch; return true; }
            if (*p == ',') p++;
        }
        return false;
    }
    return false;
}

uint16_t BridgePending::effective(uint8_t index, uint16_t current, uint32_t now) {
    for (uint8_t i = 0; i < kSlots; i++) {
        Slot& s = s_[i];
        if (!s.used || s.index != index) continue;
        if (current == s.target || (uint32_t)(now - s.at) >= kTimeoutMs) { s.used = false; return current; }
        return s.target;
    }
    return current;
}

bool BridgePending::peek(uint8_t index, uint16_t current, uint32_t now, uint16_t& target) const {
    for (uint8_t i = 0; i < kSlots; i++) {
        const Slot& s = s_[i];
        if (!s.used || s.index != index) continue;
        if (current == s.target || (uint32_t)(now - s.at) >= kTimeoutMs) return false;
        target = s.target;
        return true;
    }
    return false;
}

void BridgePending::set(uint8_t index, uint16_t target, uint32_t now) {
    uint8_t pick = 0;
    for (uint8_t i = 0; i < kSlots; i++) {
        if (s_[i].used && s_[i].index == index) { pick = i; break; }
        if (!s_[i].used) pick = i;
        else if (s_[pick].used && (uint32_t)(now - s_[i].at) > (uint32_t)(now - s_[pick].at)) pick = i;
    }
    s_[pick] = {true, index, target, now};
}
