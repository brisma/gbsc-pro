#include "../log/log.h"
#include "../log/event-ring.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../../options.h"
#include "../gbs-control-pro.h"
#include "../settings/settings-device.h"
#include "../link/adv-link-device.h"
#include "update-device.h"
#include "ota-client.h"
#include "ota-update.h"

extern userOptions* uopt;
void settingsBroadcastStatusText(const char* key, const char* value);

namespace {

enum Job : uint8_t { J_IDLE, J_CHECK, J_MANUAL, J_INSTALL };
Job gJob = J_IDLE;
bool gFromUser = false, gAuto = false;
uint8_t gChannels[OTA_CHANNELS], gCount = 0, gIndex = 0, gReadOk = 0;
const char* gLastErr = "";
OtaOffer gRec, gMan;
char gLast[32] = "";
OtaAutoClock gClock;
void (*gHook)(const char*, bool) = nullptr;

struct ManifestBuf { char buf[MANIFEST_MAX_BYTES]; size_t len; OtaOffer best; };   // heap, during a check only
ManifestBuf* gBuf = nullptr;

bool bufSink(const uint8_t* d, size_t n, void* ctx) {
    ManifestBuf* b = (ManifestBuf*)ctx;
    if (b->len + n > sizeof(b->buf)) return false;
    memcpy(b->buf + b->len, d, n);
    b->len += n;
    return true;
}
bool imageSink(const uint8_t* d, size_t n, void*) { return updateCoreWrite(d, n); }

bool staConnected() { return WiFi.getMode() != WIFI_AP && WiFi.status() == WL_CONNECTED; }
bool somethingBusy() { return gJob != J_IDLE || updateInProgress() || advLinkUpdateActive(); }

void setLast(const char* v) { strncpy(gLast, v, sizeof(gLast) - 1); gLast[sizeof(gLast) - 1] = '\0'; }

void hook(const char* text, bool fromUser) { if (gHook) gHook(text, fromUser); }

void freeBuf() { delete gBuf; gBuf = nullptr; }

// Starts reading the manifest of gChannels[gIndex]; false when it cannot even start (the caller moves on)
bool readChannel() {
    char url[96];
    if (!gBuf) { gBuf = new (std::nothrow) ManifestBuf; if (gBuf) gBuf->best = OtaOffer(); }
    if (!gBuf) { gLastErr = "memory"; return false; }
    gBuf->len = 0;
    if (!otaManifestUrl(GBSC_OTA_BASE, gChannels[gIndex], url, sizeof(url))) { gLastErr = "http"; return false; }
    if (!otaFetchStart(url, MANIFEST_MAX_BYTES, bufSink, gBuf)) { gLastErr = otaFetchReason(); return false; }
    return true;
}

void finishCheck() {
    OtaOffer best = gBuf ? gBuf->best : OtaOffer();
    freeBuf();
    char v[40];
    if (gJob == J_CHECK) {
        otaFinishRecommended(best, GBS_FW_FULL_VERSION, GBS_FW_CHANNEL);
        gRec = otaAfterCheck(gRec, best, gReadOk);    // an outage keeps the offer the user may be confirming
        if (!gReadOk) snprintf(v, sizeof(v), "error:%s", gLastErr);
        else if (gRec.any) snprintf(v, sizeof(v), "%s", gRec.m.version);
        else snprintf(v, sizeof(v), "none");
        setLast(v);
        settingsBroadcastStatusText("status.update_available", v);
        if (gRec.any) logPrintf(LC_UPDATE, LL_INFO, PSTR("recommended: %s (%s)"), gRec.m.version, otaChannelName(gRec.channel));
        if (gRec.any) eventAdd(EV_UPDATE_OFFERED, gRec.channel, 0);   // a: channel; the version is in /api/info update.offer
        else logPrintf(LC_UPDATE, LL_INFO, PSTR("recommended: %s"), v);
        char t[29];
        if (!gReadOk) snprintf_P(t, sizeof(t), PSTR("Check failed: %s"), otaReasonWords(gLastErr));
        else if (gRec.any) snprintf_P(t, sizeof(t), PSTR("%s available"), gRec.m.version);
        else snprintf_P(t, sizeof(t), PSTR("Up to date"));
        if (otaToastWanted(gFromUser, gReadOk && gRec.any)) hook(t, gFromUser);
        if (gAuto) { gClock.ran(millis(), gReadOk > 0); gAuto = false; }
    } else {
        uint8_t ch = gChannels[0];
        char t[29];
        if (gReadOk && gMan.any) {
            snprintf(v, sizeof(v), "%s:%s", otaChannelName(ch), gMan.m.version);
            snprintf_P(t, sizeof(t), PSTR("%s %s"), otaChannelLabel(ch), gMan.m.version);
        } else if (gReadOk) {
            snprintf(v, sizeof(v), "%s:none", otaChannelName(ch));
            snprintf_P(t, sizeof(t), PSTR("%s up to date"), otaChannelLabel(ch));
        } else {
            snprintf(v, sizeof(v), "error:%s", gLastErr);
            snprintf_P(t, sizeof(t), PSTR("Check failed: %s"), otaReasonWords(gLastErr));
        }
        settingsBroadcastStatusText("status.update_channel_check", v);
        logPrintf(LC_UPDATE, LL_INFO, PSTR("other channel %s: %s"), otaChannelName(ch), v);
        hook(t, true);
    }
    gJob = J_IDLE;
}

// One manifest read ended: keep it, then the next channel or the end of the check
void channelDone(bool started) {
    uint8_t ch = gChannels[gIndex];
    Manifest m;
    bool ok = started && otaFetchResult() == HTTP_DONE && gBuf && manifestParse(gBuf->buf, gBuf->len, m);
    if (started && !ok) gLastErr = otaFetchResult() == HTTP_DONE ? "manifest" : otaFetchReason();
    if (ok) {
        gReadOk++;
        logPrintf(LC_UPDATE, LL_INFO, PSTR("check %s: %s"), otaChannelName(ch), m.version);
        if (gJob == J_CHECK) otaConsider(gBuf->best, ch, m);
        else gMan = otaManual(ch, m, GBS_FW_FULL_VERSION, GBS_FW_CHANNEL);   // also clears it when equal
    } else {
        logPrintf(LC_UPDATE, LL_INFO, PSTR("check %s: error:%s"), otaChannelName(ch), gLastErr);
    }
    while (++gIndex < gCount) if (readChannel()) return;
    finishCheck();
}

void startJob(Job j, bool fromUser) {
    gJob = j;
    gFromUser = fromUser;
    gIndex = 0;
    gReadOk = 0;
    gLastErr = "";
    freeBuf();
    // the manual slot is replaced only by a check that read its manifest (channelDone)
    for (; gIndex < gCount; gIndex++) if (readChannel()) return;
    finishCheck();
}

bool refuse(Job j, bool fromUser, const char* code) {
    char v[24];
    snprintf(v, sizeof(v), "error:%s", code);
    settingsBroadcastStatusText(j == J_MANUAL ? "status.update_channel_check" : "status.update_available", v);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("check refused: %s"), code);
    if (fromUser) {
        char t[29];
        snprintf_P(t, sizeof(t), PSTR("Check failed: %s"), otaReasonWords(code));
        hook(t, true);
    }
    return false;
}

} // namespace

void otaCheck(bool fromUser) {
    if (somethingBusy()) { refuse(J_CHECK, fromUser, "busy"); return; }
    if (!staConnected()) { refuse(J_CHECK, fromUser, "no_wifi"); return; }
    gCount = otaChannelsToRead(GBS_FW_CHANNEL, gChannels);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("check started (%s build, %u channels)"), otaChannelName(GBS_FW_CHANNEL), gCount);
    startJob(J_CHECK, fromUser);
}

void otaCheckChannel(uint8_t ch) {
    if (ch >= OTA_CHANNELS) return;
    if (somethingBusy()) { refuse(J_MANUAL, true, "busy"); return; }
    if (!staConnected()) { refuse(J_MANUAL, true, "no_wifi"); return; }
    gChannels[0] = ch;
    gCount = 1;
    startJob(J_MANUAL, true);
}

// A refused install: status, log and a toast on the TV/OLED (the reason is shown and logged)
static bool installRefused(const char* code) {
    char v[24], t[29];
    snprintf(v, sizeof(v), "error:%s", code);
    settingsBroadcastStatusText("status.update", v);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("install refused: %s"), code);
    otaInstallFailText(code, t, sizeof(t));
    hook(t, true);
    return false;
}

bool otaInstall(bool manual) {
    const OtaOffer& o = manual ? gMan : gRec;
    if (!o.any) return installRefused("changed");
    if (somethingBusy()) return installRefused("busy");
    if (!staConnected()) return installRefused("no_wifi");
    char url[128];
    if (!otaImageUrl(GBSC_OTA_BASE, o.m.file, url, sizeof(url))) return installRefused("http");
    if (!updateCoreBegin(US_PULL, o.m.size, o.m.sha256)) {
        const char* e = updateInProgress() || advLinkUpdateActive() ? "busy" : updateLastError();
        return installRefused(e ? e : "memory");
    }
    if (!otaFetchStart(url, o.m.size, imageSink, nullptr)) {
        updateCoreFail(US_PULL, otaFetchReason());             // reported by updateLoop() too (status, log, overlay)
        return installRefused(otaFetchReason());
    }
    gJob = J_INSTALL;
    logPrintf(LC_UPDATE, LL_INFO, PSTR("install %s from %s: started"), o.m.version, otaChannelName(o.channel));
    return true;
}

void otaLoop() {
    otaFetchLoop();
    if ((gJob == J_CHECK || gJob == J_MANUAL) && !otaFetchBusy()) channelDone(true);
    else if (gJob == J_INSTALL && !otaFetchBusy()) {
        int r = otaFetchResult();
        if (r == HTTP_DONE) updateCoreFinish(US_PULL);   // size and sha256 are checked by updateLoop()
        else updateCoreFail(US_PULL, otaFetchReason());
        logPrintf(LC_UPDATE, LL_INFO, PSTR("install: download %s"), r == HTTP_DONE ? "complete" : otaFetchReason());
        gJob = J_IDLE;
    }
    uint32_t now = millis();
    if (gJob == J_IDLE && gClock.due(now, uopt->autoUpdateCheck == 1)) {
        if (staConnected() && !updateInProgress() && !advLinkUpdateActive()) { gAuto = true; otaCheck(false); }
        else gClock.ran(now, false);
    }
}

bool otaBusy() { return gJob != J_IDLE; }
const OtaOffer& otaRecommended() { return gRec; }
const OtaOffer& otaManualOffer() { return gMan; }
const char* otaLastResult() { return gLast; }
void otaSetMenuHook(void (*h)(const char*, bool)) { gHook = h; }

static int offerJson(char* out, size_t n, const OtaOffer& o) {
    return snprintf_P(out, n, PSTR("{\"version\":\"%s\",\"channel\":\"%s\",\"older\":%u,\"moves\":%u}"),
                      o.any ? o.m.version : "", o.any ? otaChannelName(o.channel) : "", o.any && o.older ? 1u : 0u,
                      o.any && o.moves ? 1u : 0u);
}

int otaInfoJson(char* out, size_t n) {
    int w = snprintf_P(out, n, PSTR("{\"channel\":\"%s\",\"last\":\"%s\",\"offer\":"), otaChannelName(GBS_FW_CHANNEL), gLast);
    if (w > 0 && (size_t)w < n) w += offerJson(out + w, n - w, gRec);
    if (w > 0 && (size_t)w < n) w += snprintf_P(out + w, n - w, PSTR(",\"manual\":"));
    if (w > 0 && (size_t)w < n) w += offerJson(out + w, n - w, gMan);
    if (w > 0 && (size_t)w < n) w += snprintf_P(out + w, n - w, PSTR("}"));
    return w;
}
