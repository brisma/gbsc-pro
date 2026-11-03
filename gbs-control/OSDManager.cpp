#include "OSDManager.h"
#include "options.h"
#include "pro/settings/settings-registry.h"
#include "pro/settings/settings-request.h"
#include "pro/api/api-device.h"
#include "pro/api/api-actions.h"
extern userOptions *uopt;
extern void saveUserPrefs();
extern OSDManager osdManager;

// GBSC PRO: the icons change our settings and actions, so values are saved and the profile state follows
// (upstream wrote the registers directly). Brightness/Contrast step the pending value, so several encoder
// steps in one loop pass add up and the bar shows the value that will be applied.
static const uint8_t kStep = 5;   // 20 clicks end to end

static bool stepPicture(uint8_t sid, OSDMenuConfig &config)
{
    SettingDef d;
    settingsDefAt(sid, d);
    uint16_t v = settingsGet(*uopt, d);
    apiPendingValue(sid, v);
    if (config.onChange) {
        int n = (int)v + (config.inc ? kStep : -kStep);
        v = (uint16_t)(n < (int)d.min ? d.min : n > (int)d.max ? d.max : n);
        SettingsPair p = {sid, v};
        apiQueueSettings(&p, 1);
    }
    config.barLength = 128;
    config.barActiveLength = (uint8_t)(v * 128 / 100);
    return true;
}

bool osdBrightness(OSDMenuConfig &config) { return stepPicture(SID_pic_brightness, config); }
bool osdContrast(OSDMenuConfig &config) { return stepPicture(SID_pic_contrast, config); }

// Move / Scale: the menu's screen actions (registers, profile Unsaved); a three-state bar shows the direction
static bool screenStep(uint8_t aidInc, uint8_t aidDec, OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    ActionRequest r = {};
    r.index = config.inc ? aidInc : aidDec;
    apiQueueAction(r);
    config.barActiveLength = config.inc ? 2 : 0;
    return true;
}

bool osdMoveX(OSDMenuConfig &config) { return screenStep(AID_scr_move_right, AID_scr_move_left, config); }
bool osdMoveY(OSDMenuConfig &config) { return screenStep(AID_scr_move_down, AID_scr_move_up, config); }
bool osdScaleX(OSDMenuConfig &config) { return screenStep(AID_scr_scale_right, AID_scr_scale_left, config); }
bool osdScaleY(OSDMenuConfig &config) { return screenStep(AID_scr_scale_up, AID_scr_scale_down, config); }

bool osdAutoGain(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (config.onChange) {
        uopt->enableAutoGain = config.inc ? 1 : 0;
        saveUserPrefs();
    }
    if (uopt->enableAutoGain == 1) {
        config.barActiveLength = 2;
    } else {
        config.barActiveLength = 0;
    }
    return true;
}
bool osdScanlines(OSDMenuConfig &config)
{
    if (config.onChange) {
        if (config.inc) {
            if (uopt->scanlineStrength > 0) {
                uopt->scanlineStrength -= 0x10;
            }
        } else {
            uopt->scanlineStrength = MIN(uopt->scanlineStrength + 0x10, 0x50);
        }
        if (uopt->scanlineStrength == 0x50) {
            uopt->wantScanlines = 0;
        } else {
            uopt->wantScanlines = 1;
        }
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
        saveUserPrefs();
    }
    config.barLength = 128;
    if (uopt->scanlineStrength == 0) {
        config.barActiveLength = 128;
    } else {
        config.barActiveLength = (5 - uopt->scanlineStrength / 0x10) * 25;
    }
    return true;
}
bool osdLineFilter(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (config.onChange) {
        uopt->wantVdsLineFilter = config.inc ? 1 : 0;
        GBS::VDS_D_RAM_BYPS::write(!uopt->wantVdsLineFilter);
        saveUserPrefs();
    }
    if (uopt->wantVdsLineFilter == 1) {
        config.barActiveLength = 2;
    } else {
        config.barActiveLength = 0;
    }
    return true;
}
void initOSD()
{
    osdManager.registerIcon(OSDIcon::BRIGHTNESS, osdBrightness);
    osdManager.registerIcon(OSDIcon::CONTRAST, osdContrast);

    // Hue and Volume are not registered: no matching control on a standard board

    osdManager.registerIcon(OSDIcon::MOVE_Y, osdMoveY);
    osdManager.registerIcon(OSDIcon::MOVE_X, osdMoveX);
    osdManager.registerIcon(OSDIcon::SCALE_Y, osdScaleY);
    osdManager.registerIcon(OSDIcon::SCALE_X, osdScaleX);
}