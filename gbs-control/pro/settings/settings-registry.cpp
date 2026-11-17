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

// Options of a "Name[=value][~id][@cap][#a]|..." flash string, read in place: optionNext() reads the option at p and
// moves p past it (false at the end). '#a' marks an option shown only in Advanced menu mode; '~id' is an explicit id
// (when the derived one is not wanted, or a renamed label keeps its old id); xid gets it, "" without. Every lookup
// below walks the string once.
static bool optionNext(const char*& p, uint8_t idx, char* name, size_t n, uint16_t& value, uint8_t& caps,
                       bool& advanced, char* xid = nullptr, size_t xn = 0) {
    if (!p) return false;
    char tok[48];
    size_t len = 0;
    char c;
    while ((c = (char)pgm_read_byte(p)) != '\0' && c != '|') {
        if (len < sizeof(tok) - 1) tok[len++] = c;
        p++;
    }
    if (c == '|') p++; else if (!len) { p = nullptr; return false; } else p = nullptr;
    tok[len] = '\0';
    if (!len) return false;
    advanced = false;
    char* hash = strchr(tok, '#');
    if (hash) { advanced = hash[1] == 'a'; *hash = '\0'; }
    caps = 0;
    char* at = strchr(tok, '@');
    if (at) { *at = '\0'; caps = platformCapFromName(at + 1, strlen(at + 1)); }
    char* tilde = strchr(tok, '~');
    if (xn) xid[0] = '\0';
    if (tilde) { *tilde = '\0'; if (xn) { strncpy(xid, tilde + 1, xn - 1); xid[xn - 1] = '\0'; } }
    char* eq = strchr(tok, '=');
    value = idx;
    if (eq) { *eq = '\0'; value = (uint16_t)atoi(eq + 1); }
    if (n) { strncpy(name, tok, n - 1); name[n - 1] = '\0'; }
    return true;
}

// Option i (by position)
static bool enumAtFull(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value, uint8_t& caps,
                       bool& advanced) {
    if (!d.options || n == 0) return false;
    const char* p = d.options;
    for (uint8_t idx = 0; optionNext(p, idx, name, n, value, caps, advanced); idx++)
        if (idx == i) return true;
    return false;
}

// The option holding value
static bool enumFindValue(const SettingDef& d, uint16_t value, char* name, size_t n, uint8_t& caps, bool& advanced) {
    if (!d.options) return false;
    const char* p = d.options;
    char tmp[48]; uint16_t v;
    for (uint8_t idx = 0; optionNext(p, idx, tmp, sizeof(tmp), v, caps, advanced); idx++)
        if (v == value) { if (n) { strncpy(name, tmp, n - 1); name[n - 1] = '\0'; } return true; }
    return false;
}

// The option whose id (explicit, or its normalised label) is id (digits-only ids are left to the number parser)
static bool enumFindId(const SettingDef& d, const char* id, uint16_t& value) {
    if (!d.options) return false;
    const char* p = d.options;
    char label[48], oid[48], xid[32]; uint16_t v; uint8_t caps; bool adv;
    for (uint8_t idx = 0; optionNext(p, idx, label, sizeof(label), v, caps, adv, xid, sizeof(xid)); idx++) {
        if ((xid[0] && !strcmp(xid, id)) || (settingsIdFromLabel(label, oid, sizeof(oid)) && !strcmp(oid, id))) {
            value = v;
            return true;
        }
    }
    return false;
}

bool settingsOptionId(const SettingDef& d, uint16_t value, char* id, size_t n) {
    if (d.type != SettingType::Enum || !d.options || !n) return false;
    const char* p = d.options;
    char label[48], xid[32]; uint16_t v; uint8_t caps; bool adv;
    for (uint8_t idx = 0; optionNext(p, idx, label, sizeof(label), v, caps, adv, xid, sizeof(xid)); idx++) {
        if (v != value) continue;
        if (xid[0]) { strncpy(id, xid, n - 1); id[n - 1] = '\0'; return true; }
        return settingsIdFromLabel(label, id, n) > 0;
    }
    return false;
}

size_t settingsIdFromLabel(const char* label, char* out, size_t n) {
    if (!n) return 0;
    size_t len = 0;
    bool gap = false;
    for (const char* c = label; *c; c++) {
        char ch = *c;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        bool keep = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
        if (!keep) { gap = len > 0; continue; }
        if (gap) { if (len + 1 >= n) { out[0] = '\0'; return 0; } out[len++] = '_'; gap = false; }
        if (len + 1 >= n) { out[0] = '\0'; return 0; }
        out[len++] = ch;
    }
    out[len] = '\0';
    return len;
}

bool settingsEnumAtEx(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value, uint8_t& caps) {
    bool advanced;
    return enumAtFull(d, i, name, n, value, caps, advanced);
}

bool settingsOptionAdvanced(const SettingDef& d, uint16_t value) {
    if (d.type != SettingType::Enum) return false;
    uint8_t caps; bool advanced;
    return enumFindValue(d, value, nullptr, 0, caps, advanced) && advanced;
}

bool settingsEnumAt(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value) {
    uint8_t caps;
    return settingsEnumAtEx(d, i, name, n, value, caps);
}

bool settingsOptionAvailable(const SettingDef& d, uint16_t value) {
    if (d.type != SettingType::Enum) return true;
    uint8_t caps; bool advanced;
    return enumFindValue(d, value, nullptr, 0, caps, advanced) && (caps & platformCaps()) == caps;
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
        uint8_t caps; bool advanced;
        return enumFindValue(d, value, nullptr, 0, caps, advanced);
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

// A plain number (the API) first, then for an Enum or a Bool the text normalised to an id: an option's id is its
// normalised label, so "force_4_3", "Force 4:3" and "force 4:3" are one option. Signs and marks are not numbers.
bool settingsParse(const SettingDef& d, const char* text, uint16_t& out) {
    uint16_t v;
    if (parseUnsigned(text, v)) {
        if (!settingsValid(d, v)) return false;
        out = v;
        return true;
    }
    if (d.type != SettingType::Enum && d.type != SettingType::Bool) return false;
    char id[32];
    if (!settingsIdFromLabel(text, id, sizeof(id))) return false;
    for (const char* c = id; ; c++) {                       // digits only: a number with signs or marks around it
        if (!*c) return false;
        if (*c < '0' || *c > '9') break;
    }
    if (d.type == SettingType::Bool) {
        if (!strcmp(id, "on") || !strcmp(id, "true")) { out = 1; return true; }
        if (!strcmp(id, "off") || !strcmp(id, "false")) { out = 0; return true; }
        return false;
    }
    return enumFindId(d, id, out);
}

static size_t formatNumber(uint16_t value, char* buf, size_t n) {
    char tmp[8]; int len = 0;
    uint16_t v = value; char rev[8];
    do { rev[len++] = (char)('0' + v % 10); v /= 10; } while (v && len < 7);
    for (int i = 0; i < len; i++) tmp[i] = rev[len - 1 - i];
    tmp[len] = '\0';
    copyStr(buf, n, tmp);
    return strlen(buf);
}

size_t settingsFormat(const SettingDef& d, uint16_t value, char* buf, size_t n) {
    if (d.type == SettingType::Enum) {
        uint8_t caps; bool advanced;
        if (enumFindValue(d, value, buf, n, caps, advanced)) return strlen(buf);
    }
    return formatNumber(value, buf, n);
}

size_t settingsFormatStored(const SettingDef& d, uint16_t value, char* buf, size_t n) {
    if (d.type == SettingType::Bool) { copyStr(buf, n, value ? "on" : "off"); return strlen(buf); }
    if (d.type == SettingType::Enum && settingsOptionId(d, value, buf, n)) return strlen(buf);
    return formatNumber(value, buf, n);
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
