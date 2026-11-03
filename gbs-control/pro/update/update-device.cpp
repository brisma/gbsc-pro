#include "../log/log.h"
#include "../log/event-ring.h"
#include <Arduino.h>
#include <Updater.h>
#include <ESPAsyncWebServer.h>
#include "../../options.h"
#include "../settings/settings-device.h"
#include "../api/api-device.h"
#include "update-trailer.h"
#include "update-verify.h"
#include "../api/api-watchdog.h"
#include "update-device.h"
#include "../link/adv-link-device.h"

extern userOptions* uopt;
void settingsBroadcastStatusText(const char* key, const char* value);

namespace {
enum State : uint8_t { IDLE, RECEIVING, VERIFY, FAILED, DONE };
struct {
    State state = IDLE;
    UpdateSource source = US_NONE;  // a web upload or an OTA pull: one Updater, one state
    AsyncWebServerRequest* owner = nullptr;   // the request that started this upload; others are ignored
    bool paused = false;            // this upload paused the settings saver
    UpdateImageKind kind = UPDATE_IMAGE_UNKNOWN;
    size_t received = 0;
    size_t total = 0;               // Content-Length when known
    TailBuffer tail;
    const char* error = nullptr;
    uint8_t lastPct = 255;
    UploadWatchdog wd;              // client vanished mid-upload: abort instead of staying "in progress"
    uint32_t verifyAt = 0;          // when the last chunk arrived
    uint32_t doneAt = 0;            // verified: restart 2.5 s later (the outcome stays on screen)
    bool haveSha = false;           // OTA pull: the manifest's sha256 is checked at VERIFY
    bool headHeld = false;          // the first byte (0x1F) waits for the second to tell a gzip image
    uint8_t head0 = 0;
    uint8_t expectSha[32];
    UpdateSha256 sha;
} g;

void fail(const char* reason) {
    g.wd.stop();
    if (Update.isRunning()) Update.end(false);   // abort: the next Update.begin() starts clean
    g.error = reason;
    g.state = FAILED;
}
} // namespace

// DONE counts as busy: the staged image must not be overwritten before the restart
bool updateInProgress() { return g.state == RECEIVING || g.state == VERIFY || g.state == DONE; }
bool updateUploadActive() { return updateInProgress() && g.source == US_UPLOAD; }
UpdateSource updateSource() { return updateInProgress() ? g.source : US_NONE; }
const char* updateLastError() { return g.error; }

// Shared Updater core (web upload and OTA pull)
bool updateCoreBegin(UpdateSource src, uint32_t total, const uint8_t* expectSha) {
    if (updateInProgress() || advLinkUpdateActive()) return false;   // busy: the running one keeps its state
    if (Update.isRunning()) { g.error = "busy"; g.state = FAILED; g.source = src; return false; }   // ArduinoOTA (Developer)
    g.error = nullptr;
    // No signature check, so no 6.2 KB RSA stack thunk: only the Updater's 4 KB flash buffer must fit
    if (ESP.getMaxFreeBlockSize() < 4096 + 512) { g.error = "memory"; g.state = FAILED; g.source = src; return false; }
    settingsSaverPause(SAVER_HOLD_UPDATE, true);     // no LittleFS writes during flash writes
    g.paused = true;
    g.source = src;
    g.owner = nullptr;                               // a stale upload request must not feed this one
    g.kind = UPDATE_IMAGE_UNKNOWN;
    g.total = total;
    g.received = 0; g.lastPct = 255; g.tail = TailBuffer();
    g.headHeld = false;
    g.haveSha = expectSha != nullptr;
    if (g.haveSha) { memcpy(g.expectSha, expectSha, 32); g.sha.begin(); }
    Update.runAsync(true);                           // written from the network context: the Updater must not yield
    uint32_t space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (src == US_PULL && total > space) { fail("space"); return false; }
    if (!Update.begin(space, U_FLASH)) { fail("space"); return false; }
    g.state = RECEIVING;
    g.wd.touch(millis());
    return true;
}

static bool coreFeed(const uint8_t* d, size_t n) {
    if (Update.write((uint8_t*)d, n) != n) { fail("write"); return false; }
    g.tail.add(d, n);
    if (g.haveSha) g.sha.add(d, n);
    g.received += n;
    g.wd.touch(millis());
    return true;
}

bool updateCoreWrite(const uint8_t* d, size_t n) {
    if (g.state != RECEIVING || !n) return g.state == RECEIVING;
    if (g.received == 0) {                           // the first bytes say what the image is
        uint8_t two[2];
        const uint8_t* head = d;
        size_t hn = n;
        if (g.headHeld) { two[0] = g.head0; two[1] = d[0]; head = two; hn = 2; }
        else if (updateKindNeedsMore(d, n)) { g.head0 = d[0]; g.headHeld = true; g.wd.touch(millis()); return true; }
        g.kind = updateImageKind(head, hn);
        if (g.kind == UPDATE_IMAGE_UNKNOWN || (g.source == US_PULL && g.kind != UPDATE_IMAGE_GZIP)) {
            fail("format");
            return false;
        }
        if (g.headHeld) { g.headHeld = false; if (!coreFeed(&g.head0, 1)) return false; }
    }
    return coreFeed(d, n);
}

void updateCoreFinish(UpdateSource src) {
    if (g.state != RECEIVING || g.source != src) return;
    g.state = VERIFY;                                // Update.end() runs in updateLoop()
    g.wd.stop();
    g.verifyAt = millis();
}

void updateCoreFail(UpdateSource src, const char* reason) {
    if ((g.state == RECEIVING || g.state == VERIFY) && g.source == src) fail(reason);   // never another source's run
}
// The request length includes the multipart envelope: received bytes never reach it (97-98 %), so the
// verification and the restart wait show 100 %
uint8_t updateProgress() {
    if (g.state == VERIFY || g.state == DONE) return 100;
    return g.total ? (uint8_t)(g.received * 100 / g.total) : 0;
}
uint8_t updateStage() { return g.state == DONE ? 2 : g.state == VERIFY ? 1 : 0; }

int updateUploadJson(char* out, size_t n) {
    static const char* const kState[] = {"idle", "receiving", "verifying", "failed", "done"};
    char err[24];
    if (g.error) snprintf(err, sizeof(err), "\"%s\"", g.error); else strcpy(err, "null");
    return snprintf_P(out, n, PSTR("{\"state\":\"%s\",\"error\":%s,\"received\":%u,\"total\":%u}"),
                      kState[g.state <= DONE ? g.state : 0], err, (unsigned)g.received, (unsigned)g.total);
}

void updateUploadChunk(AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        if (!apiGuardRequest(req)) return;           // runs before the middleware; finish answers 403
        if (updateInProgress() || advLinkUpdateActive()) return;   // busy (also an ADV Controller update); finish answers 409
        g.error = nullptr;
        if (updateImageKind(data, len) == UPDATE_IMAGE_UNKNOWN && !updateKindNeedsMore(data, len)) { g.error = "format"; g.state = FAILED; g.source = US_UPLOAD; return; }
        if (!updateCoreBegin(US_UPLOAD, req->contentLength(), nullptr)) return;   // memory or space: reported by the loop
        g.owner = req;
    }
    if (req != g.owner || g.state != RECEIVING) return;
    if (!updateChunkContinues(index, g.received, g.headHeld)) { g.owner = nullptr; fail("incomplete"); return; }   // stale owner pointer
    if (!updateCoreWrite(data, len)) return;
    if (final) updateCoreFinish(US_UPLOAD);
}

int updateUploadFinish(AsyncWebServerRequest* req) {
    if (req != g.owner) {
        if (!apiGuardRequest(req)) return 403;
        return (updateInProgress() || advLinkUpdateActive()) ? 409 : 400;
    }
    g.owner = nullptr;
    if (g.state == FAILED || (g.state == IDLE && g.error)) return 400;   // loop may already have reported it
    if (g.state != VERIFY) { fail("incomplete"); return 400; }        // client went away before the last chunk
    return 202;
}

void updateLoop() {
    static bool logged = false;                       // the upload start, once per upload (chunks arrive in the network context)
    if (g.state == RECEIVING && !logged) {
        logged = true;
        logPrintf(LC_UPDATE, LL_INFO, PSTR("%s started (%u KB)"), g.source == US_PULL ? "download" : "upload",
                  (unsigned)(g.total / 1024));
    }
    if (g.state != RECEIVING && g.state != VERIFY) logged = false;
    if (g.state == DONE) {
        if (millis() - g.doneAt >= 2500) ESP.restart();   // time to read the outcome on the TV and the OLED
        return;
    }
    if (g.state == RECEIVING && g.wd.expired(millis())) { g.owner = nullptr; fail("incomplete"); }
    uint8_t pct = updateProgress();
    if (g.state == RECEIVING && pct != g.lastPct && pct % 5 == 0) {
        g.lastPct = pct;
        if (pct % 25 == 0) logPrintf(LC_UPDATE, LL_DEBUG, PSTR("upload %u %%"), (unsigned)pct);
        char v[8]; snprintf(v, sizeof(v), "%u", pct);
        settingsBroadcastStatusText("status.update", v);
    }
    if (g.state == FAILED) {
        char v[32]; snprintf(v, sizeof(v), "error:%s", g.error ? g.error : "unknown");
        settingsBroadcastStatusText("status.update", v);
        logPrintf(LC_UPDATE, LL_INFO, PSTR("%s failed: %s"), g.source == US_PULL ? "download" : "upload",
                  g.error ? g.error : "unknown");
        if (g.paused) { settingsSaverPause(SAVER_HOLD_UPDATE, false); g.paused = false; }
        g.state = IDLE;
        return;
    }
    if (g.state != VERIFY) return;
    if (g.source == US_PULL) {                       // the manifest's size and sha256
        uint8_t digest[32];
        if (g.haveSha) g.sha.end(digest);
        if (g.received != g.total) { fail("size"); return; }
        if (g.haveSha && memcmp(digest, g.expectSha, 32) != 0) { fail("sha"); return; }
    }
    UpdateTrailer t = updateParseTrailer(g.tail.data(), g.tail.size(), g.kind);
    const char* err = updateCheckImage(g.kind, t, UPDATE_MAX_SKETCH);   // any gzip or raw build
    if (err) { fail(err); return; }
    // An older signed image needs a verifier so that the Updater strips its trailer (else eboot reads the length
    // field as the gzip ISIZE): the trailer-only one, no signature check
    updateInstallTrailerOnly(t.hasSignature);
    bool ok = Update.end(true);
    updateInstallTrailerOnly(false);   // ArduinoOTA (developer mode) gets plain images
    if (!ok) { fail(Update.getError() == UPDATE_ERROR_SIGN ? "signature" : "verify"); return; }
    settingsBroadcastStatusText("status.update", "done");
    logPrintf(LC_UPDATE, LL_INFO, PSTR("%s ok, restarting"), g.source == US_PULL ? "download" : "upload");
    eventAdd(EV_UPDATE_DONE, g.source == US_PULL ? 1 : 0, 0);   // reaches open pages over the WebSocket before the restart
    settingsSaverPause(SAVER_HOLD_UPDATE, false);
    g.paused = false;
    settingsFlush();
    g.state = DONE;
    g.doneAt = millis();   // the OSD/OLED show "GBS-Control updated" until the restart below
}
