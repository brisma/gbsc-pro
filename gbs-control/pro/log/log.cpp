#include "log.h"
#include <stdarg.h>
#include "../../options.h"
#include "../input/input-types.h"
#include <ESP8266WiFi.h>
#include "../net/channel-guard-device.h"

extern userOptions* uopt;

static LogRing gRing;
static LogLineSplitter gSplit;
static LogMarks gMarks;
static bool gWs = false, gUsb = true;
static volatile bool gWifiDrop = false;
static volatile uint8_t gWifiReason = 0;

bool logDebugOn() { return uopt && uopt->devMode; }
const LogRing& logRing() { return gRing; }
void logSinkEnable(bool ws, bool usb) { gWs = ws; gUsb = usb; }
void logWifiDropped(uint8_t reason) { gWifiReason = reason; gWifiDrop = true; }

static void writeLine(const LogTag& tag, const char* text);

static void flushMarks() {
    if (!gMarks.pending()) return;
    char t[LOG_TEXT_MAX + 1];
    gMarks.take(t, sizeof(t));
    LogTag tag = {LC_SYNC, LL_DEBUG, true};
    writeLine(tag, t);
}

static void writeLine(const LogTag& tag, const char* text) {
    if (tag.level == LL_DEBUG && tag.ring && !logDebugOn()) return;   // the printInfo stream (ring false) is never gated
    if (tag.cat != LC_SYNC || tag.level != LL_DEBUG) flushMarks();     // marks keep their place before later lines
    LogRecord r;
    uint32_t now = millis();
    r.ms = now;
    r.cat = tag.cat;
    r.level = tag.level;
    size_t len = strlen(text);
    if (len > LOG_TEXT_MAX) len = LOG_TEXT_MAX;
    memcpy(r.text, text, len);
    r.text[len] = '\0';
    r.len = (uint8_t)len;
    r.seq = tag.ring ? gRing.append(now, tag.cat, tag.level, r.text) : 0;   // first: the echo below may yield
    char line[LOG_TEXT_MAX + 40];
    size_t n = logFormatLine(r, line, sizeof(line));
    if (n && (gWs || gUsb)) logSinkLine(line, n, gWs, gUsb);
}

void logText(uint8_t cat, uint8_t level, const char* text) {
    LogTag tag = {cat, level, true};
    writeLine(tag, text);
}

void logPrintf(uint8_t cat, uint8_t level, PGM_P fmt, ...) {
    if (level == LL_DEBUG && !logDebugOn()) return;   // before formatting
    char t[LOG_TEXT_MAX + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf_P(t, sizeof(t), fmt, ap);
    va_end(ap);
    logText(cat, level, t);
}

static void splitOut(const LogTag& tag, const char* text, void*) { writeLine(tag, text); }

void logUpstreamWrite(const char* data, size_t n) { gSplit.feed(data, n, millis(), splitOut, nullptr); }
void logUpstreamFlush() { gSplit.flush(splitOut, nullptr); }

void logTagNext(uint8_t cat, uint8_t level, bool ring) {
    LogTag t = {cat, level, ring};
    gSplit.tagNext(t, splitOut, nullptr);
}

void logMark(uint8_t kind) { gMarks.mark(kind, millis()); }

void logBegin() { gRing.setBoot((uint16_t)(ESP.random() & 0xFFFF)); }   // hardware RNG

// Wi-Fi state changes, polled from the loop (the SDK callback only stores the disconnect reason)
static void wifiWatch(uint32_t now) {
    static uint32_t at = 0;
    static uint8_t last = 0xFF;   // 0 off, 1 connected, 2 not connected, 3 AP
    if (now - at < 500) return;
    at = now;
    uint8_t st = WiFi.getMode() == WIFI_AP ? 3 : WiFi.status() == WL_CONNECTED ? 1 : WiFi.getMode() == WIFI_OFF ? 0 : 2;
    if (gWifiDrop) {
        gWifiDrop = false;
        logPrintf(LC_WIFI, LL_INFO, PSTR("disconnected (reason %u)"), (unsigned)gWifiReason);
    }
    if (st == last) return;
    if (st == 1)
        logPrintf(LC_WIFI, LL_INFO, PSTR("connected to %s (%s, %d dBm, channel %d)"), WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(), (int)channelGuardChannel());
    else if (st == 3) logPrintf(LC_WIFI, LL_INFO, PSTR("AP mode (%s)"), WiFi.softAPSSID().c_str());
    else if (last == 1) logPrintf(LC_WIFI, LL_INFO, PSTR("not connected"));
    last = st;
}

void logLoop() {
    uint32_t now = millis();
    gSplit.tick(now, splitOut, nullptr);
    if (gMarks.due(now)) flushMarks();
    wifiWatch(now);
}

void logPresetContext() {
    logPrintf(LC_PRESET, LL_INFO, PSTR("on %s, profile %s"), inputLongName(uopt->activeInputType),
              uopt->profileActive[0] ? uopt->profileActive : "none");
}
