#pragma once
#include <stdint.h>
#include <stddef.h>
// Writes `in` as JSON string content (no surrounding quotes); stops before an escape that does not fit
size_t jsonEscape(const char* in, char* out, size_t n);
// n bytes as 2n lowercase hex characters plus NUL; 0 (and "") when out cannot hold them
// GET /api/wifi body (apPass only when ap, else null); 0 when it does not fit
#define WIFI_JSON_MAX 448
size_t wifiJson(bool ap, const char* ssid, const char* ip, const char* hostname, const char* hosts, const char* apPass,
                char* out, size_t cap);
// /api/info "signal": what the scaler and the decoder report (the menu predicates use the same values)
struct InfoSignal { bool none; uint8_t std; uint16_t lines, srcHz10, outHz10; bool interlaced, scanlinesOk, advNtsc; uint8_t preset; };
size_t infoSignalJson(const InfoSignal& s, char* out, size_t cap);   // 0 when it does not fit
size_t regsHexFormat(const uint8_t* v, size_t n, char* out, size_t cap);

// Chunked JSON without a buffer: every fill re-runs a deterministic builder (same state → same text) and copies
// only the bytes after the ones already sent. CPU for memory: a 3 KB answer in 1.4 KB chunks is built 3 times.
struct JsonReplay {
    void (*build)(void (*sink)(const char*, void*), void* ctx, const void* state);
    const void* state;
    size_t sent;
};
size_t jsonReplayFill(JsonReplay& r, char* out, size_t max);   // 0 = done
