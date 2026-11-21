#include <Arduino.h>
#include "../util/pgm.h"
#include "../../options.h"
#include "../../tv5725.h"
#include "../gbs-control-pro.h"
#include "../platform/platform-caps.h"
#include "../platform/platform-device.h"
#include "../link/adv-link-device.h"
#include "../update/update-device.h"
#include "input-auto.h"
#include "input-auto-device.h"
#include "rgb-scart-judge.h"
#include "input-identify.h"
#include "../ui/menu-device.h"
#include "../ui/menu-strings.h"
#include "../profiles/profile-autoload.h"
#include "../profiles/profile-modes.h"
#include "../settings/settings-device.h"
#include "../settings/settings-registry.h"

typedef TV5725<GBS_ADDR> GBS;
extern userOptions* uopt;
extern runTimeOptions* rto;
void saveUserPrefs();
#include "../log/log.h"
#include "../log/event-ring.h"
void tv5725ChannelContent(uint8_t mask, uint16_t out[3]);   // gbs-control.ino
void tv5725ChannelContentLive(uint8_t mask, uint16_t out[3]);   // gbs-control.ino: before a preset, registers restored
void tv5725ChannelStatsLive(uint8_t mask, uint16_t level[3], uint16_t content[3]);   // ... 8-bit p50 levels too
void goLowPowerWithInputDetection();                         // gbs-control.ino
boolean getStatus16SpHsStable();                             // gbs-control.ino: the sync processor sees a stable HS

static InputAuto gAuto;
static bool gBegun = false;
static uint32_t gSwitchAt = 0;          // last path switch made here
static uint32_t gPoweredAt = 0;         // first fresh POWERED report after the switch (S-Video/Composite probe)
static uint8_t gPasses = 0;             // TV5725 detection passes on the current path (inputTvPathDone) ...
static uint8_t gSogSeen = 0;            // ... and the SOG levels they sampled, bit per level
static uint8_t gPassSog = 0;            // the SOG level of the last pass (a find: the level that detected it)
static uint32_t gScanAt = 0;            // the scan the counts belong to: a new scan on the same path starts them again
static bool gPathSwitched = false;      // the next detection pass waits its 500 ms after the switch
static uint32_t gSyncSince = 0;         // the probed TV5725 path has a sync and no source since then (0: not so)
static bool gNeedDecoderOff = false;    // Component after S-Video/Composite: the ADV7391 drives bars on J4 while on

// Identification: a sync on RGB SCART in a
// scan, nothing shown yet; the syncwatcher is held while InputIdentify reads R/G/B, then the G level under listen
static InputIdentify gId;
static IdSample gIdNext;                // what this pass measured, read by the next one
static uint32_t gIdPassAt = 0, gListenOnAt = 0, gPoweredSince = 0, gLastReportAt = 0, gListenOffAt = 0;
static bool gIdOffReport = false;      // the release came from the decoder-off report (log)
static bool gIdTermination = false;     // an ADV Controller without listen: the termination alone
static bool gIdPaused = false;          // a pause (update, info display) came during the identification
static bool gIdPauseRedone = false;     // ... the first pause in a scan also detects again; later ones only identify again
static bool gIdSelfRedetect = false;    // our own re-detection (release with G still up): not "the source went"
static bool gIdMovePending = false;     // MOVE_COMPOSITE: listen stays until switchInputPath(AV) has run
static bool gListenOn = false;          // listen (or the termination) actually requested from the ADV Controller
static bool gListenWasOn = false;       // the encoder drove J4 in this acquisition: a switch to Component waits
static RjHint gHint;                    // RGB SCART fallback verdicts, still black after 20 s: "try Composite" once
static uint32_t gTickHoldAt = 0;
static bool gTickHold = false;          // the identification moved to Composite: absence waits kRejoinMs
static bool gJudgeMoved = false;        // ... and the next path switch resets the TV5725 detection
static const uint32_t kRejoinMs = 8000, kIdPassMs = 300;
static uint32_t gChoiceFadeAt = 0;      // a choice came with listen on (or fading): no preset until the termination
static bool gIdListenOff = false;      // this acquisition turned listen off (RELEASING): the release line is true
static bool gChoiceFade = false;        // ... has faded (kReleaseCapMs from gChoiceFadeAt)
static void identifyNewAcquisition();
static void listenSet(bool on);

// An empty TV5725 path gets detection passes (one every ~510 ms) until inputTvPathDone() (every SOG level on the sync
// slicing paths), capped in case the passes stop; an ADV path gets up to 1200 ms after the decoder is powered to
// lock (debounced report), never more than 4 s in all
static const uint32_t kTvPathCapMs = 4500, kAdvLockMs = 1200, kAdvCapMs = 4000, kDecoderOffCapMs = 1500;

static InputAutoBoard board() {
    InputAutoBoard b;
    b.proPaths = platformLinkHardware();
    b.advProbes = b.proPaths && hwHas(CAP_ADV);   // Standard set by hand on a Pro: no S-Video/Composite
    // Input > Auto search; on the standard board path 1 is the RGB header (input.scan_rgbs) and path 4 YPbPr
    b.skip = (uint8_t)((uopt->scanRgbs ? 0 : 1 << 0) | (uopt->scanRgsb ? 0 : 1 << 1) | (uopt->scanVga ? 0 : 1 << 2) |
                       (uopt->scanYuv ? 0 : 1 << 3) | (uopt->scanSv ? 0 : 1 << 4) | (uopt->scanAv ? 0 : 1 << 5));
    if (!b.proPaths) b.skip &= (uint8_t)((1 << 0) | (1 << 3));   // the Pro-only rows are hidden there
    return b;
}

// A report about the input the hardware is on, newer than our last switch
static const AdvSignal* freshReport() {
    const AdvSignal* g = advLinkSignal();
    if (!g || g->input != uopt->activeInputType || (int32_t)(advLinkSignalAtMs() - gSwitchAt) <= 0) return nullptr;
    return g;
}

static bool controllerReports() {
    const AdvControllerInfo& h = advLinkController();
    return h.status == ADV_CONTROLLER_OK && h.haveInfo && (h.info.caps & ADV_INFO_CAP_SIGNAL);
}

bool inputAdvSourcePresent() {
    if (!controllerReports()) return !rto->sourceDisconnected;   // no report from this ADV Controller: the TV5725 view
    const AdvSignal* g = freshReport();
    return g && (g->flags & ADV_SIG_POWERED) && (g->flags & ADV_SIG_PRESENT);
}

bool inputDetectMayToggle() { return !platformLinkHardware(); }
bool inputDetectAnyRgb() { return !platformLinkHardware() && uopt->inputChoice == 0; }
bool inputRgbHvAllowed() { return !platformLinkHardware() || uopt->activeInputType == InputTypeVGA; }
// The identification can start: Auto, a scan probing RGB SCART on the Pro, upstream's detection found a sync there
static bool identifyStartable() {
    return gBegun && platformLinkHardware() && hwHas(CAP_ADV) && gAuto.choice() == 0 && gAuto.state() == IA_SCANNING &&
           gAuto.want() == InputTypeRGBs && uopt->activeInputType == InputTypeRGBs && rto->boardHasPower &&
           !rto->sourceDisconnected;
}
// A sync on RGB SoG during a scan (a SCART console powering up while SoG is probed): the scan's look at RGB SCART
// (InputAuto confirm_) comes before any preset there, so the output changes once (not a SoG preset, then the RGB
// SCART one)
static bool sogFindPending() {
    return gBegun && platformLinkHardware() && gAuto.choice() == 0 && gAuto.state() == IA_SCANNING &&
           gAuto.want() == InputTypeRGsB && uopt->activeInputType == InputTypeRGsB && rto->boardHasPower &&
           !rto->sourceDisconnected;
}
bool inputAutoQuietRound() { return gBegun && gAuto.state() == IA_SCANNING && !gAuto.firstRound(); }
bool inputAutoSearching() {
    return gBegun && gAuto.choice() == 0 && (gAuto.state() == IA_SCANNING || gId.holdsSyncWatcher());
}
// From the first pass where the identification can start (the syncwatcher runs earlier in loop() than inputAutoLoop)
bool inputAutoHoldsSyncWatcher() {
    if (!gBegun) return false;
    if (gChoiceFade && millis() - gChoiceFadeAt < InputIdentify::kReleaseCapMs) return true;
    gChoiceFade = false;
    return gIdPaused || gId.holdsSyncWatcher() || (gId.phase() == ID_IDLE && identifyStartable()) || sogFindPending();
}

bool inputAutoHoldDetection() {
    if (!gBegun || !platformLinkHardware()) return false;
    if (gNeedDecoderOff) return true;
    return gAuto.state() == IA_SCANNING && inputIsDecoder(uopt->activeInputType) && controllerReports() && !inputAdvSourcePresent();
}

// ---- Console log: what the scan does and why (state kept here, written once per event) ----
static uint8_t gLogState = 0xFF, gLogFound = 0, gLogManual = 0;
static uint32_t gLogFoundAt = 0;
static bool gLogVga = false;            // the 15 kHz VGA line, once per scan

static const char* whyName(uint8_t why) {
    switch (why) {
        case IA_WHY_BOOT: return PSTR("boot");
        case IA_WHY_LOST: return PSTR("source lost");
        case IA_WHY_CHOSEN: return PSTR("Auto chosen");
        case IA_WHY_BOARD: return PSTR("Auto search changed");
        case IA_WHY_RETURN: return PSTR("source back after a gap");
        case IA_WHY_FORMAT: return PSTR("the source changed 50/60 Hz");
        default: return PSTR("back to Auto");
    }
}

static void logScanStart(bool link) {
    if (gAuto.scanReason() == IA_WHY_BACK_TO_AUTO && gLogManual)
        logPrintf(LC_INPUT, LL_INFO, PSTR("back to Auto (no signal on %s)"), inputLongName(gLogManual));
    if (!link) { logPrintf(LC_SCAN, LL_INFO, PSTR("start (%S)"), whyName(gAuto.scanReason())); return; }
    uint8_t order[6];
    uint8_t n = gAuto.scanPaths(order, 6);
    char list[72];
    size_t k = 0;
    list[0] = '\0';
    for (uint8_t i = 0; i < n && k < sizeof(list); i++)
        k += (size_t)snprintf_P(list + k, sizeof(list) - k, PSTR("%s%s"), i ? " > " : "", inputLongName(order[i]));
    logPrintf(LC_SCAN, LL_INFO, PSTR("start (%S): %s"), whyName(gAuto.scanReason()), list);
}

static void logVgaLook(bool waits) {
    if (gLogVga) return;
    gLogVga = true;
    if (waits) logPrintf(LC_SCAN, LL_INFO, PSTR("VGA shows 15 kHz or CSync: asking RGB SCART"));
    else logPrintf(LC_SCAN, LL_INFO, PSTR("VGA shows 15 kHz or CSync, RGB SCART not searched: keeping VGA"));
}

// Transitions after a loop pass: scan start, find, loss, a redirected find
static void logTransitions(bool link, uint32_t now) {
    uint8_t st = gAuto.state();
    if (gLogState == IA_FOUND && st == IA_SCANNING && gAuto.scanReason() == IA_WHY_LOST) {
        uint32_t s = (now - gLogFoundAt) / 1000;
        logPrintf(LC_INPUT, LL_INFO, PSTR("source lost on %s after %lu min %lu s"), inputLongName(gLogFound),
                  (unsigned long)(s / 60), (unsigned long)(s % 60));
        eventAdd(EV_INPUT_LOST, gLogFound, 0);
    }
    if (gAuto.takeScanStarted()) {
        logScanStart(link);
        gLogVga = false;
    }
    if (gAuto.takeRoundEnded()) logPrintf(LC_SCAN, LL_INFO, PSTR("no source on any input, scanning again"));
    if (st == IA_FOUND && (gLogState != IA_FOUND || gAuto.found() != gLogFound)) {
        uint32_t ms = now - gAuto.scanStartedAt();
        if (gLogState != IA_FOUND)
        {
            char sog[10] = "";
            uint8_t f = gAuto.found();
            if (f == InputTypeRGBs || f == InputTypeRGsB || f == InputTypeYUV) snprintf_P(sog, sizeof(sog), PSTR(", SOG %u"), gPassSog);
            logPrintf(LC_INPUT, LL_INFO, PSTR("found %s after %lu.%lu s%s%s"), inputLongName(f), (unsigned long)(ms / 1000),
                      (unsigned long)(ms % 1000 / 100), sog, gAuto.foundByFallback() ? " (nothing on RGB SCART)" : "");
        }
        else if (link)
            logPrintf(LC_INPUT, LL_INFO, PSTR("now on %s (identified on RGB SCART)"), inputLongName(gAuto.found()));
        else   // standard board: the upstream detection moved to another input type
            logPrintf(LC_INPUT, LL_INFO, PSTR("now on %s (detected)"), inputLongName(gAuto.found()));
        if (gLogState != IA_FOUND) eventAdd(EV_INPUT_FOUND, gAuto.found(), gAuto.foundByFallback() ? 1 : 0);
        else eventAdd(EV_INPUT_CHANGED, gAuto.found(), 0);                       // b 0: the identification, 1: by hand
        gLogFoundAt = now;
    }
    if (st == IA_FOUND) gLogFound = gAuto.found();
    gLogState = st;
}

uint8_t inputAutoBootInput(uint8_t hint) { return inputAutoBootPath(board(), hint); }

static void passesRestart() {
    gSyncSince = 0;
    gPasses = 0;
    gSogSeen = 0;
    gPassSog = 0;
    gPathSwitched = true;
}

void inputAutoBegin() {
    uint8_t hint = uopt->inputLast;
    if (!inputTypeValid(hint)) hint = 0;
    gAuto.begin(board(), hint, millis());          // the hint: input.last
    gSwitchAt = millis();                          // the boot detection gets the hinted path's full window
    passesRestart();
    gBegun = true;
}

void inputAutoChoose(uint8_t choice) {
    if (!gBegun) return;
    if (gListenOn || gId.phase() == ID_RELEASING) {        // like RELEASING: the preset's ADC offsets would take the raised G
        gChoiceFadeAt = millis();
        gChoiceFade = true;
        logPrintf(LC_INPUT, LL_INFO, PSTR("identify: choice while listening, no preset for %lu ms"),
                  (unsigned long)InputIdentify::kReleaseCapMs);
    }
    if (gId.holdsSyncWatcher() || gIdMovePending) {
        logPrintf(LC_INPUT, LL_INFO, PSTR("identify: abandoned (choice)"));
        gId.abandon(IW_CHOICE);
    }
    identifyNewAcquisition();
    if (choice) {
        gLogManual = choice;
        logPrintf(LC_INPUT, LL_INFO, PSTR("manual: %s"), inputLongName(choice));
        eventAdd(EV_INPUT_CHANGED, choice, 1);
    }
    gAuto.choose(choice, millis());
}

// Pro: the TV5725 detects the new path from scratch (upstream would keep the old source in its sync recovery). A
// disconnected source is normally in low power detection already; force: the identification's own moves (to
// Composite, a lost sync, a release with the termination still on), which a stale mode detector made wait the full 6 s
// of upstream's YPbPr loop (N64 PAL on the AV jack)
static void tvRedetect(bool force) {
    if (!rto->boardHasPower || (rto->sourceDisconnected && !force)) return;
    logPrintf(LC_SCAN, LL_DEBUG, force ? PSTR("TV5725 detects again (identification)")
                                       : PSTR("TV5725 detects again (the old source would stay in its sync recovery)"));
    goLowPowerWithInputDetection();
}

static void switchTo(uint8_t want, uint32_t now) {
    if (gAuto.state() == IA_SCANNING && gAuto.firstRound()) logPrintf(LC_SCAN, LL_DEBUG, PSTR("probe %s"), inputLongName(want));
    // only a switch to Component waits for the decoder off (the ADV7391 drives J4 while on); never the move to Composite
    gNeedDecoderOff = want == InputTypeYUV && (inputIsDecoder(uopt->activeInputType) || gListenWasOn);
    gListenWasOn = false;
    if (platformLinkHardware()) tvRedetect(gJudgeMoved);
    gJudgeMoved = false;
    if (!gIdMovePending) identifyNewAcquisition();         // another path: a new identification there
    switchInputPath(want, false);
    if (gIdMovePending) {                                  // the AV state is built now: listen may go (no RGBS frame between)
        gIdMovePending = false;
        gId.moveDone();
        listenSet(false);                                  // (switchInputPath cleared the termination itself)
    }
    gSwitchAt = now;
    gPoweredAt = 0;
    passesRestart();
}

// A sync on RGB SCART / RGB SoG during a scan may be the VGA connector's: its HS pin is on the wire the LM1881 drives
// (a Dreamcast switched on while those paths were probed was taken for a SCART console and never shown). With the
// switches in the VGA position the LM1881 is off the HS and VS wires: a horizontal and a vertical sync still there
// are a source on the VGA connector, and the scan goes to VGA. Both are asked for: a SCART console's bright picture
// (shared R/G/B wires) can read there as an HS with no VS, with or without a device on the VGA connector. The
// detection runs this in place of the VSync wait those two paths ignore on the Pro
static const uint32_t kVgaLookConfirmMs = 200, kVgaLookSettleMs = 60, kVgaLookSampleMs = 160, kVgaLookBackMs = 60;
static const uint8_t kVgaLookRun = 6, kVgaLookNoHs = 8;         // 5 ms samples: H and V 6 in a row; no H in the first 8
static const uint8_t kVgaLookVsEarly = 6;                       // a VS in the first samples is not a measured one
static const uint32_t kPeekSettleMs = 20, kPeekAgainMs = 60;    // the identification's look at the composite picture

// What the last look read (the return watch decides on it)
static struct { bool hs, vs, pathMeasured, pathVs; } gLook;

bool inputAutoVgaLookDue() {
    return gBegun && platformLinkHardware() && gAuto.vgaLookWanted() && uopt->activeInputType == gAuto.want() &&
           !advLinkChecking() && advLinkController().status == ADV_CONTROLLER_OK;
}

bool inputAutoLookAtVga() {
    uint8_t was = uopt->activeInputType;
    uint32_t t0 = millis();
    uopt->activeInputType = InputTypeVGA;                        // the switches only: the TV5725 stays as it is
    bool moved = advLinkStateConfirmed(kVgaLookConfirmMs);       // never read before the switches are there
    uint8_t n = 0, hs = 0, vs = 0, run = 0, vsFirst = 0;
    bool onVga = false;
    if (moved) {
        uint8_t ext = GBS::SP_EXT_SYNC_SEL::read();
        GBS::SP_EXT_SYNC_SEL::write(0);                          // the VS pin connected, only now: never the LM1881's VS
        delay(kVgaLookSettleMs);
        // The VS detector answers about 120 ms after the pin is connected (a Dreamcast: from sample 14 or later). A VS
        // reported from the first samples is its state after the TV5725 reset, not a sync (seen 8/8 and 31/31 with a
        // console on the SCART, right after a scan started): never a source. Ends early on a run of both (a source)
        // and on no HS at all (a SCART console: the common case costs least)
        for (uint32_t t = millis(); millis() - t < kVgaLookSampleMs; delay(5)) {
            uint8_t st = GBS::STATUS_16::read();
            n++;
            if (st & 0x02) hs++;
            if (st & 0x08) { vs++; if (!vsFirst) vsFirst = n; }
            run = (st & 0x0A) == 0x0A ? (uint8_t)(run + 1) : 0;
            if (run >= kVgaLookRun && vsFirst > kVgaLookVsEarly) { onVga = true; break; }
            if (n >= kVgaLookNoHs && !hs) break;
        }
        GBS::SP_EXT_SYNC_SEL::write(ext);
    }
    uopt->activeInputType = was;
    gLook = {hs != 0, vs != 0, false, false};
    // in a scan the cursor goes to VGA; after a find (the return watch) a scan starts there
    if (onVga && (gAuto.syncOnVga(millis()) || gAuto.returnOnVga(millis()))) {
        logPrintf(LC_SCAN, LL_INFO, PSTR("the sync on %s comes from the VGA connector (H %u/%u, V %u/%u from sample %u, %lu ms): probing VGA"),
                  inputLongName(was), hs, n, vs, n, vsFirst, (unsigned long)(millis() - t0));
        gJudgeMoved = true;                                      // the TV5725 detects from scratch there, as at boot
        switchTo(InputTypeVGA, millis());
        return true;
    }
    bool back = advLinkStateConfirmed(kVgaLookConfirmMs);        // back to this path's switches
    delay(kVgaLookBackMs);
    // An HS and no VS on the VGA path: a SCART console's bright picture read as sync, or a cable with composite sync
    // on the VGA connector. The two differ in the sync on SCART pin 20: the LM1881's VS (Sync cleanup on) says
    // whether there is one. Logged for reports from such cables; the return watch looks again only without it
    uint8_t m = 0, sv = 0, first = 0;
    if (hs && back) {
        uint8_t ext = GBS::SP_EXT_SYNC_SEL::read();
        GBS::SP_EXT_SYNC_SEL::write(0);
        delay(kVgaLookSettleMs);
        // the VS detector answers about 120 ms after it is connected (0/12 in the first 120 ms with a console on)
        for (uint32_t t = millis(); millis() - t < 260; delay(5)) {
            m++;
            if (GBS::STATUS_SYNC_PROC_VSACT::read()) { sv++; if (!first) first = m; }
            if (sv >= 12) break;
        }
        GBS::SP_EXT_SYNC_SEL::write(ext);
    }
    gLook.pathMeasured = m != 0;
    gLook.pathVs = sv != 0;
    char pin20[56] = "";
    if (m) snprintf_P(pin20, sizeof(pin20), PSTR("; VS from this path %u/%u from sample %u, cleanup %s"), sv, m, first,
                      uopt->advSyncStripper ? "on" : "off");
    logPrintf(LC_SCAN, (hs || vs) ? LL_INFO : LL_DEBUG,
              PSTR("look at VGA from %s: no source there (H %u/%u, V %u/%u from sample %u, %lu ms%s%s%s)"), inputLongName(was), hs, n,
              vs, n, vsFirst, (unsigned long)(millis() - t0), moved ? "" : ", switches not confirmed",
              back ? "" : ", return not confirmed", pin20);
    return false;
}

void inputAutoDetectPass(uint8_t sog) {
    if (gPasses < 255) gPasses++;
    if (sog >= 8) return;                      // the pass had to move the ADC input first: its level was not tried
    gSogSeen |= (uint8_t)(1 << sog);
    gPassSog = sog;
}

bool inputAutoTakePathSwitched() { bool r = gPathSwitched; gPathSwitched = false; return r; }

// The scan's verdict on the path the hardware is on (a find comes through tick())
// Component after S-Video/Composite, scan or manual: hold the detection until the decoder path is confirmed off
static void decoderOffStep(uint32_t now) {
    const AdvSignal* g = freshReport();
    if ((g && !(g->flags & ADV_SIG_POWERED)) || !controllerReports() || now - gSwitchAt >= kDecoderOffCapMs) {
        logPrintf(LC_SCAN, LL_DEBUG, PSTR("decoder off after %lu ms"), (unsigned long)(now - gSwitchAt));
        gNeedDecoderOff = false;
        gSwitchAt = now;                                        // the TV5725 window starts now
    }
}

// VGA path showing a source read as RGBHV (mode 14 / 15) with composite sync or at 15 kHz (the bypass cannot show
// 15 kHz): the scan asks RGB SCART first (InputAuto::lowRateVga). A source the detection read as RGBS there (no
// VSync, composite sync on the HS pin: modes below 14) is scaled as on RGB SCART and stays on VGA
static bool vgaPathLooksScart() {
    return uopt->activeInputType == InputTypeVGA && rto->videoStandardInput >= 14 &&
           (rto->syncTypeCsync || !inputVgaLineRate((uint16_t)GBS::HPERIOD_IF::read()));
}

static void passesRestart();
static void nothingOn(uint8_t want, uint32_t now) {
    uint32_t ms = now - gSwitchAt;
    if (gAuto.firstRound())                                      // later rounds repeat it: left out
        logPrintf(LC_SCAN, LL_DEBUG, PSTR("%s: nothing (%lu.%lu s, %u passes)"), inputLongName(want),
                  (unsigned long)(ms / 1000), (unsigned long)(ms % 1000 / 100), gPasses);
    gAuto.pathResult(want, false, now);
}

static void tvRedetect(bool force);

static void probe(uint8_t want, uint32_t now) {
    uint32_t since = now - gSwitchAt;
    if (!inputIsDecoder(want)) {
        bool present = inputTvPathPresent(rto->boardHasPower, rto->sourceDisconnected, rto->videoStandardInput, rto->noSyncCounter);
        if (rto->sourceDisconnected || present) gSyncSince = 0;
        else if (!gSyncSince) gSyncSince = now ? now : 1;
        if (want == InputTypeVGA && !rto->sourceDisconnected && vgaPathLooksScart()) logVgaLook(gAuto.lowRateVga(now));
        else if (rto->sourceDisconnected && (inputTvPathDone(want, gPasses, gSogSeen) || since >= kTvPathCapMs))
            nothingOn(want, now);
        else if (gSyncSince && inputTvPathNoPicture(rto->sourceDisconnected, present, now - gSyncSince)) {
            // upstream would keep this sync in its recovery for about 50 s, the other inputs unsearched meanwhile
            logPrintf(LC_SCAN, LL_INFO, PSTR("%s: a sync and no picture for %lu s, searching on"), inputLongName(want),
                      (unsigned long)((now - gSyncSince) / 1000));
            gSyncSince = 0;
            nothingOn(want, now);
            if (gAuto.want() == want) {                          // the only input searched: detect on it from scratch
                tvRedetect(false);
                gSwitchAt = now;
                passesRestart();
            }
        }
        return;
    }
    const AdvSignal* g = freshReport();
    if (g && (g->flags & ADV_SIG_POWERED) && !gPoweredAt) gPoweredAt = now;
    bool over = gPoweredAt ? now - gPoweredAt >= kAdvLockMs : since >= kAdvCapMs;
    if (over) nothingOn(want, now);
}

static uint8_t gLastLevel = 0;          // the last G level read (logs)

static void listenSet(bool on) {
    if (gIdTermination) ADV_setListenTermination(on);          // an older ADV Controller image: the termination alone
    else advLinkSetListen(on);
    gListenOn = on;
}

static void identifyNewAcquisition() {
    if (gListenOn) listenSet(false);
    gId.newAcquisition();
    gIdNext = IdSample();
    gIdListenOff = false;
    gIdMovePending = false;   // gListenWasOn stays: the next switchTo reads it (a choice or a rescan comes first)
    gHint.reset();
}

static const char* idWhyName(uint8_t w) {
    switch (w) {
        case IW_PICTURE: return PSTR("picture");
        case IW_PICTURE_LISTENING: return PSTR("picture came back");
        case IW_RGB_WIRED: return PSTR("RGB wired");
        case IW_LEVEL_AMBIGUOUS: return PSTR("G level ambiguous");
        case IW_NO_LOCK: return PSTR("no lock on pin 20");
        case IW_DECODER_SILENT: return PSTR("decoder silent");
        case IW_TIMEOUT: return PSTR("no verdict in time");
        case IW_COMPOSITE: return PSTR("composite only");
        case IW_NO_BURST: return PSTR("a sync without colour on pin 20");
        case IW_BLACK: return PSTR("black on R/G/B and on pin 20");
        case IW_WIRES: return PSTR("content on the R/G/B wires");
        case IW_COMPOSITE_LAST: return PSTR("black on both, Composite as the last source");
        case IW_COMPOSITE_OFF: return PSTR("Composite is not in Auto search");
        case IW_SYNC_LOST: return PSTR("sync lost");
        case IW_CHOICE: return PSTR("choice");
        case IW_RESCAN: return PSTR("new scan");
        case IW_PAUSED: return PSTR("paused");
        default: return PSTR("?");
    }
}

static void identifyAct(uint8_t a, uint32_t now, uint32_t elapsed) {
    switch (a) {
        case IA_SAMPLE_RGB: {
            uint16_t c[3];
            tv5725ChannelContentLive(7, c);
            gIdNext.rgbMeasured = true;
            gIdNext.rgbContent = rjRgbContent(c);
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: R/G/B content %u/%u/%u"), c[0], c[1], c[2]);
            break;
        }
        case IA_SAMPLE_LEVEL: {
            uint16_t l[3], c[3];
            tv5725ChannelStatsLive(6, l, c);                       // G and B
            gIdNext.levelMeasured = true;
            gIdNext.gLevel = (uint8_t)(l[1] > 255 ? 255 : l[1]);
            gIdNext.blueContent = rjPictureContentListening(c);
            logPrintf(LC_INPUT, LL_DEBUG, PSTR("identify: G level %u, content G %u B %u"), l[1], c[1], c[2]);
            break;
        }
        case IA_PEEK_COMPOSITE: {
            // The composite picture: while the decoder listens the encoder already drives the TV5725's YPbPr input,
            // so the ADC input alone moves (no ADV switch, no STATE). Twice in a row for a picture, then the RGB
            // wires: the console's own picture may have come up meanwhile. With the wires flat the composite once
            // more: a picture there before and after is a source that drives pin 20 alone, not a fade or a cut
            uint16_t y[3] = {0, 0, 0}, c[3];
            uint16_t y1, y2 = 0, y3 = 0;
            const uint16_t yMin = RgbScartJudge::kPeekYMin;
            uint8_t sel = GBS::ADC_INPUT_SEL::read();
            GBS::ADC_INPUT_SEL::write(0);
            delay(kPeekSettleMs);
            tv5725ChannelContentLive(2, y);
            y1 = y[1];
            if (y1 >= yMin) {
                delay(kPeekAgainMs);
                tv5725ChannelContentLive(2, y);
                y2 = y[1];
            }
            GBS::ADC_INPUT_SEL::write(sel);
            delay(kPeekSettleMs);
            tv5725ChannelContentLive(7, c);
            uint8_t wires = rjWiresAfterLook(c);
            if (y2 >= yMin && wires == RW_FLAT) {
                GBS::ADC_INPUT_SEL::write(0);
                delay(kPeekSettleMs);
                tv5725ChannelContentLive(2, y);
                y3 = y[1];
                GBS::ADC_INPUT_SEL::write(sel);
            }
            gIdNext.peekMeasured = true;
            gIdNext.peekContent = y2 >= yMin && (wires != RW_FLAT || y3 >= yMin);
            gIdNext.peekWires = wires;
            gIdPassAt = millis();                                  // the next pass a whole step after the look
            logPrintf(LC_INPUT, y1 >= yMin || wires != RW_FLAT ? LL_INFO : LL_DEBUG,
                      PSTR("identify: composite Y content %u/%u/%u (%s), R/G/B in between %u/%u/%u (%s)"), y1, y2, y3,
                      gIdNext.peekContent ? "a picture" : "black", c[0], c[1], c[2],
                      wires == RW_FLAT ? "flat" : wires == RW_BLUE ? "a picture" : wires == RW_CONTENT ? "content on G" : "faint");
            break;
        }
        case IA_LISTEN_ON:
            listenSet(true);
            gListenOnAt = now;
            gPoweredSince = 0;
            gListenWasOn = true;
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: R/G/B flat, listening on pin 20%s"),
                      gIdTermination ? " (termination only)" : "");
            break;
        case IA_LISTEN_OFF:
            listenSet(false);
            gIdListenOff = true;
            gListenOffAt = now;
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: RGB SCART (%S, G %u) after %lu ms, listen off"),
                      idWhyName(gId.why()), gLastLevel, (unsigned long)elapsed);
            break;
        case IA_RELEASE_RGB: {
            uint8_t w = gId.why();
            if (gId.releaseForced()) {                             // the termination still raised G: start clean
                logPrintf(LC_INPUT, LL_INFO, PSTR("identify: listen still on at the release, detecting again"));
                gIdSelfRedetect = true;
                tvRedetect(true);
                gSwitchAt = now;                                   // the scan's RGB SCART window starts again
                passesRestart();
            }
            if (w == IW_PICTURE || w == IW_COMPOSITE_OFF)
                logPrintf(LC_INPUT, LL_INFO, PSTR("identify: RGB SCART (%S) after %lu ms"), idWhyName(w), (unsigned long)elapsed);
            else if (!gId.releaseForced() && gIdListenOff)     // not the cap before listen ever came on
                logPrintf(LC_INPUT, LL_INFO, PSTR("identify: termination off (%s, G %u), RGB SCART preset after %lu ms"),
                          gIdOffReport ? "decoder off report" : "G low", gLastLevel, (unsigned long)elapsed);
            if (w == IW_NO_LOCK || w == IW_DECODER_SILENT || w == IW_LEVEL_AMBIGUOUS || w == IW_TIMEOUT || w == IW_NO_BURST ||
                w == IW_BLACK)
                gHint.arm(now);
            break;
        }
        case IA_MOVE_COMPOSITE:
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: Composite (%S, G %u) after %lu ms"), idWhyName(gId.why()),
                      gLastLevel, (unsigned long)elapsed);
            gAuto.redirect(InputTypeAV, now);
            gJudgeMoved = true;
            gTickHold = true;                                      // the AV path rejoins: absence waits
            gTickHoldAt = now;
            gIdMovePending = true;                                 // listen stays on until the AV state is out
            break;
        case IA_ABANDON:
            listenSet(false);
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: abandoned (%S)"), idWhyName(gId.why()));
            tvRedetect(true);                                      // the scan's RGB SCART window starts again
            gSwitchAt = now;
            passesRestart();
            break;
        default: break;
    }
}

static uint32_t gIdStartedAt = 0;

// A find in Auto on S-Video/Composite (the decoder's locked rate) or RGB SCART (the scaler's 15 kHz mode, with the
// Force 60 remap undone): the source going 50 <-> 60 Hz is a likely new source (for example N64 PAL <-> Mega Drive
// NTSC on one SCART with no gap, the cables carry CVBS on pin 20): search again, with the identification
static RateWatch gRate;
static uint8_t gRateInput = 0;                // the input gRate watches
static uint8_t sourceRateHz(uint8_t active) {
    if (inputIsDecoder(active)) {
        if (!controllerReports()) return 0;
        const AdvSignal* g = freshReport();
        return g && (g->flags & ADV_SIG_PRESENT) ? g->hz : 0;
    }
    if (active != InputTypeRGBs || rto->sourceDisconnected || autoloadStageBusy(rto->applyPresetDoneStage)) return 0;
    uint8_t reg = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    return reg == 1 ? 60 : reg == 2 ? 50 : 0;
}
static void rateWatch(bool link, uint8_t active, uint32_t now) {
    if (!link || gAuto.state() != IA_FOUND || gAuto.choice() != 0 || active != gRateInput) {
        gRate.reset();
        gRateInput = active;
        return;
    }
    uint8_t hz = sourceRateHz(active);
    if (!gRate.feed(hz, now)) return;
    if (active == InputTypeRGBs && !rjPathMeasurable(rto->videoStandardInput, rto->outModeHdBypass)) {
        logPrintf(LC_INPUT, LL_INFO, PSTR("RGB SCART: the source changed to %u Hz on a bypass path (no RGB check): kept"),
                  (unsigned)hz);   // a 50/60 switch in pass-through would search on every flip
        return;
    }
    if (active == InputTypeRGBs) {
        uint16_t c[3];
        tv5725ChannelContent(7, c);
        if (rjRgbContent(c)) {   // a picture on the RGB wires: an RGB console (maybe a PAL-60 game), RGB SCART is right
            logPrintf(LC_INPUT, LL_INFO, PSTR("RGB SCART: the source changed to %u Hz with a picture on RGB (%u/%u/%u): kept"),
                      (unsigned)hz, (unsigned)c[0], (unsigned)c[1], (unsigned)c[2]);
            return;
        }
    }
    logPrintf(LC_INPUT, LL_INFO, PSTR("%s: the source changed to %u Hz, maybe another console: searching again"),
              inputLongName(active), (unsigned)hz);
    gAuto.sourceChanged(now);
}

// Auto on the Pro, a source found on RGB SCART or RGB SoG, VGA searched: when its sync returns from a gap the VGA
// connector is looked at (ReturnWatch); a source there (H and V sync) moves the scan to VGA, anything else leaves
// the found source alone. An HS alone with no sync on SCART pin 20 either is a VGA source that has not started its
// VS yet: looked at again, ReturnLooks::kLooks in all. A composite sync cable on that connector reads the same for
// ever: once a round ends that way, the returns that follow get one look (ReturnLooks)
static ReturnWatch gReturn;
static ReturnLooks gReturnLooks;
static uint32_t gReturnLookAt = 0;
static void returnLook(uint32_t now) {
    bool moved = inputAutoLookAtVga();                         // true: scanning on VGA now
    bool again = !moved && inputVgaReturnLookAgain(gAuto.found(), uopt->advSyncStripper != 0, gLook.hs, gLook.vs,
                                                   gLook.pathMeasured, gLook.pathVs);
    if (gReturnLooks.looked(moved, again)) gReturnLookAt = now + InputAuto::kVgaLookAgainMs;
}
static void returnWatch(bool link, uint8_t active, bool judging, uint32_t now) {
    if (!link || !gAuto.vgaReturnLookWanted() || gAuto.found() != active || rto->sourceDisconnected) {
        gReturn.reset();
        gReturnLooks.reset();
        return;
    }
    if (judging || advLinkChecking() || advLinkController().status != ADV_CONTROLLER_OK) {   // no look now
        gReturnLooks.cancel();
        return;
    }
    if (gReturnLooks.pending()) {
        if ((int32_t)(now - gReturnLookAt) >= 0) returnLook(now);
        return;
    }
    bool sync = rto->noSyncCounter < 2;                         // one missed check is noise (as LogSyncReturn)
    uint8_t mode = rto->videoStandardInput == 1 || rto->videoStandardInput == 2 ? rto->videoStandardInput : 0;
    if (!gReturn.feed(sync, mode, now)) return;
    logPrintf(LC_INPUT, LL_INFO, PSTR("%s: sync back after a %lu.%lu s gap, looking at the VGA connector"), inputLongName(active),
              (unsigned long)(gReturn.gapMs() / 1000), (unsigned long)(gReturn.gapMs() % 1000 / 100));
    gReturnLooks.begin();
    returnLook(now);
}

// One pass of the identification, every kIdPassMs while it runs
static void identifyLoop(uint32_t now) {
    if (gId.phase() == ID_DONE && gAuto.state() == IA_SCANNING && rto->sourceDisconnected) {
        if (gIdSelfRedetect) { /* our own re-detection after a forced release: the next find loads the preset */ }
        else identifyNewAcquisition();                          // the source went before its preset: identify again
    }
    if (!rto->sourceDisconnected) gIdSelfRedetect = false;
    if (gId.phase() == ID_IDLE) {
        if (!identifyStartable() || gIdSelfRedetect) return;
        if (advLinkChecking()) return;                         // the boot check decides listen or termination only
        if (advLinkController().status != ADV_CONTROLLER_OK) {  // no STATE would go out: show RGB SCART as found
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: skipped, the ADV Controller does not answer (RGB SCART as found)"));
            gId.skip(IW_DECODER_SILENT);
            return;
        }
        gIdTermination = !advLinkCanListen();
        gId.begin(now, !gIdTermination, (inputAutoScanned(board()) & (1 << (InputTypeAV - 1))) != 0,
                  uopt->inputLast == InputTypeAV);
        gIdNext = IdSample();
        gIdPassAt = 0;
        gIdStartedAt = now;
        gListenOnAt = gPoweredSince = gListenOffAt = 0;
        gIdOffReport = false;
        gLastReportAt = advLinkSignalAtMs();
        logPrintf(LC_INPUT, LL_INFO, PSTR("identify: sync on RGB SCART%s"),
                  gIdTermination ? ", old ADV Controller image: termination only" : "");
    }
    if (!gId.holdsSyncWatcher()) return;
    if (gIdPassAt && now - gIdPassAt < kIdPassMs) return;
    gIdPassAt = now;
    IdSample s = gIdNext;
    gIdNext = IdSample();
    s.sync = getStatus16SpHsStable();
    if (s.levelMeasured) gLastLevel = s.gLevel;
    s.listenSent = gListenOnAt != 0;
    uint32_t sent = advLinkSwitchSentAt();
    bool sentAfterOn = gListenOnAt && (int32_t)(sent - gListenOnAt) >= 0;
    s.sinceListenMs = sentAfterOn ? now - sent : 0;
    s.switchesApplied = gId.phase() == ID_RELEASING || (sentAfterOn && s.sinceListenMs >= InputIdentify::kSwitchSettleMs);
    const AdvSignal* g = advLinkSignal();
    uint32_t at = advLinkSignalAtMs();
    if (g && at != gLastReportAt && gListenOnAt && (int32_t)(at - gListenOnAt) > 0) {
        gLastReportAt = at;
        s.reportFresh = true;
        s.lock = (g->flags & ADV_SIG_LOCK) != 0;
        s.freeRun = (g->flags & ADV_SIG_FREERUN) != 0;
        s.burst = (g->flags & ADV_SIG_FSC) != 0;
        if ((g->flags & ADV_SIG_POWERED) && !gPoweredSince) gPoweredSince = now;
    }
    s.sincePoweredMs = gPoweredSince ? now - gPoweredSince : 0;
    // RELEASING: the ADV Controller applies the switches (the termination) before it powers the decoder down, so a
    // report received after the listen-off STATE that no longer says POWERED means G is the console's own (a picture
    // keeps it high: no wait for it to fall, no re-detection)
    uint32_t offSent = advLinkSwitchSentAt();
    if (gId.phase() == ID_RELEASING && g && !gIdTermination && gPoweredSince && gListenOffAt &&
        (int32_t)(offSent - gListenOffAt) >= 0 && (int32_t)(at - offSent) > 0 && !(g->flags & ADV_SIG_POWERED)) {
        s.listenOffConfirmed = true;
        gIdOffReport = true;
    }
    identifyAct(gId.step(s, now), now, now - gIdStartedAt);
}

// RGB SCART fallback verdicts on a black picture: the "try Composite" hint after 20 s (sampled in place)
static void hintLoop(uint32_t now) {
    if (gAuto.state() != IA_FOUND || gAuto.found() != InputTypeRGBs || uopt->activeInputType != InputTypeRGBs) return;
    if (!gHint.sampleDue(now) || !rto->boardHasPower || rto->sourceDisconnected ||
        autoloadStageBusy(rto->applyPresetDoneStage) || !rjPathMeasurable(rto->videoStandardInput, rto->outModeHdBypass))
        return;
    uint16_t c[3];
    tv5725ChannelContent(7, c);
    uint8_t r = gHint.sampled(now, rjHintSeesPicture(c));
    logPrintf(LC_INPUT, LL_DEBUG, PSTR("identify: hint sample, R/G/B content %u/%u/%u (%s)"), c[0], c[1], c[2],
              r == RH_DONE ? "a picture, no hint" : r == RH_SHOW ? "black, hint" : "black");
    if (r == RH_SHOW) {
        logPrintf(LC_INPUT, LL_INFO, PSTR("identify: RGB SCART still black after 20 s (R/G/B content %u/%u/%u), hint: try Composite"),
                  c[0], c[1], c[2]);
        menuNotice(S_T_TRY_COMPOSITE);
    }
}

void inputAutoLoop() {
    if (!gBegun) return;
    uint32_t now = millis();
    static bool first = true;
    if (first) { first = false; gSwitchAt = now; }              // the hinted path's window starts when the loop does
    bool link = platformLinkHardware();
    if (updateInProgress() || advLinkUpdateActive() || isInfoDisplayActive) {   // uploads; no-signal free-run session
        if (gId.holdsSyncWatcher()) {                          // the syncwatcher stays held until the pause ends
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: abandoned (paused)"));
            listenSet(false);
            gId.abandon(IW_PAUSED);
            gIdPaused = true;
        }
        advLinkSignalPollFast(false);
        return;
    }
    if (gIdPaused) {                                           // the pause ended: detect again, identify once more
        gIdPaused = false;
        if (!gIdPauseRedone) { gIdPauseRedone = true; tvRedetect(true); gSwitchAt = now; passesRestart(); }
        else gId.newAcquisition();
    }
    uint8_t active = uopt->activeInputType;
    bool present = link && inputIsDecoder(active)
                       ? inputAdvSourcePresent()
                       : inputTvPathPresent(rto->boardHasPower, rto->sourceDisconnected, rto->videoStandardInput,
                                            rto->noSyncCounter);
    if (link && sogFindPending()) {                            // a find before its preset (see sogFindPending)
        logPrintf(LC_INPUT, LL_INFO, PSTR("identify: sync on RGB SoG, looking at RGB SCART before its preset"));
        present = true;
    }
    if (present && link && active == InputTypeVGA && gAuto.state() == IA_SCANNING && vgaPathLooksScart()) {
        bool wasVga = gAuto.want() == InputTypeVGA;              // a new scan may still sit on the old VGA source
        bool waits = gAuto.lowRateVga(now);                      // not a find yet: RGB SCART gets a look first
        if (wasVga) logVgaLook(waits);
        if (waits) present = false;
    }
    gAuto.setBoard(board(), now);                               // Auto search changed: a running scan restarts
    gAuto.setAbsentMs((uopt->inputLostWait ? uopt->inputLostWait : 2) * 1000UL);   // input.lost_wait
    if (link) { identifyLoop(now); hintLoop(now); }
    bool judging = gId.holdsSyncWatcher();                    // the identification owns the input meanwhile
    if (gTickHold && now - gTickHoldAt >= kRejoinMs) gTickHold = false;
    uint8_t was = gAuto.state();
    if (!inputAutoSourceCounts(was, active, gAuto.want())) present = false;   // still on the previous input
    if (!judging && (present || !gTickHold)) gAuto.tick(present, now);   // the hold only delays absence
    rateWatch(link, active, now);
    returnWatch(link, active, judging, now);
    active = uopt->activeInputType;                             // a look that found the source moved to VGA
    if (link && was != IA_SCANNING && gAuto.state() == IA_SCANNING) {    // the source went: search from scratch
        tvRedetect(false);
        gSwitchAt = now;
    }
    if (gAuto.state() == IA_SCANNING && gAuto.scanStartedAt() != gScanAt) {   // any new scan, also on the same path
        gScanAt = gAuto.scanStartedAt();
        if (gId.holdsSyncWatcher()) {
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: abandoned (new scan)"));
            gId.abandon(IW_RESCAN);
        }
        identifyNewAcquisition();
        gIdPauseRedone = false;
        gSwitchAt = now;
        passesRestart();
    }
    uint8_t want = gAuto.want();
    if (judging) {
        gSyncSince = 0;   // the identification owns the input: no path switch, no absence, its own time limits
    } else if (link) {
        if (want != active) switchTo(want, now);
        else if (gNeedDecoderOff) decoderOffStep(now);           // Component after S-Video/Composite, scan or manual
        else if (gAuto.state() == IA_SCANNING) probe(want, now);
    } else if (gAuto.state() == IA_MANUAL) {
        if (want != active) switchTo(want, now);                // no link: a manual choice switches to its input
    } else if (present) {                                       // the detection alternated its inputs and found this
        uint8_t t = rto->inputIsYpBpR ? InputTypeYUV : rto->videoStandardInput >= 14 ? InputTypeVGA : InputTypeRGBs;
        inputAdoptDetected(t);
        gAuto.redirect(t, now);
    }
    if (gAuto.choice() == 0 && uopt->inputChoice != 0) {        // a manual choice returned to Auto by itself
        uopt->inputChoice = 0;
        settingsBroadcastChange((uint8_t)SID_input_source);     // not saved: every start is Auto anyway
    }
    advLinkSignalPollFast(link && (gAuto.state() == IA_SCANNING || inputIsDecoder(uopt->activeInputType)));
    if (gAuto.takeHintChanged() && gAuto.found() && gAuto.found() != uopt->inputLast) {
        uopt->inputLast = gAuto.found();                        // the scan hint (input.last), once per change
        saveUserPrefs();
        logPrintf(LC_INPUT, LL_DEBUG, PSTR("input.last = %s"), inputLongName(uopt->inputLast));
    }
    logTransitions(link, now);
}
