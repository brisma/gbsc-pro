// Auto-load hook (device only): sampled every 20 ms from loop()
#include <Arduino.h>
#include "../../options.h"
#include "../gbs-control-pro.h"
#include "../platform/platform-caps.h"
#include "profile-autoload.h"
#include "profile-modes.h"
#include "profile-store.h"
#include "../ui/menu-device.h"
#include "../update/update-device.h"
#include "../settings/settings-device.h"

extern userOptions* uopt;
extern runTimeOptions* rto;
void settingsBroadcastStatusText(const char* key, const char* value);
#include "../log/log.h"
#include "../input/input-types.h"

static StableLatch gLatch;
static bool gPending = false;
static bool gWasOn = false;
static uint16_t gChoiceAtArm = 0;   // profileChoiceSeq() while the latch counts the new signal
static AutoloadNotice gNotice;      // the queued load, announced on the web and in the log once it ran

// The pop-up gate reads the scaler on its own clock: a sample the latch has not seen yet (a format change) is
// not decided, so the pop-up does not name the old profile for the 2 s the latch counts
bool profileAutoloadIdleFor(uint16_t lines) {
    AutoloadSample s;
    s.input = uopt->activeInputType;
    s.mode = profileSourceModeOf(profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60), s.input, uopt->advI2P);
    s.lines = lines;
    s.signal = true;
    s.stageBusy = false;
    return gLatch.decidedFor(s) && !gPending;
}

// /api/profiles "match": the profiles whose fingerprint matches the current signal, refreshed once a second here
// (the network handler must not read the scaler); none without a signal, while a preset applies or an update runs
static uint64_t gMatchBits = 0;
uint64_t profileMatchBits() { return gMatchBits; }

// The source's field rate, measured once per signal (each measurement waits for two or three fields). A failed
// measurement is tried again after 5 s; a signal that goes or changes drops it
static struct {
    bool valid, interlaced;
    uint8_t input, mode;
    uint16_t lines;
    uint32_t milliHz, at;
} gRate;

static bool rateHolds(uint8_t input, uint8_t mode, uint16_t lines, bool interlaced) {
    return gRate.valid && gRate.input == input && gRate.mode == mode && gRate.interlaced == interlaced &&
           abs((int)gRate.lines - (int)lines) <= 4;
}

uint32_t profileSignalMilliHzKnown(uint8_t input, uint8_t mode, uint16_t lines, bool interlaced) {
    return rateHolds(input, mode, lines, interlaced) ? gRate.milliHz : 0;
}

static uint32_t signalRate(uint8_t input, uint8_t mode, uint16_t lines, bool interlaced, bool fresh) {
    bool held = rateHolds(input, mode, lines, interlaced);
    if (fresh || !held || (!gRate.milliHz && millis() - gRate.at >= 5000)) {
        gRate.valid = true;
        gRate.input = input; gRate.mode = mode; gRate.lines = lines; gRate.interlaced = interlaced;
        gRate.milliHz = profileSourceMilliHz();
        gRate.at = millis();
        logPrintf(LC_PROFILE, LL_DEBUG, PSTR("source rate %lu.%03lu Hz (%u lines%s)"), (unsigned long)(gRate.milliHz / 1000),
                  (unsigned long)(gRate.milliHz % 1000), (unsigned)lines, interlaced ? ", interlaced" : "");
    }
    return gRate.milliHz;
}

static void profileMatchTick() {
    static uint32_t last = 0;
    if (millis() - last < 1000) return;
    last = millis();
    uint64_t bits = 0;
    if (!updateInProgress() && profileSignalPresent() && !autoloadStageBusy(rto->applyPresetDoneStage)) {
        uint8_t input = uopt->activeInputType;
        uint8_t mode = profileSourceModeOf(profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60), input, uopt->advI2P);
        uint16_t lines = profileSourceLines();
        bool inter = profileIsInterlaced(rto->videoStandardInput, lines);
        uint8_t idx[PROFILE_MAX];
        uint8_t n = profileMatches(profileList(), input, mode, lines, inter, idx, PROFILE_MAX,
                                   signalRate(input, mode, lines, inter, false));
        for (uint8_t i = 0; i < n; i++) bits |= 1ull << idx[i];
    } else if (!profileSignalPresent()) {
        gRate.valid = false;
    }
    if (bits != gMatchBits)
        logPrintf(LC_PROFILE, LL_DEBUG, PSTR("profiles matching the signal: %u (mask %08lx%08lx)"), (unsigned)__builtin_popcountll(bits),
                  (unsigned long)(bits >> 32), (unsigned long)bits);
    gMatchBits = bits;
}

// The web toast and the log line follow the load's result, not its queuing
static void noticeTick() {
    if (settingsBackupBusy()) { gNotice.hold(millis()); return; }   // the queue waits for the backup: so does the notice
    char res[24] = "";
    bool found = gNotice.waiting() && profileOpResultFor(gNotice.seq(), res, sizeof(res));
    switch (gNotice.poll(found, res, millis())) {
        case AutoloadNotice::AN_LOADED: {
            char msg[2 * PROFILE_LABEL_BUF + 2];
            const char* old = gNotice.discarded();
            if (old[0]) snprintf(msg, sizeof(msg), "%s|%s", gNotice.label(), old);   // '|' is never in a profile name
            settingsBroadcastStatusText("status.profile_autoload", old[0] ? msg : gNotice.label());
            if (old[0]) logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: %s (unsaved changes of %s discarded)"), gNotice.label(), old);
            else logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: %s"), gNotice.label());
            break;
        }
        case AutoloadNotice::AN_FAILED:
            logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: %s failed (%s), nothing discarded"), gNotice.label(), gNotice.result());
            break;
        case AutoloadNotice::AN_TIMEOUT:
            logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: no result for %s after %u s"), gNotice.label(),
                      (unsigned)(AutoloadNotice::kGiveUpMs / 1000));
            break;
        default: break;
    }
}

void profileAutoloadTick()
{
    profileMatchTick();
    noticeTick();
    if (!uopt->profilesAutoload) { gWasOn = false; gPending = false; return; }
    if (!gWasOn) { gLatch.reset(); gWasOn = true; gChoiceAtArm = profileChoiceSeq(); }
    static uint32_t last = 0;
    if (millis() - last < 20) return;
    last = millis();

    AutoloadSample s;
    s.input = uopt->activeInputType;
    s.signal = profileSignalPresent();
    s.stageBusy = autoloadStageBusy(rto->applyPresetDoneStage);
    uint8_t reg = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    s.mode = profileSourceModeOf(reg, s.input, uopt->advI2P);
    s.lines = (s.signal && !s.stageBusy) ? profileSourceLines() : 0;   // no I2C read when it is not used
    bool wasFired = gLatch.fired();
    if (gLatch.feed(s)) gPending = true;
    if (wasFired && !gLatch.fired()) {   // why a decided signal is decided again (a real change, or not)
        gChoiceAtArm = profileChoiceSeq();   // the signal changed: a Load or Defaults by hand from here wins
        logPrintf(LC_PROFILE, LL_DEBUG, PSTR("auto-load: re-armed (signal %u, input %u, mode %u, lines %u, stage %u)"),
                  (unsigned)s.signal, (unsigned)s.input, (unsigned)s.mode, (unsigned)s.lines,
                  (unsigned)rto->applyPresetDoneStage);
    }
    if (gLatch.parityRearmed())
        logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: the signal turned %s (%u lines), deciding again"),
                  profileIsInterlaced(s.mode, s.lines) ? "interlaced" : "progressive", (unsigned)s.lines);
    if (!gLatch.fired()) gPending = false;   // re-armed while a menu was open: wait for the new anchor
    if (!gPending) return;

    // Never while a menu or an overlay is open (OSD and OLED share one engine), nor on a sample of a preset still
    // being applied or of another signal than the anchor (a stale mode could decide Defaults)
    if (menuUiBusy() || s.stageBusy || !gLatch.decidedFor(s)) return;
    if (profileChoiceSeq() != gChoiceAtArm) {   // Load or Defaults by hand since the signal changed: that choice wins
        logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: a profile was chosen by hand for this signal, kept"));
        gPending = false;
        return;
    }

    int cand = -1;
    bool inter = gLatch.interlaced(rto->videoStandardInput);
    uint32_t rate = signalRate(s.input, s.mode, gLatch.anchorLines(), inter, true);   // measured for this decision
    AutoloadDecision d = profileAutoloadMatch(profileList(), s.input, s.mode, gLatch.anchorLines(), inter,
                                              profileActiveIndex(), profileModifiedNow(), &cand, reg, rate);
    char label[PROFILE_LABEL_BUF] = "";
    if (cand >= 0) profileLabel(profileList().at((uint8_t)cand).name, profileList().at((uint8_t)cand).region, label,
                                sizeof(label));   // "Mega Drive PAL": two profiles may share a name across regions
    if (d == AL_DEFAULTS) snprintf(label, sizeof(label), "Defaults");   // no profile for a signal foreign to the active one
    if (d == AL_LOAD || d == AL_DEFAULTS) {
        char old[PROFILE_LABEL_BUF] = "";                               // whose unsaved changes the load drops
        int a = profileActiveIndex();
        if (a >= 0 && profileModifiedNow())
            profileLabel(profileList().at((uint8_t)a).name, profileList().at((uint8_t)a).region, old, sizeof(old));
        if (!profileQueueOp(d == AL_LOAD ? POP_LOAD : POP_DEFAULTS, d == AL_LOAD ? (int8_t)cand : (int8_t)-1, nullptr))
            return;                                                     // queue full: retry next tick
        if (d == AL_DEFAULTS && a >= 0 && profileList().at((uint8_t)a).fpInput == s.input) {
            uint32_t own = profileList().at((uint8_t)a).fpMilliHz;
            logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: another console on %s (%lu.%03lu Hz, the active profile's runs at "
                                                "%lu.%03lu Hz) and no profile for it"),
                      inputLongName(s.input), (unsigned long)(rate / 1000), (unsigned long)(rate % 1000),
                      (unsigned long)(own / 1000), (unsigned long)(own % 1000));
        } else if (d == AL_DEFAULTS)
            logPrintf(LC_PROFILE, LL_INFO, PSTR("auto-load: no profile for this signal on %s, the active one is for another input"),
                      inputLongName(s.input));
        menuNotifyAutoload(label, old[0] ? old : nullptr);             // toast after the load result
        gNotice.start((uint16_t)(profileOpResultSeq() + profileOpsPending()), label, old, millis());   // its op is the last queued
        logPrintf(LC_PROFILE, LL_DEBUG, PSTR("auto-load: queued %s"), label);
    } else if (d == AL_ALREADY_ACTIVE) {
        logPrintf(LC_PROFILE, LL_DEBUG, PSTR("auto-load: the matching profile is already active%s"),
                  profileModifiedNow() ? " (unsaved changes kept)" : "");
    } else if (d == AL_NONE) {
        logPrintf(LC_PROFILE, LL_DEBUG, PSTR("auto-load: no profile for this signal on %s"), inputLongName(s.input));
    }
    gPending = false;   // once per signal change
}
