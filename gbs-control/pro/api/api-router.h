// One AsyncWebHandler for every API route: rows from API_ROUTES_NORMAL / API_ROUTES_SAFE (api-route-match.h)
// in PROGMEM, plain function handlers (a server.on() per route holds a handler object, a heap URI and std::functions)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "api-route-match.h"

class AsyncWebServer;
class AsyncWebServerRequest;

typedef void (*ApiRequestFn)(AsyncWebServerRequest* req, uint8_t arg);
// Uploads run before the server middleware: a handler checks apiGuardRequest() at index 0
typedef void (*ApiUploadFn)(AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final);

void apiRouterAdd(AsyncWebServer& server, bool safeMode);   // after the admission handler and the guard

// Handlers (bodies in their modules)
void webPageGet(AsyncWebServerRequest*, uint8_t);           // gbs-control.ino
void safePageGet(AsyncWebServerRequest*, uint8_t);          // safe-mode.cpp
void safeRestartPost(AsyncWebServerRequest*, uint8_t);      // api-web.cpp
void apiInfoGet(AsyncWebServerRequest*, uint8_t);
void logGet(AsyncWebServerRequest*, uint8_t);              // api-web.cpp: console log ring
void actionsListGet(AsyncWebServerRequest*, uint8_t);
void actionsPost(AsyncWebServerRequest*, uint8_t);
void devSnapshotGet(AsyncWebServerRequest*, uint8_t);
void backupGet(AsyncWebServerRequest*, uint8_t);
void restorePost(AsyncWebServerRequest*, uint8_t);
void restoreUpload(AsyncWebServerRequest*, size_t, uint8_t*, size_t, bool);
void updatePost(AsyncWebServerRequest*, uint8_t);
void updateUpload(AsyncWebServerRequest*, size_t, uint8_t*, size_t, bool);
void wifiGet(AsyncWebServerRequest*, uint8_t);
void devRegsGet(AsyncWebServerRequest*, uint8_t);
void advReadGet(AsyncWebServerRequest*, uint8_t);
void eventsGet(AsyncWebServerRequest*, uint8_t);
void liveGet(AsyncWebServerRequest*, uint8_t);
void regmapGet(AsyncWebServerRequest*, uint8_t);   // regmap-route.cpp
void advListGet(AsyncWebServerRequest*, uint8_t);  // adv-store-web.cpp (arg: ADV_ROUTE_PRESET / ADV_ROUTE_MACRO)
void advTextGet(AsyncWebServerRequest*, uint8_t);
void advSavePost(AsyncWebServerRequest*, uint8_t);
void advDeletePost(AsyncWebServerRequest*, uint8_t);
void advPresetUpload(AsyncWebServerRequest*, size_t, uint8_t*, size_t, bool);
void advMacroUpload(AsyncWebServerRequest*, size_t, uint8_t*, size_t, bool);
void wifiScanGet(AsyncWebServerRequest*, uint8_t);
void wifiConnectPost(AsyncWebServerRequest*, uint8_t);
void wifiResetPost(AsyncWebServerRequest*, uint8_t);
void settingsGet(AsyncWebServerRequest*, uint8_t);          // settings-web.cpp
void settingsPost(AsyncWebServerRequest*, uint8_t);
void schemaGet(AsyncWebServerRequest*, uint8_t);
void profilesGet(AsyncWebServerRequest*, uint8_t);          // profile-web.cpp
void profileOpPost(AsyncWebServerRequest*, uint8_t op);     // PROFILE_ROUTE_*
void profileSaveAsPost(AsyncWebServerRequest*, uint8_t);
void profileRenamePost(AsyncWebServerRequest*, uint8_t);
void profileExportGet(AsyncWebServerRequest*, uint8_t);
void profileImportPost(AsyncWebServerRequest*, uint8_t);
void profileImportUpload(AsyncWebServerRequest*, size_t, uint8_t*, size_t, bool);
