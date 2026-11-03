#include <Arduino.h>
#include "../../options.h"
#include "../../OSDManager.h"
#include "../platform/platform-caps.h"
#include "../update/update-device.h"
#include "tv-osd.h"
#include "tv-osd-device.h"

extern userOptions* uopt;
extern runTimeOptions* rto;
extern OSDManager osdManager;
uint16_t tv5725OsdGeneration();   // gbs-control.ino: bumped by every TV5725 OSD reset (preset loads)

static_assert(TVO_BRIGHTNESS == (uint8_t)OSDIcon::BRIGHTNESS && TVO_CONTRAST == (uint8_t)OSDIcon::CONTRAST &&
              TVO_MOVE_Y == (uint8_t)OSDIcon::MOVE_Y && TVO_MOVE_X == (uint8_t)OSDIcon::MOVE_X &&
              TVO_SCALE_Y == (uint8_t)OSDIcon::SCALE_Y && TVO_SCALE_X == (uint8_t)OSDIcon::SCALE_X,
              "TVO_* must match OSDIcon");

static TvOsdIndicator gIndicator;
static bool gMode = false;
static uint16_t gSeenGen = 0xFFFF;

static bool shown() { return rto->boardHasPower && !rto->sourceDisconnected && rto->videoStandardInput != 0; }
static bool allowed() { return tvOsdAllowed(hwHas(CAP_OSD), shown(), rto->presetID); }

// Upstream's OSD zoom table knows 480p (1) and 1080p (5); every other output uses its default layout
static void setLayoutPreset() {
    uint8_t n = rto->presetID & 0x0F;
    osdManager.preset = n == 4 ? 1 : n == 5 ? 5 : 0;
}

// The OSD registers are read-modify-write over I2C: no scaler traffic while an upload runs (state is still cleared;
// a successful update reboots, a failed one is cleaned by the next preset load)
static void osdOff() { if (!updateInProgress()) osdManager.menuOff(); }

void tvOsdNav(uint8_t nav) {
    if (nav == TVOSD_CLOSE) {
        if (gMode) { gMode = false; osdOff(); }
        return;
    }
    if (updateInProgress()) return;
    if (nav == TVOSD_OPEN) {
        gIndicator.hide();                            // the mode takes the OSD over (no menuOff in between)
        gMode = allowed();
        if (!gMode) return;
        setLayoutPreset();
        tvOsdNeedsRearm(gSeenGen, tv5725OsdGeneration());
        osdManager.openOnCursor();
        return;
    }
    if (!gMode) return;
    setLayoutPreset();
    bool rearm = tvOsdNeedsRearm(gSeenGen, tv5725OsdGeneration());
    if (nav == TVOSD_ENTER) { osdManager.nextIcon(); return; }   // redraws the layout as well
    if (rearm) osdManager.openOnCursor();             // a preset load turned it off
    osdManager.step(nav == TVOSD_RIGHT);
}

void tvOsdIndicate(uint8_t icon, int8_t dir) {
    if (icon == TVO_NONE || gMode || !uopt->tvChanges || !allowed() || updateInProgress()) return;
    bool rearm = tvOsdNeedsRearm(gSeenGen, tv5725OsdGeneration()) || !gIndicator.shown();
    uint8_t bar = 64;
    if (icon == TVO_BRIGHTNESS) bar = (uint8_t)(uopt->picBrightness * 128 / 100);
    else if (icon == TVO_CONTRAST) bar = (uint8_t)(uopt->picContrast * 128 / 100);
    else bar = dir > 0 ? 128 : dir < 0 ? 0 : 64;
    setLayoutPreset();
    osdManager.showIcon(icon, bar, rearm);
    gIndicator.show(icon, millis());
}

void tvOsdLoop(uint32_t now) {
    if (gMode || !gIndicator.shown()) return;
    if (gIndicator.due(now) || !allowed()) {
        gIndicator.hide();
        osdOff();
    }
}
