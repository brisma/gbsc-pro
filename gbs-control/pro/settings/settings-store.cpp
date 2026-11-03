#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "options.h"
#include "settings-registry.h"
#include "settings-store.h"
#include "../platform/platform-caps.h"
#include "../update/update-rtc.h"

static void emitNumber(SettingsSink sink, void* ctx, uint16_t v) {
    char b[8]; SettingDef dummy{}; dummy.type = SettingType::U16;
    settingsFormat(dummy, v, b, sizeof(b)); sink(b, ctx);
}

void settingsSerialize(const userOptions& u, SettingScope scope, SettingsSink sink, void* ctx) {
    settingsSerializeEx(u, scope, true, sink, ctx);
}

void settingsSerializeEx(const userOptions& u, SettingScope scope, bool withSchema, SettingsSink sink, void* ctx) {
    if (withSchema) { sink("schema=", ctx); emitNumber(sink, ctx, SETTINGS_SCHEMA_VERSION); sink("\n", ctx); }
    SettingDef d; char key[48], val[40];
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (d.scope != scope) continue;
        settingsKeyCopy(d, key, sizeof(key));
        settingsFormat(d, settingsGet(u, d), val, sizeof(val));
        sink(key, ctx); sink("=", ctx); sink(val, ctx); sink("\n", ctx);
    }
    StrSettingDef sd;
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        settingsStrDefAt(i, sd);
        if (sd.scope != scope) continue;
        settingsKeyCopyStr(sd, key, sizeof(key));
        sink(key, ctx); sink("=", ctx); sink(settingsStrGet(u, sd), ctx); sink("\n", ctx);
    }
}

void settingsLoadBegin(userOptions& u, SettingScope scope, SettingsLoadReport& rep) {
    memset(&rep, 0, sizeof(rep));
    SettingDef d;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (d.scope == scope) settingsPut(u, d, d.def);
    }
    StrSettingDef sd;
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        settingsStrDefAt(i, sd);
        if (sd.scope == scope) ((char*)&u + sd.offset)[0] = '\0';
    }
}

void settingsLoadLine(userOptions& u, SettingScope scope, const char* line, SettingsLoadReport& rep) {
    char buf[128];
    size_t n = strlen(line);
    if (n >= sizeof(buf)) return;                       // overlong line: skip
    memcpy(buf, line, n + 1);
    char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '#' || *p == '\r') return;  // blank or comment
    char* eq = strchr(p, '=');
    if (!eq) { rep.invalid++; return; }
    *eq = '\0';
    char* end = eq - 1;
    while (end >= p && (*end == ' ' || *end == '\t')) *end-- = '\0';
    const char* value = eq + 1;
    if (!strcmp(p, "schema")) {
        SettingDef s{}; s.type = SettingType::U16; s.max = 65535;
        uint16_t v; if (settingsParse(s, value, v)) rep.schema = v;
        return;
    }
    int sidx = settingsStrFind(p);
    if (sidx >= 0) {
        StrSettingDef sd; settingsStrDefAt((uint8_t)sidx, sd);
        if (sd.scope != scope) { rep.unknown++; return; }
        char val[80];
        size_t vn = strlen(value);
        if (vn >= sizeof(val)) { rep.invalid++; return; }
        memcpy(val, value, vn + 1);
        if (vn && val[vn - 1] == '\r') val[--vn] = '\0';   // verbatim otherwise: a password may end with a space
        if (settingsStrPut(u, sd, val)) rep.applied++; else rep.invalid++;
        return;
    }
    int idx = settingsFind(p);
    if (idx < 0) { rep.unknown++; return; }
    SettingDef d; settingsDefAt((uint8_t)idx, d);
    if (d.scope != scope) { rep.unknown++; return; }
    uint16_t v;
    if (!settingsParse(d, value, v)) { rep.invalid++; return; }   // keeps the default set in LoadBegin
    if (!settingsOptionAvailable(d, v)) { rep.invalid++; return; } // option needs missing hardware: keep the default
    settingsPut(u, d, v);
    rep.applied++;
}

void settingsValuesJson(const userOptions& u, bool devmode, SettingsSink sink, void* ctx) {
    sink("{", ctx);
    SettingDef d; char key[48];
    bool first = true;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (!settingsAvailable(d, devmode)) continue;
        settingsKeyCopy(d, key, sizeof(key));
        if (!first) sink(",", ctx);
        first = false;
        sink("\"", ctx); sink(key, ctx); sink("\":", ctx);
        emitNumber(sink, ctx, settingsGet(u, d));
    }
    StrSettingDef sd;
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        settingsStrDefAt(i, sd);
        settingsKeyCopyStr(sd, key, sizeof(key));
        if (!first) sink(",", ctx);
        first = false;
        const char* v = settingsStrGet(u, sd);
        if (sd.flags & SETTING_FLAG_SECRET) v = *v ? "***" : "";
        sink("\"", ctx); sink(key, ctx); sink("\":\"", ctx); sink(v, ctx); sink("\"", ctx);
    }
    sink("}", ctx);
}

static const char* typeName(SettingType t) {
    switch (t) { case SettingType::Bool: return "bool"; case SettingType::U8: return "u8";
                 case SettingType::U16: return "u16"; default: return "enum"; }
}

// One schema entry: n < settingsCount() is a numeric setting (nothing emitted when unavailable), then the
// string settings; false past the last one. The web streams these one by one (the whole schema is too big
// for the ESP heap).
bool settingsSchemaEntry(uint16_t n, bool devmode, SettingsSink sink, void* ctx) {
    char key[48], name[32];
    if (n < settingsCount()) {
        SettingDef d;
        settingsDefAt((uint8_t)n, d);
        if (!settingsAvailable(d, devmode)) return true;
        settingsKeyCopy(d, key, sizeof(key));
        sink("{\"key\":\"", ctx); sink(key, ctx);
        sink("\",\"type\":\"", ctx); sink(typeName(d.type), ctx);
        sink("\",\"scope\":\"", ctx); sink(d.scope == SettingScope::Device ? "device" : "profile", ctx);
        sink("\",\"min\":", ctx); emitNumber(sink, ctx, d.min);
        sink(",\"max\":", ctx); emitNumber(sink, ctx, d.max);
        sink(",\"def\":", ctx); emitNumber(sink, ctx, d.def);
        sink(",\"caps\":", ctx); emitNumber(sink, ctx, settingsCaps(d));
        if (d.type == SettingType::Enum) {
            sink(",\"options\":[", ctx);
            uint16_t v; uint8_t caps;
            bool firstOpt = true;
            for (uint8_t k = 0; settingsEnumAtEx(d, k, name, sizeof(name), v, caps); k++) {
                if ((caps & platformCaps()) != caps) continue;
                if (!firstOpt) sink(",", ctx);
                firstOpt = false;
                sink("{\"n\":\"", ctx); sink(name, ctx); sink("\",\"v\":", ctx); emitNumber(sink, ctx, v); sink("}", ctx);
            }
            sink("]", ctx);
        }
        if (d.flags & SETTING_FLAG_DEVICE_ONLY) sink(",\"ro\":1", ctx);
        sink("}", ctx);
        return true;
    }
    uint16_t i = (uint16_t)(n - settingsCount());
    if (i >= settingsStrCount()) return false;
    StrSettingDef sd;
    settingsStrDefAt((uint8_t)i, sd);
    settingsKeyCopyStr(sd, key, sizeof(key));
    sink("{\"key\":\"", ctx); sink(key, ctx);
    sink("\",\"type\":\"str\",\"scope\":\"", ctx); sink(sd.scope == SettingScope::Device ? "device" : "profile", ctx);
    sink("\",\"max\":", ctx); emitNumber(sink, ctx, sd.maxLen);
    if (sd.flags & SETTING_FLAG_DEVICE_ONLY) sink(",\"ro\":1", ctx);
    if (sd.flags & SETTING_FLAG_SECRET) sink(",\"secret\":1", ctx);
    sink("}", ctx);
    return true;
}

namespace {
struct EntryProbe { SettingsSink sink; void* ctx; bool any; bool* first; };
void probeSink(const char* t, void* c) {
    EntryProbe* p = (EntryProbe*)c;
    if (!p->any) { if (!*p->first) p->sink(",", p->ctx); *p->first = false; p->any = true; }
    p->sink(t, p->ctx);
}
}

namespace {
struct BufSink { char* buf; size_t cap; size_t len; };
void bufSink(const char* t, void* c) {
    BufSink* b = (BufSink*)c;
    while (*t && b->len + 1 < b->cap) b->buf[b->len++] = *t++;
}
}

bool SchemaStreamer::next() {
    len_ = off_ = 0;
    if (stage_ == 0) { buf_[0] = '['; len_ = 1; stage_ = 1; return true; }
    while (stage_ == 1) {
        BufSink b = {buf_ + 1, sizeof(buf_) - 1, 0};
        if (!settingsSchemaEntry(n_++, dev_, bufSink, &b)) { stage_ = 2; break; }
        if (!b.len) continue;
        if (first_) { memmove(buf_, buf_ + 1, b.len); len_ = b.len; first_ = false; }
        else { buf_[0] = ','; len_ = b.len + 1; }
        return true;
    }
    if (stage_ == 2) { buf_[0] = ']'; len_ = 1; stage_ = 3; return true; }
    return false;
}

size_t SchemaStreamer::fill(char* out, size_t max) {
    size_t w = 0;
    while (w < max) {
        if (off_ >= len_ && !next()) break;
        size_t k = len_ - off_;
        if (k > max - w) k = max - w;
        memcpy(out + w, buf_ + off_, k);
        off_ += k;
        w += k;
    }
    return w;
}

void settingsSchemaJson(bool devmode, SettingsSink sink, void* ctx) {
    sink("[", ctx);
    bool first = true;
    for (uint16_t n = 0; ; n++) {
        EntryProbe p = {sink, ctx, false, &first};
        if (!settingsSchemaEntry(n, devmode, probeSink, &p)) break;
    }
    sink("]", ctx);
}

static bool excludedFlags(uint8_t flags) { return settingKeptOutOfBackups(flags); }

void settingsRestoreBegin(userOptions& u, SettingScope scope) {
    userOptions keep = u;   // excluded keys survive a restore
    SettingsLoadReport rep;
    settingsLoadBegin(u, scope, rep);
    SettingDef d;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (d.scope == scope && excludedFlags(d.flags)) settingsPut(u, d, settingsGet(keep, d));
    }
    StrSettingDef s;
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        settingsStrDefAt(i, s);
        if (s.scope == scope && excludedFlags(s.flags)) settingsStrPut(u, s, settingsStrGet(keep, s));
    }
}

void settingsRestoreLine(userOptions& u, SettingScope scope, const char* line) {
    char key[48];
    const char* eq = strchr(line, '=');
    if (!eq) return;
    size_t n = (size_t)(eq - line);
    if (n >= sizeof(key)) return;
    memcpy(key, line, n); key[n] = '\0';
    if (settingsKeyExcludedFromBackup(key)) return;
    SettingsLoadReport rep;
    settingsLoadLine(u, scope, line, rep);
}

static void crcSink(const char* t, void* ctx) { uint32_t* c = (uint32_t*)ctx; *c = crc32Update(*c, t, strlen(t)); }

uint32_t settingsScopeCrc(const userOptions& u, SettingScope scope) {
    uint32_t c = 0;
    settingsSerializeEx(u, scope, false, crcSink, &c);
    return c;
}
