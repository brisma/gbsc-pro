#include "../profiles/profile-modes.h"
#include "../profiles/profile-store.h"
#include <Arduino.h>
#include <LittleFS.h>
#include "../../options.h"
#include "settings-device.h"
#include "settings-registry.h"
#include "settings-store.h"
#include "settings-saver.h"

extern userOptions* uopt;
// Defined in gbs-control.ino: forwards to SerialM (serial + web console mirror)
#include "../log/log.h"

namespace {

// Streams to <path>.tmp; commit() renames it over <path>
class LittleFsWriter : public SettingsFileWriter {
public:
    explicit LittleFsWriter(const char* path) {
        snprintf(path_, sizeof(path_), "%s", path);
        snprintf(tmp_, sizeof(tmp_), "%s.tmp", path);
        f_ = LittleFS.open(tmp_, "w");
        ok_ = (bool)f_;
    }
    ~LittleFsWriter() override { if (f_) { f_.close(); LittleFS.remove(tmp_); } }
    bool write(const char* data, size_t len) override {
        if (!ok_) return false;
        if (f_.write((const uint8_t*)data, len) != len) ok_ = false;
        return ok_;
    }
    bool commit() override {
        if (!f_) return false;
        f_.close();
        if (!ok_) { LittleFS.remove(tmp_); return false; }
        return LittleFS.rename(tmp_, path_);   // lfs_rename replaces the target atomically
    }
    void abort() override { if (f_) f_.close(); LittleFS.remove(tmp_); ok_ = false; }
private:
    File f_;
    bool ok_;
    char path_[48];
    char tmp_[52];
};

class LittleFsSettings : public SettingsFs {
public:
    SettingsFileWriter* openWriter(const char* p) override { return new LittleFsWriter(p); }
    size_t readAt(const char* p, size_t off, char* buf, size_t len) override {
        File f = LittleFS.open(p, "r");
        if (!f) return 0;
        size_t n = 0;
        if (f.seek(off)) n = f.read((uint8_t*)buf, len);
        f.close();
        return n;
    }
    size_t fileSize(const char* p) override {
        File f = LittleFS.open(p, "r");
        if (!f) return 0;
        size_t n = f.size();
        f.close();
        return n;
    }
    bool exists(const char* p) override { return LittleFS.exists(p); }
    bool remove(const char* p) override { return LittleFS.remove(p); }
    bool rename(const char* f, const char* t) override { return LittleFS.rename(f, t); }
    bool writeAll(const char* p, const char* d, size_t n) override {
        File f = LittleFS.open(p, "w");
        if (!f) return false;
        size_t w = f.write((const uint8_t*)d, n);
        f.close();
        return w == n;
    }
    bool readLines(const char* p, void (*cb)(const char*, void*), void* ctx, size_t maxBytes = SETTINGS_MAX_FILE_BYTES) override {
        File f = LittleFS.open(p, "r");
        if (!f) return false;
        LineAssembler a; lineAssemblerInit(a);
        size_t total = 0;
        while (f.available() && total < maxBytes) {
            int c = f.read();
            total++;
            // Feed the watchdog on big files; optimistic_yield is a no-op where yielding is not allowed
            // (network handlers stream backups through this reader)
            if (c == '\n') optimistic_yield(10000);
            lineAssemblerFeed(a, (char)c, cb, ctx);
        }
        lineAssemblerFinish(a, cb, ctx);
        f.close();
        return true;
    }
};

LittleFsSettings gFs;
SettingsSaver gSaver;

struct LoadCtx { SettingScope scope; SettingsLoadReport rep; };

void onLoadLine(const char* line, void* ctx) {
    LoadCtx* c = (LoadCtx*)ctx;
    settingsLoadLine(*uopt, c->scope, line, c->rep);
}

void loadScope(const char* path, SettingScope scope) {
    LoadCtx c; c.scope = scope;
    settingsLoadBegin(*uopt, scope, c.rep);
    if (!gFs.readLines(path, onLoadLine, &c)) {
        logPrintf(LC_SYS, LL_INFO, PSTR("settings: %s missing, defaults"), path);
        return;
    }
    logPrintf(LC_SYS, LL_INFO, PSTR("settings: %s applied=%u unknown=%u invalid=%u"), path, c.rep.applied, c.rep.unknown,
              c.rep.invalid);
}

// Streams the file to <path>.tmp and renames it (building the whole file in one buffer takes ~3 KB contiguous
// per save); never replaces a good file with a truncated or failed one
struct StreamSink { SettingsFileWriter* w; size_t total; bool ok; };
static void streamSink(const char* text, void* ctx) {
    StreamSink* s = (StreamSink*)ctx;
    size_t n = strlen(text);
    s->total += n;
    if (!s->ok || s->total > SETTINGS_MAX_FILE_BYTES || !s->w->write(text, n)) s->ok = false;
}

bool saveScope(const char* path, SettingScope scope) {
    StreamSink s = {gFs.openWriter(path), 0, true};
    bool ok = false;
    if (s.w) {
        settingsSerialize(*uopt, scope, streamSink, &s);
        if (s.ok) ok = s.w->commit();
        else s.w->abort();
        delete s.w;
    }
    if (!ok) logPrintf(LC_SYS, LL_INFO, PSTR("settings: save %s FAILED"), path);
    else logPrintf(LC_SYS, LL_DEBUG, PSTR("settings: saved %s"), path);
    return ok;
}

} // namespace

SettingsFs& settingsFs() { return gFs; }

void settingsInit() {
    LittleFS.mkdir("/cfg");
    settingsRecoverTmp(gFs, SETTINGS_DEVICE_PATH);
    settingsRecoverTmp(gFs, SETTINGS_CURRENT_PATH);
    if (LittleFS.exists("/preferencesv2.txt")) LittleFS.remove("/preferencesv2.txt");
    loadScope(SETTINGS_DEVICE_PATH, SettingScope::Device);
    if (profileStartupLoadsCurrent(uopt->profilesStartup)) {
        loadScope(SETTINGS_CURRENT_PATH, SettingScope::Profile);
    } else {   // Startup = Defaults: no profile, factory values at the output resolution last used
        loadScope(SETTINGS_CURRENT_PATH, SettingScope::Profile);
        uint8_t res = profileDefaultsPreset(uopt->presetPreference, profileStoredBase(uopt->profileActive));
        SettingsLoadReport rep;
        settingsLoadBegin(*uopt, SettingScope::Profile, rep);
        uopt->presetPreference = (PresetPreference)res;
        logPrintf(LC_SYS, LL_INFO, PSTR("settings: startup Defaults, resolution %u"), res);
        int s = settingsStrFind("profile.active");
        if (s >= 0) { StrSettingDef d; settingsStrDefAt((uint8_t)s, d); settingsStrPut(*uopt, d, ""); }
    }
}

static uint16_t gRevision = 0;

uint16_t settingsRevision() { return gRevision; }

void settingsMarkDirty(SettingScope s) {
    gSaver.markDirty(s, millis());
    gRevision++;
}

void settingsFlush() {
    uint8_t m = gSaver.take();
    // A failed save stays dirty and is retried on the next debounce window
    if ((m & 0x01) && !saveScope(SETTINGS_DEVICE_PATH, SettingScope::Device)) gSaver.markDirty(SettingScope::Device, millis());
    if ((m & 0x02) && !saveScope(SETTINGS_CURRENT_PATH, SettingScope::Profile)) gSaver.markDirty(SettingScope::Profile, millis());
}

extern bool gSettingsPresetPending;

void settingsSaverPause(uint8_t reason, bool on) { gSaver.hold(reason, on); }

static uint32_t gBackupAt = 0;
static bool gBackupOn = false;
static const uint32_t kBackupHoldMs = 5000;
void settingsBackupStreaming(bool on) { gBackupOn = on; gBackupAt = millis(); }
bool settingsBackupBusy() { return gBackupOn && millis() - gBackupAt < kBackupHoldMs; }

void settingsLoop() {
    if (gSettingsPresetPending) {
        gSettingsPresetPending = false;
        profileApplyPresetsKeepMode();   // Force 60Hz on a PAL source: one load, not two
    }
    if (gSaver.due(millis()) && !settingsBackupBusy()) settingsFlush();   // due() is false while a hold is set
}

void settingsDiscardPending() { gSaver.take(); }

void settingsFactoryReset() {
    gFs.remove(SETTINGS_DEVICE_PATH);
    gFs.remove(SETTINGS_CURRENT_PATH);
    settingsApplyDefaults(*uopt);
    gSaver.take();
    // Write the defaults now: a backup taken right after the reset then holds them (a missing file left it empty)
    saveScope(SETTINGS_DEVICE_PATH, SettingScope::Device);
    saveScope(SETTINGS_CURRENT_PATH, SettingScope::Profile);
}
