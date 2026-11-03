#include <Arduino.h>
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
        case IA_WHY_BOOT: return "boot";
        case IA_WHY_LOST: return "source lost";
        case IA_WHY_CHOSEN: return "Auto chosen";
        case IA_WHY_BOARD: return "Auto search changed";
        case IA_WHY_RETURN: return "source back after a gap";
        case IA_WHY_FORMAT: return "the source changed 50/60 Hz";
        default: return "back to Auto";
    }
}

static void logScanStart(bool link) {
    if (gAuto.scanReason() == IA_WHY_BACK_TO_AUTO && gLogManual)
        logPrintf(LC_INPUT, LL_INFO, PSTR("back to Auto (no signal on %s)"), inputLongName(gLogManual));
    if (!link) { logPrintf(LC_SCAN, LL_INFO, PSTR("start (%s)"), whyName(gAuto.scanReason())); return; }
    uint8_t order[6];
    uint8_t n = gAuto.scanPaths(order, 6);
    char list[72];
    size_t k = 0;
    list[0] = '\0';
    for (uint8_t i = 0; i < n && k < sizeof(list); i++)
        k += (size_t)snprintf(list + k, sizeof(list) - k, "%s%s", i ? " > " : "", inputLongName(order[i]));
    logPrintf(LC_SCAN, LL_INFO, PSTR("start (%s): %s"), whyName(gAuto.scanReason()), list);
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
static void tvRedetect(bool force = false) {
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

// VGA path showing a 15 kHz source or composite sync: a SCART console on the shared RGB wires (the TV5725 takes it
// as "RGB/HV (CSync)", where the line period reading is not reliable), or a real 15 kHz / csync RGBHV source on the
// VGA connector (Amiga, arcade board); the scan asks RGB SCART first (InputAuto::lowRateVga)
static bool vgaPathLooksScart() {
    return uopt->activeInputType == InputTypeVGA &&
           (rto->syncTypeCsync || !inputVgaLineRate((uint16_t)GBS::HPERIOD_IF::read()));
}

static void nothingOn(uint8_t want, uint32_t now) {
    uint32_t ms = now - gSwitchAt;
    if (gAuto.firstRound())                                      // later rounds repeat it: left out
        logPrintf(LC_SCAN, LL_DEBUG, PSTR("%s: nothing (%lu.%lu s, %u passes)"), inputLongName(want),
                  (unsigned long)(ms / 1000), (unsigned long)(ms % 1000 / 100), gPasses);
    gAuto.pathResult(want, false, now);
}

static void probe(uint8_t want, uint32_t now) {
    uint32_t since = now - gSwitchAt;
    if (!inputIsDecoder(want)) {
        if (want == InputTypeVGA && !rto->sourceDisconnected && vgaPathLooksScart()) logVgaLook(gAuto.lowRateVga(now));
        else if (rto->sourceDisconnected && (inputTvPathDone(want, gPasses, gSogSeen) || since >= kTvPathCapMs))
            nothingOn(want, now);
        return;
    }
    const AdvSignal* g = freshReport();
    if (g && (g->flags & ADV_SIG_POWERED) && !gPoweredAt) gPoweredAt = now;
    bool over = gPoweredAt ? now - gPoweredAt >= kAdvLockMs : since >= kAdvCapMs;
    if (over) nothingOn(want, now);
}

static uint8_t gLastLevel = 0;          // the last G level read (logs)

static void listenSet(bool on) {
    if (gIdTermination) ADV_setSyncTermination(on);          // an older ADV Controller image: the termination alone
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
        case IW_PICTURE: return "picture";
        case IW_PICTURE_LISTENING: return "picture came back";
        case IW_RGB_WIRED: return "RGB wired";
        case IW_LEVEL_AMBIGUOUS: return "G level ambiguous";
        case IW_NO_LOCK: return "no lock on pin 20";
        case IW_DECODER_SILENT: return "decoder silent";
        case IW_TIMEOUT: return "no verdict in time";
        case IW_COMPOSITE: return "composite only";
        case IW_SYNC_LOST: return "sync lost";
        case IW_CHOICE: return "choice";
        case IW_RESCAN: return "new scan";
        case IW_PAUSED: return "paused";
        default: return "?";
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
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: RGB SCART (%s, G %u) after %lu ms, listen off"),
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
            if (w == IW_PICTURE)
                logPrintf(LC_INPUT, LL_INFO, PSTR("identify: RGB SCART (picture) after %lu ms"), (unsigned long)elapsed);
            else if (!gId.releaseForced() && gIdListenOff)     // not the cap before listen ever came on
                logPrintf(LC_INPUT, LL_INFO, PSTR("identify: termination off (%s, G %u), RGB SCART preset after %lu ms"),
                          gIdOffReport ? "decoder off report" : "G low", gLastLevel, (unsigned long)elapsed);
            if (w == IW_NO_LOCK || w == IW_DECODER_SILENT || w == IW_LEVEL_AMBIGUOUS || w == IW_TIMEOUT) gHint.arm(now);
            break;
        }
        case IA_MOVE_COMPOSITE:
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: Composite (%s, G %u) after %lu ms"), idWhyName(gId.why()),
                      gLastLevel, (unsigned long)elapsed);
            gAuto.redirect(InputTypeAV, now);
            gJudgeMoved = true;
            gTickHold = true;                                      // the AV path rejoins: absence waits
            gTickHoldAt = now;
            gIdMovePending = true;                                 // listen stays on until the AV state is out
            break;
        case IA_ABANDON:
            listenSet(false);
            logPrintf(LC_INPUT, LL_INFO, PSTR("identify: abandoned (%s)"), idWhyName(gId.why()));
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
        gId.begin(now, !gIdTermination);
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
    if (gHint.sampled(now, rjRgbContent(c)) == RH_SHOW) {
        logPrintf(LC_INPUT, LL_INFO, PSTR("identify: RGB SCART still black after 20 s, hint: try Composite"));
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
    if (link && was != IA_SCANNING && gAuto.state() == IA_SCANNING) {    // the source went: search from scratch
        tvRedetect();
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
        // the identification owns the input: no path switch, no absence
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
