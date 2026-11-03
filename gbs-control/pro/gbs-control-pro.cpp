// Pro additions shared with gbs-control.ino: IR receiver globals, input path switching, picture registers (colour,
// sharpness, scanlines), the 75R sync termination and the output frequency check.
#include <Arduino.h>
#include "gbs-control-pro.h"
#include "options-pro.h"
#include "../options.h"
#include "../tv5725.h"

#include <IRremoteESP8266.h>
#include <IRutils.h>

#include "link/adv-link-device.h"
#include "log/log.h"
#include "picture/picture-color.h"
#include "platform/platform-caps.h"
#include "settings/settings-registry.h"
#include "settings/settings-request.h"
#include "api/api-device.h"

typedef TV5725<GBS_ADDR> GBS;
extern struct runTimeOptions *rto;
extern struct userOptions *uopt;
extern void saveUserPrefs();
extern void resetSyncProcessor();
extern float getOutputFrameRate();

static const int kRecvPin = 2;   // the IR receiver on the Pro (the LED pin on a standard board)
IRrecv irrecv(kRecvPin);
decode_results results;

unsigned long lastSystemTime = 0;
unsigned long lastWebUpdateTime = 0;

uint8_t isInfoDisplayActive = 0;
uint16_t horizontalBlankStart = 0;
uint16_t horizontalBlankStop = 0;

// Experimental 75R sync termination (RGB SCART only): runtime only, cleared on every input change, sent to the
// ADV Controller in the link state
bool advSyncTermination = false;

bool ADV_setSyncTermination(bool enable) {
    if (!hwHas(CAP_ADV) || uopt->activeInputType != InputTypeRGBs) return false;
    advSyncTermination = enable;
    advLinkKick();
    return true;
}

// ---- Input paths ----

static const uint8_t kAdcConfig = 0x01, kHvEnable = 0x02, kLeaveLowPower = 0x04;

struct InputConfig {
    uint8_t adcValue;       // ADC_INPUT_SEL (RGB1 or YUV0); ADC_SOGEN is on for every path
    uint8_t flags;
};

// Indexed by InputType - 1. S-Video/Composite reach the YPbPr ADC through the ADV7391
static const InputConfig kInputConfigs[6] = {
    {RGB1, kAdcConfig},                    // RGB SCART
    {RGB1, kAdcConfig},                    // RGB SoG
    {RGB1, kAdcConfig | kHvEnable},        // VGA
    {YUV0, kAdcConfig},                    // Component
    {YUV0, kAdcConfig | kLeaveLowPower},   // S-Video
    {YUV0, kAdcConfig | kLeaveLowPower},   // Composite
};

static void writeInputHardware(const InputConfig& c) {
    resetSyncProcessor();
    if (c.flags & kAdcConfig) {
        GBS::ADC_SOGEN::write(1);   // sync slicer on for every path (YPbPr: the sync is on Y); adcValue YUV0 = 0
                                    // turned it off for Component, so after a RGB path a PS2 was never found
        GBS::SP_EXT_SYNC_SEL::write((c.flags & kHvEnable) ? HV_Enable : HV_Disable);
        GBS::ADC_INPUT_SEL::write(c.adcValue);
    }
    rto->sourceDisconnected = true;
}

bool inputWatcherForced = false;

// GBS side here; the ADV Controller follows through the link state (sent at once). save = false for the automatic
// scan and temporary manual choices: no device.cfg write per probed path
void switchInputPath(uint8_t type, bool save) {
    if (!inputTypeValid(type)) return;
    const InputConfig& c = kInputConfigs[type - 1];
    advLinkKick();
    uopt->activeInputType = type;
    advSyncTermination = false;
    // The detection runs only with the sync watcher on: a profile's "Watcher Off" (dev.syncwatcher 2) left it off
    // after an input change, so the new input was never detected; that profile choice applies again after the next
    // preset load (applyProfileSyncWatcher)
    if (!rto->syncWatcherEnabled)
        logPrintf(LC_SYNC, LL_INFO, PSTR("sync watcher on for the input change (the profile's Off applies again once "
                                         "the signal is stable)"));
    rto->syncWatcherEnabled = true;
    inputWatcherForced = true;
    writeInputHardware(c);
    if (c.flags & kLeaveLowPower) rto->isInLowPowerMode = false;
    applyPictureColor();   // R/G/B stay neutral for every input: the per-input correction is in pictureColor()
    if (save) saveUserPrefs();
}

// Standard board, Auto: the TV5725 detection found this kind of source on its own input; adopt it without a switch
void inputAdoptDetected(uint8_t type) {
    if (!inputTypeValid(type) || type == uopt->activeInputType) return;
    uopt->activeInputType = type;
    applyPictureColor();
}

void applySavedInputSource(void) {
    if (!inputTypeValid(uopt->activeInputType)) uopt->activeInputType = InputTypeRGBs;
    writeInputHardware(kInputConfigs[uopt->activeInputType - 1]);
}

// ---- Picture registers ----

void applyPictureColor(void)
{
    // every colour register computed from the settings (picture-color.cpp), HDMI range last
    ColorIn in = {};
    in.yuvInput = rto->inputIsYpBpR;
    in.componentOut = uopt->wantOutputComponent;
    in.customPreset = GBS::GBS_PRESET_CUSTOM::read() != 0;
    in.r = uopt->gbsColorR;
    in.g = uopt->gbsColorG;
    in.b = uopt->gbsColorB;
    in.brightness = uopt->picBrightness;
    in.contrast = uopt->picContrast;
    in.saturation = uopt->picSaturation;
    in.hdmiRange = uopt->hdmiLimitedRange;
    in.presetId = rto->presetID;
    in.hdBypass = rto->outModeHdBypass;
    ColorRegs r = pictureColor(in);
    GBS::VDS_Y_GAIN::write(r.yGain);
    GBS::VDS_UCOS_GAIN::write(r.uGain);
    GBS::VDS_VCOS_GAIN::write(r.vGain);
    GBS::VDS_Y_OFST::write((uint8_t)r.yOff);
    GBS::VDS_U_OFST::write((uint8_t)r.uOff);
    GBS::VDS_V_OFST::write((uint8_t)r.vOff);
}

void applyPictureSharpness(void)
{
    SharpRegs p = pictureSharpness(uopt->picSharpness, rto->presetID);
    GBS::VDS_PK_Y_H_BYPS::write(p.bypass ? 1 : 0);
    GBS::VDS_PK_LB_GAIN::write(p.lb);
    GBS::VDS_PK_LH_GAIN::write(p.lh);
}

// picture.scanline_strength: the scanline mixing offsets, written only while the scanlines are on
// (enableScanlines() writes them when they turn on)
void applyScanlineStrength(void)
{
    if (!rto->scanlinesEnabled) return;
    GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
    GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength);
}

void pictureStep(uint8_t sid, int delta)
{
    // serial/web picture steps ('Z' 'T' 'N' 'M' 'V' 'R') go through the setting path: clamped and persisted
    SettingDef d;
    settingsDefAt(sid, d);
    int v = (int)settingsGet(*uopt, d) + delta;
    if (v < d.min) v = d.min;
    if (v > d.max) v = d.max;
    SettingsPair p = {sid, (uint16_t)v};
    apiQueueSettings(&p, 1);
}

void pictureResetColors(void)
{
    SettingsPair p[3] = {{SID_pic_red, 128}, {SID_pic_green, 128}, {SID_pic_blue, 128}};
    apiQueueSettings(p, 3);
}

