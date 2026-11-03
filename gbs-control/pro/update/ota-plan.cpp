#include <stdio.h>
#include <string.h>
#include "ota-plan.h"
#include "../util/pgm.h"

static const char* const kNames[OTA_CHANNELS] = {"stable", "beta"};
static const char* const kLabels[OTA_CHANNELS] = {"Stable", "Beta"};

const char* otaChannelName(uint8_t ch) { return ch < OTA_CHANNELS ? kNames[ch] : ""; }
const char* otaChannelLabel(uint8_t ch) { return ch < OTA_CHANNELS ? kLabels[ch] : ""; }

int otaChannelParse(const char* s) {
    for (uint8_t i = 0; i < OTA_CHANNELS; i++) if (s && !strcmp(s, kNames[i])) return i;
    return -1;
}

static bool fits(int w, size_t n) { return w > 0 && (size_t)w < n; }

bool otaManifestUrl(const char* base, uint8_t ch, char* out, size_t n) {
    if (ch >= OTA_CHANNELS) return false;
    return fits(snprintf(out, n, "%s%s/manifest.txt", base, kNames[ch]), n);
}

bool otaImageUrl(const char* base, const char* file, char* out, size_t n) {
    return fits(snprintf(out, n, "%s%s", base, file), n);
}

uint8_t otaChannelsToRead(uint8_t buildChannel, uint8_t out[OTA_CHANNELS]) {
    if (buildChannel >= OTA_CHANNELS) buildChannel = OTA_STABLE;
    uint8_t n = 0;
    for (int ch = buildChannel; ch >= 0; ch--) out[n++] = (uint8_t)ch;   // the order is stable < beta
    return n;
}

void otaConsider(OtaOffer& best, uint8_t channel, const Manifest& m) {
    if (best.any && versionCompare(m.version, best.m.version) <= 0) return;
    best.any = true;
    best.channel = channel;
    best.m = m;
}

void otaFinishRecommended(OtaOffer& best, const char* running, uint8_t buildChannel) {
    if (!best.any || versionCompare(best.m.version, running) <= 0) { best = OtaOffer(); return; }
    best.older = false;
    best.moves = best.channel != buildChannel;
}

OtaOffer otaManual(uint8_t channel, const Manifest& m, const char* running, uint8_t buildChannel) {
    OtaOffer o = OtaOffer();
    int c = versionCompare(m.version, running);
    if (!c) return o;
    o.any = true;
    o.channel = channel;
    o.older = c < 0;
    o.moves = channel != buildChannel;
    o.m = m;
    return o;
}

void OtaAutoClock::ran(uint32_t now, bool ok) {
    if (ok) { fails = 0; next = now + 86400000UL; return; }
    uint32_t wait = 600000UL << (fails < 4 ? fails : 4);
    if (wait > 7200000UL) wait = 7200000UL;
    if (fails < 8) fails++;
    next = now + wait;
}

OtaStep OtaPull::step(uint32_t now, bool resolved, bool connected, bool gotData, bool closed, int parser) {
    if (state_ == OS_RESOLVE && resolved) state_ = OS_CONNECT;
    if (state_ == OS_CONNECT && connected) { state_ = OS_BODY; lastAt_ = now; }
    if (state_ == OS_RESOLVE || state_ == OS_CONNECT) {
        if (now - startAt_ >= kConnectMs || closed) { state_ = OS_FAILED; reason_ = closed ? "connect" : "timeout"; }
        return state_;
    }
    if (state_ != OS_BODY) return state_;
    if (gotData) lastAt_ = now;
    if (parser > 0) state_ = OS_DONE;
    else if (parser < 0) { state_ = OS_FAILED; reason_ = "http"; }
    else if (closed) { state_ = OS_FAILED; reason_ = "short"; }
    else if (now - lastAt_ >= kIdleMs) { state_ = OS_FAILED; reason_ = "timeout"; }
    return state_;
}

const char* otaVerdict(uint32_t contentLength, uint32_t expectSize, const uint8_t* sha, const uint8_t* expectSha,
                       bool gzipKind) {
    if (!gzipKind) return "format";
    if (contentLength != expectSize) return "size";
    if (memcmp(sha, expectSha, 32) != 0) return "sha";
    return nullptr;
}

// code\0words\0 pairs in flash (a few hundred bytes of RAM otherwise), ending with an empty code
static const char kWords[] PROGMEM =
    "no_wifi\0no Wi-Fi\0dns\0no DNS\0connect\0no connection\0timeout\0timeout\0short\0cut off\0http\0server error\0"
    "manifest\0bad manifest\0size\0wrong size\0sha\0bad checksum\0format\0not firmware\0memory\0low memory\0"
    "busy\0busy\0space\0no space\0write\0write error\0changed\0offer changed\0";

const char* otaReasonWords(const char* code) {
    static char out[16];
    const char* p = kWords;
    while (pgm_read_byte(p)) {
        size_t i = 0;
        bool eq = code != nullptr;
        for (char c; (c = (char)pgm_read_byte(p + i)) != 0; i++) if (eq && code[i] != c) eq = false;
        eq = eq && code[i] == '\0';
        const char* w = p + i + 1;
        size_t wl = strlen_P(w);
        if (eq && wl < sizeof(out)) { memcpy_P(out, w, wl + 1); return out; }
        p = w + wl + 1;
    }
    return "error";
}

void otaOfferText(const OtaOffer& o, char* out, size_t n) {
    snprintf(out, n, "%s%s%s", otaChannelLabel(o.channel), o.older ? " older" : "", o.moves ? " switch" : "");
}

void otaConfirmText(const OtaOffer& o, char* out, size_t n) {
    if (o.older) snprintf(out, n, "Install an older version?");
    else if (o.moves) snprintf(out, n, "Move to the %s channel?", otaChannelLabel(o.channel));
    else if (n) out[0] = '\0';
}

void otaInstallFailText(const char* code, char* out, size_t n) { snprintf(out, n, "Not installed: %s", otaReasonWords(code)); }

void otaLastResultText(const char* last, char* out, size_t n) {
    if (!n) return;
    if (!last || !*last) snprintf(out, n, "Not checked");
    else if (!strcmp(last, "none")) snprintf(out, n, "Up to date");
    else if (!strncmp(last, "error:", 6)) {
        snprintf(out, n, "%s", otaReasonWords(last + 6));
        if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
    } else snprintf(out, n, "%s", last);
}


const char* otaConnectBlocked(bool haveRoute, uint32_t maxFreeBlock) {
    if (!haveRoute) return "connect";
    if (maxFreeBlock < OTA_CONNECT_MIN_BLOCK) return "memory";
    return nullptr;
}
