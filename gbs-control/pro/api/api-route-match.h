// API route table lookup (pure, host-tested). The router (api-router.cpp) keeps its rows in PROGMEM, built from
// the X-macro tables below; each row starts with an ApiRouteKey.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum : uint8_t { ROUTE_GET = 1, ROUTE_POST = 2 };   // own bits; the router maps the request method
enum : uint8_t { ROUTE_EXACT = 0, ROUTE_PREFIX = 1 };
enum : uint8_t { ADV_ROUTE_PRESET = 0, ADV_ROUTE_MACRO = 1 };   // the route arg of /api/adv/* (= AdvKind)

static const uint8_t ROUTE_PATH_MAX = 28;   // "/api/actions/system.restart" + NUL; a longer path fails to compile
struct ApiRouteKey {
    char path[ROUTE_PATH_MAX];   // in the row (a pointer to a literal would put the string in RAM)
    uint8_t methods;
    uint8_t match;
};

// First row, in table order, whose method bit and path match; -1 when none. rows: PROGMEM on the device,
// stride: the size of one row (a row may carry more than its key)
int apiRouteFind(const void* rows, size_t stride, uint8_t n, uint8_t method, const char* path);

// R(methods, match, path, onRequest, onUpload, arg). Groups: page, info, settings, actions, system, wifi, profiles.
// URLs are the public API (web app, tools): keep them stable.
#define API_ROUTES_NORMAL(R) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/",                      webPageGet,          nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/info",              apiInfoGet,          nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/log",               logGet,              nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/events",            eventsGet,           nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/live",              liveGet,             nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/settings",          settingsGet,         nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/settings",          settingsPost,        nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/schema",            schemaGet,           nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/actions",           actionsListGet,      nullptr,             0) \
    R(ROUTE_POST, ROUTE_PREFIX, "/api/actions/",          actionsPost,         nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/dev/snapshot",      devSnapshotGet,      nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/dev/regs",          devRegsGet,          nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/adv/read",          advReadGet,          nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/regmap",            regmapGet,           nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/adv/presets",       advListGet,          nullptr,             ADV_ROUTE_PRESET) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/adv/macros",        advListGet,          nullptr,             ADV_ROUTE_MACRO) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/adv/preset",        advTextGet,          nullptr,             ADV_ROUTE_PRESET) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/adv/macro",         advTextGet,          nullptr,             ADV_ROUTE_MACRO) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/adv/preset",        advSavePost,         advPresetUpload,     ADV_ROUTE_PRESET) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/adv/macro",         advSavePost,         advMacroUpload,      ADV_ROUTE_MACRO) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/adv/preset/delete", advDeletePost,       nullptr,             ADV_ROUTE_PRESET) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/adv/macro/delete",  advDeletePost,       nullptr,             ADV_ROUTE_MACRO) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/system/backup",     backupGet,           nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/system/restore",    restorePost,         restoreUpload,       0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/system/update",     updatePost,          updateUpload,        0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/wifi",              wifiGet,             nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/wifi/scan",         wifiScanGet,         nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/wifi/connect",      wifiConnectPost,     nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/wifi/reset",        wifiResetPost,       nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/profiles",          profilesGet,         nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/load",     profileOpPost,       nullptr,             PROFILE_ROUTE_LOAD) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/save",     profileOpPost,       nullptr,             PROFILE_ROUTE_SAVE) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/delete",   profileOpPost,       nullptr,             PROFILE_ROUTE_DELETE) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/duplicate",profileOpPost,       nullptr,             PROFILE_ROUTE_DUPLICATE) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/clear_fp", profileOpPost,       nullptr,             PROFILE_ROUTE_CLEAR_FP) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/defaults", profileOpPost,       nullptr,             PROFILE_ROUTE_DEFAULTS) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/replace",  profileOpPost,       nullptr,             PROFILE_ROUTE_REPLACE) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/saveas",   profileSaveAsPost,   nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/rename",   profileRenamePost,   nullptr,             0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/profiles/export",   profileExportGet,    nullptr,             0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/profiles/import",   profileImportPost,   profileImportUpload, 0)

#define API_ROUTES_SAFE(R) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/",                           safePageGet,     nullptr,      0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/info",                   apiInfoGet,      nullptr,      0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/log",                    logGet,          nullptr,      0) \
    R(ROUTE_GET,  ROUTE_EXACT,  "/api/system/backup",          backupGet,       nullptr,      0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/system/update",          updatePost,      updateUpload, 0) \
    R(ROUTE_POST, ROUTE_EXACT,  "/api/actions/system.restart", safeRestartPost, nullptr,      0)

// Profile op argument of the shared profileOpPost row (mapped to POP_* by profile-web.cpp)
enum : uint8_t { PROFILE_ROUTE_LOAD, PROFILE_ROUTE_SAVE, PROFILE_ROUTE_DELETE, PROFILE_ROUTE_DUPLICATE, PROFILE_ROUTE_CLEAR_FP,
                 PROFILE_ROUTE_DEFAULTS, PROFILE_ROUTE_REPLACE };
