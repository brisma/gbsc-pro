#ifdef ARDUINO
#include <Arduino.h>
#include "../util/pgm.h"
#include <SoftwareSerial.h>
#include "../platform/platform-caps.h"
#include "../platform/platform-device.h"
#include "../update/update-device.h"
#include "adv-link-device.h"
#include "adv-link-sched.h"
#include "adv-link-client.h"
#include "adv-state.h"
#include "adv-updater.h"
#include "adv-controller-image.h"
#include "i2p-guard.h"
#include "../input/input-types.h"
#include "../input/input-auto.h"
#include "../api/api-device.h"
#include "../settings/settings-registry.h"
#include "../ui/menu-device.h"
#include "../ui/menu-strings.h"

extern userOptions* uopt;
extern runTimeOptions* rto;
extern bool advListenTermination;
bool profileSignalPresent();
#include "../log/log.h"
#include "../input/input-auto-device.h"

static AdvLinkSched gSched;
static AdvLinkClient gClient;
static SoftwareSerial gSw;                 // replies from the ADV Controller arrive on GPIO1
static bool gSwReady = false;
static bool gSentThisPass = false;
static AdvUpdater gUpd;
static uint8_t gUpdResult = 0;             // 1 updated, 2 failed (read once by the menu glue)
static char gUpdStatus[24] = "idle";       // last outcome for /api/info: idle, done, failed:<step>
static I2pGuard gI2pGuard;

// 2X off for a progressive 50 Hz source the TV5725 cannot lock with I2P on (i2p-guard.h): the setting goes back to
// Off as if the user had turned it off (a toast says why); turning it on again tries once more
static bool i2pInhibited(uint32_t now) {
    const AdvSignal* g = gClient.advController().status == ADV_CONTROLLER_OK ? gClient.signal() : nullptr;
    I2pGuardIn in;
    in.i2p = uopt->advI2P != 0;
    in.input = uopt->activeInputType;
    in.decoder = inputIsDecoder(uopt->activeInputType);
    in.locked = g && (g->flags & ADV_SIG_LOCK) && !(g->flags & ADV_SIG_FREERUN);
    in.progressive = g && !(g->flags & ADV_SIG_INTERLACED);
    in.hz50 = g && g->hz == 50;
    in.shown = inputTvPathPresent(rto->boardHasPower, rto->sourceDisconnected, rto->videoStandardInput,
                                  rto->noSyncCounter);   // 2X turned on over a shown picture: upstream recovers ~50 s
    bool off = gI2pGuard.update(in, now);
    static bool waited = false;                          // console log: the countdown and how it ended
    bool waiting = gI2pGuard.waiting();
    if (waiting && !waited)
        logPrintf(LC_ADV, LL_INFO, PSTR("2X: progressive 50 Hz on %s, no picture: off in %u.%u s if it stays so"),
                  inputLongName(in.input), (unsigned)(I2pGuard::kEngageMs / 1000), (unsigned)(I2pGuard::kEngageMs % 1000 / 100));
    else if (!waiting && waited && !off)
        logPrintf(LC_ADV, LL_INFO, in.shown ? PSTR("2X: picture shown, 2X stays on") : PSTR("2X: wait ended (source or setting changed)"));
    waited = waiting;
    static bool queued = false;                          // once per inhibit: the setting applies a few passes later
    if (!off) queued = false;
    else if (!queued && uopt->advI2P) {
        SettingsPair p = {(uint8_t)SID_adv_i2p, 0};
        if (apiQueueSettings(&p, 1)) {                   // retried on the next pass while the queue is full
            queued = true;
            logPrintf(LC_ADV, LL_INFO, PSTR("2X off: progressive 50 Hz on %s (the TV5725 cannot lock it with 2X)"),
                      inputLongName(uopt->activeInputType));
            menuNotice(S_T_2X_UNSUPPORTED);
        }
    }
    return off;
}

// ---- Console log: ADV Controller status, signal report and link events (once per change) ----
static const char* statusName(uint8_t st) {
    switch (st) {
        case ADV_CONTROLLER_CHECKING: return PSTR("checking");
        case ADV_CONTROLLER_OK: return PSTR("ok");
        case ADV_CONTROLLER_NO_REPLY: return PSTR("no reply");
        default: return PSTR("incompatible");
    }
}

static void logController() {
    static uint8_t lastStatus = 0xFF;
    static bool lastInfo = false, lastNotApplied = false;
    const AdvControllerInfo& h = gClient.advController();
    if (h.status != lastStatus || h.haveInfo != lastInfo) {
        if (h.haveInfo) logPrintf(LC_ADV, LL_INFO, PSTR("status %S (fw %s)"), statusName(h.status), h.info.fw);
        else logPrintf(LC_ADV, LL_INFO, PSTR("status %S"), statusName(h.status));
        lastStatus = h.status;
        lastInfo = h.haveInfo;
    }
    if (h.notApplied && !lastNotApplied) logPrintf(LC_ADV, LL_INFO, PSTR("state not applied after 3 resends"));
    lastNotApplied = h.notApplied;
}

static void logSignal() {
    static AdvSignal last;
    static bool have = false;
    const AdvSignal* g = gClient.signal();
    if (!g || (have && g->flags == last.flags && g->hz == last.hz && g->standard == last.standard &&
               g->encoder == last.encoder && g->input == last.input))
        return;
    static const uint8_t kFlagBits[] = {ADV_SIG_POWERED, ADV_SIG_PRESENT, ADV_SIG_LOCK, ADV_SIG_FREERUN, ADV_SIG_FSC,
                                        ADV_SIG_COLORKILL, ADV_SIG_INTERLACED, ADV_SIG_NONSTD};
    static const char kFlagNames[] PROGMEM = "powered\0present\0lock\0free-run\0burst\0colour kill\0interlaced\0non-standard";
    char t[LOG_TEXT_MAX + 1];
    size_t k = (size_t)snprintf_P(t, sizeof(t), PSTR("signal:"));
    for (uint8_t i = 0; i < sizeof(kFlagBits); i++) {
        const uint8_t bit = kFlagBits[i];
        bool on = g->flags & bit;
        if (have ? on == (bool)(last.flags & bit) : !on) continue;   // the first report: what is on
        if (k < sizeof(t))
            k += (size_t)snprintf_P(t + k, sizeof(t) - k, PSTR(" %S %S,"), pgmListAt(kFlagNames, i, sizeof(kFlagBits)),
                                    on ? PSTR("on") : PSTR("off"));
    }
    if (k < sizeof(t))
        snprintf_P(t + k, sizeof(t) - k, PSTR(" %u Hz, std %u, encoder %S, input %s"), (unsigned)g->hz, (unsigned)g->standard,
                 g->encoder == 1 ? PSTR("525p") : g->encoder == 2 ? PSTR("625p") : PSTR("off"), inputLongName(g->input));
    last = *g;
    have = true;
    if (!inputAutoQuietRound()) logText(LC_ADV, LL_DEBUG, t);   // a scan's later rounds repeat the same changes
}

// Register reads: one line per finished read (also one refused without a request: old image, no reply)
static void logRegRead() {
    static uint8_t loggedGen = 0, loggedState = ADV_REG_IDLE;
    const AdvRegRead& r = gClient.regRead();
    if (r.state == ADV_REG_PENDING || r.state == ADV_REG_IDLE || (r.gen == loggedGen && r.state == loggedState)) return;
    loggedGen = r.gen;
    loggedState = r.state;
    static const char kSt[] PROGMEM = "idle\0pending\0ok\0error\0unsupported";
    static const char kWhy[] PROGMEM = "\0: the reply says status \0: fewer values than asked\0: no usable reply\0: ADV Controller not ready";
    char why[72] = "";
    if (r.state == ADV_REG_ERROR && r.why <= ADV_REG_WHY_CONTROLLER) {
        strncpy_P(why, pgmListAt(kWhy, r.why, 5), sizeof(why) - 1);
        static const char kStep[] PROGMEM = "\0 at start\0 at the address\0 at the register\0 at restart\0 at the read address\0 in the data";
        uint8_t code = r.replyStatus & 0x0F, step = r.replyStatus >> 4;   // the ADV Controller adds the failed I2C step
        if (r.why == ADV_REG_WHY_STATUS) snprintf_P(why + strlen(why), sizeof(why) - strlen(why), PSTR("%u (%S%S)"), (unsigned)code,
                                                    code == ADV_REG_READ_I2C_ERROR ? PSTR("I2C") : code == ADV_REG_READ_BAD_REQUEST ? PSTR("bad request") : PSTR("?"),
                                                    pgmListAt(kStep, step < 7 ? step : 0, 7));
        if (r.badReplies) snprintf_P(why + strlen(why), sizeof(why) - strlen(why), PSTR(", %u bad replies"), (unsigned)r.badReplies);
    }
    logPrintf(LC_ADV, LL_INFO, PSTR("reg read %02X map %02X from %02X x%u: %S%s (gen %u)"), (unsigned)r.dev, (unsigned)r.map,
              (unsigned)r.reg, (unsigned)r.count, pgmListAt(kSt, r.state <= ADV_REG_UNSUPPORTED ? r.state : 0, 5), why, (unsigned)r.gen);
}

static uint8_t gRegReadGen = 0;
uint8_t advLinkRegReadAnnounce() { return ++gRegReadGen; }
uint8_t advLinkRegReadAnnounced() { return gRegReadGen; }
const AdvRegRead& advLinkRegReadResult() { return gClient.regRead(); }
void advLinkRegRead(uint8_t dev, uint8_t map, uint8_t reg, uint8_t count, uint8_t gen) {
    if (gen != gRegReadGen) return;                       // a later request replaced this one
    gClient.requestRegRead(dev, map, reg, count, gen);
    logPrintf(LC_ADV, LL_INFO, PSTR("reg read %02X map %02X from %02X x%u: queued (gen %u)"), (unsigned)dev, (unsigned)map,
              (unsigned)reg, (unsigned)count, (unsigned)gen);
}

// Requests without a reply, summarised per 5 s window: how many of each kind, and how long after the last STATE frame
// (the ADV Controller does not answer while it applies one: decoder power-up takes ~370 ms)
static uint32_t gStateAt = 0;
static uint8_t gStateInput = 0;
static void logNoReply(uint8_t req, uint32_t now, bool flush) {
    static uint32_t windowAt = 0;
    static uint16_t n[3] = {0, 0, 0};
    static uint32_t minAfter = 0xFFFFFFFF, maxAfter = 0;
    static uint8_t input = 0;
    if (!flush) {
        uint8_t k = req == ADV_CMD_PING ? 0 : req == ADV_CMD_INFO ? 1 : 2;
        if (!n[0] && !n[1] && !n[2]) { windowAt = now; input = gStateInput; }
        n[k]++;
        uint32_t after = now - gStateAt;
        if (after < minAfter) minAfter = after;
        if (after > maxAfter) maxAfter = after;
    }
    if (!(n[0] || n[1] || n[2]) || now - windowAt < 5000) return;
    logPrintf(LC_LINK, LL_DEBUG, PSTR("no reply: PING x%u, INFO x%u, SIGNAL x%u in 5 s, %lu-%lu ms after a STATE (input %s)"),
              (unsigned)n[0], (unsigned)n[1], (unsigned)n[2], (unsigned long)minAfter, (unsigned long)maxAfter,
              inputLongName(input));
    n[0] = n[1] = n[2] = 0;
    minAfter = 0xFFFFFFFF;
    maxAfter = 0;
}

static void sendFrame(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    uint8_t f[ADV_MAX_PAYLOAD + ADV_FRAME_OVERHEAD];
    size_t n = advFrameEncode(gSched.nextSeq(), cmd, payload, len, f, sizeof(f));
    if (!n) return;
    Serial.write(f, n);                                  // UART0 TX (GPIO1) -> ADV Controller PB7
    gClient.frameSent(millis());
    gSentThisPass = true;
}

// Waits for UART0 to finish without yielding (Serial.flush() yields: async web handlers could keep U0TXD
// driving the line while the ADV Controller already answers)
static void drainTxNoYield() {
    while (((USS(0) >> USTXC) & 0xff) != 0) {}
    delayMicroseconds(100);                              // the last character's shift register at 115200
}

struct ReplyCtx { uint8_t req, seq; bool got; uint8_t cmd, len; uint8_t payload[ADV_MAX_PAYLOAD]; };
static ReplyCtx gReply;
static void onReply(const AdvParser* p, void* c) {
    ReplyCtx* r = (ReplyCtx*)c;
    if (r->got || !AdvLinkClient::acceptReply(r->req, r->seq, p->cmd, p->seq)) return;
    r->got = true;
    r->cmd = p->cmd;
    r->len = p->len;
    memcpy(r->payload, p->buf, p->len);
}

// One request/reply exchange, bounded to 40 ms (non-yielding); the reply (if any) is left in gReply
static void exchange(uint8_t req, const uint8_t* payload, uint8_t plen) {
    ReplyCtx& r = gReply;
    r.req = req;
    r.seq = gSched.nextSeq();
    r.got = false;
    uint8_t f[ADV_MAX_PAYLOAD + ADV_FRAME_OVERHEAD];
    size_t n = advFrameEncode(r.seq, req, payload, plen, f, sizeof(f));
    while (gSw.available()) (void)gSw.read();            // nothing stale from an earlier exchange
    (void)gSw.overflow();                                // nor its overflow flag
    // Byte by byte: the buffer write may yield (optimistic_yield), and SYS-context prints could then append
    // text after the request while the ADV Controller already answers
    for (size_t i = 0; i < n; i++) Serial.write(f[i]);
    drainTxNoYield();
    pinMode(1, INPUT_PULLUP);                            // release the line: the ADV Controller answers after 3 ms
    gSw.enableRx(true);
    AdvParser p;
    advParserInit(&p);
    uint32_t t0 = millis();
    // read() only: available() yields (optimistic_yield) once the loop task ran 10 ms, and the network context could
    // then run for milliseconds while the reply's edges pile up in the 128-entry ISR queue
    while (!r.got && millis() - t0 < 40) {
        int c;
        while ((c = gSw.read()) >= 0) advParserFeed(&p, (uint8_t)c, onReply, &r);
    }
    bool overflow = gSw.overflow();
    gSw.enableRx(false);
    pinMode(1, SPECIAL);                                 // U0TXD again
    gSentThisPass = true;
    if (overflow && !r.got) logPrintf(LC_LINK, LL_INFO, PSTR("reply lost: soft serial buffer overflow"));
}

static void readImage(uint32_t offset, uint8_t* out, uint8_t n, void*) { advControllerImageRead(offset, out, n); }

static uint32_t gUpdStartedAt = 0;   // for the outcome line: how long the update took

// While the updater runs it owns the link: no STATE, no client requests, one exchange per pass
static void updaterPass(uint32_t now) {
    uint8_t cmd, len, payload[ADV_MAX_PAYLOAD];
    static int lastPct = -1;
    int pct = gUpd.percent();
    if (pct != lastPct && pct % 25 == 0 && pct > 0 && pct < 100)
        logPrintf(LC_UPDATE, LL_INFO, PSTR("ADV Controller update: %d %%"), pct);
    lastPct = pct;
    if (gUpd.next(now, cmd, payload, len, readImage, nullptr)) {
        exchange(cmd, payload, len);
        gUpd.result(gReply.got, gReply.cmd, gReply.payload, gReply.len, millis());
    }
    if (gUpd.state() == AU_DONE || gUpd.state() == AU_FAILED) {
        unsigned long secs = (unsigned long)((millis() - gUpdStartedAt) / 1000);
        if (gUpd.state() == AU_DONE)
            logPrintf(LC_UPDATE, LL_INFO, PSTR("ADV Controller update: done in %lu s, build %08lX, restarts %u"), secs,
                      (unsigned long)advControllerImageCrc(), (unsigned)gUpd.restarts());
        else
            logPrintf(LC_UPDATE, LL_INFO, PSTR("ADV Controller update: failed at %s after %lu s, restarts %u"), gUpd.error(),
                      secs, (unsigned)gUpd.restarts());
        gUpdResult = gUpd.state() == AU_DONE ? 1 : 2;
        if (gUpd.state() == AU_DONE) snprintf_P(gUpdStatus, sizeof(gUpdStatus), PSTR("done"));
        else snprintf_P(gUpdStatus, sizeof(gUpdStatus), PSTR("failed:%s"), gUpd.error());
        gUpd.finish();
        gClient.restart();                               // PING -> INFO with the new firmware
        gSched.kick();                                   // and the whole state again
    }
}

static bool gListen = false;                // the input search's listen request (RGB SCART only, adv-state.cpp)
static bool gTermination = false;           // 75 ohm sync as the last state built has it (the setting, on a picture)
bool advLinkTerminationOn() { return gTermination; }
static uint32_t gSwitchSentAt = 0;          // millis() of the STATE that changed listen or the termination

static uint16_t gSentHash = 0;              // advStateHash of the last STATE sent

static void sendState(const AdvState& s) {
    static uint8_t lastListen = 0, lastTermination = 0;
    if (s.listen != lastListen || s.termination != lastTermination) gSwitchSentAt = millis();
    lastListen = s.listen; lastTermination = s.termination;
    uint8_t b[ADV_STATE_BYTES];
    advStatePack(&s, b);
    sendFrame(ADV_CMD_STATE, b, ADV_STATE_BYTES);
    uint16_t hash = advStateHash(&s);
    gSentHash = hash;
    gStateAt = millis();
    gStateInput = s.input;
    static uint16_t lastHash = 0;
    if (hash != lastHash && !inputAutoQuietRound())               // not the 5 s resend, not a scan's later rounds
        logPrintf(LC_LINK, LL_DEBUG, PSTR("state sent (hash 0x%04x)"), (unsigned)hash);
    lastHash = hash;
    gClient.stateSent(hash, millis());
}

void advLinkKick() { gSched.kick(); }

// The link follows the hardware (a Pro chip answered), not system.board: a Pro set to Standard still routes video
void advLinkBegin() {
    if (!platformLinkHardware()) return;
    // begin() reconfigures GPIO1 and enables RX: do it before the first STATE, then give the pin back to UART0
    // RX buffers: 64 bytes, 128 edges (4 bytes each). The longest reply is INFO (44 bytes on the wire, up to 440
    // edges), but exchange() drains both with read() while the reply arrives and never yields, so they only bridge
    // the ISR latency (was 512 edges: 2 KB)
    gSw.begin(38400, SWSERIAL_8N1, 1, -1, false, 64, 128);
    gSw.enableRx(false);
    pinMode(1, SPECIAL);
    gSwReady = (bool)gSw;
    gSched.kick();
    advLinkLoop();
}

void advLinkLoop() {
    if (!platformLinkHardware()) return;                 // standard board: no ADV Controller
    gSentThisPass = false;
    uint32_t now = millis();
    if (gUpd.active()) { if (gSwReady) updaterPass(now); return; }
    if (!gClient.stateBlocked()) {                       // incompatible ADV Controller: no STATE, the update is offered
        if (gClient.wantStateResend()) gSched.kickResend();
        AdvState s;
        bool term = advTerminationWanted(*uopt, profileSignalPresent(), inputAutoHoldsSyncWatcher());
        if (term != gTermination) {
            gTermination = term;
            logPrintf(LC_SYNC, LL_INFO, PSTR("75 ohm sync: %s"), term ? "on (RGB SCART picture)" : "off");
        }
        advStateFromOptions(*uopt, term || advListenTermination, gListen, s);
        if (i2pInhibited(now)) s.i2p = 0;
        if (gSched.tick(s, now)) sendState(s);
    }
    if (gSentThisPass || !gSwReady) return;              // never an exchange in a pass that sent a frame
    uint8_t req = gClient.due(now, gSched.requestAllowed(now), !updateInProgress());
    if (req) {
        uint8_t payload[ADV_REG_READ_REQ_BYTES], plen = 0;
        gClient.requestPayload(payload, &plen);          // REG_READ only; none for the others
        exchange(req, plen ? payload : nullptr, plen);
        gClient.result(req, gReply.got, gReply.cmd, gReply.payload, gReply.len, millis());
        if (!gReply.got) logNoReply(req, millis(), false);
    }
    logRegRead();
    logNoReply(0, millis(), true);
    logController();
    logSignal();
}

// The state of the current options sent now and confirmed: INFO is asked until the ADV Controller reports that state
// applied (its hash), the frame sent again while it does not; at most maxMs. For a caller that must know the
// switches moved before it looks at the result (the input search's look at the VGA path): a STATE sent while the
// controller still shuts the decoder down was applied seconds later
bool advLinkStateConfirmed(uint32_t maxMs) {
    if (!platformLinkHardware() || !gSwReady || gUpd.active() || gClient.stateBlocked()) return false;
    uint32_t t0 = millis();
    advLinkKick();
    advLinkLoop();                                       // the scheduler sends the state (gSentHash)
    while (millis() - t0 < maxMs) {
        delay(6);                                        // a STATE is applied from the controller's main loop
        exchange(ADV_CMD_INFO, nullptr, 0);
        AdvInfo in;
        if (!gReply.got || !advInfoUnpack(gReply.payload, gReply.len, &in)) continue;
        if (in.stateHash == gSentHash) return true;
        gSched.kickResend();
        advLinkLoop();
    }
    return false;
}

void advLinkI2c(const uint8_t* bytes, uint8_t len) {
    if (!platformLinkHardware() || gUpd.active()) return;   // the updater owns the link
    uint8_t n = (uint8_t)(len / 3);
    uint8_t p[1 + ADV_I2C_BATCH_MAX * 3];
    for (uint8_t i = 0; i < n;) {
        uint8_t k = (uint8_t)(n - i > ADV_I2C_BATCH_MAX ? ADV_I2C_BATCH_MAX : n - i);
        p[0] = k;
        for (uint8_t j = 0; j < k * 3; j++) p[1 + j] = bytes[i * 3 + j];
        sendFrame(ADV_CMD_I2C_BATCH, p, (uint8_t)(1 + k * 3));
        i = (uint8_t)(i + k);
    }
}

const AdvControllerInfo& advLinkController() { return gClient.advController(); }

void advLinkSetListen(bool on) {
    if (on == gListen) return;
    gListen = on;
    gSched.kick();                                       // the identification waits for it: no coalescing delay
    logPrintf(LC_ADV, LL_INFO, PSTR("listening on pin 20: %s"), on ? "on" : "off");
}
bool advLinkListenRequested() { return gListen; }
uint32_t advLinkSwitchSentAt() { return gSwitchSentAt; }
bool advLinkCanListen() {
    const AdvControllerInfo& c = gClient.advController();
    return c.status == ADV_CONTROLLER_OK && c.haveInfo && (c.info.caps & ADV_INFO_CAP_LISTEN);
}
bool advLinkChecking() { return platformLinkHardware() && gClient.advController().status == ADV_CONTROLLER_CHECKING; }
void advLinkSignalPollFast(bool on) { gClient.setSignalFast(on); }
uint32_t advLinkSignalAtMs() { return gClient.signalAtMs(); }
const AdvSignal* advLinkSignal() {
    const AdvSignal* g = gClient.signal();
    return g && gClient.advController().status == ADV_CONTROLLER_OK && millis() - gClient.signalAtMs() < 5000 ? g : nullptr;   // > 2 idle polls: one lost reply keeps it
}

bool advLinkControllerUpdateAvailable() {
    const AdvControllerInfo& h = gClient.advController();
    return platformLinkHardware() && (h.status == ADV_CONTROLLER_OK || h.status == ADV_CONTROLLER_INCOMPATIBLE) && h.haveInfo &&
           advControllerImageSize() > 0 && h.info.buildId != advControllerImageCrc();
}

bool advLinkUpdateCanStart() {
    const AdvControllerInfo& h = gClient.advController();
    if (!platformLinkHardware() || !gSwReady || gUpd.active() || updateInProgress() || advControllerImageSize() == 0) return false;
    return h.status == ADV_CONTROLLER_OK || h.status == ADV_CONTROLLER_INCOMPATIBLE;
}

bool advLinkUpdateStart() {
    if (!advLinkUpdateCanStart()) {                  // refused (e.g. after the menu's Yes): reported like a failure
        gUpdResult = 2;
        snprintf_P(gUpdStatus, sizeof(gUpdStatus), PSTR("failed:busy"));
        return false;
    }
    const AdvControllerInfo& h = gClient.advController();
    gUpdStartedAt = millis();
    gUpd.start(advControllerImageSize(), advControllerImageCrc(), advControllerImageVersion(), gUpdStartedAt);
    logPrintf(LC_UPDATE, LL_INFO, PSTR("ADV Controller update: started (%u KB, %s build %08lX -> %s build %08lX)"),
              (unsigned)(advControllerImageSize() / 1024), h.haveInfo ? h.info.fw : "?",
              (unsigned long)(h.haveInfo ? h.info.buildId : 0), advControllerImageVersion(),
              (unsigned long)advControllerImageCrc());
    return true;
}

const char* advLinkUpdateStatus() { return gUpd.active() ? "running" : gUpdStatus; }

bool advLinkUpdateActive() { return gUpd.active(); }
int advLinkUpdateProgress() { return gUpd.active() ? gUpd.percent() : -1; }
uint8_t advLinkUpdateResult() { uint8_t r = gUpdResult; gUpdResult = 0; return r; }
bool advLinkNoReplyNotice() { return gClient.noReplyNotice(); }
#endif
