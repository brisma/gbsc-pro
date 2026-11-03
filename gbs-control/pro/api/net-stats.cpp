// Device only (lwIP lists): see net-stats.h
#ifdef ARDUINO
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "net-stats.h"
#include "net-slow.h"
#include "api-admission.h"
#include "../../options.h"
#include "../log/log.h"
#include "../input/input-types.h"
#include "../input/input-auto-device.h"
#include "../net/wifi-tune-device.h"
#include "../update/update-device.h"
#include "../update/ota-update.h"
extern "C" {
#include "lwip/priv/tcp_priv.h"
}

static NetStats gNet = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0};

static os_timer_t gStarveTimer;
static uint32_t gStarveLast;
static void starveTick(void*) {   // system context (not an ISR): may run flash code
    uint32_t now = millis();
    uint32_t gap = now - gStarveLast;
    gStarveLast = now;
    if (gap > gNet.starveMaxMs) { gNet.starveMaxMs = gap; gNet.starveAtS = now / 1000; }
}

void netStarveStart() {
    static bool started = false;
    if (started) return;
    started = true;
    gStarveLast = millis();
    os_timer_setfn(&gStarveTimer, starveTick, nullptr);
    os_timer_arm(&gStarveTimer, 10, true);
}

static uint8_t countPcbs(struct tcp_pcb* p) {
    uint8_t n = 0;
    for (; p && n < 255; p = p->next) n++;
    return n;
}

extern userOptions* uopt;

// A port-80 connection still sending after 5 s: retransmits > 0 = packets lost on the air (the radio), 0 with 1 B
// unacked = the client stopped reading (zero window); a stalled loop shows in starve_max_ms instead. One line per
// 10 s at most, none during an upload or an OTA download (long by design)
static NetSlowConns gSlow;
// a connection's identity: lwIP hands a freed pcb to the next connection at once, so the address alone made
// back-to-back requests look like one connection open for seconds (a false "slow web reply")
static uintptr_t connKey(const struct tcp_pcb* p) { return (uintptr_t)p ^ ((uintptr_t)p->remote_port << 1); }
static uint32_t gSlowAt = 0, gSlowLogAt = 0;
static void slowCheck() {
    uint32_t now = millis();
    if (now - gSlowAt < 250) return;
    gSlowAt = now;
    NetConn conns[8];
    uint8_t n = 0;
    for (struct tcp_pcb* p = tcp_active_pcbs; p && n < 8; p = p->next)
        if (p->local_port == 80) conns[n++] = {connKey(p), p->unacked != nullptr || p->unsent != nullptr};
    NetSlowHit h;
    if (!gSlow.update(conns, n, now, h) || updateUploadActive() || otaBusy()) return;
    if (gSlowLogAt && now - gSlowLogAt < 10000) return;
    for (struct tcp_pcb* p = tcp_active_pcbs; p; p = p->next) {
        if (p->local_port != 80 || connKey(p) != h.id) continue;
        gSlowLogAt = now;
        char clk[12];
        logPrintf(LC_API, LL_INFO, PSTR("slow web reply: %lu.%lu s, retransmits %u, unacked %lu B, queue %u, tcp %u "
                                        "(%s %s, mem %s, heap %u, RSSI %d)"),   // <= 128 characters
                  (unsigned long)(h.ageMs / 1000), (unsigned long)(h.ageMs % 1000 / 100), (unsigned)p->nrtx,
                  (unsigned long)(p->snd_nxt - p->lastack), (unsigned)p->snd_queuelen, (unsigned)p->state,
                  inputAutoSearching() ? "scan" : "on", inputLongName(uopt->activeInputType),
                  wifiTuneClockName(wifiTuneAppliedClock(), clk, sizeof(clk)), (unsigned)ESP.getFreeHeap(),
                  (int)WiFi.RSSI());
        return;
    }
}

void netStatsSample() {
    slowCheck();
    uint32_t h = ESP.getFreeHeap(), b = ESP.getMaxFreeBlockSize();
    if (h < gNet.heapMin) gNet.heapMin = h;
    if (b < gNet.blockMin) gNet.blockMin = b;
    gNet.tcpNow = countPcbs(tcp_active_pcbs);
    gNet.twNow = countPcbs(tcp_tw_pcbs);
    if (gNet.tcpNow > gNet.tcpMax) gNet.tcpMax = gNet.tcpNow;
    if (gNet.twNow > gNet.twMax) gNet.twMax = gNet.twNow;
}

const NetStats& netStats() { return gNet; }

static tcp_accept_fn gHttpAccept = nullptr;   // ESPAsyncTCP's accept, called for the connections we keep
static uint16_t gTcpRefused = 0;

static err_t guardedAccept(void* arg, struct tcp_pcb* newpcb, err_t err) {
    if (err == ERR_OK && newpcb && !admitTcp(ESP.getFreeHeap(), netHttpConnections())) {
        gTcpRefused++;
        tcp_abort(newpcb);                       // RST at once: nothing allocated for this connection
        return ERR_ABRT;
    }
    return gHttpAccept ? gHttpAccept(arg, newpcb, err) : ERR_VAL;
}

void netGuardHttpAccept() {
    netStarveStart();
    for (struct tcp_pcb_listen* l = tcp_listen_pcbs.listen_pcbs; l; l = l->next) {
        if (l->local_port != 80 || l->accept == guardedAccept) continue;
        gHttpAccept = l->accept;
        l->accept = guardedAccept;
    }
}

uint16_t netTcpRefused() { return gTcpRefused; }

bool netHttpAlive(uint32_t remoteIp, uint16_t remotePort) {
    for (struct tcp_pcb* p = tcp_active_pcbs; p; p = p->next)
        if (p->local_port == 80 && p->remote_port == remotePort && ip_2_ip4(&p->remote_ip)->addr == remoteIp &&
            p->state >= ESTABLISHED && p->state <= CLOSE_WAIT) return true;
    return false;
}

uint8_t netHttpConnections() {
    uint8_t n = 0;
    for (struct tcp_pcb* p = tcp_active_pcbs; p && n < 255; p = p->next)
        if (p->local_port == 80 && p->state >= ESTABLISHED && p->state <= CLOSE_WAIT) n++;
    return n;
}

void netStatsReset() { gNet = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0}; gStarveLast = millis(); netStatsSample(); }
#endif
