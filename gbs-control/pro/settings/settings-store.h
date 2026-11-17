#pragma once
#include <stdint.h>
#include "settings-types.h"

struct userOptions;

static const uint16_t SETTINGS_SCHEMA_VERSION = 2;   // 2: choices written as option ids

typedef void (*SettingsSink)(const char* text, void* ctx);

struct SettingsLoadReport {
    uint16_t applied;
    uint16_t unknown;
    uint16_t invalid;
    uint16_t schema;
};

void settingsSerialize(const userOptions& u, SettingScope scope, SettingsSink sink, void* ctx);
void settingsSerializeEx(const userOptions& u, SettingScope scope, bool withSchema, SettingsSink sink, void* ctx);
uint32_t settingsScopeCrc(const userOptions& u, SettingScope scope);   // CRC32 of the serialized scope (no schema line)
void settingsLoadBegin(userOptions& u, SettingScope scope, SettingsLoadReport& rep);
void settingsLoadLine(userOptions& u, SettingScope scope, const char* line, SettingsLoadReport& rep);
// Both skip settings (and enum options) whose hardware is missing; dev.* only with devmode
// Restore: defaults for the scope except secret/device-only keys, which keep their current value
void settingsRestoreBegin(userOptions& u, SettingScope scope);
void settingsRestoreLine(userOptions& u, SettingScope scope, const char* line);   // ignores excluded keys
void settingsValuesJson(const userOptions& u, bool devmode, SettingsSink sink, void* ctx);
void settingsSchemaJson(bool devmode, SettingsSink sink, void* ctx);
bool settingsSchemaEntry(uint16_t n, bool devmode, SettingsSink sink, void* ctx);   // false past the last entry

// The schema as a chunked stream: fill() copies up to max bytes, 0 at the end. RAM use is one 512 B buffer; an entry
// longer than it is rendered again for each piece, skipping the bytes already sent. bufCap < 512: tests only.
class SchemaStreamer {
public:
    explicit SchemaStreamer(bool devmode, size_t bufCap = 512) : dev_(devmode), cap_(bufCap < 512 ? bufCap : 512) {}
    size_t fill(char* out, size_t max);
private:
    bool next();                          // renders the next piece into buf_; false at the end
    bool dev_;
    uint16_t n_ = 0;
    uint8_t stage_ = 0;                   // 0 "[", 1 entries, 2 "]", 3 done
    bool first_ = true;
    char buf_[512];
    size_t cap_;
    size_t len_ = 0, off_ = 0;
    uint16_t entrySkip_ = 0;              // bytes of the current entry already sent
    bool entryMore_ = false;              // the current entry did not fit: the next piece continues it
};
