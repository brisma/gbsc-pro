#pragma once
#include <stdint.h>
#include <stddef.h>
#include "settings-types.h"
#include "settings-table.h"

// Generated ids: SID_<table id> == the setting's index (menu tree references, compile-time checked)
#define SETTING_ENUM(id, ...) SID_##id,
enum SettingId : uint8_t { SETTINGS_TABLE(SETTING_ENUM) SID_COUNT };
#undef SETTING_ENUM

struct userOptions;

uint8_t settingsCount();
void settingsDefAt(uint8_t index, SettingDef& out);
int settingsFind(const char* key);
bool settingsKeyEquals(const SettingDef& d, const char* key);
void settingsKeyCopy(const SettingDef& d, char* buf, size_t n);
uint8_t settingsCaps(const SettingDef& d);
bool settingsIsDevKey(const SettingDef& d);                    // key starts with "dev."
bool settingsAvailable(const SettingDef& d, bool devmode);     // hardware present (and devmode for dev.*)
bool settingsOptionAvailable(const SettingDef& d, uint16_t value);   // enum option's hardware present
bool settingsOptionAdvanced(const SettingDef& d, uint16_t value);    // enum option marked '#a' (Advanced menu only)

uint16_t settingsGet(const userOptions& u, const SettingDef& d);
void settingsPut(userOptions& u, const SettingDef& d, uint16_t value);
bool settingsValid(const SettingDef& d, uint16_t value);
void settingsApplyDefaults(userOptions& u);
// Apply groups of the settings of this scope whose values differ (a profile Load or Defaults applies only what changed)
uint16_t settingsChangedApply(const userOptions& before, const userOptions& after, SettingScope scope);

bool settingsParse(const SettingDef& d, const char* text, uint16_t& out);
size_t settingsFormat(const SettingDef& d, uint16_t value, char* buf, size_t n);   // labels: menus and the WebSocket echo
// Files: an Enum's option id, a Bool's on/off, the number for the rest (and for an Enum value outside its list)
size_t settingsFormatStored(const SettingDef& d, uint16_t value, char* buf, size_t n);
// An option's id: its label in lower case, every run of other characters than a-z and 0-9 one '_', none at the ends;
// 0 (and "") when it does not fit
size_t settingsIdFromLabel(const char* label, char* out, size_t n);
// The id files hold for an Enum option: its explicit "~id", else its derived id; false when value is not an option
bool settingsOptionId(const SettingDef& d, uint16_t value, char* id, size_t n);

uint8_t settingsEnumCount(const SettingDef& d);
bool settingsEnumAt(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value);
bool settingsEnumAtEx(const SettingDef& d, uint8_t i, char* name, size_t n, uint16_t& value, uint8_t& caps);

// String settings (separate table; values are char[] fields in uopt)
uint8_t settingsStrCount();
void settingsStrDefAt(uint8_t i, StrSettingDef& out);
int settingsStrFind(const char* key);
void settingsKeyCopyStr(const StrSettingDef& d, char* buf, size_t n);
const char* settingsStrGet(const userOptions& u, const StrSettingDef& d);
bool settingsStrValid(const StrSettingDef& d, const char* value);   // printable ASCII, no quote/backslash, fits
bool settingsStrPut(userOptions& u, const StrSettingDef& d, const char* value);   // false: too long or bad chars
bool settingsKeyExcludedFromBackup(const char* key);   // settingKeptOutOfBackups (numeric or string)
