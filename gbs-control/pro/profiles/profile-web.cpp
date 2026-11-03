// /api/profiles routes (device only): validate in the handler, run in the loop (profileOpsLoop)
#include "../input/input-types.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "../../options.h"
#include "../settings/settings-device.h"
#include "../api/api-device.h"
#include "profile-store.h"
#include "profile-names.h"
#include "profile-modes.h"
#include "profile-web.h"
#include "profile-json.h"
#include "profile-args.h"
#include "../log/log.h"
#include "../api/api-router.h"

extern userOptions* uopt;

namespace {

// id param -> list index, or -1 (unknown or unused)
int indexParam(AsyncWebServerRequest* req) {
    if (!req->hasArg("id")) return -1;
    int i = ProfileList::indexOfId(req->arg("id").c_str());
    if (i < 0 || !profileList().at((uint8_t)i).used) return -1;
    return i;
}

bool nameParam(AsyncWebServerRequest* req, char out[PROFILE_NAME_BUF]) {
    return req->hasArg("name") && profileNameSanitize(req->arg("name").c_str(), out, 25);
}

void queued(AsyncWebServerRequest* req, bool ok) {
    if (ok) sendJson(req, 202, F("{\"ok\":1}"));
    else sendJson(req, 409, F("{\"error\":\"busy\"}"));
}

const char* argOrNull(AsyncWebServerRequest* req, const char* name, String& keep) {
    if (!req->hasArg(name)) return nullptr;
    keep = req->arg(name);
    return keep.c_str();
}

// 400 with the field the rules refused ("name" or "region")
void argError(AsyncWebServerRequest* req, ProfileArgError e) {
    if (e == PARG_REGION) sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"region\":\"invalid\"}}"));
    else sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"name\":\"invalid\"}}"));
}

struct ImportState { bool rejected; bool tooBig; File f; size_t size; };
ImportState gImport;

} // namespace

// Rows in API_ROUTES_NORMAL (api-route-match.h)
void profileOpPost(AsyncWebServerRequest* req, uint8_t op) {
    static const uint8_t kOps[] = {POP_LOAD, POP_SAVE, POP_DELETE, POP_DUPLICATE, POP_CLEAR_FP,   // PROFILE_ROUTE_*
                                   POP_DEFAULTS, POP_REPLACE};
    int i = op == PROFILE_ROUTE_DEFAULTS ? -1 : indexParam(req);   // Defaults loads no profile
    if (op >= sizeof(kOps) || (i < 0 && op != PROFILE_ROUTE_DEFAULTS)) { sendJson(req, 404, F("{\"error\":\"unknown\"}")); return; }
    if (op == PROFILE_ROUTE_REPLACE) {   // optional new name and region (the menu's same-signal "Replace it")
        String n, r;
        ProfileOpArgs a;
        ProfileArgError e = profileReplaceArgs(argOrNull(req, "name", n), argOrNull(req, "region", r), &a);
        if (e != PARG_OK) { argError(req, e); return; }
        logPrintf(LC_PROFILE, LL_INFO, PSTR("web: replace p%02d%s%s region %s"), i, a.name[0] ? " as " : "", a.name,
                  a.name[0] ? profileRegionName(a.region) : "kept");
        queued(req, profileQueueOp(POP_REPLACE, (int8_t)i, a.name[0] ? a.name : nullptr, a.region));
        return;
    }
    queued(req, profileQueueOp(kOps[op], (int8_t)i, nullptr));
}

void profileSaveAsPost(AsyncWebServerRequest* req, uint8_t) {
    String n, r;
    ProfileOpArgs a;
    ProfileArgError e = profileSaveAsArgs(argOrNull(req, "name", n), argOrNull(req, "region", r), &a);
    if (e != PARG_OK) { argError(req, e); return; }
    logPrintf(LC_PROFILE, LL_INFO, PSTR("web: save as \"%s\" region %s"), a.name, profileRegionName(a.region));
    queued(req, profileQueueOp(POP_SAVEAS, -1, a.name, a.region));
}

void profileRenamePost(AsyncWebServerRequest* req, uint8_t) {
    int i = indexParam(req);
    if (i < 0) { sendJson(req, 404, F("{\"error\":\"unknown\"}")); return; }
    char name[PROFILE_NAME_BUF];
    if (!nameParam(req, name)) { sendJson(req, 400, F("{\"ok\":0,\"errors\":{\"name\":\"invalid\"}}")); return; }
    queued(req, profileQueueOp(POP_RENAME, (int8_t)i, name));
}

void profileExportGet(AsyncWebServerRequest* req, uint8_t) {
    int i = indexParam(req);
    if (i < 0) { sendJson(req, 404, F("{\"error\":\"unknown\"}")); return; }
    char path[24]; profilePath((uint8_t)i, path);
    char file[64]; profileExportFileName(profileList().at((uint8_t)i).name, file, sizeof(file));
    AsyncWebServerResponse* resp = req->beginResponse(LittleFS, path, "text/plain");
    char cd[96]; snprintf_P(cd, sizeof(cd), PSTR("attachment; filename=\"%s\""), file);
    resp->addHeader("Content-Disposition", cd);
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

void profileImportPost(AsyncWebServerRequest* req, uint8_t) {
    if (gImport.rejected) { sendJson(req, 403, F("{\"error\":\"forbidden\"}")); return; }
    if (gImport.tooBig) { sendJson(req, 413, F("{\"error\":\"too_big\"}")); return; }
    queued(req, profileQueueOp(POP_IMPORT, -1, nullptr));
}

void profileImportUpload(AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        gImport.rejected = !apiGuardRequest(req);   // upload runs before the middleware
        gImport.tooBig = false;
        gImport.size = 0;
        if (gImport.rejected) return;
        gImport.f = LittleFS.open(PROFILE_IMPORT_TMP, "w");   // temp upload file (as restore)
    }
    if (gImport.rejected || gImport.tooBig || !gImport.f) return;
    if (gImport.size + len > PROFILE_MAX_FILE_BYTES || gImport.f.write(data, len) != len) {
        gImport.tooBig = true;
        gImport.f.close();
        LittleFS.remove(PROFILE_IMPORT_TMP);
        return;
    }
    gImport.size += len;
    if (final) gImport.f.close();
}

// From a copy taken now (~1.6 KB while the answer is sent): a list change between chunks cut or spliced the JSON
void profilesGet(AsyncWebServerRequest* req, uint8_t) {
    std::shared_ptr<ProfilesSnapshot> snap = std::make_shared<ProfilesSnapshot>();
    profilesSnapshotTake(*snap, profileList(), profileActiveIndex(), profileModified(), uopt->profilesAutoload,
                         profileMatchBits());
    sendJsonReplay(req, profilesJson, snap);
}
