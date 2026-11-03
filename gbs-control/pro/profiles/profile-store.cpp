#include <Arduino.h>
#include <LittleFS.h>
#include <memory>
#include <new>
#include "../../options.h"
#include "../settings/settings-registry.h"
#include "../settings/settings-store.h"
#include "../settings/settings-device.h"
#include "../settings/settings-fs.h"
#include "profile-modes.h"
#include "profile-names.h"
#include "profile-store.h"
#include "../util/str-copy.h"
#include "../api/api-queue.h"
#include "../api/api-device.h"
#include "../settings/settings-request.h"

extern userOptions* uopt;
extern runTimeOptions* rto;
void applyPresets(uint8_t result);
void settingsBroadcastStatusText(const char* key, const char* value);   // settings-web.cpp
#include "../log/log.h"
#include "../log/event-ring.h"

namespace {

const char* kLastusedPath = "/cfg/profiles.cfg";

ProfileList gList;
int gActive = -1;
bool gModified = false;
uint16_t gRev = 0;
uint16_t gChoiceSeq = 0;   // Load or Defaults done (auto-load: a choice made since the signal changed wins)
uint16_t gRegsTouched = 0;   // register actions since the last save, load or preset reload

// ---------------------------------------------------------------- helpers

void writerSink(const char* t, void* ctx) { ((SettingsFileWriter*)ctx)->write(t, strlen(t)); }

void entryFromMeta(ProfileEntry& e, const ProfileMeta& m) { profileEntryFromMeta(e, m); }

struct HeaderCtx { ProfileMeta* m; };
void headerLine(const char* line, void* ctx) { profileParseHeaderLine(line, *((HeaderCtx*)ctx)->m); }

bool readMeta(uint8_t index, ProfileMeta& m) {
    char path[24]; profilePath(index, path);
    profileMetaInit(m);
    HeaderCtx c = {&m};
    return settingsFs().readLines(path, headerLine, &c, PROFILE_HEADER_BYTES) && m.name[0];
}

void saveLastused() {
    TextBuf b; textBufInit(b, 2048);
    gList.serializeLastused(textBufSink, &b);
    if (!b.failed) settingsAtomicWrite(settingsFs(), kLastusedPath, b.data ? b.data : "", b.len);
    textBufFree(b);
}

void lastusedLine(const char* line, void*) { gList.parseLastusedLine(line); }

bool spaceForProfile() {
    FSInfo i;
    if (!LittleFS.info(i)) return false;
    return i.totalBytes - i.usedBytes >= PROFILE_MAX_FILE_BYTES + i.blockSize;
}

bool nameTaken(const char* name, void* ctx) {   // ctx: ProfileNameCtx (uniqueness is per name and region)
    const ProfileNameCtx* c = (const ProfileNameCtx*)ctx;
    return profileNameTakenIn(*c->list, name, c->region, c->self);
}

void setActive(int index) {
    gActive = index;
    char id[4] = "";
    if (index >= 0) ProfileList::idOf((uint8_t)index, id);
    int s = settingsStrFind("profile.active");
    if (s >= 0) { StrSettingDef d; settingsStrDefAt((uint8_t)s, d); settingsStrPut(*uopt, d, id); }
}

void storeHash() {
    char hex[9];
    snprintf(hex, sizeof(hex), "%08lx", (unsigned long)settingsScopeCrc(*uopt, SettingScope::Profile));
    int s = settingsStrFind("profile.hash");
    if (s >= 0) { StrSettingDef d; settingsStrDefAt((uint8_t)s, d); settingsStrPut(*uopt, d, hex); }
    gModified = false;
    gRegsTouched = 0;
}

bool computeModified() {
    char hex[9];
    snprintf(hex, sizeof(hex), "%08lx", (unsigned long)settingsScopeCrc(*uopt, SettingScope::Profile));
    return profileModifiedRule(strcmp(hex, uopt->profileHash) != 0, gRegsTouched, gActive >= 0);
}

void changed() {
    gRev++;
    settingsMarkDirty(SettingScope::Device);
}

uint16_t profileApplyMask() {
    uint16_t mask = 0;
    SettingDef d;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (d.scope == SettingScope::Profile) mask |= d.apply;
    }
    return mask & (uint16_t)~APPLY_PRESET;
}

// Apply groups after a Load or Defaults: only what changed (every group re-ran the sync stripper's resync, a second
// detection and preset load); no snapshot (no memory): every Profile group, as before
static uint16_t changedApplyMask(const userOptions* before) {
    return before ? (uint16_t)(settingsChangedApply(*before, *uopt, SettingScope::Profile) & ~APPLY_PRESET)
                  : profileApplyMask();
}

// Copies a profile file line by line, optionally replacing the name and dropping the fingerprint
struct CopyCtx { SettingsFileWriter* w; const char* name; bool dropFp; };
void copyLine(const char* line, void* ctx) {
    CopyCtx* c = (CopyCtx*)ctx;
    if (c->name && !strncmp(line, "profile.name=", 13)) {
        c->w->write("profile.name=", 13); c->w->write(c->name, strlen(c->name)); c->w->write("\n", 1);
        return;
    }
    if (c->dropFp && !strncmp(line, "profile.fp.", 11)) return;
    c->w->write(line, strlen(line));
    c->w->write("\n", 1);
}

bool copyProfile(uint8_t src, uint8_t dst, const char* newName, bool dropFp) {
    char from[24], to[24]; profilePath(src, from); profilePath(dst, to);
    SettingsFileWriter* w = settingsFs().openWriter(to);
    if (!w) return false;
    CopyCtx c = {w, newName, dropFp};
    bool ok = settingsFs().readLines(from, copyLine, &c, PROFILE_MAX_FILE_BYTES) && w->commit();
    if (!ok) w->abort();
    delete w;
    return ok;
}

// Writes the working state into profile `index` (the entry must be allocated); keeps other modes' registers
struct KeepCtx { SettingsFileWriter* w; uint8_t mode; };
void keepLine(const char* line, void* ctx) {
    KeepCtx* k = (KeepCtx*)ctx;
    if (profileKeepOldLine(line, k->mode)) { k->w->write(line, strlen(line)); k->w->write("\n", 1); }
}

// region: PROFILE_REGION_COUNT keeps the stored one (overwrite), else it is written into the header
const char* writeWorkingState(uint8_t index, const char* name, bool existed, uint8_t region = PROFILE_REGION_COUNT,
                              bool replace = true) {
    if (!spaceForProfile()) return "storage_full";
    ProfileMeta m;
    if (!existed || !readMeta(index, m)) { profileMetaInit(m); }
    strncpy(m.name, name, sizeof(m.name) - 1);
    m.name[sizeof(m.name) - 1] = '\0';
    if (region < PROFILE_REGION_COUNT) snprintf(m.region, sizeof(m.region), "%s", profileRegionName(region));

    bool signal = profileSignalPresent();
    uint8_t regMode = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    int8_t mi = profileModeIndex(regMode);
    bool capture = signal && mi >= 0;
    ProfileRegBuf regBuf(capture);
    if (capture && !regBuf.p) return "memory";       // never a profile without the registers it should hold
    uint8_t* regs = regBuf.p;
    uint8_t oldPreset = uopt->presetPreference;
    if (uopt->presetPreference == OutputCustomized && !existed && gActive >= 0)
        m.basePreset = gList.at((uint8_t)gActive).basePreset;   // save as new from a Custom state
    if (capture) {
        if (uopt->presetPreference != OutputCustomized) m.basePreset = uopt->presetPreference;
        profileCaptureRegs(regs);
        m.modes |= (uint8_t)(1 << mi);
        uopt->presetPreference = OutputCustomized;   // stored in the profile: loads use these registers
    }
    if (signal && !profileSaveKeepsFp(replace, existed)) {   // only a new profile or Replace writes it
        m.hasFp = true;
        m.fpInput = uopt->activeInputType;
        m.fpMode = profileSourceModeOf(regMode, uopt->activeInputType, uopt->advI2P);
        m.fpLines = profileSourceLines();
        m.fpInterlaced = profileIsInterlaced(rto->videoStandardInput, m.fpLines) ? 1 : 0;
        m.fpMilliHz = profileSourceMilliHz();        // 0 when it cannot be measured: the profile then fits any rate
    }
    profileReadExtras(m.adcGain);

    char path[24]; profilePath(index, path);
    SettingsFileWriter* w = settingsFs().openWriter(path);
    if (!w) { uopt->presetPreference = (PresetPreference)oldPreset; return "io"; }
    profileWriteHeader(m, writerSink, w);
    settingsSerializeEx(*uopt, SettingScope::Profile, false, writerSink, w);
    if (capture) profileWriteRegs((uint8_t)mi, regs, writerSink, w);
    if (existed) {
        KeepCtx k = {w, capture ? (uint8_t)mi : PROFILE_KEEP_ALL};
        settingsFs().readLines(path, keepLine, &k, PROFILE_MAX_FILE_BYTES);
    }
    bool ok = w->commit();
    if (!ok) w->abort();
    delete w;
    if (!ok) { uopt->presetPreference = (PresetPreference)oldPreset; return "io"; }

    ProfileEntry& e = gList.at(index);
    uint32_t lu = e.lastused;
    entryFromMeta(e, m);
    e.used = true;
    e.lastused = lu ? lu : gList.nextLastused();
    setActive(index);
    storeHash();
    settingsMarkDirty(SettingScope::Profile);
    changed();
    saveLastused();
    return nullptr;
}

// Load: header → extras, settings → uopt; registers stay in the file for applyPresets
struct LoadCtx { ProfileMeta m; SettingsLoadReport rep; };
void loadLine(const char* line, void* ctx) {
    LoadCtx* c = (LoadCtx*)ctx;
    if (profileParseHeaderLine(line, c->m)) return;
    if (!strncmp(line, "reg.", 4)) return;
    settingsLoadLine(*uopt, SettingScope::Profile, line, c->rep);
}

// Register lookup for applyPresets
struct RegCtx { ProfileRegReader r; };
void regLine(const char* line, void* ctx) { ((RegCtx*)ctx)->r.line(line); }

} // namespace

void profilePath(uint8_t index, char out[24]) { snprintf(out, 24, "/profiles/p%02u.cfg", (unsigned)index); }

const ProfileList& profileList() { return gList; }
int profileActiveIndex() { return gActive; }
bool profileModified() { return gModified; }

ProfileRegBuf::ProfileRegBuf(bool want) : p(want ? new (std::nothrow) uint8_t[PROFILE_REG_BYTES] : nullptr) {
    if (want && !p) logPrintf(LC_PROFILE, LL_INFO, PSTR("no memory for the profile registers (%u bytes)"),
                              (unsigned)PROFILE_REG_BYTES);
}
ProfileRegBuf::~ProfileRegBuf() { delete[] p; }

static uint16_t gSeenRev = 0xFFFF;   // settings revision gModified was computed for

bool profileModifiedNow() {
    if (settingsRevision() != gSeenRev) { gSeenRev = settingsRevision(); gModified = computeModified(); }
    return gModified;
}

void profileStoreInit() {
    LittleFS.mkdir("/profiles");
    LittleFS.remove(PROFILE_IMPORT_TMP);                 // an import aborted by a reset or a lost client
    gList.clear();
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {
        char path[24]; profilePath(i, path);
        settingsRecoverTmp(settingsFs(), path);
        if (!LittleFS.exists(path)) continue;
        ProfileMeta m;
        if (!readMeta(i, m)) {
            logPrintf(LC_PROFILE, LL_INFO, PSTR("profiles: %s unreadable, skipped"), path);
            continue;
        }
        ProfileEntry& e = gList.at(i);
        e.used = true;
        entryFromMeta(e, m);
    }
    settingsFs().readLines(kLastusedPath, lastusedLine, nullptr);
    int a = ProfileList::indexOfId(uopt->profileActive);
    gActive = (a >= 0 && gList.at((uint8_t)a).used) ? a : -1;
    if (gActive < 0 && uopt->profileActive[0]) setActive(-1);
    gModified = computeModified();
}

void profileReloadList() {
    profileStoreInit();
    gRev++;
}

void profileApplyPresetsKeepMode() {
    uint8_t mode = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    applyPresets(mode);
    if (mode) rto->videoStandardInput = mode;   // 0 (no source): applyPresets may have loaded the 480p fallback (3)
}

const char* profileLoad(uint8_t index) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    char path[24]; profilePath(index, path);
    ProfileMeta check;
    if (!readMeta(index, check)) return "io";          // unreadable: leave the working state untouched
    static LoadCtx c;
    std::unique_ptr<userOptions> before(new (std::nothrow) userOptions(*uopt));
    profileMetaInit(c.m);
    settingsLoadBegin(*uopt, SettingScope::Profile, c.rep);
    if (!settingsFs().readLines(path, loadLine, &c, PROFILE_MAX_FILE_BYTES)) return "io";
    profileApplyAdcGain(c.m.adcGain);                  // doPostPresetLoadSteps applies the adco gains
    setActive(index);                                  // applyPresets reads this profile's registers
    settingsApply(changedApplyMask(before.get()));
    profileApplyPresetsKeepMode();
    storeHash();
    gList.at(index).lastused = gList.nextLastused();
    saveLastused();
    settingsMarkDirty(SettingScope::Profile);
    changed();
    gChoiceSeq++;
    if (profileLoadSwitchesInput(uopt->profilesSwitchInput, check.hasFp, check.fpInput, uopt->activeInputType)) {
        SettingDef d; settingsDefAt(SID_input_source, d);
        if (settingsOptionAvailable(d, check.fpInput)) {   // S-Video/Composite need the ADV Controller
            SettingsPair p = {SID_input_source, check.fpInput};
            apiQueueSettings(&p, 1);                     // applied by apiLoop, like a web change
        }
    }
    return nullptr;
}

const char* profileSave(uint8_t index) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    char name[PROFILE_NAME_BUF]; strcpy(name, gList.at(index).name);
    return writeWorkingState(index, name, true, PROFILE_REGION_COUNT, false);
}

const char* profileReplace(uint8_t index, const char* name, uint8_t region) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    char clean[PROFILE_NAME_BUF];
    bool named = name && profileNameSanitize(name, clean, sizeof(clean));
    uint8_t reg = profileReplaceRegion(named, region);   // COUNT: keep its own
    if (!named) strcpy(clean, gList.at(index).name);
    else {
        char unique[PROFILE_NAME_BUF];
        ProfileNameCtx nc = {&gList, index, reg};
        profileNameUnique(clean, nameTaken, &nc, unique, sizeof(unique));
        strcpy(clean, unique);
    }
    return writeWorkingState(index, clean, true, reg, true);
}

const char* profileDefaults() {
    SettingsLoadReport rep;
    std::unique_ptr<userOptions> before(new (std::nothrow) userOptions(*uopt));
    uint8_t res = profileDefaultsPreset(uopt->presetPreference,
                                        gActive >= 0 ? gList.at((uint8_t)gActive).basePreset : PROFILE_BASE_NONE);
    settingsLoadBegin(*uopt, SettingScope::Profile, rep);   // every Profile setting back to its default
    uopt->presetPreference = (PresetPreference)res;         // but the output resolution in use
    profileResetAdcGain();
    setActive(-1);
    settingsApply(changedApplyMask(before.get()));
    profileApplyPresetsKeepMode();
    storeHash();
    settingsMarkDirty(SettingScope::Profile);
    changed();
    gChoiceSeq++;
    return nullptr;
}

uint16_t profileChoiceSeq() { return gChoiceSeq; }

void profileRegsTouched() {
    if (gRegsTouched < 0xFFFF) gRegsTouched++;
    gModified = gActive >= 0;                            // seen at once (profileTick broadcasts it)
    gSeenRev = 0xFFFF;
}

void profileRegsClear() { gRegsTouched = 0; gSeenRev = 0xFFFF; }

namespace {
struct ForgetCtx { SettingsFileWriter* w; uint8_t mode; };
void forgetLine(const char* line, void* ctx) {
    ForgetCtx* f = (ForgetCtx*)ctx;
    if (profileForgetKeepsLine(line, f->mode)) { f->w->write(line, strlen(line)); f->w->write("\n", 1); }
}
} // namespace

// Reset geometry with a profile active: the running mode goes back to the base preset
const char* profileForgetMode(uint8_t index) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    int8_t mi = profileModeIndex(profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60));
    ProfileMeta m;
    if (mi >= 0 && readMeta(index, m) && (m.modes & (1 << mi))) {
        m.modes &= (uint8_t)~(1 << mi);
        char path[24]; profilePath(index, path);
        SettingsFileWriter* w = settingsFs().openWriter(path);
        if (!w) return "io";
        profileWriteHeader(m, writerSink, w);
        ForgetCtx f = {w, (uint8_t)mi};
        bool ok = settingsFs().readLines(path, forgetLine, &f, PROFILE_MAX_FILE_BYTES) && w->commit();
        if (!ok) w->abort();
        delete w;
        if (!ok) return "io";
        ProfileEntry& e = gList.at(index);
        uint32_t lu = e.lastused;
        entryFromMeta(e, m);
        e.used = true;
        e.lastused = lu;
        changed();                 // settings untouched: the modified flag (storeHash) stays as it was
    }
    settingsApply(APPLY_PRESET);   // reload: base preset for this mode
    return nullptr;
}

const char* profileSaveAs(const char* name, int* newIndex, uint8_t region) {
    int i = gList.allocate();
    if (i < 0) return "limit";
    char unique[PROFILE_NAME_BUF];
    ProfileNameCtx nc = {&gList, i, region < PROFILE_REGION_COUNT ? region : PROFILE_REGION_NONE};
    profileNameUnique(name, nameTaken, &nc, unique, sizeof(unique));
    const char* err = writeWorkingState((uint8_t)i, unique, false, region);
    if (err) { gList.remove((uint8_t)i); return err; }
    if (newIndex) *newIndex = i;
    return nullptr;
}

const char* profileRename(uint8_t index, const char* name) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    char unique[PROFILE_NAME_BUF];
    ProfileNameCtx nc = {&gList, index, gList.at(index).region};
    profileNameUnique(name, nameTaken, &nc, unique, sizeof(unique));
    if (!copyProfile(index, index, unique, false)) return "io";
    strcpy(gList.at(index).name, unique);
    changed();
    return nullptr;
}

const char* profileDuplicate(uint8_t index, int* newIndex) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    if (!spaceForProfile()) return "storage_full";
    int dst = gList.allocate();
    if (dst < 0) return "limit";
    char unique[PROFILE_NAME_BUF];
    ProfileNameCtx nc = {&gList, dst, gList.at(index).region};
    profileNameUnique(gList.at(index).name, nameTaken, &nc, unique, sizeof(unique));
    if (!copyProfile(index, (uint8_t)dst, unique, false)) { gList.remove((uint8_t)dst); return "io"; }
    ProfileEntry& e = gList.at((uint8_t)dst);
    e = gList.at(index);
    strcpy(e.name, unique);
    e.lastused = 0;
    saveLastused();
    changed();
    if (newIndex) *newIndex = dst;
    return nullptr;
}

const char* profileClearFingerprint(uint8_t index) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    if (!copyProfile(index, index, nullptr, true)) return "io";
    gList.at(index).hasFp = false;
    changed();
    return nullptr;
}

const char* profileDelete(uint8_t index) {
    if (index >= PROFILE_MAX || !gList.at(index).used) return "unknown";
    char path[24]; profilePath(index, path);
    uint8_t base = gList.at(index).basePreset;
    LittleFS.remove(path);
    gList.remove(index);
    if (gActive == (int)index) {
        setActive(-1);
        if (uopt->presetPreference == OutputCustomized) {   // its registers are gone
            uopt->presetPreference = (PresetPreference)(base != PROFILE_BASE_NONE ? base : 0);
            settingsMarkDirty(SettingScope::Profile);
            settingsApply(APPLY_PRESET);
        }
    }
    saveLastused();
    changed();
    return nullptr;
}

void profileDeleteAll() {
    for (uint8_t i = 0; i < PROFILE_MAX; i++) {
        char path[24]; profilePath(i, path);
        LittleFS.remove(path);
    }
    LittleFS.remove(kLastusedPath);
    gList.clear();
    setActive(-1);
    gModified = false;
    changed();
}

bool profileLoadRegs(uint8_t gbsMode, uint8_t* out432) {
    if (gActive < 0) return false;
    int8_t mi = profileModeIndex(gbsMode);
    if (mi < 0 || !(gList.at((uint8_t)gActive).modes & (1 << mi))) return false;
    char path[24]; profilePath((uint8_t)gActive, path);
    static RegCtx c;
    c.r.begin((uint8_t)mi, out432);
    settingsFs().readLines(path, regLine, &c, PROFILE_MAX_FILE_BYTES);
    return c.r.complete();
}

uint8_t profileStoredBase(const char* id) {
    int i = ProfileList::indexOfId(id);
    ProfileMeta m;
    return i >= 0 && readMeta((uint8_t)i, m) ? m.basePreset : PROFILE_BASE_NONE;
}

uint8_t profileBasePreset() {
    if (gActive < 0) return 0;
    uint8_t b = gList.at((uint8_t)gActive).basePreset;
    return (b == PROFILE_BASE_NONE || b == OutputCustomized) ? 0 : b;
}

void profileTick() {
    static uint16_t sentListRev = 0xFFFF;
    static uint32_t lastCheck = 0;
    static int sentActive = -2;
    static int sentModified = -1;
    if (settingsRevision() != gSeenRev && millis() - lastCheck >= 1000) {
        gSeenRev = settingsRevision();
        lastCheck = millis();
        gModified = computeModified();
    }
    if (gActive != sentActive) {
        sentActive = gActive;
        settingsBroadcastStatusText("status.profile", gActive >= 0 ? gList.at((uint8_t)gActive).name : "");
    }
    if ((int)gModified != sentModified) {
        sentModified = gModified;
        settingsBroadcastStatusText("status.profile_modified", gModified ? "1" : "0");
    }
    if (gRev != sentListRev) {
        sentListRev = gRev;
        char v[8]; snprintf(v, sizeof(v), "%u", gRev);
        settingsBroadcastStatusText("status.profiles_rev", v);
    }
}

// ---------------------------------------------------------------- operation queue

namespace {
struct ProfileOp { uint8_t kind; int8_t index; uint8_t region; char name[PROFILE_NAME_BUF]; };
RingQueue<ProfileOp, 5> gOps;
}

uint8_t profileOpsPending() { return (uint8_t)gOps.size(); }

bool profileQueueOp(uint8_t kind, int8_t index, const char* name, uint8_t region) {
    ProfileOp op;
    op.kind = kind;
    op.index = index;
    op.region = region;
    op.name[0] = '\0';
    if (name) copyStr(op.name, sizeof(op.name), name);
    return gOps.push(op);
}

void profileGeneratedNameNow(char out[PROFILE_NAME_BUF]) {
    bool signal = profileSignalPresent();
    uint8_t regMode = profileRegModeOf(rto->videoStandardInput, rto->presetIsPalForce60);
    uint8_t src = profileSourceModeOf(regMode, uopt->activeInputType, uopt->advI2P);
    bool inter = profileIsInterlaced(rto->videoStandardInput, signal ? profileSourceLines() : 0);
    profileGeneratedName(uopt->activeInputType, src, inter, signal, out, 25);
}

static uint16_t gOpSeq = 0;
static char gOpResult[24] = "";
// The last results by sequence number: a drain of several queued ops keeps each one's result
static struct { uint16_t seq; char res[24]; } gOpRing[4];
uint16_t profileOpResultSeq() { return gOpSeq; }
bool profileOpResultFor(uint16_t seq, char* out, size_t n) {
    if (!seq) return false;
    for (auto& r : gOpRing)
        if (r.seq == seq) { copyStr(out, n, r.res); return true; }
    return false;
}

// Console log: one line per profile operation
static void logProfileOp(const ProfileOp& op, const char* err) {
    static const char* const kOp[] = {"load", "save", "save as", "save as", "rename", "delete", "duplicate",
                                      "clear fingerprint", "import", "forget mode", "defaults", "replace"};
    const char* what = "op";
    switch (op.kind) {
        case POP_LOAD: what = kOp[0]; break;
        case POP_SAVE: what = kOp[1]; break;
        case POP_SAVEAS: case POP_SAVEAS_GENERATED: what = kOp[2]; break;
        case POP_RENAME: what = kOp[4]; break;
        case POP_DELETE: what = kOp[5]; break;
        case POP_DUPLICATE: what = kOp[6]; break;
        case POP_CLEAR_FP: what = kOp[7]; break;
        case POP_IMPORT: what = kOp[8]; break;
        case POP_FORGET_MODE: what = kOp[9]; break;
        case POP_DEFAULTS: what = kOp[10]; break;
        case POP_REPLACE: what = kOp[11]; break;
        default: break;
    }
    char who[24] = "";
    if (op.kind == POP_SAVEAS && op.name[0]) snprintf(who, sizeof(who), " %s", op.name);
    else if (op.index >= 0 && op.kind != POP_DEFAULTS && op.kind != POP_IMPORT && op.kind != POP_SAVEAS_GENERATED)
        snprintf(who, sizeof(who), " p%02d", op.index);
    if (err) logPrintf(LC_PROFILE, LL_INFO, PSTR("%s%s: failed (%s)"), what, who, err);
    else logPrintf(LC_PROFILE, LL_INFO, PSTR("%s%s: ok"), what, who);
}

void profileOpsLoop() {
    if (settingsBackupBusy()) return;                 // a backup is reading the profile files: the ops wait
    ProfileOp op;
    while (gOps.pop(op)) {
        const char* err = nullptr;
        uint8_t i = op.index >= 0 ? (uint8_t)op.index : 0;
        switch (op.kind) {
            case POP_LOAD: err = profileLoad(i); break;
            case POP_SAVE: err = profileSave(i); break;
            case POP_SAVEAS: err = profileSaveAs(op.name, nullptr, op.region); break;
            case POP_SAVEAS_GENERATED: { char n[PROFILE_NAME_BUF]; profileGeneratedNameNow(n); err = profileSaveAs(n, nullptr); } break;
            case POP_RENAME: err = profileRename(i, op.name); break;
            case POP_DELETE: err = profileDelete(i); break;
            case POP_DUPLICATE: err = profileDuplicate(i, nullptr); break;
            case POP_CLEAR_FP: err = profileClearFingerprint(i); break;
            case POP_IMPORT: err = profileImport(PROFILE_IMPORT_TMP); LittleFS.remove(PROFILE_IMPORT_TMP); break;
            case POP_FORGET_MODE: err = profileForgetMode(i); break;
            case POP_DEFAULTS: err = profileDefaults(); break;
            case POP_REPLACE: err = profileReplace(i, op.name[0] ? op.name : nullptr, op.region); break;
            default: err = "unknown"; break;
        }
        char v[32];
        snprintf(v, sizeof(v), err ? "error:%s" : "ok", err);
        settingsBroadcastStatusText("status.profile_op", v);
        logProfileOp(op, err);
        if (!err && op.kind == POP_LOAD) eventAdd(EV_PROFILE_LOADED, (uint8_t)profileActiveIndex(), 0);   // a: index
        if (!err && (op.kind == POP_SAVE || op.kind == POP_SAVEAS || op.kind == POP_SAVEAS_GENERATED || op.kind == POP_REPLACE))
            eventAdd(EV_PROFILE_SAVED, (uint8_t)profileActiveIndex(), 0);
        strncpy(gOpResult, err ? err : "", sizeof(gOpResult) - 1);   // stored for the menu: "" = ok
        gOpResult[sizeof(gOpResult) - 1] = '\0';
        gOpSeq++;
        if (!gOpSeq) gOpSeq = 1;                                   // 0 means "no result" for the ring
        gOpRing[gOpSeq % 4].seq = gOpSeq;
        memcpy(gOpRing[gOpSeq % 4].res, gOpResult, sizeof(gOpResult));
    }
}

// ---------------------------------------------------------------- import

namespace {
struct ImportCheck { ProfileMeta m; bool bad; uint16_t lines; };
void importCheckLine(const char* line, void* ctx) {
    ImportCheck* c = (ImportCheck*)ctx;
    if (!*line || *line == '#' || *line == '\r') return;
    if (!strchr(line, '=')) { c->bad = true; return; }
    profileParseHeaderLine(line, c->m);
    c->lines++;
}
} // namespace

const char* profileImport(const char* path) {
    if (settingsFs().fileSize(path) > PROFILE_MAX_FILE_BYTES) return "too_big";
    static ImportCheck c;
    profileMetaInit(c.m);
    c.bad = false; c.lines = 0;
    if (!settingsFs().readLines(path, importCheckLine, &c, PROFILE_MAX_FILE_BYTES)) return "io";
    char clean[PROFILE_NAME_BUF];
    if (c.bad || !c.lines || !profileNameSanitize(c.m.name, clean, sizeof(clean))) return "invalid";
    if (!spaceForProfile()) return "storage_full";
    int dst = gList.allocate();
    if (dst < 0) return "limit";
    char unique[PROFILE_NAME_BUF];
    ProfileNameCtx nc = {&gList, dst, profileRegionIndex(c.m.region)};
    profileNameUnique(clean, nameTaken, &nc, unique, sizeof(unique));
    char to[24]; profilePath((uint8_t)dst, to);
    SettingsFileWriter* w = settingsFs().openWriter(to);
    if (!w) { gList.remove((uint8_t)dst); return "io"; }
    CopyCtx cc = {w, unique, false};
    bool ok = settingsFs().readLines(path, copyLine, &cc, PROFILE_MAX_FILE_BYTES) && w->commit();
    if (!ok) w->abort();
    delete w;
    if (!ok) { gList.remove((uint8_t)dst); return "io"; }
    ProfileEntry& e = gList.at((uint8_t)dst);
    entryFromMeta(e, c.m);
    strcpy(e.name, unique);
    e.used = true;
    saveLastused();
    changed();
    return nullptr;
}
