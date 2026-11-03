#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "profile-file.h"
#include "profile-modes.h"
#include "profile-list.h"
#include "profile-names.h"

void profileEntryFromMeta(ProfileEntry& e, const ProfileMeta& m) {
    strncpy(e.name, m.name, sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';
    e.modes = m.modes;
    e.hasFp = m.hasFp;
    e.fpInput = m.fpInput; e.fpMode = m.fpMode; e.fpInterlaced = m.fpInterlaced; e.fpLines = m.fpLines;
    e.fpMilliHz = m.fpMilliHz;
    e.basePreset = m.basePreset;
    e.region = profileRegionIndex(m.region);
}

void profileMetaInit(ProfileMeta& m) {
    memset(&m, 0, sizeof(m));
    strcpy(m.region, "None");
    m.basePreset = PROFILE_BASE_NONE;
}

static void kv(SettingsSink sink, void* ctx, const char* key, const char* value) {
    sink(key, ctx); sink("=", ctx); sink(value, ctx); sink("\n", ctx);
}

static void kvNum(SettingsSink sink, void* ctx, const char* key, unsigned v) {
    char b[8]; snprintf(b, sizeof(b), "%u", v); kv(sink, ctx, key, b);
}

void profileWriteHeader(const ProfileMeta& m, SettingsSink sink, void* ctx) {
    sink("schema=1\n", ctx);
    kv(sink, ctx, "profile.name", m.name);
    kv(sink, ctx, "profile.region", m.region);
    if (m.basePreset != PROFILE_BASE_NONE) kvNum(sink, ctx, "profile.base_preset", m.basePreset);
    char modes[80] = "";
    for (uint8_t i = 0; i < PROFILE_MODE_COUNT; i++) {
        if (!(m.modes & (1 << i))) continue;
        if (modes[0]) strcat(modes, ",");
        strcat(modes, profileModeName(i));
    }
    kv(sink, ctx, "profile.modes", modes);
    if (m.hasFp) {
        kvNum(sink, ctx, "profile.fp.input", m.fpInput);
        kvNum(sink, ctx, "profile.fp.mode", m.fpMode);
        kvNum(sink, ctx, "profile.fp.lines", m.fpLines);
        kvNum(sink, ctx, "profile.fp.interlaced", m.fpInterlaced);
        if (m.fpMilliHz) kvNum(sink, ctx, "profile.fp.mhz", m.fpMilliHz);
    }
    kvNum(sink, ctx, "profile.adc_gain", m.adcGain);
}

static bool keyIs(const char* line, const char* key, const char*& value) {
    size_t n = strlen(key);
    if (strncmp(line, key, n) != 0 || line[n] != '=') return false;
    value = line + n + 1;
    return true;
}

static void copyTrim(char* out, size_t n, const char* v) {
    size_t len = strlen(v);
    while (len && (v[len - 1] == '\r' || v[len - 1] == ' ')) len--;
    if (len >= n) len = n - 1;
    memcpy(out, v, len);
    out[len] = '\0';
}

bool profileParseHeaderLine(const char* line, ProfileMeta& m) {
    const char* v;
    if (keyIs(line, "schema", v)) return true;
    if (strncmp(line, "profile.", 8) != 0) return false;
    if (keyIs(line, "profile.name", v)) {                // hand-edited files: '"' or '\\' would break the JSON
        char t[sizeof(m.name) + 8];
        copyTrim(t, sizeof(t), v);
        if (!profileNameSanitize(t, m.name, sizeof(m.name))) m.name[0] = '\0';
    }
    else if (keyIs(line, "profile.region", v)) {           // stored canonical: old "NTSC"/"JP" become "NTSC-U"/"NTSC-J"
        char t[sizeof(m.region)];
        copyTrim(t, sizeof(t), v);
        strcpy(m.region, profileRegionName(profileRegionIndex(t)));
    }
    else if (keyIs(line, "profile.base_preset", v)) m.basePreset = (uint8_t)atoi(v);
    else if (keyIs(line, "profile.modes", v)) {
        m.modes = 0;
        char buf[96]; copyTrim(buf, sizeof(buf), v);
        char* save = nullptr;
        for (char* t = strtok_r(buf, ",", &save); t; t = strtok_r(nullptr, ",", &save)) {
            int8_t idx = profileModeIndexByName(t);
            if (idx >= 0) m.modes |= (uint8_t)(1 << idx);
        }
    }
    else if (keyIs(line, "profile.fp.input", v)) { m.fpInput = (uint8_t)atoi(v); m.hasFp = true; }
    else if (keyIs(line, "profile.fp.mode", v)) { m.fpMode = (uint8_t)atoi(v); m.hasFp = true; }
    else if (keyIs(line, "profile.fp.lines", v)) { m.fpLines = (uint16_t)atoi(v); m.hasFp = true; }
    else if (keyIs(line, "profile.fp.interlaced", v)) { m.fpInterlaced = (uint8_t)atoi(v); m.hasFp = true; }
    else if (keyIs(line, "profile.fp.mhz", v)) m.fpMilliHz = (uint32_t)strtoul(v, nullptr, 10);
    else if (keyIs(line, "profile.adc_gain", v)) m.adcGain = (uint8_t)atoi(v);
    return true;   // unknown profile.* keys are ignored, still header lines
}

void profileWriteRegs(uint8_t modeIndex, const uint8_t* regs, SettingsSink sink, void* ctx) {
    static const char kHex[] = "0123456789abcdef";
    const uint8_t per = PROFILE_REG_BYTES / PROFILE_REG_LINES;   // 48
    for (uint8_t l = 0; l < PROFILE_REG_LINES; l++) {
        char line[128];
        int n = snprintf(line, sizeof(line), "reg.%s.%u=", profileModeName(modeIndex), l);
        for (uint8_t i = 0; i < per; i++) {
            uint8_t b = regs[l * per + i];
            line[n++] = kHex[b >> 4];
            line[n++] = kHex[b & 15];
        }
        line[n++] = '\n';
        line[n] = '\0';
        sink(line, ctx);
    }
}

// "reg.<mode>.<n>=..." → mode index and line number; false when not a register line
static bool parseRegKey(const char* l, int8_t& mode, int& lineNo, const char*& value) {
    if (strncmp(l, "reg.", 4) != 0) return false;
    const char* eq = strchr(l, '=');
    if (!eq) return false;
    const char* dot = eq;
    while (dot > l + 4 && dot[-1] != '.') dot--;
    if (dot <= l + 5) return false;                    // need "<mode>." before the number
    char name[24];
    size_t nl = (size_t)(dot - 1 - (l + 4));
    if (nl == 0 || nl >= sizeof(name)) return false;
    memcpy(name, l + 4, nl); name[nl] = '\0';
    if (dot == eq) return false;
    for (const char* p = dot; p < eq; p++) if (*p < '0' || *p > '9') return false;
    mode = profileModeIndexByName(name);
    lineNo = atoi(dot);
    value = eq + 1;
    return true;
}

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void ProfileRegReader::begin(uint8_t mode, uint8_t* buffer) { modeIndex = mode; out = buffer; seenMask = 0; bad = false; }

void ProfileRegReader::line(const char* l) {
    int8_t mode; int n; const char* v;
    if (!parseRegKey(l, mode, n, v) || mode != (int8_t)modeIndex) return;
    const uint8_t per = PROFILE_REG_BYTES / PROFILE_REG_LINES;
    size_t len = strlen(v);
    while (len && (v[len - 1] == '\r' || v[len - 1] == ' ')) len--;
    if (n < 0 || n >= PROFILE_REG_LINES || len != (size_t)per * 2) { bad = true; return; }
    for (uint8_t i = 0; i < per; i++) {
        int hi = hexv(v[2 * i]), lo = hexv(v[2 * i + 1]);
        if (hi < 0 || lo < 0) { bad = true; return; }
        out[n * per + i] = (uint8_t)(hi << 4 | lo);
    }
    seenMask |= (uint16_t)(1 << n);
}

bool ProfileRegReader::complete() const { return !bad && seenMask == (1 << PROFILE_REG_LINES) - 1; }

bool profileKeepOldLine(const char* line, uint8_t currentModeIndex) {
    int8_t mode; int n; const char* v;
    if (!parseRegKey(line, mode, n, v)) return false;
    return mode >= 0 && (currentModeIndex == PROFILE_KEEP_ALL || mode != (int8_t)currentModeIndex);
}

bool profileForgetKeepsLine(const char* line, uint8_t modeIndex) {
    ProfileMeta scratch; profileMetaInit(scratch);
    if (profileParseHeaderLine(line, scratch)) return false;
    int8_t mode; int n; const char* v;
    if (parseRegKey(line, mode, n, v)) return mode != (int8_t)modeIndex;
    return true;
}
