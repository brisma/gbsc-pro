#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "update-rtc.h"
#include "update-device.h"
#include "update-verify.h"
#include "safe-mode.h"
#include "../api/api-device.h"
#include "../api/net-stats.h"
#include "../api/api-router.h"
#include "../log/log.h"
#include "../net/channel-guard-device.h"

extern AsyncWebServer server;   // gbs-control.ino
extern const char* ap_ssid;     // gbs-control.ino AP credentials
extern const char* ap_password;

static RtcRecord gRtc;

RtcRecord& rtcRecord() { return gRtc; }
void rtcLoad() { ESP.rtcUserMemoryRead(RTC_RECORD_BLOCK, (uint32_t*)&gRtc, sizeof(gRtc)); }
void rtcStore() { ESP.rtcUserMemoryWrite(RTC_RECORD_BLOCK, (uint32_t*)&gRtc, sizeof(gRtc)); }
static void rtcRead() { rtcLoad(); }
static void rtcWrite() { rtcStore(); }

// Postmortem hook (core_esp8266_postmortem): runs for exceptions, WDT, abort() and panic() before the restart
extern "C" void custom_crash_callback(struct rst_info* rst, uint32_t stack, uint32_t stack_end) {
    uint32_t mark = RTC_CRASH_MAGIC;
    ESP.rtcUserMemoryWrite(RTC_CRASH_MARK_BLOCK, &mark, sizeof(mark));
    CrashInfo c = {};
    c.magic = RTC_CRASH_MAGIC;
    c.exccause = rst->exccause; c.epc1 = rst->epc1; c.epc2 = rst->epc2; c.epc3 = rst->epc3;
    c.excvaddr = rst->excvaddr; c.depc = rst->depc;
    crashCollectPcs((const uint32_t*)stack, (const uint32_t*)stack_end, c.pcs, CRASH_PCS);
    ESP.rtcUserMemoryWrite(RTC_CRASH_INFO_BLOCK, (uint32_t*)&c, sizeof(c));
}

int crashInfoJson(char* out, size_t n) {
    CrashInfo c;
    ESP.rtcUserMemoryRead(RTC_CRASH_INFO_BLOCK, (uint32_t*)&c, sizeof(c));
    if (c.magic != RTC_CRASH_MAGIC) return snprintf_P(out, n, PSTR("null"));
    int w = snprintf_P(out, n, PSTR("{\"cause\":%u,\"epc1\":\"%08lX\",\"epc2\":\"%08lX\",\"epc3\":\"%08lX\","
                                    "\"excvaddr\":\"%08lX\",\"depc\":\"%08lX\",\"pcs\":\""),
                       (unsigned)c.exccause, (unsigned long)c.epc1, (unsigned long)c.epc2, (unsigned long)c.epc3,
                       (unsigned long)c.excvaddr, (unsigned long)c.depc);
    for (uint8_t i = 0; i < CRASH_PCS && c.pcs[i] && w > 0 && (size_t)w < n; i++)
        w += snprintf_P(out + w, n - w, PSTR("%s%08lX"), i ? " " : "", (unsigned long)c.pcs[i]);
    if (w > 0 && (size_t)w < n) w += snprintf_P(out + w, n - w, PSTR("\"}"));
    return w;
}

// Console log at boot: the crash recorded in RTC memory; info when this boot follows it (exception or watchdog
// reset), debug otherwise (the record stays until the next crash)
void crashLogBoot() {
    CrashInfo c;
    ESP.rtcUserMemoryRead(RTC_CRASH_INFO_BLOCK, (uint32_t*)&c, sizeof(c));
    if (c.magic != RTC_CRASH_MAGIC) return;
    uint32_t why = ESP.getResetInfoPtr()->reason;
    bool now = why == REASON_EXCEPTION_RST || why == REASON_SOFT_WDT_RST || why == REASON_WDT_RST;
    char pcs[48];
    size_t k = 0;
    pcs[0] = '\0';
    for (uint8_t i = 0; i < CRASH_PCS && i < 4 && c.pcs[i]; i++)
        k += (size_t)snprintf_P(pcs + k, sizeof(pcs) - k, PSTR(" %08lX"), (unsigned long)c.pcs[i]);
    logPrintf(LC_SYS, now ? LL_INFO : LL_DEBUG, PSTR("%s crash: exception %u at %08lX (addr %08lX), stack%s"),
              now ? "this boot follows a" : "last recorded", (unsigned)c.exccause, (unsigned long)c.epc1,
              (unsigned long)c.excvaddr, pcs);
}

void restartNoteSet(uint8_t note) {
    rtcRead();
    rtcSetNote(gRtc, note);
    rtcWrite();
}

void restartNoteLogBoot() {
    rtcRead();
    uint8_t n = rtcTakeNote(gRtc);
    rtcWrite();
    if (n == RN_RESTORE_OK) logPrintf(LC_SYS, LL_INFO, PSTR("restore: the backup was applied before this restart"));
    else if (n == RN_RESTORE_WRITE)
        logPrintf(LC_SYS, LL_INFO, PSTR("restore: writing the backup failed before this restart, some settings may "
                                        "be missing"));
}

bool safeModeCheckOnBoot() {
    uint32_t mark = 0, clear = 0;
    ESP.rtcUserMemoryRead(RTC_CRASH_MARK_BLOCK, &mark, sizeof(mark));
    ESP.rtcUserMemoryWrite(RTC_CRASH_MARK_BLOCK, &clear, sizeof(clear));
    rtcRead();
    bool enter = safeModeOnBoot(gRtc, resetKindFromReason(ESP.getResetInfoPtr()->reason, mark == RTC_CRASH_MAGIC));
    rtcWrite();
    return enter;
}

void safeModeHealthyTick() {
    static bool done = false;
    if (done || millis() < 30000) return;
    done = true;
    rtcRead();
    safeModeMarkHealthy(gRtc);
    rtcWrite();
}

static const char kSafePage[] PROGMEM =
    "<!doctype html><meta name=viewport content='width=device-width'><title>GBSC safe mode</title>"
    "<h3>GBSC safe mode</h3><p>The firmware crashed 3 times in a row. Upload a firmware image "
    "(.bin.gz) or download a settings backup.</p>"
    "<input type=file id=f accept='.gz,.bin'> <button onclick=up()>Upload</button> "
    "<a href='/api/system/backup'>Backup</a> <button onclick=rs()>Restart</button><pre id=o></pre><script>"
    "function up(){var d=new FormData();d.append('file',f.files[0]);o.textContent='Uploading...';"
    "fetch('/api/system/update',{method:'POST',body:d,headers:{'X-GBSC':'1'}}).then(r=>{o.textContent=r.ok?"
    "'Verifying, the device restarts when done':'Error '+r.status})}"
    "function rs(){fetch('/api/actions/system.restart',{method:'POST',headers:{'X-GBSC':'1'}})}</script>";

void safePageGet(AsyncWebServerRequest* req, uint8_t) { req->send_P(200, "text/html", kSafePage); }

static bool gSafeModeActive = false;

bool safeModeActive() { return gSafeModeActive; }

void safeModeRun() {
    gSafeModeActive = true;
    Serial.begin(115200);
    Serial.println(F("SAFE MODE"));
    logSinkEnable(false, false);                    // ring only: GPIO1 may be the ADV Controller link wire (no probes yet)
    logPrintf(LC_SYS, LL_INFO, PSTR("safe mode (3 crash resets)"));
    LittleFS.begin();
    apiSetReady();                                  // mounted before the server starts
    WiFi.persistent(false);
    channelGuardBegin();
    WiFi.mode(WIFI_STA);
    WiFi.begin();                                   // stored network
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(20); channelGuardLoop(); }
    if (WiFi.status() != WL_CONNECTED) { WiFi.mode(WIFI_AP); WiFi.softAP(ap_ssid, ap_password); }
    apiWebRegisterSafe(server);                     // admission, guard middleware, safe routes (and "/")
    server.begin();
    netGuardHttpAccept();
    for (;;) {
        channelGuardLoop();
        updateLoop();
        apiSafeLoop();                              // restart request
        netStatsSample();                           // /api/info "net" marks
        delay(10);
    }
}
