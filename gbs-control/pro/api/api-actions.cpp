#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../settings/settings-pgm.h"
#include "../platform/platform-caps.h"
#include "api-actions.h"
#include "api-actions-table.h"
#include "adv-link.h"

#define ACTION_NAME(id, name, caps, flags, kind, arg) static const char kActName_##id[] SET_PROGMEM = name;
ACTIONS_TABLE(ACTION_NAME)
#undef ACTION_NAME

struct ActionRow { const char* name; uint8_t caps; uint8_t flags; uint8_t kind; uint8_t arg; };
#define ACTION_ROW(id, name, caps, flags, kind, arg) \
    { kActName_##id, (uint8_t)(caps), (uint8_t)(flags), (uint8_t)(kind), (uint8_t)(arg) },
static const ActionRow kActions[] SET_PROGMEM = { ACTIONS_TABLE(ACTION_ROW) };
#undef ACTION_ROW

uint8_t actionsCount() { return (uint8_t)(sizeof(kActions) / sizeof(kActions[0])); }

void actionsDefAt(uint8_t i, ActionDef& out) {
    static char name[40];   // single-threaded use: copied out of flash for the caller
    ActionRow r; memcpy_P(&r, &kActions[i], sizeof(r));
    strncpy_P(name, r.name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    out.name = name; out.caps = r.caps; out.flags = r.flags; out.kind = r.kind; out.arg = r.arg;
}

int actionsFind(const char* name) {
    for (uint8_t i = 0; i < actionsCount(); i++) {
        ActionRow r; memcpy_P(&r, &kActions[i], sizeof(r));
        if (strcmp_P(name, r.name) == 0) return i;
    }
    return -1;
}

bool actionsAvailable(const ActionDef& d, bool devmode) {
    if ((d.caps & platformCaps()) != d.caps) return false;
    return devmode || !(d.flags & ACTION_DEV);
}

// Byte parameter rules: exact count when minLen == maxLen; every byte <= maxValue
struct ParamRule { uint8_t af; uint8_t minLen; uint8_t maxLen; uint8_t maxValue; };
static const ParamRule kParamRules[] = {
    {AF_TERMINATION, 1, 1, 1},     // 0 off, 1 on
    {AF_TV5725_READ, 1, 1, 5},     // segment
    {AF_ADV_READ, 4, 4, 0xFF},     // dev, map, reg, count: advRegReadReqUnpack decides
    {AF_CLOCKGEN, 1, 1, 1},        // 0 Si5351 output off, 1 on
    {AF_LOOP_PAUSE, 1, 1, 100},    // ms of pause at every loop pass, 0 off
    {AF_SI5351, 1, 3, 0xFF},       // op, then op-specific bytes (si5351Lab checks them)
};
static const ParamRule kParamDefault = {0, 1, 30, 0xFF};   // ActionRequest.data

static const ParamRule& paramRule(const ActionDef& d) {
    if (d.kind == ACT_FUNC)
        for (const ParamRule& r : kParamRules)
            if (r.af == d.arg) return r;
    return kParamDefault;
}

int actionsValidateParams(const ActionDef& d, const uint8_t* data, uint8_t len) {
    if (!(d.flags & ACTION_PARAM_BYTES)) return ACT_PARAM_OK;
    const ParamRule& r = paramRule(d);
    if (len < r.minLen || len > r.maxLen) return ACT_ERR_PARAM;
    for (uint8_t i = 0; i < len; i++)
        if (data[i] > r.maxValue) return ACT_ERR_PARAM;
    if (d.kind == ACT_FUNC && d.arg == AF_ADV_READ) {
        uint8_t dev, map, reg, count;
        if (!advRegReadReqUnpack(data, len, &dev, &map, &reg, &count)) return ACT_ERR_PARAM;
    }
    return ACT_PARAM_OK;
}

void actionsListJson(bool devmode, SettingsSink sink, void* ctx) {
    sink("[", ctx);
    bool first = true;
    char num[6];
    for (uint8_t i = 0; i < actionsCount(); i++) {
        ActionDef d; actionsDefAt(i, d);
        if (!actionsAvailable(d, devmode)) continue;
        if (!first) sink(",", ctx);
        first = false;
        sink("{\"name\":\"", ctx); sink(d.name, ctx);
        sink("\",\"caps\":", ctx); snprintf(num, sizeof(num), "%u", d.caps); sink(num, ctx);
        if (d.flags & ACTION_CONFIRM) sink(",\"confirm\":1", ctx);
        if (d.flags & ACTION_PARAM_HEX) sink(",\"params\":[{\"name\":\"data\",\"type\":\"hex_triplets\",\"max\":10}]", ctx);
        if (d.flags & ACTION_PARAM_BYTES) {   // hex bytes "01" or "06,1,4"
            const ParamRule& r = paramRule(d);
            sink(",\"params\":[{\"name\":\"data\",\"type\":\"bytes\",", ctx);
            snprintf(num, sizeof(num), "%u", r.maxLen);
            sink(r.minLen == r.maxLen ? "\"len\":" : "\"max_len\":", ctx); sink(num, ctx);
            if (r.maxValue != 0xFF) { snprintf(num, sizeof(num), "%u", r.maxValue); sink(",\"max\":", ctx); sink(num, ctx); }
            sink("}]", ctx);
        }
        if (d.flags & (ACTION_PARAM_TEXT | ACTION_PARAM_OPT_TEXT)) {   // the accepted words (OTA)
            sink(",\"params\":[{\"name\":\"data\",\"type\":\"text\",", ctx);
            if (d.flags & ACTION_PARAM_OPT_TEXT) sink("\"optional\":1,", ctx);
            if (d.kind == ACT_FUNC && d.arg == AF_CHECK_CHANNEL) sink("\"values\":[\"stable\",\"beta\",\"dev\"]", ctx);
            else if (d.kind == ACT_FUNC && d.arg == AF_INSTALL_UPDATE) sink("\"values\":[\"manual\"]", ctx);
            else sink("\"values\":[]", ctx);
            sink("}]", ctx);
        }
        sink("}", ctx);
    }
    sink("]", ctx);
}

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool actionsParseHexBytes(const char* text, uint8_t* out, uint8_t maxOut, uint8_t& len) {
    len = 0;
    const char* p = text;
    while (*p) {
        int v = 0, digits = 0, h;
        while ((h = hexDigit(*p)) >= 0) { v = v * 16 + h; p++; if (++digits > 2) return false; }
        if (!digits || len >= maxOut) return false;
        out[len++] = (uint8_t)v;
        if (*p == ',') { p++; if (!*p) return false; }
        else if (*p) return false;
    }
    return len > 0;
}

bool actionsParseHexTriplets(const char* text, uint8_t* out, uint8_t maxOut, uint8_t& len) {
    return actionsParseHexBytes(text, out, maxOut, len) && len % 3 == 0;
}

bool actionsParseText(const char* text, uint8_t* out, uint8_t maxOut, uint8_t& len) {
    size_t n = text ? strlen(text) : 0;
    if (!n || n + 1 > maxOut) return false;
    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    memcpy(out, text, n + 1);
    len = (uint8_t)n;
    return true;
}
