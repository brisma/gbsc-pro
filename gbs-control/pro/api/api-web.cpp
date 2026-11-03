// HTTP routes of the unified API and the request guard middleware (device only)
#include "../input/input-auto-device.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../../options.h"
#include "../settings/settings-device.h"
#include "api-guard.h"
#include "api-device.h"
#include "api-actions.h"
#include "api-backup.h"
#include "../adv/adv-store-web.h"
#include "../link/adv-link-device.h"
#include "../link/adv-controller-format.h"
#include "../gbs-control-pro.h"
#include "../platform/platform-device.h"
#include "api-json.h"
#include "api-live.h"
#include "screen-snapshot.h"
#include "net-stats.h"
#include "../ui/menu-device.h"
#include "../ui/menu-types.h"
#include "api-admission.h"
#include "../update/update-device.h"
#include "../update/ota-update.h"
#include "../update/safe-mode.h"
#include "api-router.h"
#include "../log/log.h"
#include "../log/event-ring.h"
#include "../profiles/profile-store.h"
#include <memory>
#include <new>
#include <ESP8266WiFi.h>
#include "../../src/WebSocketsServer.h"
#include "../net/channel-guard-device.h"
#include "../net/wifi-tune-device.h"

extern userOptions* uopt;
extern runTimeOptions* rto;
extern const char* device_hostname_partial;
extern const char* device_hostname_full;   // gbs-control.ino
extern const char* ap_ssid;
extern const char* ap_password;
extern WebSocketsServer webSocket;
extern const char kBuildId[];   // gbs-control.ino

// Host check on every request; POST/PUT/DELETE under /api also need X-GBSC: 1 (a form cannot send it)
bool apiGuardRequest(AsyncWebServerRequest* req) {
    if (!guardHostAllowed(req->host().c_str(), device_hostname_partial, uopt->extraHosts)) return false;
    if (req->hasHeader("Origin") &&
        !guardOriginAllowed(req->header("Origin").c_str(), device_hostname_partial, uopt->extraHosts)) return false;
    if (req->method() != HTTP_GET && req->url().startsWith("/api/") && req->header("X-GBSC") != "1") return false;
    return true;
}

static bool wsValidateHeader(String name, String value) {
    if (name.equalsIgnoreCase("Origin"))
        return guardOriginAllowed(value.c_str(), device_hostname_partial, uopt->extraHosts);
    return true;
}

void apiWebSocketGuard() {
    static const char* kMandatory[] = {"Origin"};   // browsers always send it on a WebSocket handshake
    webSocket.onValidateHttpHeader(wsValidateHeader, kMandatory, 1);
}

static void addGuard(AsyncWebServer& server) {
    server.addMiddleware([](AsyncWebServerRequest* req, ArMiddlewareNext next) {
        if (!apiGuardRequest(req)) {
            // Access point clients (phone captive-portal probes, any typed name): a page load goes to the IP
            // instead of a JSON 403; only GET, so nothing that writes is ever redirected
            if (req->method() == HTTP_GET && (WiFi.getMode() & WIFI_AP) &&
                req->client()->localIP() == WiFi.softAPIP() && !req->url().startsWith("/api/") &&
                !req->url().startsWith("/ws")) {
                req->redirect("http://" + WiFi.softAPIP().toString() + "/");
                return;
            }
            // Most likely a router-given name: tell the user what works
            sendJson(req, 403, F("{\"error\":\"forbidden\",\"hint\":\"open the device by IP or http://gbscontrol.local/ "
                                 "(other names: system.extra_hosts, serial command H)\"}"));
            return;
        }
        next();
    });
}


static volatile bool gSafeRestart = false;

// Safe mode: guard + info, backup, firmware upload and restart only
// Admission at header time (api-admission.h): registered first, so it sees every request before any handler
// allocates; a refused request is answered here (its body is never parsed) and skips the server middlewares
static Admission gAdmission;

// Admitted connections by remote address and port (unique on port 80); an entry counts while lwIP still lists
// that connection, so a request destroyed without a callback cannot leak a slot. Rejected ones never count.
static struct { uint32_t ip; uint16_t port; } gAdmitted[6];

static uint8_t admittedAlive() {
    uint8_t n = 0;
    for (auto& a : gAdmitted) {
        if (!a.port) continue;
        if (netHttpAlive(a.ip, a.port)) n++;
        else a.port = 0;
    }
    return n;
}

// Why a connection was refused, for its 503 (handleRequest runs later, after other requests may have been decided)
static struct { uint16_t port; uint8_t verdict; } gRefused[4];
static uint8_t gRefusedNext;

static volatile uint16_t gRefusedCount[4];   // memory, update, load, starting: counted here (network context), logged by apiWebLogTick

// The server answers while setup() still runs (WiFi is up before LittleFS is mounted): until then every request
// gets 503 "starting", or a backup came back empty and settings read as defaults
static volatile bool gApiReady = false;

void apiSetReady() { gApiReady = true; }

static void noteRefusal(uint16_t port, uint8_t verdict) {
    gRefusedCount[verdict == BUSY_MEMORY ? 0 : verdict == BUSY_UPDATE ? 1 : verdict == BUSY_STARTING ? 3 : 2]++;
    gRefused[gRefusedNext] = {port, verdict};
    gRefusedNext = (uint8_t)((gRefusedNext + 1) % 4);
}

static const char* refusalOf(uint16_t port) {
    for (uint8_t k = 1; k <= 4; k++) {                    // newest first: a client may reuse a port
        const auto& r = gRefused[(gRefusedNext + 4 - k) % 4];
        if (r.port == port) {
            return r.verdict == BUSY_MEMORY ? "memory" : r.verdict == BUSY_UPDATE ? "update" : r.verdict == BUSY_STARTING ? "starting" : "load";
        }
    }
    return "load";
}

static void remember(uint32_t ip, uint16_t port) {
    for (auto& a : gAdmitted) if (!a.port) { a.ip = ip; a.port = port; return; }
    gAdmitted[0].ip = ip; gAdmitted[0].port = port;   // full (cannot happen with 3 admitted): reuse a slot
}

class AdmissionHandler : public AsyncWebHandler {
public:
    bool canHandle(AsyncWebServerRequest* req) const override {
        if (req->method() == HTTP_GET && req->url() == "/api/info") return false;   // small; shows why others wait
        if (req->method() == HTTP_GET && req->url() == "/api/log") return false;    // the tool when memory is short
        // a way out of every state: refusing the restart left a fragmented device without remote recovery
        if (req->method() == HTTP_POST && req->url() == "/api/actions/system.restart") return false;
        bool isUpload = req->method() == HTTP_POST && req->url() == "/api/system/update";
        AdmitVerdict v = gAdmission.decide(ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), admittedAlive() + 1,
                                           admitUpdateBusy(updateSource(), req->method() == HTTP_GET), isUpload, !gApiReady);
        if (v != ADMIT) {
            noteRefusal(req->client()->remotePort(), v);
            return true;
        }
        remember((uint32_t)req->client()->remoteIP(), req->client()->remotePort());
        return false;
    }
    void handleRequest(AsyncWebServerRequest* req) override {
        // a browser opening the page gets a page that retries by itself, not a blank 503; a script, not a meta
        // refresh: on a web app route (/#/...) Chromium takes the refresh for a jump within the page and never reloads
        static const char kBusyPage[] PROGMEM = "<script>setTimeout(()=>location.reload(),2000)</script>GBSC PRO is busy, retrying...";
        // a basic response from a String copy: a PROGMEM response would need a ~1 KB send buffer on this low-memory path
        AsyncWebServerResponse* resp = req->url() == "/" ? req->beginResponse(503, "text/html", String(FPSTR(kBusyPage)))
                                                          : req->beginResponse(503);
        resp->addHeader("Retry-After", "1");
        // the web app retries only these (the request never ran); "memory" on an upload: restart, then upload
        resp->addHeader("X-GBSC-Busy", refusalOf(req->client()->remotePort()));
        req->send(resp);
    }
};

static void addAdmission(AsyncWebServer& server) {
    static bool added = false;
    if (added) return;
    added = true;
    server.addHandler(new AdmissionHandler()).skipServerMiddlewares();
}

void apiWebRegisterSafe(AsyncWebServer& server) {
    addAdmission(server);
    addGuard(server);
    apiRouterAdd(server, true);
}

void safeRestartPost(AsyncWebServerRequest* req, uint8_t) {
    gSafeRestart = true;
    sendJson(req, 200, F("{\"ok\":1}"));
}

void apiSafeLoop() {
    if (gSafeRestart) { delay(200); ESP.restart(); }
}

void apiWebRegister(AsyncWebServer& server) {
    addAdmission(server);
    addGuard(server);
    apiRouterAdd(server, false);
}

// Rows in API_ROUTES_NORMAL (api-route-match.h)
void actionsListGet(AsyncWebServerRequest* req, uint8_t) {
    sendJsonReplay(req, [](void (*sink)(const char*, void*), void* ctx, const void* st) {
        actionsListJson(*(const bool*)st, sink, ctx);   // the action table is static: same text every chunk
    }, std::make_shared<bool>(uopt->devMode));
}

void actionsPost(AsyncWebServerRequest* req, uint8_t) {
    String name = req->url().substring(strlen("/api/actions/"));
    int idx = actionsFind(name.c_str());
    ActionDef d;
    if (idx >= 0) actionsDefAt((uint8_t)idx, d);
    if (idx < 0 || !actionsAvailable(d, uopt->devMode)) { sendJson(req, 404, F("{\"error\":\"unknown\"}")); return; }
    ActionRequest r; r.index = (uint8_t)idx; r.len = 0;
    if (d.flags & ACTION_PARAM_HEX) {
        if (!req->hasArg("data") || !actionsParseHexTriplets(req->arg("data").c_str(), r.data, sizeof(r.data), r.len)) {
            sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"data\":\"invalid\"}}"));
            return;
        }
    }
    if ((d.flags & ACTION_PARAM_OPT_BYTES) && req->hasArg("data") &&
        !actionsParseHexBytes(req->arg("data").c_str(), r.data, sizeof(r.data), r.len)) {
        sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"data\":\"invalid\"}}"));
        return;
    }
    if ((d.flags & (ACTION_PARAM_TEXT | ACTION_PARAM_OPT_TEXT)) && (req->hasArg("data") || (d.flags & ACTION_PARAM_TEXT))) {
        bool ok = req->hasArg("data") && actionsParseText(req->arg("data").c_str(), r.data, sizeof(r.data), r.len);
        if (ok && d.kind == ACT_FUNC && d.arg == AF_CHECK_CHANNEL) ok = otaChannelParse((const char*)r.data) >= 0;
        if (ok && d.kind == ACT_FUNC && d.arg == AF_INSTALL_UPDATE) ok = !strcmp((const char*)r.data, "manual");
        if (!ok) { sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"data\":\"invalid\"}}")); return; }
    }
    if (d.flags & ACTION_PARAM_BYTES) {
        if (!req->hasArg("data") || !actionsParseHexBytes(req->arg("data").c_str(), r.data, sizeof(r.data), r.len)) {
            sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"data\":\"invalid\"}}"));
            return;
        }
    }
    if (actionsValidateParams(d, r.data, r.len) != ACT_PARAM_OK) {
        logPrintf(LC_API, LL_INFO, PSTR("action %s: invalid data"), d.name);
        sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"data\":\"invalid\"}}"));
        return;
    }
    if (d.kind == ACT_FUNC && d.arg == AF_TERMINATION && uopt->activeInputType != InputTypeRGBs) {   // 75R sync: RGB SCART only
        logPrintf(LC_API, LL_INFO, PSTR("termination: refused, input %u"), (unsigned)uopt->activeInputType);
        sendJson(req, 409, F("{\"error\":\"not_rgb_scart\"}"));
        return;
    }
    bool canStart = !(d.kind == ACT_FUNC && d.arg == AF_ADV_CONTROLLER_UPDATE) || advLinkUpdateCanStart();
    if (!canStart || apiActionQueueFull()) { sendJson(req, 409, F("{\"error\":\"busy\"}")); return; }
    if (d.kind == ACT_FUNC && d.arg == AF_ADV_READ) {         // as dev.tv5725_read: the gen of the result comes back now
        r.data[4] = advLinkRegReadAnnounce();
        apiQueueAction(r);
        char body[24]; snprintf(body, sizeof(body), "{\"ok\":1,\"gen\":%u}", (unsigned)r.data[4]);
        sendJson(req, 200, body);
        return;
    }
    if (d.kind == ACT_FUNC && d.arg == AF_TV5725_READ) {      // the result's gen: a GET never shows an older read as new
        r.data[1] = apiRegsAnnounce(r.data[0]);
        apiQueueAction(r);
        char body[24]; snprintf(body, sizeof(body), "{\"ok\":1,\"gen\":%u}", (unsigned)r.data[1]);
        sendJson(req, 200, body);
        return;
    }
    if (!apiQueueAction(r)) { sendJson(req, 409, F("{\"error\":\"busy\"}")); return; }
    sendJson(req, 200, F("{\"ok\":1}"));
}

// Developer screen readback: the last dev.screen_snapshot (screen-snapshot.h layout)
void devSnapshotGet(AsyncWebServerRequest* req, uint8_t) {
    const uint8_t* s = apiSnapshot();
    if (!uopt->devMode || !s) { sendJson(req, 404, F("{\"error\":\"none\"}")); return; }
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/octet-stream", s, snap::kBytes);
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

// ADV register read: the last dev.adv_read (Developer mode, a board with the ADV Controller)
void advReadGet(AsyncWebServerRequest* req, uint8_t) {
    if (!uopt->devMode || !hwHas(CAP_ADV)) { sendJson(req, 404, F("{\"error\":\"none\"}")); return; }
    char json[ADV_REG_READ_JSON_MAX];
    if (!advRegReadJson(advLinkRegReadResult(), advLinkRegReadAnnounced(), json, sizeof(json))) {
        sendJson(req, 500, F("{\"error\":\"size\"}"));
        return;
    }
    sendJson(req, 200, json);
}

// Developer register view: the last dev.tv5725_read
void devRegsGet(AsyncWebServerRequest* req, uint8_t) {
    if (!uopt->devMode) { sendJson(req, 404, F("{\"error\":\"none\"}")); return; }
    static const char* const kState[] = {"idle", "pending", "done", "busy"};
    TvRegsResult r = apiRegsResult();
    std::unique_ptr<char[]> json(new (std::nothrow) char[600]);
    if (!json) { sendJson(req, 503, F("{\"error\":\"memory\"}")); return; }
    int w = snprintf(json.get(), 600, "{\"state\":\"%s\",\"seg\":%u,\"gen\":%u,\"hex\":\"", kState[r.state & 3],
                     (unsigned)r.seg, (unsigned)r.gen);
    if (r.v) w += (int)regsHexFormat(r.v, 256, json.get() + w, 600 - w);
    snprintf(json.get() + w, 600 - w, "\"}");
    sendJson(req, 200, json.get());
}

void restorePost(AsyncWebServerRequest* req, uint8_t) {
    int code = apiRestoreFinish(req);
    sendJson(req, code, code == 202 ? "{\"ok\":1}" : "{\"ok\":0}");
}

void restoreUpload(AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final) {
    apiRestoreChunk(req, index, data, len, final);
}

// WiFi details for System > WiFi (the menu's LT_WIFI_* sources); the AP password only in AP mode (needed to join).
// The body is built on the heap: the network context stack is small
void wifiGet(AsyncWebServerRequest* req, uint8_t) {
    bool ap = WiFi.getMode() == WIFI_AP;
    String ssid = ap ? String(ap_ssid) : WiFi.SSID();
    String ip = (ap ? WiFi.softAPIP() : WiFi.localIP()).toString();
    std::unique_ptr<char[]> json(new (std::nothrow) char[WIFI_JSON_MAX]);
    if (!json) { sendJson(req, 503, F("{\"error\":\"memory\"}")); return; }
    if (!wifiJson(ap, ssid.c_str(), ip.c_str(), device_hostname_full, uopt->extraHosts, ap_password, json.get(), WIFI_JSON_MAX)) {
        sendJson(req, 500, F("{\"error\":\"size\"}"));
        return;
    }
    sendJson(req, 200, json.get());
}

void wifiScanGet(AsyncWebServerRequest* req, uint8_t) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); n = WIFI_SCAN_RUNNING; }
    if (n == WIFI_SCAN_RUNNING) { sendJson(req, 200, F("{\"scanning\":1}")); return; }
    int ix[32]; int count = n < 32 ? n : 32;
    for (int i = 0; i < count; i++) ix[i] = i;
    for (int i = 0; i < count; i++)                      // strongest first
        for (int j = 1; j < count - i; j++)
            if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1])) { int t = ix[j]; ix[j] = ix[j - 1]; ix[j - 1] = t; }
    AsyncResponseStream* resp = req->beginResponseStream("application/json");
    resp->addHeader("Cache-Control", "no-store");
    resp->print("{\"networks\":[");
    bool first = true;
    for (int i = 0; i < count; i++) {
        String s = WiFi.SSID(ix[i]);
        if (!s.length()) continue;
        bool dup = false;                                // keep the strongest entry per SSID
        for (int k = 0; k < i && !dup; k++) dup = WiFi.SSID(ix[k]) == s;
        if (dup) continue;
        char ssid[72]; jsonEscape(s.c_str(), ssid, sizeof(ssid));
        int q = (constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2;
        resp->printf_P(PSTR("%s{\"ssid\":\"%s\",\"quality\":%d,\"secure\":%d}"), first ? "" : ",", ssid, q,
                     WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE ? 0 : 1);
        first = false;
    }
    resp->print("]}");
    WiFi.scanDelete();
    req->send(resp);
}

void wifiConnectPost(AsyncWebServerRequest* req, uint8_t) {
    String ssid = req->hasArg("ssid") ? req->arg("ssid") : String();
    String pass = req->hasArg("password") ? req->arg("password") : String();
    if (!apiQueueWifiConnect(ssid.c_str(), pass.c_str())) {
        sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"ssid\":\"invalid\"}}"));
        return;
    }
    sendJson(req, 200, F("{\"ok\":1}"));
}

void wifiResetPost(AsyncWebServerRequest* req, uint8_t) {
    apiQueueWifiReset();
    sendJson(req, 200, F("{\"ok\":1}"));
}

// Console log from the loop: refused web requests and accept-guard resets (one line per 10 s at most), the
// low-memory state of the admission, and a heap line every minute (debug)
void apiWebLogTick(uint32_t now) {
    static uint32_t at = 0, heapAt = 0;
    static uint16_t tcpSeen = 0;
    static bool low = false;
    if (gAdmission.low() != low) {
        low = gAdmission.low();
        if (low) logPrintf(LC_SYS, LL_INFO, PSTR("low memory: %u bytes free (block %u), new web requests wait"),
                           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
        else logPrintf(LC_SYS, LL_INFO, PSTR("memory ok again: %u bytes free"), (unsigned)ESP.getFreeHeap());
    }
    // loop() stack: free words never touched since boot (painted guard), the deepest call path so far
    static bool stackWarned = false;
    static uint32_t stackAt = 0, stackFree = 0xFFFF;
    if (now - stackAt >= 1000 || !stackAt) {             // a scan of the guard words (~20 us); it only ever falls
        stackAt = now ? now : 1;
        stackFree = ESP.getFreeContStack();
    }
    if (stackFree < 1024 && !stackWarned) {
        stackWarned = true;
        logPrintf(LC_SYS, LL_INFO, PSTR("low stack: %u bytes of the loop stack never used"), (unsigned)stackFree);
    }
    if (now - heapAt >= 60000) {
        heapAt = now;
        logPrintf(LC_SYS, LL_DEBUG, PSTR("heap %u, block %u, lowest %u, loop stack free %u"), (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxFreeBlockSize(), (unsigned)netStats().heapMin, (unsigned)stackFree);
    }
    if (now - at < 10000) return;
    at = now;
    uint16_t m = gRefusedCount[0], u = gRefusedCount[1], l = gRefusedCount[2], st = gRefusedCount[3];
    if (m || u || l || st) {
        gRefusedCount[0] = gRefusedCount[1] = gRefusedCount[2] = gRefusedCount[3] = 0;   // a refusal between the read and here is lost: rare
        logPrintf(LC_API, LL_INFO, PSTR("web requests refused: %u (memory %u, update %u, too many %u, starting %u)"),
                  (unsigned)(m + u + l + st), (unsigned)m, (unsigned)u, (unsigned)l, (unsigned)st);
    }
    uint16_t tcp = (uint16_t)netTcpRefused();
    if (tcp != tcpSeen) {
        logPrintf(LC_API, LL_INFO, PSTR("connections reset by the accept guard: %u"), (unsigned)(uint16_t)(tcp - tcpSeen));
        tcpSeen = tcp;
    }
}

void apiInfoGet(AsyncWebServerRequest* req, uint8_t) {
    if (req->hasArg("reset_net")) netStatsReset();   // diagnostics: start a new measurement window
    const PlatformProbes& p = platformProbes();
    char ssid[72]; jsonEscape(WiFi.SSID().c_str(), ssid, sizeof(ssid));
    bool ap = WiFi.getMode() == WIFI_AP;
    String ip = ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    // heap, only while the reply is built (no static buffer); the format string lives in flash
    const size_t kJson = 2150;   // worst case ~1950: advController + signal 511 (ADV_CONTROLLER_JSON_MAX), ssid 71, crash ~250, update ~220
    char* json = (char*)malloc(kJson + ADV_CONTROLLER_JSON_MAX);
    if (!json) { sendJson(req, 503, F("{\"error\":\"memory\"}")); return; }
    char* advController = json + kJson;   // one allocation; the network context stack stays small
    if (!hwHas(CAP_ADV)) snprintf_P(advController, ADV_CONTROLLER_JSON_MAX, PSTR("{\"status\":\"absent\"}"));
    else if (!advControllerStatusJson(advLinkController(), advLinkControllerUpdateAvailable(), advLinkUpdateStatus(), advController, ADV_CONTROLLER_JSON_MAX,
                             advLinkSignal()))
        snprintf_P(advController, ADV_CONTROLLER_JSON_MAX, PSTR("{\"status\":\"error\"}"));
    int w = snprintf_P(json, kJson, PSTR(
        "{\"api\":1,\"safe_mode\":%s,\"fw\":\"%s\",\"marker\":\"%s\",\"build\":\"%s\",\"board\":\"%s\",\"caps\":%u,"
        "\"probes\":{\"0x5D\":{\"ack\":%u,\"tries\":%u},\"0x44\":{\"ack\":%u,\"tries\":%u},\"0x3C\":{\"ack\":%u,\"tries\":%u},\"rounds\":%u,\"bus\":%u},"
        "\"heap\":%u,\"max_block\":%u,\"wifi\":{\"mode\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"sleep\":%u,\"rssi\":%d,\"channel\":%d,\"slow\":%u},"
        "\"adv_controller\":%s,"
        "\"net\":{\"heap_min\":%u,\"block_min\":%u,\"tcp\":%u,\"tcp_max\":%u,\"tw\":%u,\"tw_max\":%u,"
        "\"uptime\":%lu,\"reset\":\"%s\",\"exc\":%u,\"epc1\":\"%08lX\",\"http\":%u,\"low\":%u,\"tcp_refused\":%u,\"starve_max_ms\":%lu,\"starve_at\":%lu},"
        "\"clockgen\":{\"detected\":%u,\"hz\":%lu,\"preset\":%u},"
        "\"input\":{\"choice\":%u,\"active\":%u,\"last\":%u,\"state\":\"%s\",\"termination\":%s},"
        "\"signal\":"),
        safeModeActive() ? "true" : "false", GBS_FW_FULL_VERSION, GBS_FW_VERSION_MARKER, kBuildId, platformBoardName(), platformCaps(),
        p.stv.ack, p.stv.attempts, p.pt.ack, p.pt.attempts, p.oled.ack, p.oled.attempts, platformProbeRounds(), platformBusAtBoot(),
        ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), ap ? "ap" : "sta", ip.c_str(), ap ? "" : ssid,
        (unsigned)WiFi.getSleepMode(),   // 0 none, 1 light, 2 modem
        ap ? 0 : (int)WiFi.RSSI(), (int)channelGuardChannel(),   // channel: EMI investigation (1080p output vs Wi-Fi)
        !ap && wifiTuneSlowChannel() ? 1u : 0u,                  // slow: the WiFi page suggests another router channel
        advController,
        (unsigned)netStats().heapMin, (unsigned)netStats().blockMin, netStats().tcpNow, netStats().tcpMax,
        netStats().twNow, netStats().twMax, (unsigned long)(millis() / 1000), ESP.getResetReason().c_str(),
        (unsigned)ESP.getResetInfoPtr()->exccause, (unsigned long)ESP.getResetInfoPtr()->epc1,
        netHttpConnections(), gAdmission.low() ? 1u : 0u, (unsigned)netTcpRefused(),
        (unsigned long)netStats().starveMaxMs, (unsigned long)netStats().starveAtS,
        rto->extClockGenDetected ? 1u : 0u, (unsigned long)rto->freqExtClockGen, (unsigned)rto->presetID,   // RAM copies: no I2C here
        (unsigned)uopt->inputChoice, (unsigned)uopt->activeInputType, (unsigned)uopt->inputLast,   // auto input
        uopt->inputChoice ? "manual" : inputAutoSearching() ? "searching" : "found",
        advSyncTermination ? "true" : "false");                                                   // 75R sync (RGB SCART)
    const MenuSignal& ms = menuDeviceSignal();   // RAM copy, no I2C here
    InfoSignal sg = {ms.noSignal, (uint8_t)rto->videoStandardInput, ms.srcLines, ms.srcHzX10, ms.outHzX10, ms.interlaced,
                     ms.scanlinesAllowed, ms.advNtsc, ms.presetId};
    if (w > 0 && (size_t)w < kJson) w += infoSignalJson(sg, json + w, kJson - w);
    if (w > 0 && (size_t)w < kJson) w += snprintf_P(json + w, kJson - w, PSTR(",\"crash\":"));
    if (w > 0 && (size_t)w < kJson) w += crashInfoJson(json + w, kJson - w);   // the last crash (RTC)
    if (w > 0 && (size_t)w < kJson) w += snprintf_P(json + w, kJson - w, PSTR(",\"upload\":"));
    if (w > 0 && (size_t)w < kJson) w += updateUploadJson(json + w, kJson - w);   // the last web upload
    if (w > 0 && (size_t)w < kJson) w += snprintf_P(json + w, kJson - w, PSTR(",\"update\":"));
    if (w > 0 && (size_t)w < kJson) w += otaInfoJson(json + w, kJson - w);        // OTA: channel, last check, offers
    if (w > 0 && (size_t)w < kJson) w += snprintf_P(json + w, kJson - w, PSTR("}"));
    if (w < 0 || (size_t)w >= kJson) { free(json); sendJson(req, 500, F("{\"error\":\"info_too_long\"}")); return; }
    sendJson(req, 200, json);
    free(json);
}

// Console log ring (log-ring.h): {"lines":[...],"next":N,"dropped":D,"boot":B}, streamed one whole record at a time;
// ?since=<seq>&boot=<B>: a since from another boot (the device restarted) reads everything
void logGet(AsyncWebServerRequest* req, uint8_t) {
    int32_t since = -1;
    if (req->hasArg("since")) {
        unsigned long v = strtoul(req->arg("since").c_str(), nullptr, 10);
        since = v > 65535 ? -1 : (int32_t)v;
    }
    int32_t boot = -1;
    if (req->hasArg("boot")) {
        unsigned long v = strtoul(req->arg("boot").c_str(), nullptr, 10);
        boot = v > 65535 ? -1 : (int32_t)v;
    }
    std::shared_ptr<LogJsonCursor> c = std::make_shared<LogJsonCursor>();
    logJsonBegin(*c, since, boot, logRing());
    AsyncWebServerResponse* resp = req->beginChunkedResponse("application/json", [c](uint8_t* buf, size_t max, size_t) -> size_t {
        return logJsonFill(*c, logRing(), (char*)buf, max);
    });
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

// Status history: the event ring after ?since= (seq), with the log ring's boot id (a since from another boot means
// nothing: the page starts over when boot changes)
void eventsGet(AsyncWebServerRequest* req, uint8_t) {
    uint16_t since = 0;
    if (req->hasArg("since")) {
        unsigned long v = strtoul(req->arg("since").c_str(), nullptr, 10);
        since = v > 65535 ? 0 : (uint16_t)v;
    }
    std::unique_ptr<char[]> json(new (std::nothrow) char[EVENTS_JSON_MAX]);
    if (!json) { sendJson(req, 503, F("{\"error\":\"memory\"}")); return; }
    if (!eventsJson(eventRing(), since, logRing().boot(), json.get(), EVENTS_JSON_MAX)) { sendJson(req, 500, F("{\"error\":\"size\"}")); return; }
    sendJson(req, 200, json.get());
}

// Live readbacks for the web rows (gain, SOG, position, Developer toggles): the cache the loop fills while asked;
// the first answer after a pause has no values yet (the page asks again a second later)
void liveGet(AsyncWebServerRequest* req, uint8_t) {
    apiLiveAsked();
    std::unique_ptr<char[]> json(new (std::nothrow) char[LIVE_JSON_MAX]);
    if (!json) { sendJson(req, 503, F("{\"error\":\"memory\"}")); return; }
    if (!apiLiveJson(json.get(), LIVE_JSON_MAX)) { sendJson(req, 500, F("{\"error\":\"size\"}")); return; }
    sendJson(req, 200, json.get());
}

void backupGet(AsyncWebServerRequest* req, uint8_t) {
    // Chunked: the body (profiles included) never sits in RAM; the filler runs in the network context
    // and only reads LittleFS. Profile ids are snapshotted now.
    std::shared_ptr<BackupStreamer> st = std::make_shared<BackupStreamer>();
    char ids[PROFILE_MAX][4];
    const char* idp[PROFILE_MAX];
    uint8_t n = 0;
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {   // files, not the RAM list: safe mode has no list
        char path[24]; profilePath(i, path);
        if (!settingsFs().exists(path)) continue;
        ProfileList::idOf(i, ids[n]);
        idp[n] = ids[n];
        n++;
    }
    st->begin(&settingsFs(), idp, n);
    char presets[ADV_PRESET_MAX][4], macros[ADV_MACRO_MAX][4];   // ADV presets and macros, also snapshotted now
    const char* pp[ADV_PRESET_MAX];
    const char* mp[ADV_MACRO_MAX];
    uint8_t np = advStoreIds(ADV_PRESET, presets, pp), nm = advStoreIds(ADV_MACRO, macros, mp);
    st->beginAdv(pp, np, mp, nm);
    settingsBackupStreaming(true);                    // no save replaces a file while it is read in pieces
    AsyncWebServerResponse* resp = req->beginChunkedResponse("text/plain", [st](uint8_t* buf, size_t max, size_t) -> size_t {
        size_t n = st->fill((char*)buf, max);
        settingsBackupStreaming(n != 0);              // 0: the end; a stalled client lets it lapse
        return n;
    });
    resp->addHeader("Cache-Control", "no-store");
    resp->addHeader("Content-Disposition", "attachment; filename=\"gbsc-backup-" GBS_FW_FULL_VERSION ".txt\"");
    req->send(resp);
}

void updatePost(AsyncWebServerRequest* req, uint8_t) {
    int code = updateUploadFinish(req);
    sendJson(req, code, code == 202 ? "{\"ok\":1}" : code == 409 ? "{\"error\":\"busy\"}" : "{\"ok\":0}");
}

void updateUpload(AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final) {
    updateUploadChunk(req, index, data, len, final);
}
