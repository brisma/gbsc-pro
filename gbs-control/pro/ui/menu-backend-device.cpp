// Device MenuBackend (whole file device-only)
#ifdef ARDUINO
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../../options.h"
#include "../../tv5725.h"
#include "../../ntsc_720x480.h"
#include "../gbs-control-pro.h"
#include "../settings/settings-registry.h"
#include "../settings/settings-device.h"
#include "../settings/settings-request.h"
#include "../api/api-device.h"
#include "../api/api-actions.h"
#include "../profiles/profile-store.h"
#include "../link/adv-link-device.h"
#include "../link/adv-controller-format.h"
#include "../link/adv-controller-image.h"
#include "../profiles/profile-autoload.h"
#include "../profiles/profile-modes.h"
#include "../update/update-device.h"
#include "../update/update-verify.h"
#include "../update/ota-update.h"
#include "../platform/platform-caps.h"
#include "../util/str-copy.h"
#include "../input/input-auto-device.h"
#include "../input/input-auto.h"
#include "tv-osd-device.h"
#include "../drivers/stv9426-device.h"
#include "menu-backend-device.h"
#include "menu-device.h"
#include "menu-signal.h"

typedef TV5725<GBS_ADDR> GBS;
extern userOptions* uopt;
extern runTimeOptions* rto;
extern const char* ap_ssid;
extern const char* ap_password;
extern const char* device_hostname_full;
float getSourceFieldRate(boolean useSPBus);
float getOutputFrameRate();
boolean areScanLinesAllowed();
void writeProgramArrayNew(const uint8_t* programArray, boolean skipMDSection);
void doPostPresetLoadSteps();
void freezeVideo();
bool menuFrameLockReady();   // gbs-control.ino: FrameSync::ready()
static const uint8_t kPresetBypassRGBHV = 0x22;   // PresetBypassRGBHV (gbs-control.ino)

void DeviceMenuBackend::refreshSignal() {
    // No signal as the automatic input sees it (inputTvPathPresent): upstream keeps sourceDisconnected false for
    // ~50 s of sync recovery, and meanwhile the line counter and the rates read garbage ("1220L 0.0Hz" after the
    // console went off). Stale rates go too: the next source shows "--.-Hz" until measured.
    // While Auto searches, the probed paths show half-detected sources for moments: no signal until a find either
    sig_.searching = inputAutoSearching();
    sig_.noSignal = sig_.searching ||
                    !inputTvPathPresent(rto->boardHasPower, rto->sourceDisconnected, rto->videoStandardInput,
                                        rto->noSyncCounter) ||
                    GBS::PAD_CKIN_ENZ::read();
    if (sig_.noSignal) sig_.srcHzX10 = sig_.outHzX10 = 0;
    uint8_t format = uopt->activeInputType == InputTypeSV ? uopt->svVideoFormat : uopt->avVideoFormat;
    sig_.advNtsc = hwHas(CAP_ADV) && menuAdvIsNtsc(format, (uint8_t)GBS::STATUS_00::read());
    sig_.scanlinesAllowed = areScanLinesAllowed();
    const AdvControllerInfo& h = advLinkController();                   // decoder standard (PAL60 is not visible in the rate)
    bool advIn = inputIsDecoder(uopt->activeInputType);
    const AdvSignal* g = advLinkSignal();                // fresh within 5 s (advLinkSignal)
    sig_.advStandard = advIn ? advControllerStandard(h, g, g != nullptr) : -1;
    sig_.advFreeRun = advIn && g && (g->flags & ADV_SIG_POWERED) && (g->flags & ADV_SIG_FREERUN) &&
                      !(g->flags & ADV_SIG_PRESENT);
    sig_.presetId = rto->presetID;
    uint16_t vperiod = GBS::VPERIOD_IF::read();
    vperiod_ = vperiod;
    static MenuLineFilter lineFilter;                    // an unstable count reads "--", not "1220L"
    sig_.srcLines = lineFilter.feed(sig_.noSignal ? 0 : menuSourceLines(rto->videoStandardInput, vperiod), millis());
    sig_.interlaced = profileIsInterlaced(rto->videoStandardInput, vperiod);
    sig_.syncType = menuSyncType(GBS::SP_SOG_MODE::read() == 1, rto->syncTypeCsync);
    sig_.frameLocked = uopt->enableFrameTimeLock && menuFrameLockReady();
    sig_.palGroup = rto->videoStandardInput == 2 || rto->videoStandardInput == 4;
    uint8_t regMode = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    sig_.palSource = regMode == 2 || regMode == 4;
    if (!autoloadStageBusy(rto->applyPresetDoneStage)) profileGeneratedNameNow(srcName_);   // reads the line count
    sig_.sogLevel = rto->currentLevelSOG;
    sig_.updateAvailable = otaRecommended().any;
    sig_.updateManual = otaManualOffer().any;
    sig_.advControllerUpdateAvailable = advLinkControllerUpdateAvailable();
}

void DeviceMenuBackend::refreshRates() {
    if (sig_.noSignal) return;
    // VGA pass-through: the IF test bus carries no pulse and with HV sync the SP path reads that same bus: no
    // measurement rather than a stall
    if (rto->presetID == kPresetBypassRGBHV) {   // and not the rates of the mode before: "--.-Hz" on the screens
        sig_.srcHzX10 = 0;
        sig_.outHzX10 = 0;
        return;
    }
    float src, out;
    if (rto->outModeHdBypass) {   // the IF test bus carries no pulse in HD bypass: each try waited ~1 s
        src = getSourceFieldRate(1);   // the sync processor still sees the source (upstream's csync detection path)
        if (src < 40.0f) src = 0;      // ~18.5 Hz = no embedded vsync: unknown, not a rate
        out = src;                     // the output is the source, passed through
    } else {
        src = getSourceFieldRate(0);
        out = getOutputFrameRate();
    }
    sig_.srcHzX10 = (uint16_t)(src > 0 ? src * 10 + 0.5f : 0);
    sig_.outHzX10 = (uint16_t)(out > 0 ? out * 10 + 0.5f : 0);
}

const MenuSignal& DeviceMenuBackend::signal() {
    // live: the locked overlay, for the ESP image upload or the ADV Controller update
    sig_.updateAdvController = !updateInProgress() && advLinkUpdateActive();
    sig_.updatePct = updateInProgress() ? (int8_t)updateProgress() : (int8_t)advLinkUpdateProgress();
    sig_.updateStage = updateInProgress() ? updateStage() : (uint8_t)UPD_RECEIVING;
    return sig_;
}

uint16_t DeviceMenuBackend::value(uint8_t sid) {
    uint16_t v;
    if (apiPendingValue(sid, v)) return v;
    SettingDef d; settingsDefAt(sid, d);
    return settingsGet(*uopt, d);
}

bool DeviceMenuBackend::settled(uint8_t sid) {
    uint16_t v;
    return !apiPendingValue(sid, v);
}

bool DeviceMenuBackend::applyMany(const MenuWrite* w, uint8_t n) {
    SettingsPair pairs[12];                        // Reset picture: 12 pairs in one batch
    if (n > 12) return false;
    for (uint8_t i = 0; i < n; i++) {
        SettingDef d; settingsDefAt(w[i].sid, d);
        if ((d.flags & SETTING_FLAG_DEVICE_ONLY) && !uopt->devMode) return false;   // read-only outside Developer mode
        pairs[i] = {w[i].sid, w[i].v};
    }
    if (!apiQueueSettings(pairs, n)) return false;
    return true;
}

bool DeviceMenuBackend::run(uint8_t aid) {
    ActionRequest r = {};
    r.index = aid;
    return apiQueueAction(r);
}

bool DeviceMenuBackend::fn(uint8_t menuFn) {
    MenuWrite w[12];
    uint8_t n;
    switch (menuFn) {
        case FN_CHECK_CHANNEL_STABLE: otaCheckChannel(OTA_STABLE); return true;   // the result arrives as a message
        case FN_CHECK_CHANNEL_BETA: otaCheckChannel(OTA_BETA); return true;
        case FN_INSTALL_MANUAL: otaInstall(true); return true;   // a refusal shows its own toast
        case FN_DECODER_RESET:
            n = menuDecoderResetWrites(w);
            if (!applyMany(w, n)) return false;
            return run(AID_adv_filt_defaults);
        case FN_GEOMETRY_RESET:                        // back to the base preset of the running mode
            return screenResetToPreset();              // false: busy toast
        case FN_GEOMETRY_UNDO:                         // adjust Back: the running mode's preset (or profile registers)
            settingsApply(APPLY_PRESET);
            return true;
        case FN_WIFI_RESET:
            apiQueueWifiReset();
            return true;
        case FN_GEOMETRY_KEEP: {                       // adjust OK: the active profile keeps this mode's registers
            int a = profileActiveIndex();
            return a < 0 || profileQueueOp(POP_SAVE, (int8_t)a, nullptr);   // no profile: until the mode changes
        }
        case FN_ADV_CONTROLLER_SKIP:                             // boot prompt "No": this build is not offered again
            snprintf(uopt->advControllerSkip, sizeof(uopt->advControllerSkip), "%08lX", (unsigned long)advControllerImageCrc());
            settingsMarkDirty(SettingScope::Device);
            return true;
        default:
            return false;
    }
}

bool DeviceMenuBackend::toggleGet(uint8_t) { return advSyncTermination; }
void DeviceMenuBackend::toggleSet(uint8_t, bool on) { ADV_setSyncTermination(on); }

extern bool syncPolarityInverted;   // gbs-control.ino: serial '8' (Developer > Sync > Sync polarity)

int32_t DeviceMenuBackend::live(uint8_t lv) {
    switch (lv) {
        case LV_ADC_GAIN: return 255 - GBS::ADC_RGCTRL::read();
        case LV_POSITION: {
            int32_t h = (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) ? GBS::IF_HBIN_SP::read()
                                                                                         : GBS::IF_HB_SP2::read();
            return (h << 16) | GBS::IF_VB_ST::read();
        }
        case LV_SIZE: return ((int32_t)GBS::VDS_HSCALE::read() << 16) | GBS::VDS_VSCALE::read();
        case LV_BORDERS: return ((int32_t)GBS::VDS_DIS_HB_ST::read() << 16) | GBS::VDS_DIS_VB_ST::read();
        case LV_BORDER_POS: return ((int32_t)GBS::VDS_DIS_HB_SP::read() << 16) | GBS::VDS_DIS_VB_SP::read();   // left / top edge
        case LV_HTOTAL: return GBS::VDS_HSYNC_RST::read();
        case LV_PLL_DIV: return GBS::PLLAD_MD::read();
        case LV_SDRAM: return GBS::PLL_MS::read();
        case LV_OSR: return rto->osr;
        case LV_ADC_FILTER: return GBS::ADC_FLTR::read();
        case LV_DEBUG_VIEW: return GBS::ADC_UNUSED_62::read() != 0;
        case LV_FREEZE: return GBS::CAPTURE_ENABLE::read() == 0;
        case LV_IF_OFFSET: return GBS::IF_AUTO_OFST_EN::read() != 0;
        case LV_SOG: return rto->currentLevelSOG;
        case LV_OUT_RATE: return sig_.outHzX10;
        case LV_SYNCWATCHER: return rto->syncWatcherEnabled;
        case LV_OSD_ALWAYS_ON: return osdDriver().alwaysOn();
        case LV_OSD_TEST: return osdDriver().exclusive();        // the test screen owns the chip while shown
        case LV_PRINT_INFO: return rto->printInfos;
        case LV_IR_LOG: return menuIrLogOn();
        case LV_SYNC_POLARITY: return syncPolarityInverted;
        case LV_OTA: return rto->allowUpdatesOTA;
        case LV_WIFI_RSSI: return (WiFi.getMode() & WIFI_STA) && WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
        default: return 0;
    }
}


bool DeviceMenuBackend::actionsIdle() { return apiActionsIdle(); }

bool DeviceMenuBackend::setText(uint8_t lt, const char* v) {
    const char* key = lt == LT_OTA_PASSWORD ? "system.ota_password" : lt == LT_EXTRA_HOSTS ? "system.extra_hosts" : nullptr;
    int idx = key ? settingsStrFind(key) : -1;
    if (idx < 0) return false;
    SettingsStrWrite w;
    w.index = (int8_t)idx;
    snprintf(w.value, sizeof(w.value), "%s", v);
    return apiQueueStrSetting(w);                  // applied and broadcast by apiLoop (one pending at a time)
}

void DeviceMenuBackend::text(uint8_t lt, char* out, size_t n) {
    bool ap = WiFi.getMode() == WIFI_AP;
    switch (lt) {
        case LT_WIFI_MODE: copyStr(out, n, ap ? "AP" : WiFi.getMode() == WIFI_OFF ? "Off" : "Client"); break;
        case LT_WIFI_SSID: copyStr(out, n, ap ? ap_ssid : WiFi.SSID().c_str()); break;
        case LT_WIFI_IP: copyStr(out, n, (ap ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str()); break;
        case LT_WIFI_PASS: copyStr(out, n, ap ? ap_password : "-"); break;   // needed to join the AP only
        case LT_WIFI_HOST: copyStr(out, n, device_hostname_full); break;
        case LT_SOURCE_NAME: copyStr(out, n, srcName_); break;
        case LT_UPDATE_VERSION: copyStr(out, n, otaRecommended().any ? otaRecommended().m.version : ""); break;
        case LT_ADV_CONTROLLER_NEW: copyStr(out, n, advControllerImageVersion()); break;
        case LT_UPDATE_RESULT: otaLastResultText(otaLastResult(), out, n); break;
        case LT_FW_CHANNEL: copyStr(out, n, otaChannelLabel(GBS_FW_CHANNEL)); break;
        case LT_UPDATE_OFFER: if (otaRecommended().any) otaOfferText(otaRecommended(), out, n); else copyStr(out, n, ""); break;
        case LT_UPDATE_CONFIRM: if (otaRecommended().any) otaConfirmText(otaRecommended(), out, n); else copyStr(out, n, ""); break;
        case LT_MANUAL_VERSION: copyStr(out, n, otaManualOffer().any ? otaManualOffer().m.version : ""); break;
        case LT_MANUAL_OFFER: if (otaManualOffer().any) otaOfferText(otaManualOffer(), out, n); else copyStr(out, n, ""); break;
        case LT_MANUAL_CONFIRM: if (otaManualOffer().any) otaConfirmText(otaManualOffer(), out, n); else copyStr(out, n, ""); break;
        case LT_FW_GBS: copyStr(out, n, GBS_FW_FULL_VERSION); break;
        case LT_FW_ADV:                                           // read from the ADV Controller (link v3 INFO)
            if (hwHas(CAP_ADV)) advControllerMenuText(advLinkController(), out, n);
            else copyStr(out, n, "-");                          // standard board: no ADV Controller
            break;
        case LT_OTA_PASSWORD: copyStr(out, n, uopt->otaPassword[0] ? "Set" : "Not set"); break;
        case LT_EXTRA_HOSTS: copyStr(out, n, uopt->extraHosts); break;
        case LT_PROFILE_ACTIVE: {
            int a = profileActiveIndex();
            copyStr(out, n, a >= 0 ? profileList().at((uint8_t)a).name : "");
            break;
        }
        default: copyStr(out, n, ""); break;
    }
}

uint8_t DeviceMenuBackend::caps() { return platformCaps(); }
bool DeviceMenuBackend::devMode() { return uopt->devMode; }
uint8_t DeviceMenuBackend::menuMode() { return uopt->menuMode; }
// While Auto searches the hardware walks the paths every few seconds: the menus follow the last found input
// (input.last), or every probe moved the rows that depend on the input
uint8_t DeviceMenuBackend::activeInput() { return inputAutoSearching() ? uopt->inputLast : uopt->activeInputType; }
uint8_t DeviceMenuBackend::basePreset() { return profileBasePreset(); }
void DeviceMenuBackend::saverHold(bool on) { settingsSaverPause(SAVER_HOLD_MENU, on); }

// No-signal menu: 720x480 frozen output so the OSD can sync
void DeviceMenuBackend::noSignalEnter() {
    if (!hwHas(CAP_OSD)) return;                   // the 720x480 free-run output only serves the TV OSD
    isInfoDisplayActive = 1;
    horizontalBlankStart = GBS::VDS_DIS_HB_ST::read();
    horizontalBlankStop = GBS::VDS_DIS_HB_SP::read();
    writeProgramArrayNew(ntsc_720x480, false);
    doPostPresetLoadSteps();
    GBS::VDS_DIS_HB_ST::write(0x00);
    GBS::VDS_DIS_HB_SP::write(0xffff);
    freezeVideo();
    GBS::SP_CLAMP_MANUAL::write(1);
}

void DeviceMenuBackend::noSignalExit(bool restoreBlanking) {
    if (!isInfoDisplayActive) return;
    if (restoreBlanking) {
        GBS::VDS_DIS_HB_ST::write(horizontalBlankStart);
        GBS::VDS_DIS_HB_SP::write(horizontalBlankStop);
    }
    isInfoDisplayActive = 0;                       // sync detection runs again
}

const ProfileList& DeviceMenuBackend::profiles() { return profileList(); }
int DeviceMenuBackend::profileActive() { return profileActiveIndex(); }
uint8_t DeviceMenuBackend::profileMatches(uint8_t* out, uint8_t max) {
    if (sig_.noSignal || autoloadStageBusy(rto->applyPresetDoneStage)) return 0;
    uint8_t reg = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    uint8_t mode = profileSourceModeOf(reg, uopt->activeInputType, uopt->advI2P);
    bool inter = profileIsInterlaced(rto->videoStandardInput, vperiod_);
    return ::profileMatches(profileList(), uopt->activeInputType, mode, vperiod_, inter, out, max,
                            profileSignalMilliHzKnown(uopt->activeInputType, mode, vperiod_, inter));
}
bool DeviceMenuBackend::profileModified() { return profileModifiedNow(); }   // never a second late
bool DeviceMenuBackend::profileOp(uint8_t popKind, int8_t index, const char* name, uint8_t region) {
    return profileQueueOp(popKind, index, name, region);
}
uint8_t DeviceMenuBackend::profileOpsPending() { return ::profileOpsPending(); }
uint16_t DeviceMenuBackend::profileResultSeq() { return profileOpResultSeq(); }
bool DeviceMenuBackend::profileResultFor(uint16_t seq, char* out, size_t n) { return profileOpResultFor(seq, out, n); }
void DeviceMenuBackend::tvOsd(uint8_t nav) { tvOsdNav(nav); }

#endif
