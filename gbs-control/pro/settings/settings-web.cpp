#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <memory>
#include "../../options.h"
#include "settings-registry.h"
#include "settings-store.h"
#include "settings-request.h"
#include "settings-device.h"
#include "../update/update-device.h"
#include "../api/api-json.h"
#include "settings-fs.h"
#include "../api/api-device.h"
#include "../api/api-router.h"
#include "../api/api-guard.h"
#include "../log/log.h"

extern userOptions* uopt;
#include "../../src/WebSocketsServer.h"
extern const char* device_hostname_partial;   // gbs-control.ino
extern WebSocketsServer webSocket;   // gbs-control.ino: `WebSocketsServer webSocket(81);`

void settingsBroadcastChange(uint8_t index) {
    SettingDef d; settingsDefAt(index, d);
    char key[48], val[40], msg[96];
    settingsKeyCopy(d, key, sizeof(key));
    settingsFormat(d, settingsGet(*uopt, d), val, sizeof(val));
    snprintf(msg, sizeof(msg), "s:%s=%s", key, val);
    webSocket.broadcastTXT(msg);
}

void settingsBroadcastStatusText(const char* key, const char* value) {
    char msg[96];   // s:status.profile_autoload=<label>|<label>: 26 + 2 x 25 + 1
    snprintf(msg, sizeof(msg), "s:%s=%s", key, value);
    webSocket.broadcastTXT(msg);
}

char profileStatusPresetChar();   // gbs-control.ino: profilePresetStatusChar() for the current preset

// Periodic status: revision + read-only state
void settingsBroadcastStatus() {
    char msg[40];
    snprintf_P(msg, sizeof(msg), PSTR("s:status.rev=%u"), settingsRevision());
    webSocket.broadcastTXT(msg);
    if (updateInProgress()) return;   // no scaler read (I2C) while an upload writes the flash (Twi crashes)
    snprintf_P(msg, sizeof(msg), PSTR("s:status.preset=%c"), profileStatusPresetChar());
    webSocket.broadcastTXT(msg);
}

void sendJson(AsyncWebServerRequest* req, int code, const char* json) {
    AsyncWebServerResponse* resp = req->beginResponse(code, "application/json", json);
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

void sendJson(AsyncWebServerRequest* req, int code, const __FlashStringHelper* json) {
    AsyncWebServerResponse* resp = req->beginResponse(code, "application/json", String(json));
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

void sendJsonReplay(AsyncWebServerRequest* req, void (*build)(void (*sink)(const char*, void*), void*, const void*),
                    std::shared_ptr<const void> state) {
    std::shared_ptr<JsonReplay> r = std::make_shared<JsonReplay>(JsonReplay{build, state.get(), 0});
    AsyncWebServerResponse* resp = req->beginChunkedResponse("application/json", [r, state](uint8_t* buf, size_t max, size_t) -> size_t {
        return jsonReplayFill(*r, (char*)buf, max);
    });
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

// /api/settings answers from a copy taken when the request arrived: every chunk rebuilds the same text
struct SettingsSnapshot { userOptions u; bool dev; };
static void settingsSnapshotJson(void (*sink)(const char*, void*), void* ctx, const void* state) {
    const SettingsSnapshot* s = (const SettingsSnapshot*)state;
    settingsValuesJson(s->u, s->dev, sink, ctx);
}

// Rows in API_ROUTES_NORMAL (api-route-match.h)
void settingsGet(AsyncWebServerRequest* req, uint8_t) {
    std::shared_ptr<SettingsSnapshot> snap = std::make_shared<SettingsSnapshot>();
    snap->u = *uopt;
    snap->dev = uopt->devMode;
    sendJsonReplay(req, settingsSnapshotJson, snap);
}

void schemaGet(AsyncWebServerRequest* req, uint8_t) {
    // Chunked, one entry at a time: the whole schema (~15 KB) does not fit the free heap
    std::shared_ptr<SchemaStreamer> st = std::make_shared<SchemaStreamer>(uopt->devMode);
    AsyncWebServerResponse* resp = req->beginChunkedResponse("application/json", [st](uint8_t* buf, size_t max, size_t) -> size_t {
        return st->fill((char*)buf, max);
    });
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

void settingsPost(AsyncWebServerRequest* req, uint8_t) {
    const uint8_t kMax = 24;
    const char* keys[kMax]; const char* vals[kMax];
    String keyStore[kMax], valStore[kMax];
    uint8_t n = 0;
    size_t posted = 0;
    for (size_t i = 0; i < req->params(); i++) posted += req->getParam(i)->isPost() ? 1 : 0;
    if (posted > kMax) { sendJson(req, 400, F("{\"error\":\"too_many\"}")); return; }   // never drop the extras
    for (size_t i = 0; i < req->params() && n < kMax; i++) {
        const AsyncWebParameter* p = req->getParam(i);
        if (!p->isPost()) continue;
        keyStore[n] = p->name(); valStore[n] = p->value();
        keys[n] = keyStore[n].c_str(); vals[n] = valStore[n].c_str(); n++;
    }
    TextBuf err; textBufInit(err, 1024);
    SettingsPair pairs[kMax];
    SettingsStrWrite sw; sw.index = -1;
    SettingsValidateResult r = settingsValidateRequest(keys, vals, n, uopt->devMode, textBufSink, &err, pairs, kMax, &sw);
    if (sw.index >= 0 && sw.index == settingsStrFind("system.extra_hosts") &&
        guardHostLostByChange(req->host().c_str(), device_hostname_partial, uopt->extraHosts, sw.value)) {
        logPrintf(LC_API, LL_INFO, PSTR("extra hosts: refused, drops the name of this page (%s)"), req->host().c_str());
        textBufFree(err);
        sendJson(req, 409, F("{\"error\":\"in_use\"}"));
        return;
    }
    // Applied by apiLoop(), never in this network context; all or nothing
    bool strQueued = sw.index >= 0 && apiQueueStrSetting(sw);
    bool queued = (sw.index < 0 || strQueued) && apiQueueSettings(pairs, r.pairCount);
    memset(sw.value, 0, sizeof(sw.value));
    if (!queued) {
        if (strQueued) apiDropStrSetting();   // only our own pending write
        textBufFree(err);
        sendJson(req, 409, F("{\"error\":\"busy\"}"));
        return;
    }
    TextBuf out; textBufInit(out, 1280);
    textBufAppend(out, "{\"ok\":");
    char num[6]; snprintf(num, sizeof(num), "%u", r.okCount); textBufAppend(out, num);
    textBufAppend(out, ",\"errors\":{"); if (err.data) textBufAppend(out, err.data); textBufAppend(out, "}}");
    sendJson(req, out.failed ? 500 : (r.errCount ? 400 : 200), out.data ? out.data : "{}");
    textBufFree(out); textBufFree(err);
}
