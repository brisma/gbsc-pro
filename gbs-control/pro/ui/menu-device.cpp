// Device glue of the menu (whole file device-only)
#ifdef ARDUINO
#include <Arduino.h>
#include <IRrecv.h>
#include <SSD1306Wire.h>
#include "../../options.h"
#include "../../tv5725.h"
#include "../platform/platform-caps.h"
#include "../platform/platform-device.h"
#include "tv-osd-device.h"
#include "../update/update-device.h"
#include "../api/api-device.h"
#include "../api/api-actions.h"
#include "../drivers/stv9426-device.h"
#include "menu-device.h"
#include "../update/ota-update.h"
#include "../link/adv-link-device.h"
#include "../link/adv-controller-image.h"
#include "menu-strings.h"
#include "menu-engine.h"
#include "menu-backend-device.h"
#include "menu-signal.h"
#include "osd-renderer.h"
#include "oled-renderer.h"
#include "ir-input.h"
#include "encoder-input.h"
#include "../profiles/profile-store.h"
#include "../profiles/profile-autoload.h"
#include "../profiles/profile-modes.h"

typedef TV5725<GBS_ADDR> GBS;
extern userOptions* uopt;
extern runTimeOptions* rto;
extern IRrecv irrecv;
extern decode_results results;
extern SSD1306Wire display;
#include "../log/log.h"
#include "../log/log-signal.h"

static const uint8_t kPinClk = 14, kPinData = 13, kPinSwitch = 0;   // encoder (GPIO0: pressed = LOW)

static DeviceMenuBackend gBe;
static MenuEngine gEngine(gBe);
static IrRepeater gRepeat;
static bool gIrLog = false;

static EncoderRotary gRot;
static EncoderButton gBtn;
static volatile int8_t gRotSteps = 0;

static OsdRenderer& renderer() {
    static OsdRenderer r(osdDriver());
    return r;
}

static OledRenderer& oled() {
    static OledRenderer r(display);
    return r;
}

void IRAM_ATTR menuEncoderRotateIsr() {
    int8_t d = gRot.edge((uint8_t)((digitalRead(kPinClk) << 1) | digitalRead(kPinData)), millis());
    int8_t s = gRotSteps + d;
    if (s >= -8 && s <= 8) gRotSteps = s;
}

void IRAM_ATTR menuEncoderPushIsr() { gBtn.edge(millis(), digitalRead(kPinSwitch) == LOW); }

void menuEncoderAttach() {
    if (!hwHas(CAP_ENCODER)) return;
    pinMode(kPinClk, INPUT_PULLUP);
    pinMode(kPinData, INPUT_PULLUP);
    pinMode(kPinSwitch, INPUT_PULLUP);
    gRot.seed((uint8_t)((digitalRead(kPinClk) << 1) | digitalRead(kPinData)));   // the rest position
    attachInterrupt(digitalPinToInterrupt(kPinClk), menuEncoderRotateIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(kPinData), menuEncoderRotateIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(kPinSwitch), menuEncoderPushIsr, CHANGE);
}

int32_t menuDeviceLive(uint8_t lv) { return gBe.live(lv); }

static void toastLog(const char* text) { logPrintf(LC_MENU, LL_INFO, PSTR("toast: %s"), text); }

void menuDeviceBegin() {
    otaSetMenuHook([](const char* text, bool) { gEngine.message(text, millis()); });   // OTA check results
    gEngine.setToastHook(toastLog);   // the popups seen on the TV/OLED are in the console log too
    gEngine.setWindow(7);
    gEngine.setEncoderBackRow(hwHas(CAP_ENCODER));
    gBe.refreshSignal();
}

bool menuUiBusy() { return gEngine.blocking(); }   // the status pop-up and toasts do not hold auto-load

const MenuSignal& menuDeviceSignal() { return gBe.signal(); }   // diagnostics (/api/info): RAM copy, refreshed in loop()

bool menuIrLogOn() { return gIrLog; }

void menuToggleIrLog() {
    gIrLog = !gIrLog;
    logPrintf(LC_MENU, LL_INFO, gIrLog ? PSTR("IR: code log on") : PSTR("IR: code log off"));
}

static const char* keyName(uint8_t k) {
    static const char* const kNames[] = {"up", "down", "left", "right", "OK", "back", "menu", "info", "save", "mute",
                                         "vol+", "vol-", "knob left", "knob right", "click", "long press"};
    return k < sizeof(kNames) / sizeof(kNames[0]) ? kNames[k] : "?";
}

// Every key (IR, encoder, buttons) passes here: upload lock, test screen, screensaver activity
static void menuKey(uint8_t k, bool repeat, uint32_t now, const char* src) {
    if (!repeat && src) logPrintf(LC_MENU, LL_DEBUG, PSTR("key %s (%s)"), keyName(k), src);   // held keys: the first only
    if (updateInProgress()) return;                        // locked while an upload runs
    if (hwHas(CAP_OLED)) oled().activity(now);
    if (osdDriver().exclusive()) {                         // Developer test screen: any key closes it
        if (!repeat) { ActionRequest r = {}; r.index = AID_dev_osd_test; apiQueueAction(r); }
        return;
    }
    gBe.refreshSignal();
    gEngine.key(k, repeat, now);
}

void menuDeviceKey(uint8_t k) { menuKey(k, false, millis(), "button"); }

void menuNotice(uint16_t str) { gEngine.notice(str, millis()); }

void menuNotifyAutoload(const char* name, const char* discarded) { gEngine.autoloadStarted(name, millis(), discarded); }

// Encoder events: rotation steps counted by the ISR, button decided here from the pin level
static void encoderInput(uint32_t now) {
    if (!hwHas(CAP_ENCODER)) return;
    // Normal = the board's own direction: the standard gbs-control turns the other way; the base follows
    // the hardware, so Board = Standard on a Pro does not flip it
    static const uint8_t kQuarters[3] = {4, 2, 1};
    gRot.configure((uopt->encoderReverse != 0) != !platformLinkHardware(), kQuarters[uopt->encoderSteps < 3 ? uopt->encoderSteps : 0]);
    noInterrupts();
    int8_t steps = gRotSteps;
    gRotSteps = 0;
    bool down = digitalRead(kPinSwitch) == LOW;
    int b = gBtn.poll(now, down);
    interrupts();
    if (steps) logPrintf(LC_MENU, LL_DEBUG, PSTR("key knob %s x%d"), steps > 0 ? "right" : "left", steps > 0 ? steps : -steps);
    for (; steps > 0; steps--) menuKey(K_ROT_R, false, now, nullptr);   // dropped inside menuKey during an upload
    for (; steps < 0; steps++) menuKey(K_ROT_L, false, now, nullptr);
    if (b >= 0) menuKey((uint8_t)b, false, now, "knob");
}

static void irInput(uint32_t now) {
    if (!hwHas(CAP_IR) || !irrecv.decode(&results)) return;
    bool isRepeat = results.repeat;
    uint32_t code = (uint32_t)results.value;
    irrecv.resume();
    if (gIrLog) {
        char line[32];
        snprintf(line, sizeof(line), "IR: %08lX%s", (unsigned long)code, isRepeat ? " repeat" : "");
        logText(LC_MENU, isRepeat ? LL_DEBUG : LL_INFO, line);   // a held key repeats every ~110 ms
    }
    bool repeat = false;
    int k = gRepeat.feed(code, isRepeat, now, repeat);
    if (k < 0) return;
    menuKey((uint8_t)k, repeat, now, "IR");
}

// Console log: the page or overlay on screen (debug), and the shown signal when it changed and stayed a second
static void logView(const MenuView& v) {
    static const char* const kOv[] = {"none", "mute", "volume", "info", "confirm", "revert", "adjust", "toast",
                                      "update", "keyboard", "slider", "same signal", "status", "TV OSD"};
    static uint8_t mode = 0xFF, ov = 0xFF;
    static char title[sizeof(v.title)] = "";
    if (v.mode == mode && (v.mode != VIEW_OVERLAY || v.overlay == ov) &&
        (v.mode != VIEW_LIST || !strcmp(title, v.title)))
        return;
    mode = v.mode;
    ov = v.overlay;
    strncpy(title, v.title, sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
    if (v.mode == VIEW_CLOSED) logPrintf(LC_MENU, LL_DEBUG, PSTR("closed"));
    else if (v.mode == VIEW_LIST) logPrintf(LC_MENU, LL_DEBUG, PSTR("page %s"), v.title[0] ? v.title : "(top)");
    else logPrintf(LC_MENU, LL_DEBUG, PSTR("overlay %s"), v.overlay < sizeof(kOv) / sizeof(kOv[0]) ? kOv[v.overlay] : "?");
}

static void logSignal(const MenuSignal& g, uint32_t now) {
    static LogSyncReturn back;   // a short dropout keeps the same signal line: say when the sync came back
    uint32_t gone = back.update((uint16_t)rto->noSyncCounter, rto->sourceDisconnected, now);
    if (gone) logPrintf(LC_SYNC, LL_INFO, PSTR("signal back after %lu.%lu s"), (unsigned long)(gone / 1000),
                        (unsigned long)(gone % 1000 / 100));
    static LogSignalWatch watch;
    LogSignalInfo s = {};
    s.none = g.noSignal;
    s.lines = g.srcLines;
    s.srcHz10 = g.srcHzX10;
    s.outHz10 = g.outHzX10;
    s.interlaced = g.interlaced;
    s.sync = g.syncType;
    s.locked = g.frameLocked;
    s.preset = g.presetId;
    s.input = uopt->activeInputType;
    if (!s.none && (!s.srcHz10 || !s.lines)) return;   // rates or line count not settled yet: one line once they are
    if (!watch.update(s, now)) return;
    char t[LOG_TEXT_MAX + 1];
    logSignalText(s, t, sizeof(t));
    logText(LC_SYNC, LL_INFO, t);
}

// ADV Controller update: the boot prompt once per boot unless this build was skipped
static void advControllerUpdate(uint32_t now) {
    static bool prompted = false;
    if (!prompted && gBe.signal().advControllerUpdateAvailable) {
        char id[9];
        snprintf(id, sizeof(id), "%08lX", (unsigned long)advControllerImageCrc());
        if (strcmp(uopt->advControllerSkip, id) == 0) prompted = true;
        else if (gEngine.promptAdvController(now)) prompted = true;    // retried while a menu or an overlay is shown
    }
}

// Update outcomes reach the engine before its tick, so the update overlay shows them instead of closing: the
// ADV Controller result, and an ESP upload that left "in progress" without the restart (it failed)
static void updateOutcomes(uint32_t now) {
    uint8_t result = advLinkUpdateResult();
    if (result) gEngine.updateResult(result == 1, true, now);
    static bool wasEsp = false;
    bool esp = updateInProgress();
    if (wasEsp && !esp) gEngine.updateResult(false, false, now);
    wasEsp = esp;
}

void menuDeviceLoop() {
    uint32_t now = millis();
    if (advLinkNoReplyNotice()) gEngine.notice(S_T_ADV_CONTROLLER_OLD, now);   // once, after the boot check
    irInput(now);
    encoderInput(now);
    tvOsdLoop(now);

    // Signal changes: visibility re-evaluated at once; the rates (a blocking measurement of a few fields) 1.5 s
    // later, once the signal settled, for the OLED home screen (which otherwise keeps stale values such as
    // "6.4Hz"), and again for the info panel
    static MenuSignalKey lastKey;
    static bool haveKey = false;
    static bool wasInfo = false, ratesDue = false;
    static uint32_t ratesAt = 0;
    // An ESP upload writes the flash from the network context between chunks: no scaler reads meanwhile (VPERIOD_IF
    // reads during uploads crashed in Twi::read_byte). The update overlay reads its progress from signal(), which
    // needs no I2C.
    bool quiet = updateInProgress();
    // VPERIOD_IF at most every 20 ms: each read is a software-I2C transfer that keeps the bus and the CPU busy
    static MenuRatesRetry ratesRetry;   // rates still unknown on a shown source: a few more tries
    static uint32_t linesAt = 0;
    static uint16_t lines = 0;
    if (!quiet && now - linesAt >= 20) { linesAt = now; lines = (uint16_t)GBS::VPERIOD_IF::read(); }
    // the menus' input (the last found one while Auto searches): every probe of the scan rebuilt the menu otherwise
    MenuSignalKey key = {gBe.activeInput(), rto->videoStandardInput, rto->outModeHdBypass, uopt->deintMode,
                         quiet ? lastKey.lines : lines, rto->presetID};   // a new output preset re-measures the rates
    bool changed = !quiet && (!haveKey || menuSignalChanged(lastKey, key));
    if (changed) {
        lastKey = key; haveKey = true; gBe.refreshSignal(); gEngine.onSignalChange();
        ratesDue = true; ratesAt = now + 1500;
        ratesRetry.restart();
    }

    static uint32_t lastRefresh = 0;
    if (!quiet && now - lastRefresh >= 250) { lastRefresh = now; gBe.refreshSignal(); }

    const MenuView& v = gEngine.view();
    bool info = v.mode == VIEW_OVERLAY && (v.overlay == OV_INFO || v.overlay == OV_STATUS);   // both show rates
    if (info && !wasInfo) { ratesDue = true; ratesAt = now + (v.overlay == OV_STATUS ? 100 : 500); }
    wasInfo = info;
    if (!quiet && ratesDue && (int32_t)(now - ratesAt) >= 0) {
        ratesDue = false;
        gBe.refreshRates();                                 // blocks for a few fields; no-op without a signal
    }
    // A shown source whose rates are still unknown (the planned measurement ran while the menu still said no signal,
    // e.g. during the RGB SCART judge): measure again every 1.5 s until known
    const MenuSignal& rs = gBe.signal();
    if (!quiet && !ratesDue && ratesRetry.due(rs.noSignal, rs.srcHzX10, now)) gBe.refreshRates();

    updateOutcomes(now);
    gEngine.tick(now);
    logView(gEngine.view());
    if (!quiet) logSignal(gBe.signal(), now);
    advControllerUpdate(now);
    // Status pop-up: once the signal settled, the preset stage finished and auto-load decided (its load
    // ran), so the pop-up names the profile in use; "before" moves on every new key (no late pop-up)
    static StatusKey statusBefore = {};
    static bool haveStatus = false;
    if (!quiet && hwHas(CAP_OSD) && profileSignalPresent() && !autoloadStageBusy(rto->applyPresetDoneStage) &&
        profileOpsPending() == 0 && (!uopt->profilesAutoload || profileAutoloadIdleFor(lines))) {
        StatusKey k = {uopt->activeInputType, rto->videoStandardInput, lines, (int8_t)profileActiveIndex(),
                       profileIsInterlaced(rto->videoStandardInput, lines)};
        bool changed = !haveStatus || menuStatusKeyChanged(statusBefore, k);
        if (changed || gEngine.statusNoticePending()) {
            bool due = changed && (!haveStatus || menuStatusPopupDue(uopt->osdStatusPopup, statusBefore, k));
            statusBefore = k;
            haveStatus = true;
            gEngine.statusPopup(now, due);
        }
    }
    if (hwHas(CAP_OSD)) {
        // Horizontal placement follows the scaler's output window: read it when the OSD opens and after every
        // Screen adjust step (position, borders and htotal move it)
        const MenuView& ov = gEngine.view();
        static bool wasOpen = false;
        static uint16_t adjustRev = 0;
        bool open = ov.mode != VIEW_CLOSED, adjust = ov.mode == VIEW_OVERLAY && ov.overlay == OV_ADJUST;
        if (!updateInProgress() && ((open && !wasOpen) || (adjust && ov.rev != adjustRev))) {   // no I2C reads
            // during an upload (the update overlay opens the OSD then)
            osdReadWindow();
            if (adjust) adjustRev = ov.rev;
        }
        wasOpen = open;
        renderer().setPlacement(uopt->osdPosition, uopt->osdPopupPosition);
        renderer().render(ov, uopt->osdTheme, now);
    }

    if (hwHas(CAP_OLED)) {
        static MenuIdle idle = {};
        static uint32_t idleAt = 0;
        static bool haveIdle = false;
        if (!haveIdle || now - idleAt >= 1000) { gEngine.idle(idle); idleAt = now; haveIdle = true; }
        oled().render(gEngine.view(), idle, now);
    }
}
#endif
