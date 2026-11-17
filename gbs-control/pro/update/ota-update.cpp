#include "../log/log.h"
#include "../util/pgm.h"
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

enum Job : uint8_t { J_IDLE, J_CHECK, J_INSTALL };
Job gJob = J_IDLE;
bool gFromUser = false, gAuto = false;
uint8_t gChannels[OTA_CHANNELS], gCount = 0, gIndex = 0, gReadOk = 0;
uint8_t gChosen = OTA_STABLE;      // the channel the running check is for
const char* gLastErr = "";
OtaOffer gOffer;
uint8_t gOfferFor = 0xFF;          // the channel gOffer and gLast were worked out for
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
uint8_t chosenChannel() { return uopt->updateChannel < OTA_CHANNELS ? uopt->updateChannel : (uint8_t)OTA_STABLE; }

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

void startCheck(bool fromUser);

void finishCheck() {
    OtaOffer best = gBuf ? gBuf->best : OtaOffer();
    freeBuf();
    gJob = J_IDLE;
    if (gChosen != chosenChannel()) {             // the channel changed meanwhile: this answer is for the other one
        logPrintf(LC_UPDATE, LL_INFO, PSTR("check for %s dropped: the channel is %s now"), otaChannelName(gChosen),
                  otaChannelName(chosenChannel()));
        if (staConnected() && !updateInProgress() && !advLinkUpdateActive()) startCheck(gFromUser);
        else if (gAuto) { gClock.ran(millis(), false); gAuto = false; }
        return;
    }
    char v[40];
    otaFinishCheck(best, GBS_FW_FULL_VERSION);
    bool had = gOfferFor == gChosen;
    gOffer = otaAfterCheck(had ? gOffer : OtaOffer(), best, gReadOk);   // an outage keeps the offer being confirmed
    gOfferFor = gChosen;
    if (!gReadOk) snprintf_P(v, sizeof(v), PSTR("error:%s"), gLastErr);
    else if (gOffer.any) snprintf_P(v, sizeof(v), PSTR("%s"), gOffer.m.version);
    else snprintf_P(v, sizeof(v), PSTR("none"));
    setLast(v);
    settingsBroadcastStatusText("status.update_available", v);
    if (gOffer.any) {
        logPrintf(LC_UPDATE, LL_INFO, PSTR("offer: %s (%s%s), channel %s"), gOffer.m.version, otaChannelName(gOffer.channel),
                  gOffer.older ? ", older" : "", otaChannelName(gChosen));
        eventAdd(EV_UPDATE_OFFERED, gOffer.channel, gOffer.older ? 1 : 0);   // a: channel, b: older; the version is in /api/info
    } else logPrintf(LC_UPDATE, LL_INFO, PSTR("offer: %s, channel %s"), v, otaChannelName(gChosen));
    char t[29];
    otaCheckToast(gOffer, gReadOk > 0, gLastErr, t, sizeof(t));
    if (otaToastWanted(gFromUser, gReadOk ? gOffer : OtaOffer())) hook(t, gFromUser);
    if (gAuto) { gClock.ran(millis(), gReadOk > 0); gAuto = false; }
}

// One manifest read ended: keep it, then the next channel or the end of the check
void channelDone(bool started) {
    uint8_t ch = gChannels[gIndex];
    Manifest m;
    bool done = started && otaFetchResult() == HTTP_DONE;
    bool ok = done && gBuf && manifestParse(gBuf->buf, gBuf->len, m);
    // a channel with nothing published yet (404): read, no version
    bool empty = started && !done && otaFetchStatus() == 404;
    if (started && !ok && !empty) gLastErr = done ? "manifest" : otaFetchReason();
    if (ok) {
        gReadOk++;
        logPrintf(LC_UPDATE, LL_INFO, PSTR("check %s: %s"), otaChannelName(ch), m.version);
        otaConsider(gBuf->best, ch, m);
    } else if (empty) {
        gReadOk++;
        logPrintf(LC_UPDATE, LL_INFO, PSTR("check %s: nothing published"), otaChannelName(ch));
    } else {
        logPrintf(LC_UPDATE, LL_INFO, PSTR("check %s: error:%s"), otaChannelName(ch), gLastErr);
    }
    while (++gIndex < gCount) if (readChannel()) return;
    finishCheck();
}

void startCheck(bool fromUser) {
    gChosen = chosenChannel();
    gCount = otaChannelsToRead(gChosen, gChannels);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("check started (channel %s, %s build, %u to read)"), otaChannelName(gChosen),
              otaChannelName(GBS_FW_CHANNEL), gCount);
    gJob = J_CHECK;
    gFromUser = fromUser;
    gIndex = 0;
    gReadOk = 0;
    gLastErr = "";
    freeBuf();
    for (; gIndex < gCount; gIndex++) if (readChannel()) return;
    finishCheck();
}

bool refuse(bool fromUser, const char* code) {
    char v[24];
    snprintf_P(v, sizeof(v), PSTR("error:%s"), code);
    settingsBroadcastStatusText("status.update_available", v);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("check refused: %s"), code);
    if (fromUser) {
        char t[29];
        snprintf_P(t, sizeof(t), PSTR("Check failed: %s"), otaReasonWords(code));
        hook(t, true);
    }
    return false;
}

// A refused install: status, log and a toast on the TV/OLED (the reason is shown and logged)
bool installRefused(const char* code) {
    char v[24], t[29];
    snprintf_P(v, sizeof(v), PSTR("error:%s"), code);
    settingsBroadcastStatusText("status.update", v);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("install refused: %s"), code);
    otaInstallFailText(code, t, sizeof(t));
    hook(t, true);
    return false;
}

bool install(const OtaOffer& o) {
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
    logPrintf(LC_UPDATE, LL_INFO, PSTR("install %s from %s%s: started"), o.m.version, otaChannelName(o.channel),
              o.older ? " (older)" : "");
    return true;
}

} // namespace

void otaCheck(bool fromUser) {
    if (somethingBusy()) { refuse(fromUser, "busy"); return; }
    if (!staConnected()) { refuse(fromUser, "no_wifi"); return; }
    startCheck(fromUser);
}

bool otaInstall(bool olderConfirmed) {
    const OtaOffer& o = otaOffer();
    if (!otaInstallMatches(o, olderConfirmed)) return installRefused("changed");
    return install(o);
}

void otaLoop() {
    otaFetchLoop();
    if (gJob == J_CHECK && !otaFetchBusy()) channelDone(true);
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
const OtaOffer& otaOffer() {
    static const OtaOffer kNone = OtaOffer();
    return gOfferFor == chosenChannel() ? gOffer : kNone;
}
const char* otaLastResult() { return gOfferFor == chosenChannel() ? gLast : ""; }
void otaSetMenuHook(void (*h)(const char*, bool)) { gHook = h; }

// "channel": the running build's; "follows": the chosen one (system.update_channel)
int otaInfoJson(char* out, size_t n) {
    const OtaOffer& o = otaOffer();
    return snprintf_P(out, n, PSTR("{\"channel\":\"%s\",\"follows\":\"%s\",\"last\":\"%s\",\"offer\":{\"version\":\"%s\","
                                   "\"channel\":\"%s\",\"older\":%u}}"),
                      otaChannelName(GBS_FW_CHANNEL), otaChannelName(chosenChannel()), otaLastResult(),
                      o.any ? o.m.version : "", o.any ? otaChannelName(o.channel) : "", o.any && o.older ? 1u : 0u);
}
