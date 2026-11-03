#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <new>
#include "../../options.h"
#include "../settings/settings-registry.h"
#include "../settings/settings-device.h"
#include "../profiles/profile-list.h"
#include "api-backup.h"
#include "../adv/adv-store.h"

static const char kHeader[] = "# gbsc-backup 1";
static const char kProfilesPath[] = "/cfg/profiles.cfg";
static const uint32_t kBlockBytes = 8192;

struct SettingsSection { const char* name; const char* path; SettingScope scope; };
static const SettingsSection kSettings[] = {
    {"cfg/device.cfg", SETTINGS_DEVICE_PATH, SettingScope::Device},
    {"cfg/current.cfg", SETTINGS_CURRENT_PATH, SettingScope::Profile},
};
static const size_t kSettingsCount = sizeof(kSettings) / sizeof(kSettings[0]);

static bool excludedLine(const char* line) {
    char key[48];
    const char* eq = strchr(line, '=');
    if (!eq || (size_t)(eq - line) >= sizeof(key)) return false;
    memcpy(key, line, (size_t)(eq - line)); key[eq - line] = '\0';
    return settingsKeyExcludedFromBackup(key);
}

// ---------------------------------------------------------------- streamer
// Section numbering: 0..kSettingsCount-1 settings files, then marker, then raw profiles.cfg, then profiles

void BackupStreamer::begin(SettingsFs* fs, const char* const* ids, uint8_t n) {
    fs_ = fs;
    idCount_ = 0;
    for (uint8_t i = 0; i < n && i < 36; i++) { strncpy(ids_[idCount_], ids[i], 3); ids_[idCount_][3] = '\0'; idCount_++; }
    section_ = -1;
    inFile_ = false;
    pendingLen_ = pendingPos_ = 0;
    adv_ = false;
    presetCount_ = macroCount_ = 0;
    total_ = 0;
}

void BackupStreamer::beginAdv(const char* const* presets, uint8_t np, const char* const* macros, uint8_t nm) {
    adv_ = true;
    presetCount_ = macroCount_ = 0;
    for (uint8_t i = 0; i < np && presetCount_ < ADV_PRESET_MAX; i++) { strncpy(advIds_[presetCount_], presets[i], 3); advIds_[presetCount_][3] = '\0'; presetCount_++; }
    for (uint8_t i = 0; i < nm && macroCount_ < ADV_MACRO_MAX; i++) {
        char* d = advIds_[presetCount_ + macroCount_];
        strncpy(d, macros[i], 3); d[3] = '\0'; macroCount_++;
    }
}

bool BackupStreamer::nextSection() {
    const int marker = (int)kSettingsCount, raw = marker + 1, firstProfile = raw + 1;
    while (true) {
        section_++;
        inFile_ = false;
        offset_ = 0; lineLen_ = 0; lineOverflow_ = false; readLen_ = readPos_ = 0;
        lastByte_ = '\n';
        if (section_ < marker) {
            const SettingsSection& s = kSettings[section_];
            if (!fs_->exists(s.path)) continue;
            snprintf(path_, sizeof(path_), "%s", s.path);
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "[file %s]\n", s.name);
            filtered_ = true;
        } else if (section_ == marker) {
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "[profiles]\n");
            pendingPos_ = 0;
            return true;   // no file
        } else if (section_ == raw) {
            if (!fs_->exists(kProfilesPath)) continue;
            snprintf(path_, sizeof(path_), "%s", kProfilesPath);
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "[raw cfg/profiles.cfg]\n");
            filtered_ = false;
        } else if (section_ - firstProfile < idCount_) {
            const char* id = ids_[section_ - firstProfile];
            snprintf(path_, sizeof(path_), "/profiles/%s.cfg", id);
            if (!fs_->exists(path_)) continue;   // deleted since the snapshot
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "[profile %s]\n", id);
            filtered_ = false;
        } else if (adv_ && section_ - firstProfile - idCount_ == 0) {
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "[adv]\n");
            pendingPos_ = 0;
            return true;   // no file
        } else if (adv_ && section_ - firstProfile - idCount_ - 1 < presetCount_ + macroCount_) {
            int k = section_ - firstProfile - idCount_ - 1;
            bool preset = k < presetCount_;
            const char* id = advIds_[k];
            int i = advParseId(id, preset ? ADV_PRESET : ADV_MACRO);
            if (i < 0) continue;
            advPath(preset ? ADV_PRESET : ADV_MACRO, (uint8_t)i, path_, sizeof(path_));
            if (!fs_->exists(path_)) continue;   // deleted since the snapshot
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "[adv %s %s]\n", preset ? "preset" : "macro", id);
            filtered_ = false;
        } else {
            return false;
        }
        pendingPos_ = 0;
        inFile_ = true;
        return true;
    }
}

size_t BackupStreamer::fill(char* buf, size_t len) {
    size_t out = fillSome(buf, len);
    total_ += out;
    return out;
}

size_t BackupStreamer::fillSome(char* buf, size_t len) {
    size_t out = 0;
    static const int kHeaderSent = -2;
    while (out < len) {
        if (pendingPos_ < pendingLen_) {
            size_t n = pendingLen_ - pendingPos_;
            if (n > len - out) n = len - out;
            memcpy(buf + out, pending_ + pendingPos_, n);
            pendingPos_ += n;
            out += n;
            continue;
        }
        if (section_ == -1) {   // header line first
            pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "%s\n", kHeader);
            pendingPos_ = 0;
            section_ = kHeaderSent;
            continue;
        }
        if (section_ == kHeaderSent) { section_ = -1; if (!nextSection()) return out; continue; }
        if (!inFile_) { if (!nextSection()) return out; continue; }
        // inside a file: refill the read buffer
        if (readPos_ >= readLen_) {
            readLen_ = fs_->readAt(path_, offset_, readBuf_, sizeof(readBuf_));
            readPos_ = 0;
            offset_ += readLen_;
            if (!readLen_) {   // end of file: flush a last line without newline
                inFile_ = false;
                pendingLen_ = pendingPos_ = 0;   // an excluded last line must not resend the previous one
                if (filtered_ && lineLen_ && !lineOverflow_) {
                    line_[lineLen_] = '\0';
                    if (!excludedLine(line_)) pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "%s\n", line_);
                }
                if (!filtered_ && lastByte_ != '\n') { pending_[0] = '\n'; pendingLen_ = 1; }   // the next header on its own line
                lineLen_ = 0;
                continue;
            }
        }
        if (!filtered_) {   // verbatim: pass the read buffer through pending
            size_t n = readLen_ - readPos_;
            memcpy(pending_, readBuf_ + readPos_, n);
            pendingLen_ = n; pendingPos_ = 0;
            if (n) lastByte_ = readBuf_[readPos_ + n - 1];
            readPos_ = readLen_;
            continue;
        }
        // filtered: assemble one line, emit it unless excluded
        while (readPos_ < readLen_) {
            char c = readBuf_[readPos_++];
            if (c == '\n') {
                line_[lineLen_] = '\0';
                bool emit = !lineOverflow_ && !excludedLine(line_);
                if (emit) { pendingLen_ = (size_t)snprintf(pending_, sizeof(pending_), "%s\n", line_); pendingPos_ = 0; }
                lineLen_ = 0; lineOverflow_ = false;
                if (emit) break;
            } else if (lineLen_ < sizeof(line_) - 2) {
                line_[lineLen_++] = c;
            } else {
                lineOverflow_ = true;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------- restore

enum SectionKind : int8_t { SK_NONE = -1, SK_SETTINGS = 0, SK_MARKER = 1, SK_RAW = 2, SK_PROFILE = 3,
                            SK_ADV_MARKER = 4, SK_ADV_PRESET = 5, SK_ADV_MACRO = 6 };

struct Section { SectionKind kind; int index; };   // index: settings section or profile index

// "-2" = not a section line, kind SK_NONE with index -1 = unknown/invalid section
static Section sectionOf(const char* line, bool& isSection) {
    Section s = {SK_NONE, -1};
    isSection = line[0] == '[';
    if (!isSection) return s;
    size_t n = strlen(line);
    while (n && line[n - 1] == '\r') n--;
    if (n < 3 || line[n - 1] != ']') return s;
    char body[40];
    if (n - 2 >= sizeof(body)) return s;
    memcpy(body, line + 1, n - 2); body[n - 2] = '\0';
    if (!strcmp(body, "profiles")) { s.kind = SK_MARKER; return s; }
    if (!strcmp(body, "raw cfg/profiles.cfg")) { s.kind = SK_RAW; return s; }
    if (!strcmp(body, "adv")) { s.kind = SK_ADV_MARKER; return s; }
    if (!strncmp(body, "adv preset ", 11)) {
        int i = advParseId(body + 11, ADV_PRESET);
        if (i >= 0) { s.kind = SK_ADV_PRESET; s.index = i; }
        return s;
    }
    if (!strncmp(body, "adv macro ", 10)) {
        int i = advParseId(body + 10, ADV_MACRO);
        if (i >= 0) { s.kind = SK_ADV_MACRO; s.index = i; }
        return s;
    }
    if (!strncmp(body, "file ", 5)) {
        for (size_t i = 0; i < kSettingsCount; i++)
            if (!strcmp(body + 5, kSettings[i].name)) { s.kind = SK_SETTINGS; s.index = (int)i; }
        return s;
    }
    if (!strncmp(body, "profile ", 8)) {
        int i = ProfileList::indexOfId(body + 8);
        if (i >= 0) { s.kind = SK_PROFILE; s.index = i; }
    }
    return s;
}

static bool isBlankOrComment(const char* l) { return !*l || *l == '#' || *l == '\r'; }

static bool isKeyValue(const char* l) {
    const char* p = l;
    while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '_') p++;
    return p != l && *p == '=';
}

struct ValidateCtx {
    bool header; Section cur; uint8_t sections; bool marker; bool profileHasName; uint32_t profileBytes;
    bool advMarker; AdvYamlCheck adv;
    RestorePlan plan; BackupError err;
};

static bool isAdvFile(SectionKind k) { return k == SK_ADV_PRESET || k == SK_ADV_MACRO; }

static void closeProfile(ValidateCtx* v) {
    if (isAdvFile(v->cur.kind)) {
        if (!advYamlFinish(v->adv) && v->err == BACKUP_OK) v->err = BACKUP_ERR_SYNTAX;
        v->plan.advBlocks += v->adv.size ? (uint32_t)((v->adv.size + kBlockBytes - 1) / kBlockBytes) : 1;
        v->plan.advCount++;
        return;
    }
    if (v->cur.kind != SK_PROFILE) return;
    if (!v->profileHasName && v->err == BACKUP_OK) v->err = BACKUP_ERR_SYNTAX;
    v->plan.profileBlocks += (v->profileBytes + kBlockBytes - 1) / kBlockBytes;
    v->plan.profileCount++;
}

static void validateLine(const char* line, void* c) {
    ValidateCtx* v = (ValidateCtx*)c;
    if (v->err != BACKUP_OK) return;
    if (!v->header) {
        if (!*line || *line == '\r') return;   // leading blank lines
        if (strcmp(line, kHeader) != 0) { v->err = BACKUP_ERR_HEADER; return; }
        v->header = true;
        return;
    }
    bool isSection;
    Section s = sectionOf(line, isSection);
    if (isSection) {
        if (s.kind == SK_NONE) { v->err = BACKUP_ERR_SECTION; return; }
        if ((s.kind == SK_RAW || s.kind == SK_PROFILE) && !v->marker) { v->err = BACKUP_ERR_SECTION; return; }
        if (isAdvFile(s.kind) && !v->advMarker) { v->err = BACKUP_ERR_SECTION; return; }
        closeProfile(v);
        if (v->err != BACKUP_OK) return;
        if (s.kind == SK_MARKER) { v->marker = true; v->plan.hasProfiles = true; }
        if (s.kind == SK_ADV_MARKER) { v->advMarker = true; v->plan.hasAdv = true; }
        if (isAdvFile(s.kind)) advYamlBegin(v->adv, advMaxBytes(s.kind == SK_ADV_PRESET ? ADV_PRESET : ADV_MACRO));
        v->cur = s;
        v->sections++;
        v->profileHasName = false;
        v->profileBytes = 0;
        return;
    }
    if (isAdvFile(v->cur.kind)) {   // YAML text, verbatim: the ADV store's own rules
        if (!advYamlFeed(v->adv, (const uint8_t*)line, strlen(line)) || !advYamlFeed(v->adv, (const uint8_t*)"\n", 1))
            v->err = BACKUP_ERR_SYNTAX;
        return;
    }
    if (isBlankOrComment(line)) return;
    if (v->cur.kind == SK_NONE || v->cur.kind == SK_MARKER || v->cur.kind == SK_ADV_MARKER || !isKeyValue(line)) {
        v->err = BACKUP_ERR_SYNTAX;
        return;
    }
    if (v->cur.kind == SK_PROFILE) {
        if (!strncmp(line, "profile.name=", 13) && line[13]) v->profileHasName = true;
        v->profileBytes += strlen(line) + 1;
    }
}

BackupError backupValidate(SettingsFs& fs, const char* path, RestorePlan* plan) {
    ValidateCtx* vp = new (std::nothrow) ValidateCtx();   // ~0.4 KB only while a restore is checked (RAM is short)
    if (!vp) return BACKUP_ERR_READ;
    std::unique_ptr<ValidateCtx> own(vp);
    ValidateCtx& v = *vp;
    memset(&v, 0, sizeof(v));
    v.cur.kind = SK_NONE;
    v.err = BACKUP_OK;
    if (!fs.readLines(path, validateLine, &v, BACKUP_MAX_BYTES)) return BACKUP_ERR_READ;
    if (v.err == BACKUP_OK) closeProfile(&v);
    if (plan) *plan = v.plan;
    if (v.err != BACKUP_OK) return v.err;
    if (!v.header) return BACKUP_ERR_HEADER;
    return v.sections ? BACKUP_OK : BACKUP_ERR_EMPTY;
}

const char* backupErrorName(BackupError e) {
    switch (e) {
        case BACKUP_OK: return "ok";
        case BACKUP_ERR_HEADER: return "header";
        case BACKUP_ERR_SECTION: return "section";
        case BACKUP_ERR_SYNTAX: return "syntax";
        case BACKUP_ERR_EMPTY: return "empty";
        default: return "read";
    }
}

struct ApplyCtx {
    SettingsFs* fs; userOptions* u; Section cur; SettingsFileWriter* w;
    void (*deleteProfiles)(void*); void* ctx;
    void (*deleteAdv)(void*); void* advCtx;
    bool ok;
};

static void closeWriter(ApplyCtx* a) {
    if (!a->w) return;
    if (!a->w->commit()) { a->w->abort(); a->ok = false; }
    delete a->w;
    a->w = nullptr;
}

static void applyLine(const char* line, void* c) {
    ApplyCtx* a = (ApplyCtx*)c;
    bool isSection;
    Section s = sectionOf(line, isSection);
    if (isSection) {
        closeWriter(a);
        a->cur = s;
        if (s.kind == SK_SETTINGS) settingsRestoreBegin(*a->u, kSettings[s.index].scope);
        else if (s.kind == SK_MARKER) { if (a->deleteProfiles) a->deleteProfiles(a->ctx); }
        else if (s.kind == SK_RAW) a->w = a->fs->openWriter(kProfilesPath);
        else if (s.kind == SK_PROFILE) {
            char path[24]; snprintf(path, sizeof(path), "/profiles/p%02d.cfg", s.index);
            a->w = a->fs->openWriter(path);
        }
        else if (s.kind == SK_ADV_MARKER) { if (a->deleteAdv) a->deleteAdv(a->advCtx); }
        else if (isAdvFile(s.kind)) {
            char path[28]; advPath(s.kind == SK_ADV_PRESET ? ADV_PRESET : ADV_MACRO, (uint8_t)s.index, path, sizeof(path));
            a->w = a->fs->openWriter(path);
        }
        return;
    }
    if (isBlankOrComment(line) && a->cur.kind != SK_PROFILE && a->cur.kind != SK_RAW && !isAdvFile(a->cur.kind)) return;
    if (a->cur.kind == SK_SETTINGS) settingsRestoreLine(*a->u, kSettings[a->cur.index].scope, line);
    else if (a->w && *line) { a->w->write(line, strlen(line)); a->w->write("\n", 1); }
}

bool backupApply(SettingsFs& fs, const char* path, userOptions& u, void (*deleteProfiles)(void*), void* ctx,
                 void (*deleteAdv)(void*), void* advCtx) {
    ApplyCtx a = {&fs, &u, {SK_NONE, -1}, nullptr, deleteProfiles, ctx, deleteAdv, advCtx, true};
    if (!fs.readLines(path, applyLine, &a, BACKUP_MAX_BYTES)) a.ok = false;
    closeWriter(&a);
    return a.ok;
}

bool restoreSpaceOk(uint32_t freeBlocks, uint32_t currentProfileBlocks, uint32_t neededBlocks) {
    return neededBlocks + 2 <= freeBlocks + currentProfileBlocks;
}
