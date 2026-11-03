#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <lwip/dns.h>
#include <lwip/ip4.h>
#include "../log/log.h"
#include "../gbs-control-pro.h"
#include "../util/pgm.h"
#include "ota-plan.h"
#include "ota-client.h"

namespace {

// Every callback compares its captured generation with gGen: a late event of an older fetch changes nothing.
// lwIP's DNS callback is our own static function (never an AsyncClient waiting on DNS, which could be deleted
// while lwIP still holds it), and the client is created only once the address is known
volatile uint32_t gGen = 0;
volatile bool gResolved = false, gDnsFailed = false, gConnected = false, gGotData = false, gClosed = false;
volatile int gParser = HTTP_MORE;
ip_addr_t gIp;
bool gBusy = false;
int gResult = HTTP_DONE;
const char* gReason = "";
AsyncClient* gClient = nullptr;   // the fetch's client, or a zombie still waiting for its connect or error
OtaClientLife gLife;
HttpResponse gResp;
OtaPull gPull;
HttpSink gSink = nullptr;
void* gCtx = nullptr;
uint16_t gPort = 80;
char gHost[64];
char gRequest[256];

void dnsFound(const char*, const ip_addr_t* ip, void* arg) {
    if ((uint32_t)(uintptr_t)arg != gGen || !gBusy) return;
    if (ip) { gIp = *ip; gResolved = true; }
    else gDnsFailed = true;
}

const char* reasonOf(int code) {
    switch (code) {
        case HTTP_E_STATUS: case HTTP_E_HEADER: return "http";
        case HTTP_E_LENGTH: case HTTP_E_TOO_BIG: case HTTP_E_EXTRA: return "size";
        case HTTP_E_MEMORY: return "memory";
        case HTTP_E_SINK: return "write";
        case OTA_E_DNS: return "dns";
        case OTA_E_CONNECT: return "connect";
        case OTA_E_TIMEOUT: return "timeout";
        case OTA_E_SHORT: return "short";
        case OTA_E_URL: return "http";
        default: return "error";
    }
}

void dropClient() {
    gClient->close(true);
    delete gClient;                                   // only in the loop, never inside a callback
    gClient = nullptr;
    gLife.deleted();
}

void finish(int result) {
    gGen++;                                           // later callbacks of this fetch are ignored
    // A client still connecting is kept (ESPAsyncTCP has not got its pcb yet, lwIP would call a freed object):
    // it is deleted once its late connect (closed in the callback) or error arrives
    if (gClient && gLife.finish()) dropClient();
    gResp.end();
    gResult = result;
    gReason = result == HTTP_DONE ? "" : reasonOf(result);
    gBusy = false;
}

bool connectNow() {
    gClient = new (std::nothrow) AsyncClient();
    if (!gClient) return false;
    gLife.created();
    uint32_t g = gGen;
    gClient->onConnect([g](void*, AsyncClient* c) {
        gLife.connected();
        if (g != gGen) { c->close(true); return; }   // a zombie's late connect: close it, the loop deletes it
        gConnected = true;
        c->write(gRequest, strlen(gRequest));
    });
    gClient->onData([g](void*, AsyncClient*, void* data, size_t len) {
        if (g != gGen) return;
        gGotData = true;
        if (gParser == HTTP_MORE) gParser = gResp.feed((const uint8_t*)data, len, gSink, gCtx);
    });
    gClient->onDisconnect([g](void*, AsyncClient*) { gLife.ended(); if (g == gGen) gClosed = true; });
    gClient->onError([g](void*, AsyncClient*, err_t) { gLife.ended(); if (g == gGen) gClosed = true; });
    gClient->onTimeout([g](void*, AsyncClient*, uint32_t) { gLife.ended(); if (g == gGen) gClosed = true; });
    return gClient->connect(IPAddress(gIp), gPort);
}

} // namespace

bool otaFetchStart(const char* url, uint32_t maxBody, HttpSink sink, void* ctx) {
    if (gBusy || !url || !sink) return false;
    if (gClient) { gResult = OTA_E_CONNECT; gReason = "busy"; return false; }   // a zombie still connecting
    UrlParts u;
    if (!urlSplit(url, u) || u.tls) { gResult = OTA_E_URL; gReason = "http"; return false; }
    char ua[32];
    snprintf_P(ua, sizeof(ua), PSTR("GBSC-Pro/%s"), GBS_FW_FULL_VERSION);
    if (!httpRequest(u, ua, gRequest, sizeof(gRequest))) { gResult = OTA_E_URL; gReason = "http"; return false; }
    if (!gResp.begin(maxBody)) { gResult = HTTP_E_MEMORY; gReason = "memory"; return false; }
    strncpy(gHost, u.host, sizeof(gHost) - 1);
    gHost[sizeof(gHost) - 1] = '\0';
    gPort = u.port;
    gSink = sink;
    gCtx = ctx;
    gGen++;
    gResolved = gDnsFailed = gConnected = gGotData = gClosed = false;
    gParser = HTTP_MORE;
    gBusy = true;
    gPull.start(millis());
    ip_addr_t addr;
    err_t e = dns_gethostbyname(gHost, &addr, dnsFound, (void*)(uintptr_t)gGen);
    if (e == ERR_OK) { gIp = addr; gResolved = true; }
    else if (e != ERR_INPROGRESS) gDnsFailed = true;
    return true;
}

void otaFetchLoop() {
    if (!gBusy && gClient && gLife.deletable()) dropClient();   // a zombie whose connect or error has arrived
    if (!gBusy) return;
    if (gDnsFailed) { finish(OTA_E_DNS); return; }
    if (gResolved && !gClient) {
        // AsyncClient::connect() would lose its pcb on a tcp_connect() failure: no connect without a route or memory
        uint32_t block = ESP.getMaxFreeBlockSize();
        const char* blocked = otaConnectBlocked(ip4_route(ip_2_ip4(&gIp)) != nullptr, block);
        if (blocked) {
            logPrintf(LC_UPDATE, LL_INFO, PSTR("fetch: not connecting to %s (%s, largest free block %u)"), gHost,
                      blocked, (unsigned)block);
            finish(strcmp(blocked, "memory") ? OTA_E_CONNECT : HTTP_E_MEMORY);
            return;
        }
        if (!connectNow()) { gLife.ended(); finish(OTA_E_CONNECT); return; }
    }
    bool data = gGotData;
    gGotData = false;
    int parser = gParser;
    OtaStep s = gPull.step(millis(), gResolved, gConnected, data, gClosed, parser);
    if (s == OS_DONE) finish(HTTP_DONE);
    else if (s == OS_FAILED) {
        const char* r = gPull.reason();
        finish(parser < 0 ? parser : !strcmp(r, "short") ? OTA_E_SHORT : !strcmp(r, "connect") ? OTA_E_CONNECT
                                                                                                  : OTA_E_TIMEOUT);
    }
}

bool otaFetchBusy() { return gBusy; }
int otaFetchResult() { return gResult; }
const char* otaFetchReason() { return gReason; }
