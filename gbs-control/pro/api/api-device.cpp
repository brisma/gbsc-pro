#include "../log/log.h"
#include <Arduino.h>
#include "../../options.h"
#include "../settings/settings-registry.h"
#include "../settings/settings-device.h"
#include "../settings/settings-bridge.h"
#include "api-queue.h"
#include "api-pending.h"
#include "api-watchdog.h"
#include "api-device.h"
#include "../ui/tv-osd.h"
#include "../ui/tv-osd-device.h"
#include "../link/adv-link-device.h"
#include "api-actions.h"
#include "api-backup.h"
#include "../adv/adv-store-web.h"
#include "../profiles/profile-store.h"
#include "../update/update-device.h"
#include "../update/ota-update.h"
#include "../update/safe-mode.h"
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include "../gbs-control-pro.h"
#include "../drivers/stv9426-device.h"
#include "../ui/osd-theme.h"
#include "../platform/platform-caps.h"
#include "../ui/menu-device.h"
#include "../ui/menu-types.h"
#include "screen-snapshot.h"
#include "api-live.h"
#include "../ui/menu-signal.h"
#include "../ui/menu-backend.h"
#include <SSD1306Wire.h>
void tv5725WriteReg(uint8_t segment, uint8_t reg, uint8_t value);   // gbs-control.ino
void clockgenOutput(bool on);                                         // gbs-control.ino
extern uint8_t gDevLoopPauseMs;                                       // gbs-control.ino
void si5351Lab(const uint8_t* d, uint8_t n);                          // gbs-control.ino
void contentProbe(const uint8_t* d, uint8_t n);                       // gbs-control.ino
void tv5725ReadSegment(uint8_t segment, uint8_t out[256]);

extern userOptions* uopt;
extern SSD1306Wire display;
extern String serialCommandBuffer;   // gbs-control.ino: legacy command queues, drained by loop()
extern String userCommandBuffer;

static RingQueue<SettingsPair, 33> gSettingsQueue;
static BridgePending gBridgePending;   // values that queued legacy chars will produce

static uint8_t gTvIcon = TVO_NONE;   // a user adjustment the TV5725 icon OSD shows after it ran (standard board)
static int8_t gTvDir = 0;

bool apiQueueSettings(const SettingsPair* pairs, uint8_t n) {
    if (gSettingsQueue.freeSlots() < n) return false;
    for (uint8_t i = 0; i < n; i++) gSettingsQueue.push(pairs[i]);
    uint8_t icon = tvOsdIconForWrite(pairs, n);   // one-pair writes only: Reset picture and profiles never show it
    if (icon != TVO_NONE) { gTvIcon = icon; gTvDir = 0; }
    return true;
}

void settingsBroadcastStatusText(const char* key, const char* value);   // settings-web.cpp

bool apiPendingValue(uint8_t index, uint16_t& v) {
    SettingDef d; settingsDefAt(index, d);
    return pendingValueOf(gSettingsQueue, gBridgePending, index, settingsGet(*uopt, d), millis(), v);
}

static SettingsStrWrite gStrWrite = {-1, {0}};

bool apiQueueStrSetting(const SettingsStrWrite& w) {
    if (gStrWrite.index >= 0) return false;
    gStrWrite = w;
    return true;
}

void apiDropStrSetting() {
    gStrWrite.index = -1;
    memset(gStrWrite.value, 0, sizeof(gStrWrite.value));
}

static void drainStrSetting() {
    if (gStrWrite.index < 0) return;
    StrSettingDef sd; settingsStrDefAt((uint8_t)gStrWrite.index, sd);
    settingsStrPut(*uopt, sd, gStrWrite.value);
    apiDropStrSetting();
    settingsMarkDirty(sd.scope);
    char key[48]; settingsKeyCopyStr(sd, key, sizeof(key));
    settingsBroadcastStatusText(key, (sd.flags & SETTING_FLAG_SECRET) ? "***" : settingsStrGet(*uopt, sd));
}

static void drainSettings() {
    uint16_t mask = 0;
    uint8_t scopes = 0;
    uint8_t changed[32];
    uint8_t nChanged = 0;
    SettingsPair p;
    while (gSettingsQueue.pop(p)) {
        SettingDef d; settingsDefAt(p.index, d);
        SettingsBridgeHit hit;
        uint16_t current = gBridgePending.effective(p.index, settingsGet(*uopt, d), millis());
        char key[40] = "";                                      // only when debug lines are on (devmode may turn on here)
        if (settingsBridgeLookup(d, current, p.value, hit)) {
            if (logDebugOn()) {
                settingsKeyCopy(d, key, sizeof(key));
                logPrintf(LC_API, LL_DEBUG, PSTR("set %s=%u (serial command '%c')"), key, (unsigned)p.value, hit.ch);
            }
            (hit.queue == BRIDGE_SERIAL ? serialCommandBuffer : userCommandBuffer) += hit.ch;
            gBridgePending.set(p.index, p.value, millis());
            continue;
        }
        // a write that changes nothing runs no apply group: a client sending two settings of one choice (Scanlines:
        // on + strength) would reload the preset for the one that stayed as it was
        if (p.value == current) continue;
        settingsPut(*uopt, d, p.value);
        if (logDebugOn()) {                                     // web, menu or guard
            settingsKeyCopy(d, key, sizeof(key));
            logPrintf(LC_API, LL_DEBUG, PSTR("set %s=%u"), key, (unsigned)p.value);
        }
        mask |= d.apply;
        scopes |= (d.scope == SettingScope::Device) ? 0x01 : 0x02;
        if (nChanged < sizeof(changed)) changed[nChanged++] = p.index;
    }
    if (!nChanged) return;
    settingsApply(mask);
    if (scopes & 0x01) settingsMarkDirty(SettingScope::Device);
    if (scopes & 0x02) settingsMarkDirty(SettingScope::Profile);
    for (uint8_t i = 0; i < nChanged; i++) settingsBroadcastChange(changed[i]);
}

void saveUserPrefs();
void screenBorderMoveBy(int16_t dx, int16_t dy);   // gbs-control.ino: Screen > Border position
void sogLevelStep(int8_t dir);                     // gbs-control.ino: Input > Sampling > SOG level, clamped 0..31
void disableScanlines();

static RingQueue<ActionRequest, 9> gActionQueue;

bool apiQueueAction(const ActionRequest& r) { return gActionQueue.push(r); }
bool apiActionQueueFull() { return gActionQueue.freeSlots() == 0; }


static void romLine(const char* text, void*) { logText(LC_API, LL_DEBUG, text); }

// Developer OSD tools: test screen, ROM dump, power A/B, timing tuning
static void runOsdTool(uint8_t id, const ActionRequest& r) {
    Stv9426& d = osdDriver();
    char msg[96];
    switch (id) {
        case AF_OSD_TEST: {
            bool on = !d.exclusive();     // the test screen owns the chip while it is shown
            if (on) {
                d.setExclusive(true);     // the menu renderer pauses while the test screen owns the chip
                stvDrawTestScreen(d);
                d.powerOn(millis());
            } else {
                for (uint8_t r = 0; r < stv::kRows; r++) d.shadow().fill(r, 0, stv::kRowWidth, OSD_BLANK, OSD_ATTR_TRANSPARENT);
                d.flush();
                d.powerOff();
                d.setExclusive(false);    // the menu renderer redraws on its next frame
            }
            logPrintf(LC_MENU, LL_INFO, on ? PSTR("osd: test screen on") : PSTR("osd: test screen off"));
            break;
        }
        case AF_OSD_ROM_DUMP:   // data "27,20": 12 lines from 0x2720 (the whole ROM floods the WebSocket log)
            if (r.len == 2) d.romDump(romLine, nullptr, (uint16_t)((r.data[0] << 8) | r.data[1]), 12);
            else d.romDump(romLine, nullptr);
            break;
        case AF_OSD_ALWAYS_ON:
            d.setAlwaysOn(!d.alwaysOn());
            logPrintf(LC_MENU, LL_INFO, d.alwaysOn() ? PSTR("osd: always on (old behaviour)") : PSTR("osd: power management on"));
            break;
        case AF_OSD_TIMING: {
            if (r.len != 3) { logPrintf(LC_MENU, LL_INFO, PSTR("osd: timing needs LD,DD,CH")); break; }
            d.setTimingOverride(r.data[0], r.data[1], r.data[2]);
            uint32_t fh = d.lineHz();
            bool ok = stvWithinPll(r.data[0], fh, d.xtal(), stvFm(d.xtal()));
            snprintf_P(msg, sizeof(msg), PSTR("osd: LD=%u DD=%u CH=%u pixel=%lu kHz %s (%s)"), r.data[0], r.data[1], r.data[2],
                     (unsigned long)(12UL * (r.data[0] + 1) * fh / 1000), ok ? "ok" : "OVER PLL LIMIT",
                     stvOutStdName(d.output()));
            logText(LC_MENU, LL_INFO, msg);
            break;
        }
    }
}

// Developer screen readback (dev.screen_snapshot, GET /api/dev/snapshot): allocated on first use, then kept
static uint8_t* gSnapshot = nullptr;
static uint32_t gSnapshotAt = 0;
const uint8_t* apiSnapshot() { return gSnapshot; }

// 2 KB kept only for a minute after the last snapshot (the readback tool fetches it at once)
static void snapshotExpire() {
    if (gSnapshot && millis() - gSnapshotAt > 60000) { free(gSnapshot); gSnapshot = nullptr; }
}

static void takeSnapshot() {
    gSnapshotAt = millis();
    if (!gSnapshot) gSnapshot = (uint8_t*)malloc(snap::kBytes);
    if (!gSnapshot) { logPrintf(LC_API, LL_INFO, PSTR("snapshot: no memory")); return; }
    screenSnapshotBuild(hwHas(CAP_OSD) ? &osdBus() : nullptr, (uint8_t)osdDriver().output(),
                        hwHas(CAP_OLED) ? display.buffer : nullptr, gSnapshot);
}

static struct { uint8_t state, seg, gen; uint8_t* v; } gRegs;   // v: 256 bytes, allocated at the first read

uint8_t apiRegsAnnounce(uint8_t seg) {
    gRegs.gen = (uint8_t)(gRegs.gen + 1);
    gRegs.state = REGS_PENDING;
    gRegs.seg = seg;
    return gRegs.gen;
}

TvRegsResult apiRegsResult() { return {gRegs.state, gRegs.seg, gRegs.gen, gRegs.state == REGS_DONE ? gRegs.v : nullptr}; }

static void readRegs(uint8_t seg, uint8_t gen) {
    if (gen != gRegs.gen) return;                        // a later request replaced this one: only the last runs
    if (updateInProgress()) {                            // no scaler I2C while an upload writes the flash
        gRegs.state = REGS_BUSY;
        logPrintf(LC_API, LL_INFO, PSTR("tv5725 read: seg %u refused, update running"), (unsigned)seg);
        return;
    }
    if (!gRegs.v) gRegs.v = (uint8_t*)malloc(256);
    if (!gRegs.v) { gRegs.state = REGS_BUSY; logPrintf(LC_API, LL_INFO, PSTR("tv5725 read: no memory")); return; }
    tv5725ReadSegment(seg, gRegs.v);
    gRegs.state = REGS_DONE;
    logPrintf(LC_API, LL_INFO, PSTR("tv5725 read: seg %u (gen %u)"), (unsigned)seg, (unsigned)gen);
}

bool screenResetToPreset() {
    int a = profileActiveIndex();
    if (a >= 0) {
        bool ok = profileQueueOp(POP_FORGET_MODE, (int8_t)a, nullptr);
        logPrintf(LC_API, LL_INFO, PSTR("screen reset: profile %d forgets this mode%s"), a, ok ? "" : " (busy)");
        return ok;
    }
    logPrintf(LC_API, LL_INFO, PSTR("screen reset: base preset"));
    settingsApply(APPLY_PRESET);
    return true;
}

// Picture and decoder resets: the menu's write lists, as one all-or-nothing settings batch
static bool queueMenuWrites(const char* what, uint8_t (*build)(MenuWrite*)) {
    MenuWrite w[12];
    uint8_t n = build(w);
    SettingsPair pairs[12];
    for (uint8_t i = 0; i < n; i++) pairs[i] = {w[i].sid, w[i].v};
    bool ok = apiQueueSettings(pairs, n);
    logPrintf(LC_API, LL_INFO, PSTR("%s: %u settings %s"), what, (unsigned)n, ok ? "queued" : "refused, queue full");
    return ok;
}

static void runFunc(uint8_t id, const ActionRequest& r) {
    switch (id) {
        case AF_ACE_DEFAULTS:
            uopt->advACELumaGain = ADV_ACE_LUMA_GAIN_DEFAULT;
            uopt->advACEChromaGain = ADV_ACE_CHROMA_GAIN_DEFAULT;
            uopt->advACEChromaMax = ADV_ACE_CHROMA_MAX_DEFAULT;
            uopt->advACEGammaGain = ADV_ACE_GAMMA_GAIN_DEFAULT;
            uopt->advACEResponseSpeed = ADV_ACE_RESPONSE_SPEED_DEFAULT;
            saveUserPrefs();                       // the link v3 state carries the new values to the ADV Controller
            break;
        case AF_FILTERS_DEFAULTS:
            uopt->advFilterYShaping = ADV_FILTER_Y_SHAPING_DEFAULT;
            uopt->advFilterCShaping = ADV_FILTER_C_SHAPING_DEFAULT;
            uopt->advFilterWYShaping = ADV_FILTER_WY_SHAPING_DEFAULT;
            uopt->advFilterWYOverride = ADV_FILTER_WY_OVERRIDE_DEFAULT;
            uopt->advFilterCombNTSC = ADV_FILTER_COMB_NTSC_DEFAULT;
            uopt->advFilterCombPAL = ADV_FILTER_COMB_PAL_DEFAULT;
            uopt->advCombLumaModeNTSC = ADV_COMB_LUMA_MODE_NTSC_DEFAULT;
            uopt->advCombChromaModeNTSC = ADV_COMB_CHROMA_MODE_NTSC_DEFAULT;
            uopt->advCombChromaTapsNTSC = ADV_COMB_CHROMA_TAPS_NTSC_DEFAULT;
            uopt->advCombLumaModePAL = ADV_COMB_LUMA_MODE_PAL_DEFAULT;
            uopt->advCombChromaModePAL = ADV_COMB_CHROMA_MODE_PAL_DEFAULT;
            uopt->advCombChromaTapsPAL = ADV_COMB_CHROMA_TAPS_PAL_DEFAULT;
            uopt->advDnr = 0;   // off by default
            uopt->advCti = 0;
            saveUserPrefs();
            logPrintf(LC_API, LL_INFO, PSTR("filters, comb, DNR and CTI back to the defaults"));
            break;
        case AF_PICTURE_RESET:
            queueMenuWrites("picture reset", menuPictureResetWrites);
            break;
        case AF_DECODER_RESET:                                  // levels, Enhance, 2X, then the filter defaults
            if (queueMenuWrites("decoder reset", menuDecoderResetWrites)) runFunc(AF_FILTERS_DEFAULTS, r);
            break;
        case AF_SCREEN_RESET:
            screenResetToPreset();
            break;
        case AF_TERMINATION: {                                  // checked against the input by the web handler
            bool ok = ADV_setSyncTermination(r.data[0] != 0);   // the input may have changed since
            logPrintf(LC_API, LL_INFO, PSTR("termination: %s%s"), r.data[0] ? "on" : "off", ok ? "" : " refused (not RGB SCART)");
            break;
        }
        case AF_ADV_READ:                                       // dev, map, reg, count, then the gen of the handler
            advLinkRegRead(r.data[0], r.data[1], r.data[2], r.data[3], r.data[4]);
            break;
        case AF_ADV_I2C:
            advLinkI2c(r.data, r.len);
            break;
        case AF_TV5725_READ:                                    // data: segment, then the gen set by the web handler
            readRegs(r.data[0], r.data[1]);
            break;
        case AF_LOOP_PAUSE:                                     // no-signal WiFi experiments: CPU time for the SDK
            gDevLoopPauseMs = r.data[0];
            logPrintf(LC_SYS, LL_INFO, PSTR("loop pause: %u ms per pass"), (unsigned)gDevLoopPauseMs);
            break;
        case AF_SI5351:                                         // Si5351 measurements (gbs-control-nx evaluation)
            si5351Lab(r.data, r.len);
            break;
        case AF_LISTEN:                                         // input search: the decoder listening on pin 20 by hand
            advLinkSetListen(r.data[0] != 0);
            break;
        case AF_CONTENT_PROBE:                                  // input search: channel content before a preset
            contentProbe(r.data, r.len);
            break;
        case AF_CLOCKGEN:                                       // 0 off, 1 on: no-signal WiFi experiments
            clockgenOutput(r.data[0] != 0);
            break;
        case AF_TV5725_REG:                                     // segment, register, value triplets
            for (uint8_t i = 0; i + 2 < r.len; i += 3) tv5725WriteReg(r.data[i], r.data[i + 1], r.data[i + 2]);
            break;
        case AF_CHECK_UPDATE:
            otaCheck(true);
            break;
        case AF_CHECK_CHANNEL: {
            int ch = otaChannelParse((const char*)r.data);   // validated by the web handler
            if (ch >= 0) otaCheckChannel((uint8_t)ch);
            break;
        }
        case AF_INSTALL_UPDATE:
            otaInstall(r.len && !strcmp((const char*)r.data, "manual"));
            break;
        case AF_BORDER_MOVE_LEFT:  screenBorderMoveBy(-4, 0); break;   // 4 px / 2 lines per step
        case AF_BORDER_MOVE_RIGHT: screenBorderMoveBy(4, 0); break;
        case AF_BORDER_MOVE_UP:    screenBorderMoveBy(0, -2); break;
        case AF_BORDER_MOVE_DOWN:  screenBorderMoveBy(0, 2); break;
        case AF_SOG_UP:   sogLevelStep(1); break;
        case AF_SOG_DOWN: sogLevelStep(-1); break;
        case AF_ADV_CONTROLLER_UPDATE:
            advLinkUpdateStart();      // refused (no reply, OTA, already running): /api/info shows why
            break;
        case AF_OSD_TEST:
        case AF_OSD_ROM_DUMP:
        case AF_OSD_ALWAYS_ON:
        case AF_OSD_TIMING:
            runOsdTool(id, r);
            break;
        case AF_MENU_KEY:           // a MenuKey sequence, as if pressed on the remote
            for (uint8_t i = 0; i < r.len; i++)
                if (r.data[i] <= K_LONG) menuDeviceKey(r.data[i]);
            break;
        case AF_SCREEN_SNAPSHOT:
            takeSnapshot();
            break;
        case AF_CRASH:              // safe-mode test: three of these in a row enter safe mode
            *(volatile int*)0 = 0;
            break;
    }
}

bool apiActionsIdle() {
    return gActionQueue.size() == 0 && serialCommandBuffer.length() == 0 && userCommandBuffer.length() == 0;
}

static void runActions() {
    ActionRequest r;
    while (gActionQueue.pop(r)) {
        ActionDef d; actionsDefAt(r.index, d);
        logPrintf(LC_API, (d.flags & ACTION_DEV) ? LL_DEBUG : LL_INFO, PSTR("action %s"), d.name);
        if (d.flags & ACTION_REGS) profileRegsTouched();   // web and remote alike
        uint8_t icon = tvOsdIconForAction(r.index);
        if (icon != TVO_NONE) { gTvIcon = icon; gTvDir = tvOsdActionDir(r.index); }
        if (d.kind == ACT_SERIAL) serialCommandBuffer += (char)d.arg;
        else if (d.kind == ACT_USER) userCommandBuffer += (char)d.arg;
        else runFunc(d.arg, r);
    }
}

static struct {
    bool active; bool rejected; bool tooBig; bool pending; bool running; File file; size_t size; UploadWatchdog wd;
    AsyncWebServerRequest* owner;   // the upload this state belongs to (a second one must not take it over)
    AsyncWebServerRequest* busy;    // a second upload refused while one runs: its finish answers 409
} gRestore;

void apiRestoreChunk(AsyncWebServerRequest* req, size_t index, const uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        if (gRestore.active || gRestore.pending || gRestore.running) { gRestore.busy = req; return; }
        gRestore.owner = req;
        gRestore.active = true; gRestore.rejected = false; gRestore.tooBig = false; gRestore.size = 0;
        if (!apiGuardRequest(req)) { gRestore.rejected = true; return; }   // upload runs before the middleware
        FSInfo fi;   // the temp file is written while the old profiles still exist
        if (!LittleFS.info(fi) || req->contentLength() + 65536 > fi.totalBytes - fi.usedBytes) {
            gRestore.tooBig = true;
            return;
        }
        settingsSaverPause(SAVER_HOLD_RESTORE, true);
        gRestore.wd.touch(millis());
        gRestore.file = LittleFS.open(RESTORE_TMP_PATH, "w");   // the one LittleFS write allowed in a handler
    }
    if (req != gRestore.owner || gRestore.rejected || gRestore.tooBig || !gRestore.file) return;
    if (gRestore.size + len > BACKUP_MAX_BYTES) {
        gRestore.tooBig = true;
        gRestore.wd.stop();
        gRestore.file.close();
        LittleFS.remove(RESTORE_TMP_PATH);
        settingsSaverPause(SAVER_HOLD_RESTORE, false);   // the client may never collect the 413
        return;
    }
    if (gRestore.file.write(data, len) != len) {   // filesystem full
        gRestore.tooBig = true;
        gRestore.wd.stop();
        gRestore.file.close();
        LittleFS.remove(RESTORE_TMP_PATH);
        settingsSaverPause(SAVER_HOLD_RESTORE, false);   // the client may never collect the 413
        return;
    }
    gRestore.size += len;
    gRestore.wd.touch(millis());
    if (final) gRestore.file.close();
}

// HTTP status for the finished upload; on 202 the loop validates and applies the file
int apiRestoreFinish(AsyncWebServerRequest* req) {
    if (req == gRestore.busy) { gRestore.busy = nullptr; return 409; }
    if (!gRestore.active || req != gRestore.owner) return 400;
    gRestore.owner = nullptr;
    gRestore.active = false;
    gRestore.wd.stop();
    if (gRestore.rejected) return 403;
    if (gRestore.tooBig) { settingsSaverPause(SAVER_HOLD_RESTORE, false); return 413; }   // too big or not enough space
    if (gRestore.file) gRestore.file.close();
    gRestore.pending = true;
    return 202;
}

static void runRestore() {
    if (gRestore.wd.expired(millis())) {   // client went away mid-upload: drop it and resume saving
        gRestore.wd.stop();
        gRestore.active = false;
        if (gRestore.file) gRestore.file.close();
        LittleFS.remove(RESTORE_TMP_PATH);
        settingsSaverPause(SAVER_HOLD_RESTORE, false);
        logPrintf(LC_API, LL_INFO, PSTR("restore: upload abandoned (no data for 30 s), nothing changed"));
    }
    if (!gRestore.pending) return;
    gRestore.pending = false;
    gRestore.running = true;                // the check and the apply yield: no new upload may reopen the file meanwhile
    struct Done { ~Done() { gRestore.running = false; } } done;
    RestorePlan plan;
    BackupError e = backupValidate(settingsFs(), RESTORE_TMP_PATH, &plan);
    const char* err = e != BACKUP_OK ? backupErrorName(e) : nullptr;
    if (!err && (plan.hasProfiles || plan.hasAdv)) {
        // Profiles and ADV files are replaced: they need their blocks (+2 spare) from free space plus the blocks
        // the files they replace release
        FSInfo fi;
        uint32_t current = 0;
        if (LittleFS.info(fi)) {
            for (uint8_t i = 0; plan.hasProfiles && i < PROFILE_MAX; i++) {
                char path[24]; profilePath(i, path);
                size_t sz = settingsFs().fileSize(path);
                if (sz) current += (sz + fi.blockSize - 1) / fi.blockSize;
            }
            if (plan.hasAdv) current += advStoreBlocks(fi.blockSize);
            uint32_t need = (plan.hasProfiles ? plan.profileBlocks : 0) + (plan.hasAdv ? plan.advBlocks : 0);
            if (!restoreSpaceOk((fi.totalBytes - fi.usedBytes) / fi.blockSize, current, need)) err = "space";
        } else {
            err = "space";
        }
    }
    if (err) {
        LittleFS.remove(RESTORE_TMP_PATH);
        settingsSaverPause(SAVER_HOLD_RESTORE, false);
        char v[24]; snprintf(v, sizeof(v), "error:%s", err);
        settingsBroadcastStatusText("status.restore", v);
        logPrintf(LC_API, LL_INFO, PSTR("restore refused: %s, nothing changed"), err);
        return;
    }
    logPrintf(LC_API, LL_INFO, PSTR("restore: backup valid%s%s, applying"), plan.hasProfiles ? " (with profiles)" : "",
              plan.hasAdv ? " (with ADV presets and macros)" : "");
    settingsDiscardPending();
    // [profiles] replaces every profile: profile.active/hash, just read from the backup's settings, stay across the
    // wipe; the reload below clears them when the restored set has no profile with that id
    bool wrote = backupApply(settingsFs(), RESTORE_TMP_PATH, *uopt, [](void*) {
        char active[sizeof(uopt->profileActive)], hash[sizeof(uopt->profileHash)];
        memcpy(active, uopt->profileActive, sizeof(active));
        memcpy(hash, uopt->profileHash, sizeof(hash));
        profileDeleteAll();
        memcpy(uopt->profileActive, active, sizeof(active));
        memcpy(uopt->profileHash, hash, sizeof(hash));
    }, nullptr, [](void*) { advStoreDeleteAll(); }, nullptr);
    LittleFS.remove(RESTORE_TMP_PATH);
    profileReloadList();
    settingsSaverPause(SAVER_HOLD_RESTORE, false);
    settingsMarkDirty(SettingScope::Device);
    settingsMarkDirty(SettingScope::Profile);
    settingsFlush();
    settingsBroadcastStatusText("status.restore", wrote ? "ok" : "error:write");   // restart either way
    logPrintf(LC_API, LL_INFO, PSTR("restore: %s, restarting"), wrote ? "applied" : "write failed");
    restartNoteSet(wrote ? RN_RESTORE_OK : RN_RESTORE_WRITE);   // the log is in RAM: the next boot says it again
    userCommandBuffer += 'a';   // restart through the legacy handler (disconnects WS, flushes, resets)
}

static struct { bool connect; bool reset; char ssid[33]; char pass[65]; } gWifi;

bool apiQueueWifiConnect(const char* ssid, const char* pass) {
    size_t s = strlen(ssid), p = strlen(pass);
    if (!s || s > 32 || p > 64 || gWifi.connect || gWifi.reset) return false;
    memcpy(gWifi.ssid, ssid, s + 1);
    memcpy(gWifi.pass, pass, p + 1);
    gWifi.connect = true;
    return true;
}

void apiQueueWifiReset() { gWifi.reset = true; }

static void runWifi() {
    if (gWifi.connect) {
        gWifi.connect = false;
        WiFi.persistent(true);                               // save credentials only
        WiFi.begin(gWifi.ssid, gWifi.pass, 0, 0, false);
        WiFi.persistent(false);                              // avoid the core 3.1.x persistence bug
        memset(gWifi.pass, 0, sizeof(gWifi.pass));
        userCommandBuffer += 'u';                            // legacy handler: STA mode + flush + reset
    }
    if (gWifi.reset) {
        gWifi.reset = false;
        WiFi.persistent(true);
        WiFi.disconnect(true);                               // erases the stored network: AP mode at boot
        WiFi.persistent(false);
        userCommandBuffer += 'a';                            // restart
    }
}

void apiWebLogTick(uint32_t now);   // api-web.cpp

// ---- GET /api/live cache ----
static volatile uint32_t gLiveAskedAt = 0;   // millis() of the last GET (network context)
static volatile bool gLiveAsked = false;
static int32_t gLive[LV_COUNT];
static uint32_t gLiveValid = 0, gLiveAt = 0;
static bool gLiveOn = false;

void apiLiveAsked() { gLiveAskedAt = millis(); gLiveAsked = true; }

size_t apiLiveJson(char* out, size_t cap) {
    int32_t v[LV_COUNT];
    memcpy(v, gLive, sizeof(v));   // the loop may refresh meanwhile: a copy keeps one value per name
    return liveJson(v, gLiveValid, gLiveAt, out, cap);
}

static void apiLiveRefresh() {
    uint32_t now = millis();
    bool want = gLiveAsked && now - gLiveAskedAt < 10000;
    if (want != gLiveOn) {
        gLiveOn = want;
        logPrintf(LC_API, LL_INFO, PSTR("live readbacks %s"), want ? "on (a page asked /api/live)" : "off (not asked for 10 s)");
        if (!want) { gLiveAsked = false; gLiveValid = 0; }
    }
    if (!gLiveOn || (gLiveValid && now - gLiveAt < 1000)) return;
    if (updateInProgress()) {
        if (gLiveValid) logPrintf(LC_API, LL_DEBUG, PSTR("live readbacks paused: update running"));
        gLiveValid = 0;
        return;
    }
    for (uint8_t i = LV_NONE + 1; i < LV_COUNT; i++) gLive[i] = menuDeviceLive(i);
    gLiveValid = ((LV_COUNT >= 32) ? 0xffffffffu : ((1u << LV_COUNT) - 1)) & ~1u;
    gLiveAt = now;
}

void apiLoop() {
    apiWebLogTick(millis());
    snapshotExpire();
    drainStrSetting();
    drainSettings();
    runActions();
    if (gTvIcon != TVO_NONE) { tvOsdIndicate(gTvIcon, gTvDir); gTvIcon = TVO_NONE; }
    runRestore();
    advStoreLoop();
    runWifi();
    apiLiveRefresh();
    updateLoop();
}
