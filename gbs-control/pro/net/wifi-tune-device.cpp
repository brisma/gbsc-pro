#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip_addr.h"
}
#include "../../options.h"
#include "../../tv5725.h"
#include "../profiles/profile-autoload.h"
#include "../update/update-device.h"
#include "../update/ota-update.h"
#include "../log/log.h"
#include "wifi-tune.h"
#include "link-watch.h"
#include "wifi-tune-device.h"
#include "channel-guard-device.h"
#include "../input/input-auto-device.h"

typedef TV5725<GBS_ADDR> GBS;
extern userOptions* uopt;
extern runTimeOptions* rto;
void ResetSDRAM();   // gbs-control.ino
static const uint8_t kPresetBypassRgbhv = 0x22;   // gbs-control.ino PresetBypassRGBHV: sets PLL_MS 2 itself

static WifiTune gTune;
static LinkWatch gWatch;
static uint8_t gPresetClock = 2;           // PLL_MS of the last preset/profile load (setResetParameters writes 2)
static uint32_t gStableSince = 0;
static bool gStableRun = false;

// The action being carried out: settle after a clock write, then the burst
enum : uint8_t { ST_IDLE, ST_SETTLE, ST_BURST, ST_WATCH };   // ST_WATCH: the link watch's few echoes
static uint8_t gState = ST_IDLE;
static uint8_t gClock = 0;                 // the clock of the running burst
static uint32_t gUntil = 0, gLastLoop = 0;

// ICMP burst (raw lwIP pcb, created once)
static raw_pcb* gPcb = nullptr;
static ip_addr_t gGw;
static uint16_t gEchoId = 0x5A00;
static uint32_t gSentAt[kWifiBurstCount], gRttUs[kWifiBurstCount];
static uint8_t gSent = 0;
static uint32_t gNextAt = 0;

// Log state (one line per change)
static uint8_t gLogCh = 0, gRoundBursts = 0;
static bool gEverMeasured = false, gDecisionLogged = true, gLoggedAp = false, gWasEnabled = false;

static const char* clockName(uint8_t c, char* buf, size_t n) {
    switch (c) {
        case 2: return "FB";
        case 3: return "162 MHz";
        case 4: return "144 MHz";
        case 7: return "129.6 MHz";
        default: snprintf_P(buf, n, PSTR("PLL_MS %u"), c); return buf;
    }
}

const char* wifiTuneClockName(uint8_t clock, char* buf, unsigned n) { return clockName(clock, buf, n); }

static u8_t onEcho(void*, raw_pcb*, pbuf* p, const ip_addr_t*) {
    uint8_t b[28];
    if ((gState != ST_BURST && gState != ST_WATCH) || p->tot_len < sizeof(b) || pbuf_copy_partial(p, b, sizeof(b), 0) != sizeof(b)) return 0;
    if ((b[0] & 0x0F) * 4 != 20 || b[20] != ICMP_ER) return 0;
    uint16_t id = (uint16_t)(b[24] << 8 | b[25]), seq = (uint16_t)(b[26] << 8 | b[27]);
    if (id != gEchoId || seq >= gSent || gRttUs[seq]) return 0;   // not ours: lwIP goes on with it
    uint32_t us = micros() - gSentAt[seq];
    gRttUs[seq] = us ? us : 1;
    pbuf_free(p);
    return 1;
}

static bool pcbReady() {
    if (gPcb) return true;
    gPcb = raw_new(IP_PROTO_ICMP);
    if (!gPcb) return false;
    raw_recv(gPcb, onEcho, nullptr);
    raw_bind(gPcb, IP_ADDR_ANY);
    return true;
}

static void sendEcho(uint8_t seq) {
    pbuf* p = pbuf_alloc(PBUF_IP, sizeof(icmp_echo_hdr) + 32, PBUF_RAM);
    if (!p) return;   // counts as lost
    icmp_echo_hdr* h = (icmp_echo_hdr*)p->payload;
    ICMPH_TYPE_SET(h, ICMP_ECHO);
    ICMPH_CODE_SET(h, 0);
    h->id = lwip_htons(gEchoId);
    h->seqno = lwip_htons(seq);
    memset((uint8_t*)p->payload + sizeof(icmp_echo_hdr), 0xA5, 32);
    h->chksum = 0;
    h->chksum = inet_chksum(h, p->len);
    gSentAt[seq] = micros();
    raw_sendto(gPcb, p, &gGw);
    pbuf_free(p);
}

static void writeClock(uint8_t c) {
    GBS::PLL_MS::write(c);
    ResetSDRAM();
}

static void dropBurst(const char* why) {
    if (gState == ST_IDLE || gState == ST_WATCH) return;
    gState = ST_IDLE;
    gTune.burstAborted();
    logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u burst dropped (%s)"), gTune.channel(), why);
}

static bool inBypass() { return rto->outModeHdBypass || rto->presetID == kPresetBypassRgbhv; }

static bool pictureStable(uint32_t now) {
    // the input search identifying a cable has a sync and no preset yet, and its sampling stalls the loop
    bool s = !rto->sourceDisconnected && !autoloadStageBusy(rto->applyPresetDoneStage) && !inBypass() &&
             !inputAutoHoldsSyncWatcher();
    if (!s) { gStableRun = false; return false; }
    if (!gStableRun) { gStableRun = true; gStableSince = now; }
    return (int32_t)(now - gStableSince) >= 3000;   // clamp and autogain settle in the first seconds
}

static void logDecision() {
    char a[12], b[12];
    uint8_t ch = gTune.channel();
    if (gTune.gaveUp()) {
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u the router does not answer ping, skipped"), ch);
        return;
    }
    if (gTune.stalled()) {
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u bursts kept being dropped, round ended on the preset clock"), ch);
        return;
    }
    uint8_t c = gTune.clockFor(gPresetClock);
    uint16_t pc = gTune.p90x10(gPresetClock);
    if (c != gPresetClock)
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u keep %s (p90 %u vs %u ms%s)"), ch, clockName(c, a, sizeof(a)),
                  gTune.p90x10(c) / 10, pc / 10,
                  gTune.p90x10(kWifiClock162) == 0xFFFF && gPresetClock != kWifiClock162 ? ", 162 MHz not tried" : "");
    else
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u keep the preset clock (%s)"), ch,
                  clockName(gPresetClock, b, sizeof(b)));
    if (gTune.slow())
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u still slow (p90 %u ms): try another router channel"), ch,
                  gTune.p90x10(c) / 10);
}

static void startBurst(uint32_t now) {
    if (!pcbReady()) { gState = ST_IDLE; return; }
    IPAddress g = WiFi.gatewayIP();
    IP_ADDR4(&gGw, g[0], g[1], g[2], g[3]);
    memset(gRttUs, 0, sizeof(gRttUs));
    gEchoId++;
    gSent = 0;
    gNextAt = now;
    gState = ST_BURST;
}

static void logStart(uint8_t ch) {
    char a[12];
    const char* why = !gEverMeasured ? "boot" : ch != gLogCh ? "channel change" : "new round";
    logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: start (%s), channel %u, RSSI %d, preset clock %s"), why, ch,
              (int)WiFi.RSSI(), clockName(gPresetClock, a, sizeof(a)));
    gEverMeasured = true;
    gLogCh = ch;
}

static void runAction(const WifiTuneAction& a, uint32_t now) {
    char n[12];
    if (a.kind == WT_APPLY) {
        writeClock(a.clock);
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u memory clock %s"), gTune.channel(), clockName(a.clock, n, sizeof(n)));
        return;
    }
    if (gDecisionLogged || gTune.channel() != gLogCh) { logStart(gTune.channel()); gRoundBursts = 0; }
    gDecisionLogged = false;
    gClock = a.clock;
    if (a.kind == WT_SET_MEASURE) {
        writeClock(a.clock);
        gState = ST_SETTLE;
        gUntil = now + kWifiSettleMs;
    } else {
        startBurst(now);
    }
}

static void burstStep(uint32_t now) {
    if (gSent < kWifiBurstCount) {
        if ((int32_t)(now - gNextAt) >= 0) {
            sendEcho(gSent++);
            gNextAt += kWifiBurstGapMs;
            if (gSent == kWifiBurstCount) gUntil = now + kWifiLostMs;   // the last reply may still come
        }
        return;
    }
    if ((int32_t)(now - gUntil) < 0) return;
    gState = ST_IDLE;
    WifiBurst b = wifiBurstStats(gRttUs, kWifiBurstCount);
    char n[12];
    logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u %s p50 %u.%u p90 %u.%u max %u.%u lost %u/%u"), gTune.channel(),
              clockName(gClock, n, sizeof(n)), b.p50x10 / 10, b.p50x10 % 10, b.p90x10 / 10, b.p90x10 % 10, b.maxx10 / 10,
              b.maxx10 % 10, (unsigned)(b.sent - b.got), (unsigned)b.sent);
    gTune.burstDone(b);
    gRoundBursts++;
}

// Link watch: kEchoes echoes to the gateway, kGapMs apart, then kWaitMs for the last reply; a later one is lost
static void watchStep(uint32_t now, bool station) {
    if (!station) { gState = ST_IDLE; return; }          // dropped: the next check comes on time
    if (gSent < LinkWatch::kEchoes) {
        if ((int32_t)(now - gNextAt) >= 0) {
            sendEcho(gSent++);
            gNextAt += LinkWatch::kGapMs;
            if (gSent == LinkWatch::kEchoes) gUntil = now + LinkWatch::kWaitMs;
        }
        return;
    }
    if ((int32_t)(now - gUntil) < 0) return;
    gState = ST_IDLE;
    LinkCheck c = {LinkWatch::kEchoes, 0, 0};
    for (uint8_t i = 0; i < LinkWatch::kEchoes; i++) {
        if (!gRttUs[i]) continue;
        c.got++;
        uint32_t ms = gRttUs[i] / 1000;
        if (ms > c.worstMs) c.worstMs = (uint16_t)(ms > 0xFFFF ? 0xFFFF : ms);
    }
    char n[12];
    switch (gWatch.result(c, now)) {
        case LW_BAD:
            logPrintf(LC_WIFI, LL_INFO, PSTR("wifi link: bad, %u/%u echoes lost, worst %u ms (RSSI %d, ch %u, mem %s)"),
                      (unsigned)(c.sent - c.got), (unsigned)c.sent, (unsigned)c.worstMs, (int)WiFi.RSSI(),
                      (unsigned)channelGuardChannel(), clockName(gTune.appliedClock(), n, sizeof(n)));
            break;
        case LW_OK_AGAIN:
            logPrintf(LC_WIFI, LL_INFO, PSTR("wifi link: ok again after %lu s (RSSI %d)"),
                      (unsigned long)(gWatch.badForMs() / 1000), (int)WiFi.RSSI());
            break;
        default: {                                       // ok: one debug summary per 10 checks (the ring is small)
            static uint8_t okChecks = 0;
            static uint16_t okWorst = 0;
            if (c.worstMs > okWorst) okWorst = c.worstMs;
            if (++okChecks < 10) break;
            logPrintf(LC_WIFI, LL_DEBUG, PSTR("wifi link: ok, last %u checks, worst %u ms"), (unsigned)okChecks,
                      (unsigned)okWorst);
            okChecks = 0;
            okWorst = 0;
            break;
        }
    }
}

static void startWatch(uint32_t now) {
    if (!pcbReady()) return;
    IPAddress g = WiFi.gatewayIP();
    IP_ADDR4(&gGw, g[0], g[1], g[2], g[3]);
    memset(gRttUs, 0, sizeof(gRttUs));
    gEchoId++;
    gSent = 0;
    gNextAt = now;
    gState = ST_WATCH;
}

void wifiTuneLoop() {
    uint32_t now = millis();
    uint32_t gap = now - gLastLoop;
    gLastLoop = now;
    bool station = WiFi.getMode() != WIFI_AP && WiFi.status() == WL_CONNECTED;
    bool busy = updateInProgress() || otaBusy() || WiFi.scanComplete() == WIFI_SCAN_RUNNING;
    bool stable = pictureStable(now);
    bool enabled = uopt->wifiOptimize != 0;

    if (!enabled && gWasEnabled) dropBurst("turned off");
    if (enabled && WiFi.getMode() == WIFI_AP) {
        if (!gLoggedAp) logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: skipped: AP mode"));
        gLoggedAp = true;
    } else {
        gLoggedAp = false;
    }
    gWasEnabled = enabled;
    if (gState == ST_WATCH) { watchStep(now, station); return; }   // independent of the picture and the option
    if (gState != ST_IDLE) {
        if (!stable) dropBurst("picture changed");
        else if (busy) dropBurst("busy");
        else if (!station) dropBurst("WiFi lost");
        else if (gState == ST_BURST && gap > 100) dropBurst("loop stalled");
    }
    gTune.setEnabled(enabled);
    gTune.setLink(station, station ? channelGuardChannel() : 0);
    gTune.setPicture(stable, gPresetClock);
    gTune.setBusy(busy);
    gTune.setBypass(inBypass());

    if (gState == ST_SETTLE) {
        if ((int32_t)(now - gUntil) >= 0) startBurst(now);
        return;
    }
    if (gState == ST_BURST) { burstStep(now); return; }

    WifiTuneAction a = gTune.next();
    if (a.kind != WT_NONE) { runAction(a, now); return; }
    if (!gDecisionLogged && (gRoundBursts || gTune.stalled()) && enabled && gTune.roundComplete()) { logDecision(); gDecisionLogged = true; }
    bool watchOn = uopt->devMode && uopt->wifiWatch;   // Developer toggle (dev.wifi_watch)
    static bool watchWasOn = false;
    if (watchOn != watchWasOn) {
        watchWasOn = watchOn;
        logPrintf(LC_WIFI, LL_INFO, PSTR("wifi link watch %s"), watchOn ? "on: 3 pings to the router every minute" : "off");
    }
    if (gWatch.due(now, station && watchOn, busy)) startWatch(now);
}

void wifiTunePresetLoaded() {
    gPresetClock = GBS::PLL_MS::read();
    if (gState != ST_IDLE) dropBurst("preset loaded");
    gStableRun = false;
    gTune.presetLoaded(gPresetClock);
    if (!uopt->wifiOptimize) return;
    uint8_t c = gTune.clockFor(gPresetClock);
    if (c == gPresetClock) return;
    GBS::PLL_MS::write(c);   // the caller's ResetSDRAM() follows
    char a[12], b[12];
    logPrintf(LC_WIFI, LL_INFO, PSTR("wifi tune: ch %u preset load: memory clock %s (the session's choice, preset %s)"),
              gTune.channel(), clockName(c, a, sizeof(a)), clockName(gPresetClock, b, sizeof(b)));
}

uint8_t wifiTunePresetClock() { return gPresetClock; }
uint8_t wifiTuneAppliedClock() { return gTune.appliedClock(); }
bool wifiTuneSlowChannel() { return gTune.slowChannel(); }
uint8_t wifiTuneIdleClock() { return gTune.idleClock(uopt->wifiOptimize != 0); }
