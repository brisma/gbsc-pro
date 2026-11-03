// GET /api/regmap: the register map of the Developer tools (tools/regmap.py), a gzipped JSON in flash served like
// the page (ETag = a hash of the map from tools/regmap.py, revalidated on every load); asked only when a register
// section opens
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../../regmap_gz.h"
#include "regmap-status.h"
#include "api-router.h"
#include "../log/log.h"

void regmapGet(AsyncWebServerRequest* req, uint8_t) {
    static const String etag = String("\"") + regmap_etag + "\"";
    bool match = req->hasHeader("If-None-Match") && req->header("If-None-Match") == etag;
    uint32_t heap = ESP.getFreeHeap();
    int status = regmapStatus(regmap_gz_len, heap, match);
    logPrintf(LC_API, LL_DEBUG, PSTR("regmap: %d (%u B, heap %u)"), status, regmap_gz_len, (unsigned)heap);
    if (status == 404) { req->send(404, F("application/json"), F("{\"error\":\"none\"}")); return; }
    if (status != 200) { req->send(status); return; }
    AsyncWebServerResponse* res = req->beginResponse(200, F("application/json"), regmap_gz, regmap_gz_len);
    res->addHeader(F("Content-Encoding"), F("gzip"));
    res->addHeader(F("Cache-Control"), F("no-cache"));
    res->addHeader(F("ETag"), etag);
    req->send(res);
}
