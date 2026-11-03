#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "options.h"
#include "settings-pgm.h"
#include "settings-registry.h"
#include "settings-table.h"
#include "../platform/platform-caps.h"
#include "../util/str-copy.h"
#include "../api/api-guard.h"

// Enum option strings in flash: one array per OPT_* name, which the table rows then reference
#define SETTING_OPTION(name, text) static const char name[] SET_PROGMEM = text;
SETTINGS_OPTIONS(SETTING_OPTION)
#undef SETTING_OPTION

// Key strings in flash
#define SETTING_KEY(id, key, ...) static const char kKey_##id[] SET_PROGMEM = key;
SETTINGS_TABLE(SETTING_KEY)
#undef SETTING_KEY

// Option strings in flash (one per setting, nullptr when not an enum)
#define SETTING_OPT(id, key, type, field, def, mn, mx, scope, apply, opts, flags, caps) \
    static const char* const kOpt_##id = opts;
SETTINGS_TABLE(SETTING_OPT)
#undef SETTING_OPT

// Compile-time check: field size matches the declared type
#define SETTING_SIZE_CHECK(id, key, type, field, ...) \
    static_assert(sizeof(((userOptions*)0)->field) == ((SettingType::type == SettingType::U16) ? 2 : 1), \
                  "settings-table: size mismatch for " key);
SETTINGS_TABLE(SETTING_SIZE_CHECK)
#undef SETTING_SIZE_CHECK

#define SETTING_ROW(id, key, type, field, def, mn, mx, scope, apply, opts, flags, caps) \
    { kKey_##id, (uint16_t)offsetof(userOptions, field), SettingType::type, SettingScope::scope, \
      (uint16_t)(def), (uint16_t)(mn), (uint16_t)(mx), (uint16_t)(apply), kOpt_##id, (uint8_t)(flags), (uint8_t)(caps) },
static const SettingDef kSettings[] SET_PROGMEM = { SETTINGS_TABLE(SETTING_ROW) };
#undef SETTING_ROW

uint8_t settingsCount() { return (uint8_t)(sizeof(kSettings) / sizeof(kSettings[0])); }

void settingsDefAt(uint8_t index, SettingDef& out) { memcpy_P(&out, &kSettings[index], sizeof(SettingDef)); }

bool settingsKeyEquals(const SettingDef& d, const char* key) { return strcmp_P(key, d.key) == 0; }

void settingsKeyCopy(const SettingDef& d, char* buf, size_t n) {
    if (n == 0) return;
    strncpy_P(buf, d.key, n - 1);
    buf[n - 1] = '\0';
}

uint8_t settingsCaps(const SettingDef& d) { return d.caps; }

bool settingsIsDevKey(const SettingDef& d) {
    char key[48]; settingsKeyCopy(d, key, sizeof(key));
    return !strncmp(key, "dev.", 4);
}

bool settingsAvailable(const SettingDef& d, bool devmode) {
    if ((d.caps & platformCaps()) != d.caps) return false;
    return devmode || !settingsIsDevKey(d);
}

int settingsFind(const char* key) {
    SettingDef d;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (settingsKeyEquals(d, key)) return i;
    }
    return -1;
}

uint16_t settingsGet(const userOptions& u, const SettingDef& d) {
    const uint8_t* p = (const uint8_t*)&u + d.offset;
    uint16_t v;
    if (d.type == SettingType::U16) memcpy(&v, p, 2); else v = *p;
    if (d.flags & SETTING_FLAG_INVERT) v = v ? 0 : 1;
    return v;
}

void settingsPut(userOptions& u, const SettingDef& d, uint16_t value) {
    if (d.flags & SETTING_FLAG_INVERT) value = value ? 0 : 1;
    uint8_t* p = (uint8_t*)&u + d.offset;
    if (d.type == SettingType::U16) memcpy(p, &value, 2); else *p = (uint8_t)value;
}

// Option i of a "Name[=value][@cap][#a]|..." flash string, read in place (no length limit); '#a' marks an option
// shown only in Advanced menu mode. Returns false past the end.
static bool enumAtFull(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value, uint8_t& caps,
                       bool& advanced) {
    if (!d.options || n == 0) return false;
    const char* p = d.options;
    for (uint8_t idx = 0;; idx++) {
        char tok[48];
        size_t len = 0;
        char c;
        while ((c = (char)pgm_read_byte(p)) != '\0' && c != '|') {
            if (len < sizeof(tok) - 1) tok[len++] = c;
            p++;
        }
        tok[len] = '\0';
        if (idx == i) {
            if (!len) return false;
            advanced = false;
            char* hash = strchr(tok, '#');
            if (hash) { advanced = hash[1] == 'a'; *hash = '\0'; }
            caps = 0;
            char* at = strchr(tok, '@');
            if (at) { *at = '\0'; caps = platformCapFromName(at + 1, strlen(at + 1)); }
            char* eq = strchr(tok, '=');
            value = idx;
            if (eq) { *eq = '\0'; value = (uint16_t)atoi(eq + 1); }
            strncpy(name, tok, n - 1);
            name[n - 1] = '\0';
            return true;
        }
        if (!c) return false;
        p++;
    }
}

bool settingsEnumAtEx(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value, uint8_t& caps) {
    bool advanced;
    return enumAtFull(d, i, name, n, value, caps, advanced);
}

bool settingsOptionAdvanced(const SettingDef& d, uint16_t value) {
    if (d.type != SettingType::Enum) return false;
    char name[32]; uint16_t v; uint8_t caps; bool advanced;
    for (uint8_t i = 0; enumAtFull(d, i, name, sizeof(name), v, caps, advanced); i++)
        if (v == value) return advanced;
    return false;
}

bool settingsEnumAt(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value) {
    uint8_t caps;
    return settingsEnumAtEx(d, i, name, n, value, caps);
}

bool settingsOptionAvailable(const SettingDef& d, uint16_t value) {
    if (d.type != SettingType::Enum) return true;
    char name[32]; uint16_t v; uint8_t caps;
    for (uint8_t i = 0; settingsEnumAtEx(d, i, name, sizeof(name), v, caps); i++)
        if (v == value) return (caps & platformCaps()) == caps;
    return false;
}

uint8_t settingsEnumCount(const SettingDef& d) {
    char name[32]; uint16_t v; uint8_t c = 0;
    while (settingsEnumAt(d, c, name, sizeof(name), v)) c++;
    return c;
}

bool settingsValid(const SettingDef& d, uint16_t value) {
    if (value < d.min || value > d.max) return false;
    if (d.type == SettingType::Bool) return value <= 1;
    if (d.type == SettingType::Enum) {
        char name[32]; uint16_t v;
        for (uint8_t i = 0; settingsEnumAt(d, i, name, sizeof(name), v); i++)
            if (v == value) return true;
        return false;
    }
    return true;
}

uint16_t settingsChangedApply(const userOptions& before, const userOptions& after, SettingScope scope) {
    uint16_t mask = 0;
    SettingDef d;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        if (d.scope == scope && settingsGet(before, d) != settingsGet(after, d)) mask |= d.apply;
    }
    return mask;
}

void settingsApplyDefaults(userOptions& u) {
    SettingDef d;
    for (uint8_t i = 0; i < settingsCount(); i++) {
        settingsDefAt(i, d);
        // def is the user-facing value; settingsPut handles inversion
        settingsPut(u, d, d.def);
    }
    StrSettingDef s;
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        settingsStrDefAt(i, s);
        ((char*)&u + s.offset)[0] = '\0';
    }
}

static bool parseUnsigned(const char* s, uint16_t& out) {
    while (*s == ' ' || *s == '\t') s++;
    if (*s < '0' || *s > '9') return false;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); if (v > 65535) return false; s++; }
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    if (*s != '\0') return false;
    out = (uint16_t)v;
    return true;
}

bool settingsParse(const SettingDef& d, const char* text, uint16_t& out) {
    while (*text == ' ' || *text == '\t') text++;
    uint16_t v;
    if (d.type == SettingType::Bool) {
        if (!strcmp(text, "true") || !strcmp(text, "on")) { out = 1; return true; }
        if (!strcmp(text, "false") || !strcmp(text, "off")) { out = 0; return true; }
    }
    if (d.type == SettingType::Enum) {
        char name[32]; uint16_t ev;
        size_t len = strlen(text);
        while (len && (text[len - 1] == ' ' || text[len - 1] == '\r')) len--;
        for (uint8_t i = 0; settingsEnumAt(d, i, name, sizeof(name), ev); i++)
            if (strlen(name) == len && !strncmp(name, text, len)) { out = ev; return true; }
    }
    if (!parseUnsigned(text, v)) return false;
    if (!settingsValid(d, v)) return false;
    out = v;
    return true;
}

size_t settingsFormat(const SettingDef& d, uint16_t value, char* buf, size_t n) {
    if (d.type == SettingType::Enum) {
        char name[32]; uint16_t ev;
        for (uint8_t i = 0; settingsEnumAt(d, i, name, sizeof(name), ev); i++)
            if (ev == value) { copyStr(buf, n, name); return strlen(buf); }
    }
    char tmp[8]; int len = 0;
    uint16_t v = value; char rev[8];
    do { rev[len++] = (char)('0' + v % 10); v /= 10; } while (v && len < 7);
    for (int i = 0; i < len; i++) tmp[i] = rev[len - 1 - i];
    tmp[len] = '\0';
    copyStr(buf, n, tmp);
    return strlen(buf);
}

// ---- String settings ----
#define STR_KEY(id, key, field, scope, flags) static const char kStrKey_##id[] SET_PROGMEM = key;
SETTINGS_STR_TABLE(STR_KEY)
#undef STR_KEY
#define STR_ROW(id, key, field, scope, flags) \
    { kStrKey_##id, (uint16_t)offsetof(userOptions, field), (uint8_t)(sizeof(((userOptions*)0)->field) - 1), \
      SettingScope::scope, (uint8_t)(flags) },
static const StrSettingDef kStrSettings[] SET_PROGMEM = { SETTINGS_STR_TABLE(STR_ROW) };
#undef STR_ROW

uint8_t settingsStrCount() { return (uint8_t)(sizeof(kStrSettings) / sizeof(kStrSettings[0])); }
void settingsStrDefAt(uint8_t i, StrSettingDef& out) { memcpy_P(&out, &kStrSettings[i], sizeof(out)); }

int settingsStrFind(const char* key) {
    StrSettingDef d;
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        settingsStrDefAt(i, d);
        if (strcmp_P(key, d.key) == 0) return i;
    }
    return -1;
}

void settingsKeyCopyStr(const StrSettingDef& d, char* buf, size_t n) {
    if (n == 0) return;
    strncpy_P(buf, d.key, n - 1);
    buf[n - 1] = '\0';
}

const char* settingsStrGet(const userOptions& u, const StrSettingDef& d) { return (const char*)&u + d.offset; }

// Printable ASCII only, no quote or backslash: values are written to JSON and cfg files unescaped
bool settingsStrValid(const StrSettingDef& d, const char* value) {
    size_t n = strlen(value);
    if (n > d.maxLen) return false;
    for (size_t i = 0; i < n; i++) {
        char c = value[i];
        if (c < 0x20 || c > 0x7E || c == '"' || c == '\\') return false;
    }
    if (strcmp_P("system.extra_hosts", d.key) == 0) return guardExtraHostsValid(value);   // names the Host guard matches
    return true;
}

bool settingsStrPut(userOptions& u, const StrSettingDef& d, const char* value) {
    if (!settingsStrValid(d, value)) return false;
    memcpy((char*)&u + d.offset, value, strlen(value) + 1);
    return true;
}

bool settingsKeyExcludedFromBackup(const char* key) {
    int i = settingsFind(key);
    if (i >= 0) { SettingDef d; settingsDefAt((uint8_t)i, d); return settingKeptOutOfBackups(d.flags); }
    i = settingsStrFind(key);
    if (i >= 0) { StrSettingDef d; settingsStrDefAt((uint8_t)i, d); return settingKeptOutOfBackups(d.flags); }
    return false;
}
