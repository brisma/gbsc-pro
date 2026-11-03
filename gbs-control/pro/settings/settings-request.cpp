#include <stdint.h>
#include <string.h>
#include "settings-registry.h"
#include "settings-request.h"

static void reportError(SettingsSink sink, void* ctx, uint8_t& count, const char* key, const char* reason) {
    if (count) sink(",", ctx);
    sink("\"", ctx); sink(key, ctx); sink("\":\"", ctx); sink(reason, ctx); sink("\"", ctx);
    count++;
}

SettingsValidateResult settingsValidateRequest(const char* const* keys, const char* const* values, uint8_t n,
                                               bool devmode, SettingsSink errSink, void* errCtx,
                                               SettingsPair* out, uint8_t maxOut, SettingsStrWrite* strOut) {
    SettingsValidateResult r = {0, 0, 0};
    for (uint8_t i = 0; i < n; i++) {
        int idx = settingsFind(keys[i]);
        if (idx < 0) {
            int s = settingsStrFind(keys[i]);
            if (s < 0) { reportError(errSink, errCtx, r.errCount, keys[i], "unknown"); continue; }
            StrSettingDef sd; settingsStrDefAt((uint8_t)s, sd);
            if (!strOut || (sd.flags & SETTING_FLAG_DEVICE_ONLY)) {
                reportError(errSink, errCtx, r.errCount, keys[i], "readonly"); continue;
            }
            if (strOut->index >= 0 || !settingsStrValid(sd, values[i])) {
                reportError(errSink, errCtx, r.errCount, keys[i], "invalid"); continue;
            }
            strOut->index = (int8_t)s;
            strncpy(strOut->value, values[i], sizeof(strOut->value) - 1);
            strOut->value[sizeof(strOut->value) - 1] = '\0';
            r.okCount++;
            continue;
        }
        SettingDef d; settingsDefAt((uint8_t)idx, d);
        if (d.flags & SETTING_FLAG_DEVICE_ONLY) { reportError(errSink, errCtx, r.errCount, keys[i], "readonly"); continue; }
        if (!settingsAvailable(d, devmode)) { reportError(errSink, errCtx, r.errCount, keys[i], "unavailable"); continue; }
        uint16_t v;
        if (!settingsParse(d, values[i], v)) { reportError(errSink, errCtx, r.errCount, keys[i], "invalid"); continue; }
        if (!settingsOptionAvailable(d, v)) { reportError(errSink, errCtx, r.errCount, keys[i], "unavailable"); continue; }
        if (r.pairCount >= maxOut) { reportError(errSink, errCtx, r.errCount, keys[i], "invalid"); continue; }
        out[r.pairCount].index = (uint8_t)idx;   // own counter: okCount also counts the string write
        out[r.pairCount].value = v;
        r.pairCount++;
        r.okCount++;
    }
    return r;
}
