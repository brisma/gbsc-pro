// ADV presets and macros on LittleFS, device glue of adv-store.cpp: routes /api/adv/presets|macros
// (list), /api/adv/preset|macro (text, upload), /api/adv/preset|macro/delete. Dev mode and the ADV Controller are
// required. Handlers only check and queue: an upload streams to a temp file in its folder (advTempPath), advStoreLoop() validates it line by
// line, renames it into place and answers s:status.adv_store=ok|error:<reason>.
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <memory>
#include <new>
#include "../../options.h"
#include "../settings/settings-device.h"
#include "../platform/platform-caps.h"
#include "../profiles/profile-file.h"
#include "../api/api-device.h"
#include "../api/api-router.h"
#include "../log/log.h"
#include "adv-store.h"
#include "adv-store-web.h"

extern userOptions* uopt;
void settingsBroadcastStatusText(const char* key, const char* value);   // settings-web.cpp

static const char* kindName(AdvKind k) { return k == ADV_PRESET ? PSTR("preset") : PSTR("macro"); }   // for %S

static bool advAllowed() { return uopt->devMode && hwHas(CAP_ADV); }

static bool fileExists(AdvKind k, uint8_t i) {
    char p[28];
    advPath(k, i, p, sizeof(p));
    return LittleFS.exists(p);
}

uint8_t advStoreIds(AdvKind k, char ids[][4], const char* idp[]) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < advMax(k); i++) {
        if (!fileExists(k, i)) continue;
        advId(k, i, ids[n]);
        idp[n] = ids[n];
        n++;
    }
    return n;
}

void advStoreDeleteAll() {
    for (uint8_t k = 0; k < 2; k++)
        for (uint8_t i = 0; i < advMax((AdvKind)k); i++) {
            char p[28];
            advPath((AdvKind)k, i, p, sizeof(p));
            if (LittleFS.exists(p)) LittleFS.remove(p);
        }
    logPrintf(LC_ADV, LL_INFO, PSTR("adv store: every preset and macro removed (restore)"));
}

uint32_t advStoreBlocks(size_t blockSize) {
    uint32_t blocks = 0;
    for (uint8_t k = 0; k < 2; k++)
        for (uint8_t i = 0; i < advMax((AdvKind)k); i++) {
            char p[28];
            advPath((AdvKind)k, i, p, sizeof(p));
            File f = LittleFS.open(p, "r");
            if (!f) continue;
            blocks += (f.size() + blockSize - 1) / blockSize;
            f.close();
        }
    return blocks;
}

// ---------------------------------------------------------------- jobs (one at a time)
enum JobOp : uint8_t { JOB_NONE, JOB_SAVE, JOB_DELETE };
static struct { JobOp op; AdvKind kind; uint8_t index; } gJob;

static struct Upload {
    bool rejected, denied, tooBig, noSpace, writeFailed;
    AdvKind kind;
    size_t size;
    File f;
} gUp;
static AdvUploadGate gGate;   // the request whose upload gUp belongs to (one at a time)

// The client went away mid-upload: close and drop its temp file, free the gate (a queued job keeps its file)
static void uploadAbandon(AsyncWebServerRequest* req) {
    if (!advGateOwns(gGate, (uintptr_t)req)) return;
    if (gUp.f) gUp.f.close();
    if (gJob.op == JOB_NONE) { char tmp[28]; advTempPath(gUp.kind, tmp, sizeof(tmp)); LittleFS.remove(tmp); }
    logPrintf(LC_ADV, LL_INFO, PSTR("adv store: upload abandoned (%u B), nothing changed"), (unsigned)gUp.size);
    gUp = Upload();
    advGateEnd(gGate, (uintptr_t)req);
}

// The request answered: its upload state goes, the gate opens for the next one
static void uploadDone(AsyncWebServerRequest* req) {
    if (!advGateOwns(gGate, (uintptr_t)req)) return;
    if (gUp.f) gUp.f.close();
    gUp = Upload();
    advGateEnd(gGate, (uintptr_t)req);
}

static void uploadChunk(AdvKind k, AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        if (!advGateBegin(gGate, (uintptr_t)req)) return;   // another upload runs: its file is left alone (409)
        req->onDisconnect([req]() { uploadAbandon(req); });
        if (gUp.f) gUp.f.close();
        gUp = Upload();
        gUp.kind = k;
        gUp.rejected = !apiGuardRequest(req);   // uploads run before the middleware
        if (gUp.rejected) return;
        if (!advAllowed()) { gUp.denied = true; return; }
        if (gJob.op != JOB_NONE) return;        // busy: the request handler answers 409
        size_t body = req->contentLength();
        if (body > advMaxBytes(k) + 1024) { gUp.tooBig = true; return; }   // multipart framing on top of the file
        FSInfo fi;
        size_t reserve = PROFILE_MAX_FILE_BYTES + 8192;
        if (!LittleFS.info(fi) || !advSpaceOk(fi.totalBytes - fi.usedBytes, fi.blockSize, body, reserve)) {
            gUp.noSpace = true;
            return;
        }
        char tmp[28];
        advTempPath(k, tmp, sizeof(tmp));
        gUp.f = LittleFS.open(tmp, "w");   // creates the folder when it is missing
        if (!gUp.f) { gUp.writeFailed = true; return; }
    }
    if (!advGateOwns(gGate, (uintptr_t)req) || !gUp.f) return;
    if (gUp.size + len > advMaxBytes(k)) {
        gUp.tooBig = true;
    } else if (gUp.f.write(data, len) != len) {
        gUp.writeFailed = true;
    } else {
        gUp.size += len;
    }
    if (gUp.tooBig || gUp.writeFailed) { char tmp[28]; advTempPath(k, tmp, sizeof(tmp)); gUp.f.close(); LittleFS.remove(tmp); return; }
    if (final) gUp.f.close();
}

void advPresetUpload(AsyncWebServerRequest* r, size_t i, uint8_t* d, size_t n, bool f) { uploadChunk(ADV_PRESET, r, i, d, n, f); }
void advMacroUpload(AsyncWebServerRequest* r, size_t i, uint8_t* d, size_t n, bool f) { uploadChunk(ADV_MACRO, r, i, d, n, f); }

static int idArg(AsyncWebServerRequest* req, AdvKind k, bool* given) {
    const AsyncWebParameter* p = req->hasParam("id", true) ? req->getParam("id", true)
                               : req->hasParam("id") ? req->getParam("id") : nullptr;
    *given = p && p->value().length();
    return *given ? advParseId(p->value().c_str(), k) : -1;
}

// POST /api/adv/preset, /api/adv/macro (after the upload): 202 {"ok":1,"id"} or the reason
void advSavePost(AsyncWebServerRequest* req, uint8_t kind) {
    AdvKind k = (AdvKind)kind;
    if (!advGateOwns(gGate, (uintptr_t)req)) {   // no file part, or another upload holds the gate
        bool busy = advGateBusy(gGate);
        logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %S save without an upload of its own (%s)"), kindName(k), busy ? "another runs" : "no file");
        if (!advAllowed()) sendJson(req, 404, F("{\"error\":\"none\"}"));
        else if (busy) sendJson(req, 409, F("{\"error\":\"busy\"}"));
        else sendJson(req, 400, F("{\"ok\":0,\"error\":\"invalid\"}"));
        return;
    }
    const __FlashStringHelper* refused = nullptr;
    int code = 202;
    if (gUp.rejected) { code = 403; refused = F("{\"error\":\"forbidden\"}"); }
    else if (gUp.denied || !advAllowed()) { code = 404; refused = F("{\"error\":\"none\"}"); }
    else if (gJob.op != JOB_NONE) { code = 409; refused = F("{\"error\":\"busy\"}"); }
    else if (gUp.tooBig) { code = 400; refused = F("{\"ok\":0,\"error\":\"size\"}"); }
    else if (gUp.noSpace) { code = 507; refused = F("{\"error\":\"space\"}"); }
    else if (gUp.writeFailed || !gUp.size) { code = 400; refused = F("{\"ok\":0,\"error\":\"invalid\"}"); }
    bool given = false;
    int id = -1;
    if (!refused) {
        id = idArg(req, k, &given);
        if (given && id < 0) { code = 400; refused = F("{\"ok\":0,\"error\":\"id\"}"); }
        for (uint8_t i = 0; !refused && !given && i < advMax(k); i++)   // new: the first free id
            if (!fileExists(k, i)) { id = i; break; }
        if (!refused && id < 0) { code = 409; refused = F("{\"error\":\"full\"}"); }
    }
    if (refused) {
        if (code != 409 || gJob.op == JOB_NONE) {   // never the file a queued job reads
            char tmp[28];
            advTempPath(k, tmp, sizeof(tmp));
            LittleFS.remove(tmp);
        }
        logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %S upload refused (%d)"), kindName(k), code);
        uploadDone(req);
        sendJson(req, code, refused);
        return;
    }
    gJob = {JOB_SAVE, k, (uint8_t)id};
    char cid[4];
    advId(k, (uint8_t)id, cid);
    logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %S %s queued (%u B%s)"), kindName(k), cid, (unsigned)gUp.size,
              given ? ", replaces" : ", new");
    uploadDone(req);
    char json[32];
    snprintf_P(json, sizeof(json), PSTR("{\"ok\":1,\"id\":\"%s\"}"), cid);
    sendJson(req, 202, json);
}

void advDeletePost(AsyncWebServerRequest* req, uint8_t kind) {
    AdvKind k = (AdvKind)kind;
    if (!advAllowed()) { sendJson(req, 404, F("{\"error\":\"none\"}")); return; }
    bool given = false;
    int id = idArg(req, k, &given);
    if (id < 0 || !fileExists(k, (uint8_t)id)) { sendJson(req, 404, F("{\"error\":\"unknown\"}")); return; }
    if (gJob.op != JOB_NONE) { sendJson(req, 409, F("{\"error\":\"busy\"}")); return; }
    gJob = {JOB_DELETE, k, (uint8_t)id};
    logPrintf(LC_ADV, LL_INFO, PSTR("adv store: delete %S %d queued"), kindName(k), id);
    sendJson(req, 202, F("{\"ok\":1}"));
}

static void finish(const char* result) {
    settingsBroadcastStatusText("status.adv_store", result);
    gJob.op = JOB_NONE;
}

static void runSave() {
    char path[28], cid[4], tmp[28];
    advPath(gJob.kind, gJob.index, path, sizeof(path));
    advTempPath(gJob.kind, tmp, sizeof(tmp));
    advId(gJob.kind, gJob.index, cid);
    std::unique_ptr<AdvYamlCheck> cp(new (std::nothrow) AdvYamlCheck());   // only while a save runs (RAM is short)
    if (!cp) { logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %s no memory to check"), cid); finish("error:memory"); return; }
    AdvYamlCheck& c = *cp;
    advYamlBegin(c, advMaxBytes(gJob.kind));
    File f = LittleFS.open(tmp, "r");
    if (!f) { logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %s upload missing"), cid); finish("error:write"); return; }
    uint8_t buf[64];
    bool ok = true;
    while (ok && f.available()) {
        int n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        ok = advYamlFeed(c, buf, (size_t)n);
    }
    f.close();
    ok = advYamlFinish(c) && ok;
    if (!ok) {
        LittleFS.remove(tmp);
        char v[24];
        snprintf_P(v, sizeof(v), PSTR("error:%s"), c.why ? c.why : "invalid");
        logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %s refused, %s; the old file is kept"), cid, v + 6);
        finish(v);
        return;
    }
    // same-folder rename: lfs_rename replaces an existing file atomically (removing it first could drop its folder)
    if (!LittleFS.rename(tmp, path)) {
        LittleFS.remove(tmp);
        logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %s rename failed"), cid);
        finish("error:write");
        return;
    }
    logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %s saved \"%s\" (%u registers or commands, %u B)"), cid, c.name,
              (unsigned)c.count, (unsigned)c.size);
    finish("ok");
}

void advStoreLoop() {
    static bool started = false;
    if (!started) {   // an upload cut by a restart leaves its temp file
        started = true;
        for (uint8_t k = 0; k < 2; k++) {
            char tmp[28];
            advTempPath((AdvKind)k, tmp, sizeof(tmp));
            if (!LittleFS.exists(tmp)) continue;
            LittleFS.remove(tmp);
            logPrintf(LC_ADV, LL_INFO, PSTR("adv store: stale upload %s removed"), tmp);
        }
    }
    if (gJob.op == JOB_SAVE) { runSave(); return; }
    if (gJob.op == JOB_DELETE) {
        char path[28];
        advPath(gJob.kind, gJob.index, path, sizeof(path));
        bool ok = LittleFS.remove(path);
        logPrintf(LC_ADV, LL_INFO, PSTR("adv store: %S %u %s"), kindName(gJob.kind), gJob.index, ok ? "deleted" : "delete failed");
        finish(ok ? "ok" : "error:write");
    }
}

// ---------------------------------------------------------------- reads
void advTextGet(AsyncWebServerRequest* req, uint8_t kind) {
    AdvKind k = (AdvKind)kind;
    if (!advAllowed()) { sendJson(req, 404, F("{\"error\":\"none\"}")); return; }
    bool given = false;
    int id = idArg(req, k, &given);
    char path[28];
    if (id >= 0) advPath(k, (uint8_t)id, path, sizeof(path));
    if (id < 0 || !LittleFS.exists(path)) { sendJson(req, 404, F("{\"error\":\"unknown\"}")); return; }
    AsyncWebServerResponse* resp = req->beginResponse(LittleFS, path, "text/yaml");
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

// {"items":[...]} one file per chunk: the filler reads the next file, checks it and sends its entry
struct ListState {
    AdvKind kind;
    uint8_t next;
    bool opened, closed;
    char pending[320];
    size_t len, pos;
    uint8_t sentItems;
    AdvYamlCheck c;
};

static bool nextEntry(ListState& s) {
    while (s.next < advMax(s.kind)) {
        uint8_t i = s.next++;
        char path[28], cid[4];
        advPath(s.kind, i, path, sizeof(path));
        File f = LittleFS.open(path, "r");
        if (!f) continue;
        advYamlBegin(s.c, advMaxBytes(s.kind));
        uint8_t buf[64];
        while (f.available()) {
            int n = f.read(buf, sizeof(buf));
            if (n <= 0 || !advYamlFeed(s.c, buf, (size_t)n)) break;
        }
        size_t bytes = f.size();
        f.close();
        advYamlFinish(s.c);   // a broken file still lists (its name may be empty): the page can delete it
        advId(s.kind, i, cid);
        size_t off = s.sentItems ? 1 : 0;
        if (off) s.pending[0] = ',';
        size_t n = advListEntryJson(cid, s.c, (uint32_t)bytes, s.pending + off, sizeof(s.pending) - off);
        if (!n) continue;
        s.len = n + off;
        s.pos = 0;
        s.sentItems++;
        return true;
    }
    return false;
}

void advListGet(AsyncWebServerRequest* req, uint8_t kind) {
    if (!advAllowed()) { sendJson(req, 404, F("{\"error\":\"none\"}")); return; }
    std::shared_ptr<ListState> st(new (std::nothrow) ListState());
    if (!st) { sendJson(req, 503, F("{\"error\":\"memory\"}")); return; }
    st->kind = (AdvKind)kind;
    AsyncWebServerResponse* resp = req->beginChunkedResponse("application/json", [st](uint8_t* buf, size_t max, size_t) -> size_t {
        ListState& s = *st;
        size_t out = 0;
        while (out < max) {
            if (s.pos < s.len) {
                size_t n = s.len - s.pos < max - out ? s.len - s.pos : max - out;
                memcpy(buf + out, s.pending + s.pos, n);
                s.pos += n; out += n;
                continue;
            }
            if (!s.opened) { s.opened = true; s.len = (size_t)snprintf(s.pending, sizeof(s.pending), "{\"items\":["); s.pos = 0; continue; }
            if (out) break;   // one file per chunk
            if (nextEntry(s)) continue;
            if (s.closed) break;
            s.closed = true;
            s.len = (size_t)snprintf(s.pending, sizeof(s.pending), "]}");
            s.pos = 0;
        }
        return out;
    });
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}
