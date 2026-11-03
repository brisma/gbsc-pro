#include <stdio.h>
#include "api-json.h"

size_t jsonEscape(const char* in, char* out, size_t n) {
    if (!n) return 0;
    size_t o = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p; p++) {
        char esc[7];
        size_t len;
        if (*p == '"' || *p == '\\') { esc[0] = '\\'; esc[1] = (char)*p; len = 2; }
        else if (*p < 0x20) { snprintf(esc, sizeof(esc), "\\u%04x", *p); len = 6; }
        else { esc[0] = (char)*p; len = 1; }
        if (o + len >= n) break;
        for (size_t i = 0; i < len; i++) out[o++] = esc[i];
    }
    out[o] = '\0';
    return o;
}

namespace {
struct ReplayWindow { size_t skip, pos; char* out; size_t max, len; };
void replaySink(const char* t, void* c) {
    ReplayWindow* w = (ReplayWindow*)c;
    for (; *t; t++, w->pos++) {
        if (w->pos < w->skip) continue;
        if (w->len >= w->max) return;
        w->out[w->len++] = *t;
    }
}
}

size_t jsonReplayFill(JsonReplay& r, char* out, size_t max) {
    ReplayWindow w = {r.sent, 0, out, max, 0};
    r.build(replaySink, &w, r.state);
    r.sent += w.len;
    return w.len;
}

size_t regsHexFormat(const uint8_t* v, size_t n, char* out, size_t cap) {
    static const char kHex[] = "0123456789abcdef";
    if (cap < n * 2 + 1) { if (cap) out[0] = '\0'; return 0; }
    for (size_t i = 0; i < n; i++) { out[i * 2] = kHex[v[i] >> 4]; out[i * 2 + 1] = kHex[v[i] & 15]; }
    out[n * 2] = '\0';
    return n * 2;
}

size_t wifiJson(bool ap, const char* ssid, const char* ip, const char* hostname, const char* hosts, const char* apPass,
                char* out, size_t cap) {
    char e[136];                                    // the widest escaped value: a 32-char SSID of quotes = 64
    int w = snprintf(out, cap, "{\"mode\":\"%s\",\"ssid\":\"", ap ? "AP" : "STA");
    const char* parts[] = {ssid, "\",\"ip\":\"", ip, "\",\"hostname\":\"", hostname, "\",\"extra_hosts\":\"", hosts};
    for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]) && w >= 0 && (size_t)w < cap; i++) {
        bool value = i % 2 == 0;                    // values escaped, the glue between them verbatim
        if (value) jsonEscape(parts[i], e, sizeof(e));
        w += snprintf(out + w, cap - w, "%s", value ? e : parts[i]);
    }
    if (w >= 0 && (size_t)w < cap) {
        if (ap) { jsonEscape(apPass, e, sizeof(e)); w += snprintf(out + w, cap - w, "\",\"ap_password\":\"%s\"}", e); }
        else w += snprintf(out + w, cap - w, "\",\"ap_password\":null}");
    }
    if (w < 0 || (size_t)w >= cap) { if (cap) out[0] = '\0'; return 0; }
    return (size_t)w;
}

size_t infoSignalJson(const InfoSignal& s, char* out, size_t cap) {
    int w = snprintf(out, cap, "{\"none\":%u,\"std\":%u,\"lines\":%u,\"src_hz10\":%u,\"out_hz10\":%u,\"interlaced\":%u,"
                     "\"scanlines_ok\":%u,\"adv_ntsc\":%u,\"preset\":%u}", s.none ? 1u : 0u, (unsigned)s.std, (unsigned)s.lines,
                     (unsigned)s.srcHz10, (unsigned)s.outHz10, s.interlaced ? 1u : 0u, s.scanlinesOk ? 1u : 0u,
                     s.advNtsc ? 1u : 0u, (unsigned)s.preset);
    if (w < 0 || (size_t)w >= cap) { if (cap) out[0] = '\0'; return 0; }
    return (size_t)w;
}
