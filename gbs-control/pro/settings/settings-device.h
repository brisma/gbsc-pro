#pragma once
#include <memory>
#include "settings-types.h"
#include "settings-fs.h"
#include "settings-saver.h"

#define SETTINGS_DEVICE_PATH  "/cfg/device.cfg"
#define SETTINGS_CURRENT_PATH "/cfg/current.cfg"

void settingsInit();
void settingsMarkDirty(SettingScope s);
void settingsLoop();
void settingsFlush();
void settingsFactoryReset();
void settingsDiscardPending();                      // drop unsaved changes (used when /cfg files are restored)
void settingsSaverPause(uint8_t reason, bool on);   // SAVER_HOLD_*: no debounced saves while any hold is set
// A backup download reads the files in pieces: no settings save and no profile operation replaces one meanwhile (a
// spliced file). Each piece refreshes it; it lapses kBackupHoldMs after the last one (a stalled client)
void settingsBackupStreaming(bool on);
bool settingsBackupBusy();
SettingsFs& settingsFs();
void settingsApply(uint16_t mask);
uint16_t settingsRevision();   // bumped on every change, lets the web UI detect stale data
void settingsBroadcastChange(uint8_t index);   // settings-web.cpp: "s:key=value" to WebSocket clients

class AsyncWebServerRequest;
// Every API response is JSON and never cached (settings-web.cpp)
void sendJson(AsyncWebServerRequest* req, int code, const char* json);
class __FlashStringHelper;
void sendJson(AsyncWebServerRequest* req, int code, const __FlashStringHelper* json);   // F("..."): literal in flash
// Chunked JSON rebuilt per chunk (api-json.h jsonReplayFill): no response buffer; build must be deterministic for
// `state` (the shared_ptr keeps it alive until the response ends)
void sendJsonReplay(AsyncWebServerRequest* req, void (*build)(void (*sink)(const char*, void*), void*, const void*),
                    std::shared_ptr<const void> state);
