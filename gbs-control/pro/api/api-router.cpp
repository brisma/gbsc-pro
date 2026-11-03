#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "api-router.h"
#include "api-watchdog.h"

static const uint32_t kUploadRxTimeoutS = 30;
static_assert(kUploadRxTimeoutS * 1000 >= UploadWatchdog::kTimeoutMs, "the upload watchdog must expire first");

namespace {

struct ApiRoute {
    ApiRouteKey key;   // first: apiRouteFind reads it at offset 0
    ApiRequestFn onRequest;
    ApiUploadFn onUpload;
    uint8_t arg;
};

#define ROW(methods, match, path, onRequest, onUpload, arg) {{path, methods, match}, onRequest, onUpload, arg},
const ApiRoute kNormal[] PROGMEM = {API_ROUTES_NORMAL(ROW)};
const ApiRoute kSafe[] PROGMEM = {API_ROUTES_SAFE(ROW)};
#undef ROW

uint8_t routeMethod(const AsyncWebServerRequest* req) {
    return req->method() == HTTP_GET ? ROUTE_GET : req->method() == HTTP_POST ? ROUTE_POST : 0;
}

class ApiRouter : public AsyncWebHandler {
public:
    ApiRouter(const ApiRoute* rows, uint8_t n) : _rows(rows), _n(n) {}

    bool canHandle(AsyncWebServerRequest* req) const override { return req->isHTTP() && find(req) >= 0; }

    // No state on the request: the row is looked up again (a table walk, cheaper than a per-request allocation)
    void handleRequest(AsyncWebServerRequest* req) override {
        ApiRoute r;
        if (!row(req, r)) { req->send(404); return; }
        r.onRequest(req, r.arg);
    }

    void handleUpload(AsyncWebServerRequest* req, const String&, size_t index, uint8_t* data, size_t len, bool final) override {
        ApiRoute r;
        if (!row(req, r) || !r.onUpload) return;
        // The library closes a connection after 3 s without received data (setRxTimeout(3) until the response):
        // on a disturbed Wi-Fi link an upload stalls that long and the close reset the transfer mid-way. Never 0:
        // a client that vanishes without FIN/RST would then hold its connection (and an admission slot) forever.
        if (index == 0) req->client()->setRxTimeout(kUploadRxTimeoutS);
        r.onUpload(req, index, data, len, final);
    }

    bool isRequestHandlerTrivial() const override { return false; }   // form bodies and uploads are parsed

private:
    int find(AsyncWebServerRequest* req) const {
        return apiRouteFind(_rows, sizeof(ApiRoute), _n, routeMethod(req), req->url().c_str());
    }
    bool row(AsyncWebServerRequest* req, ApiRoute& out) const {
        int i = find(req);
        if (i < 0) return false;
        memcpy_P(&out, &_rows[i], sizeof(out));
        return true;
    }
    const ApiRoute* _rows;
    uint8_t _n;
};

}  // namespace

void apiRouterAdd(AsyncWebServer& server, bool safeMode) {
    if (safeMode) server.addHandler(new ApiRouter(kSafe, sizeof(kSafe) / sizeof(kSafe[0])));
    else server.addHandler(new ApiRouter(kNormal, sizeof(kNormal) / sizeof(kNormal[0])));
}
