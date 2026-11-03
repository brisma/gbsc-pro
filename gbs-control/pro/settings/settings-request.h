#pragma once
#include <stdint.h>
#include "settings-types.h"
#include "settings-store.h"

struct SettingsPair { uint8_t index; uint16_t value; };
struct SettingsValidateResult { uint8_t okCount; uint8_t errCount; uint8_t pairCount; };   // pairCount: entries in out[]
struct SettingsStrWrite { int8_t index; char value[64]; };   // string setting write; index -1: none

// Checks key/value pairs against the registry without touching uopt (runs in the network context).
// Valid pairs go to `out`; each error is emitted as "key":"reason" (comma-separated) to errSink.
SettingsValidateResult settingsValidateRequest(const char* const* keys, const char* const* values, uint8_t n,
                                               bool devmode, SettingsSink errSink, void* errCtx,
                                               SettingsPair* out, uint8_t maxOut, SettingsStrWrite* strOut);
// strOut: receives at most one writable (not device-only) string setting; nullptr makes string writes "readonly"
