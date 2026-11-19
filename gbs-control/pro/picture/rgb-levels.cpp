#include <Arduino.h>
#include "../../options.h"
#include "../../tv5725.h"
#include "../gbs-control-pro.h"
#include "../settings/settings-registry.h"
#include "../settings/settings-device.h"
#include "../api/api-device.h"
#include "../profiles/profile-store.h"
#include "../ui/menu-device.h"
#include "../ui/menu-strings.h"
#include "../log/log.h"
#include "rgb-levels-math.h"
#include "rgb-levels.h"

typedef TV5725<GBS_ADDR> GBS;
extern userOptions* uopt;
extern runTimeOptions* rto;
extern adcOptions* adco;
void handleWiFi(boolean instant);
void setAdcGain(uint8_t gain);
void settingsBroadcastStatusText(const char* key, const char* value);

enum RgbLevelsJob : uint8_t { JOB_NONE, JOB_BLACK, JOB_WHITE };
static const uint8_t kSamples = 160;         // test bus reads per look, taken at random points of the picture
static_assert(kSamples <= 255, "counts are one byte per value");
static const uint32_t kSettleMs = 150;       // a register change takes a few frames to reach the input stage
static const uint32_t kLookGapMs = 100;
static const uint8_t kMaxLooks = 20;         // looks for two readings in a row within 1 code
static const uint32_t kJobMs = 15000;        // the whole job, waits for a stable signal included
static const uint8_t kStableCount = 20;      // continousStableCounter before a look (auto gain waits for 90)

static struct {
    RgbLevelsJob job;
    uint8_t input;                 // the input the job started on: another one ends it
    uint8_t round, looks;
    uint32_t start, at;            // job start; next look not before
    float prevY, firstY;           // last look (-1000: none), the reading before any change
    int16_t off;                   // working offset steps (black)
    RgbLevelsWhite white;          // working gain (white)
    uint8_t offs[3], gains[3];     // the registers at the start, back on a failure
} gJob = {JOB_NONE, 0, 0, 0, 0, 0, -1000.0f, -1.0f, 0, {0, -1, -1.0f}, {0, 0, 0}, {0, 0, 0}};

bool rgbLevelsActive() { return gJob.job != JOB_NONE; }

// The offsets the steps count from: the boot calibration, or the preset default every built-in preset writes
// (setAdcParametersGainAndOffset). false: no boot calibration on a profile's register dump, nothing to count from.
static bool baseOffsets(uint8_t b[3]) {
    if (adco->r_off && adco->g_off && adco->b_off) { b[0] = adco->r_off; b[1] = adco->g_off; b[2] = adco->b_off; return true; }
    if (rto->isCustomPreset) return false;
    b[0] = b[1] = b[2] = 0x40;
    return true;
}

static void writeOffsets(const uint8_t b[3], int16_t steps) {
    GBS::ADC_ROFCTRL::write((uint8_t)constrain(b[0] + steps, 0, 255));
    GBS::ADC_GOFCTRL::write((uint8_t)constrain(b[1] + steps, 0, 255));
    GBS::ADC_BOFCTRL::write((uint8_t)constrain(b[2] + steps, 0, 255));
}

static void writeGains(uint8_t r, uint8_t g, uint8_t b) {
    GBS::ADC_RGCTRL::write(r);
    GBS::ADC_GGCTRL::write(g);
    GBS::ADC_BGCTRL::write(b);
}

static void writeWorking() {
    uint8_t b[3];
    if (baseOffsets(b)) writeOffsets(b, gJob.off);
    if (gJob.job == JOB_WHITE) writeGains((uint8_t)gJob.white.gain, (uint8_t)gJob.white.gain, (uint8_t)gJob.white.gain);
}

void rgbLevelsApplyAdc() {
    if (rgbLevelsActive()) { writeWorking(); return; }
    if (!rgbLevelsInput(uopt->activeInputType)) return;
    if (GBS::IF_AUTO_OFST_EN::read()) return;   // dev.if_auto_offset owns the offsets while it is on
    uint8_t b[3];
    if (baseOffsets(b)) writeOffsets(b, rgbLevelsStepsFromSetting(uopt->rgbBlack));   // 128: the base itself
}

// One look: the most common input-stage Y of the screen (IF_TEST_SEL 8 on test bus 0: 0x2F = Y), blanking left out.
// The test bus and the debug pin are put back before the pass ends.
static float lookY() {
    uint8_t counts[256];
    memset(counts, 0, sizeof(counts));
    const uint8_t sel = GBS::TEST_BUS_SEL::read(), en = GBS::TEST_BUS_EN::read(), ifSel = GBS::IF_TEST_SEL::read(),
                  ifEn = GBS::IF_TEST_EN::read(), pin = GBS::PAD_BOUT_EN::read();
    GBS::PAD_BOUT_EN::write(0);   // as auto gain: the test bus off the debug pin while it is borrowed
    GBS::TEST_BUS_SEL::write(0x00);
    GBS::TEST_BUS_EN::write(1);
    GBS::IF_TEST_SEL::write(8);
    GBS::IF_TEST_EN::write(1);
    uint16_t used = 0;
    for (uint8_t i = 0; i < kSamples; i++) {
        const uint8_t y = GBS::TEST_BUS::read() >> 8;
        if (y) { counts[y]++; used++; }
        if ((i & 31) == 31) handleWiFi(0);
    }
    GBS::IF_TEST_EN::write(ifEn);
    GBS::IF_TEST_SEL::write(ifSel);
    GBS::TEST_BUS_EN::write(en);
    GBS::TEST_BUS_SEL::write(sel);
    GBS::PAD_BOUT_EN::write(pin);
    return rgbLevelsScreenY(counts, used);
}

static void status(const char* text) {
    settingsBroadcastStatusText("status.rgb_levels", text);
    logPrintf(LC_INPUT, LL_INFO, PSTR("RGB levels: %s"), text);
}

static void statusP(PGM_P text) {   // the fixed texts stay in flash
    char t[72];
    strncpy_P(t, text, sizeof(t) - 1);
    t[sizeof(t) - 1] = 0;
    status(t);
}

// A profile setting changed from here: saved with the profile and shown on the web page, as a write from the menu
static void putProfileSetting(uint8_t id, uint8_t& field, uint8_t value) {
    if (field == value) return;
    field = value;
    settingsMarkDirty(SettingScope::Profile);
    settingsBroadcastChange(id);
}

static void finish(bool ok, uint16_t toast, PGM_P text, const char* doneText = nullptr) {
    const RgbLevelsJob job = gJob.job;
    gJob.job = JOB_NONE;
    if (ok && job == JOB_BLACK) {
        putProfileSetting((uint8_t)SID_input_rgb_black, uopt->rgbBlack, rgbLevelsSettingFromSteps(gJob.off));
    } else if (ok && job == JOB_WHITE) {
        putProfileSetting((uint8_t)SID_input_auto_gain, uopt->enableAutoGain, 0);   // as the Gain stepper
        setAdcGain((uint8_t)gJob.white.gain);   // adco: kept across preset loads; the profile saves it as adc_gain
        profileRegsTouched();
    } else {
        if (job == JOB_WHITE) writeGains(gJob.gains[0], gJob.gains[1], gJob.gains[2]);   // black never moves them
        uint8_t b[3];
        if (baseOffsets(b)) {
            rgbLevelsApplyAdc();   // the saved level on the current preset (one may have loaded meanwhile)
        } else {
            GBS::ADC_ROFCTRL::write(gJob.offs[0]);
            GBS::ADC_GOFCTRL::write(gJob.offs[1]);
            GBS::ADC_BOFCTRL::write(gJob.offs[2]);
        }
    }
    menuNotice(toast);
    if (doneText) status(doneText); else statusP(text);
}

void rgbLevelsStart(bool white) {
    uint8_t b[3];
    if (rgbLevelsActive()) { statusP(PSTR("busy: a level is being set")); return; }
    if (!rgbLevelsInput(uopt->activeInputType)) { menuNotice(S_T_CAL_FAILED); statusP(PSTR("failed: select RGBs or RGsB")); return; }
    if (rto->sourceDisconnected) { menuNotice(S_T_NO_SIGNAL); statusP(PSTR("failed: no signal")); return; }
    if (!rto->syncWatcherEnabled) {   // the job waits for its stable count
        menuNotice(S_T_CAL_FAILED);
        statusP(PSTR("failed: needs the sync watcher on"));
        return;
    }
    if (rto->outModeHdBypass || !baseOffsets(b)) {   // a pass-through, or a register dump without the boot calibration
        menuNotice(S_T_CAL_FAILED);
        statusP(PSTR("failed: not with this preset (or turn Boot calibration on)"));
        return;
    }
    gJob.job = white ? JOB_WHITE : JOB_BLACK;
    gJob.input = uopt->activeInputType;
    gJob.round = gJob.looks = 0;
    gJob.prevY = -1000.0f;
    gJob.firstY = -1.0f;
    gJob.off = rgbLevelsStepsFromSetting(uopt->rgbBlack);   // white is set on top of the saved black
    gJob.offs[0] = GBS::ADC_ROFCTRL::read();
    gJob.offs[1] = GBS::ADC_GOFCTRL::read();
    gJob.offs[2] = GBS::ADC_BOFCTRL::read();
    gJob.gains[0] = GBS::ADC_RGCTRL::read();
    gJob.gains[1] = GBS::ADC_GGCTRL::read();
    gJob.gains[2] = GBS::ADC_BGCTRL::read();
    gJob.white = {(int16_t)gJob.gains[1], -1, -1.0f};
    writeWorking();
    gJob.start = millis();
    gJob.at = gJob.start + kSettleMs;
    menuNotice(S_T_CAL_MEASURING);
    statusP(white ? PSTR("measuring the white screen") : PSTR("measuring the black screen"));
}

void rgbLevelsReset() {
    if (rgbLevelsActive()) { statusP(PSTR("busy: a level is being set")); return; }
    putProfileSetting((uint8_t)SID_input_rgb_black, uopt->rgbBlack, 128);
    rgbLevelsApplyAdc();
    statusP(PSTR("black level reset"));
}

void rgbLevelsLoop() {
    if (!rgbLevelsActive() || (int32_t)(millis() - gJob.at) < 0) return;
    if (rto->sourceDisconnected || uopt->activeInputType != gJob.input) {
        finish(false, S_T_CAL_FAILED, PSTR("failed: the signal changed; nothing saved"));
        return;
    }
    if (millis() - gJob.start > kJobMs) {
        finish(false, S_T_CAL_FAILED, PSTR("failed: the signal was not stable; nothing saved"));
        return;
    }
    uint8_t b[3];
    if (!baseOffsets(b)) {   // a profile's register dump loaded meanwhile, without the boot calibration
        finish(false, S_T_CAL_FAILED, PSTR("failed: the preset changed; nothing saved"));
        return;
    }
    // a sync drop, a preset being applied or a gain / auto gain command still queued (it would move the registers
    // under the look): wait, then look again
    if (!apiActionsIdle() || rto->noSyncCounter != 0 || rto->continousStableCounter < kStableCount || rto->applyPresetDoneStage == 1 ||
        rto->applyPresetDoneStage == 10) {
        gJob.prevY = -1000.0f;
        gJob.at = millis() + kLookGapMs;
        return;
    }
    const bool moved = gJob.job == JOB_WHITE ? GBS::ADC_GGCTRL::read() != (uint8_t)gJob.white.gain
                                             : GBS::ADC_GOFCTRL::read() != (uint8_t)constrain(b[1] + gJob.off, 0, 255);
    if (moved) {   // something else wrote the register the job owns
        finish(false, S_T_CAL_FAILED, PSTR("failed: the ADC changed while measuring; nothing saved"));
        return;
    }
    const float y = lookY();
    if (fabsf(y - gJob.prevY) > 1.0f) {   // not settled yet (or the first look)
        gJob.prevY = y;
        gJob.at = millis() + kLookGapMs;
        if (++gJob.looks >= kMaxLooks) finish(false, S_T_CAL_FAILED, PSTR("failed: the picture didn't settle; show a still, full screen"));
        return;
    }
    const float v = (y + gJob.prevY) / 2;
    if (gJob.firstY < 0) gJob.firstY = v;
    gJob.round++;
    RgbLevelsVerdict r = gJob.job == JOB_BLACK ? rgbLevelsBlackStep(v, gJob.round, gJob.off)
                                               : rgbLevelsWhiteStep(v, gJob.round, gJob.white);
    char text[64];
    switch (r) {
        case RL_CONTINUE:
            writeWorking();
            gJob.looks = 0;
            gJob.prevY = -1000.0f;
            gJob.at = millis() + kSettleMs;
            return;
        case RL_DONE:
            if (gJob.job == JOB_BLACK)
                snprintf_P(text, sizeof(text), PSTR("black set: Y %d -> %d (offset %+d)"), (int)lroundf(gJob.firstY), (int)lroundf(v), gJob.off);
            else
                snprintf_P(text, sizeof(text), PSTR("white set: Y %d -> %d (gain 0x%02X)"), (int)lroundf(gJob.firstY), (int)lroundf(v), gJob.white.gain);
            finish(true, S_T_CAL_SET, nullptr, text);
            return;
        case RL_BAD_PICTURE:
            if (gJob.job == JOB_BLACK) finish(false, S_T_CAL_NOT_BLACK, PSTR("failed: not a full black screen; nothing saved"));
            else finish(false, S_T_CAL_NOT_WHITE, PSTR("failed: not a full white screen; nothing saved"));
            return;
        case RL_OUT_OF_RANGE: finish(false, S_T_CAL_FAILED, PSTR("failed: the source is outside the ADC's range; nothing saved")); return;
        default: finish(false, S_T_CAL_FAILED, PSTR("failed: didn't converge; nothing saved")); return;
    }
}
