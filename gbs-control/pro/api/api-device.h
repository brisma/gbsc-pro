// Device glue for the unified API: queues filled by network handlers, drained by loop()
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../settings/settings-request.h"

bool apiQueueSettings(const SettingsPair* pairs, uint8_t n);   // all or nothing
bool apiQueueStrSetting(const SettingsStrWrite& w);            // one pending string write at a time
bool apiActionsIdle();                                         // no queued action and no legacy command left
void apiDropStrSetting();
void apiLoop();                                                // call from loop()

struct ActionRequest { uint8_t index; uint8_t len; uint8_t data[30]; };   // data: dev.adv_i2c triplets
bool apiQueueAction(const ActionRequest& r);                    // false when the queue is full
bool apiActionQueueFull();
bool screenResetToPreset();   // Screen > Reset: the running mode back to its base preset (the active profile forgets it); false: busy
const uint8_t* apiSnapshot();
// dev.tv5725_read: a read is announced in the network handler (its gen goes back in the POST response) and run by
// the loop; GET /api/dev/regs reads the result. state: 0 idle, 1 pending, 2 done, 3 busy (an update was running)
enum : uint8_t { REGS_IDLE, REGS_PENDING, REGS_DONE, REGS_BUSY };
struct TvRegsResult { uint8_t state; uint8_t seg; uint8_t gen; const uint8_t* v; };   // v: 256 values when done
uint8_t apiRegsAnnounce(uint8_t seg);    // network context: pending, returns the gen the result will carry
TvRegsResult apiRegsResult();   // dev.screen_snapshot result (snap::kBytes), null before the first one
// GET /api/live: the loop reads every MenuLive readback once a second while a page asked within the last 10 s
// (never while an update runs); the network handler answers from that cache
void apiLiveAsked();                                   // network context
size_t apiLiveJson(char* out, size_t cap);             // network context: the cache as liveJson
bool apiPendingValue(uint8_t index, uint16_t& v);   // value of a queued write or of a bridged char not run yet

class AsyncWebServer;
class AsyncWebServerRequest;
bool apiGuardRequest(AsyncWebServerRequest* req);   // Host/Origin/X-GBSC rules (api-web.cpp)
void apiWebRegister(AsyncWebServer& server);        // middleware + /api routes
void apiSetReady();   // setup() mounted LittleFS and read the settings: requests stop answering 503 "starting"
void apiWebSocketGuard();                           // WebSocket handshake needs a valid Origin

// Restore upload (POST /api/system/restore): chunks go to a temp file, the loop validates and applies
void apiRestoreChunk(AsyncWebServerRequest* req, size_t index, const uint8_t* data, size_t len, bool final);
int apiRestoreFinish(AsyncWebServerRequest* req);   // HTTP status: 202 accepted, 403 guard, 413 too large, 409 another restore runs, 400 no upload

bool apiQueueWifiConnect(const char* ssid, const char* pass);   // false: invalid or already pending
void apiQueueWifiReset();                                        // forget the network, reboot into AP mode

void apiWebRegisterSafe(AsyncWebServer& server);   // safe mode: guard + info/backup/update/restart
void apiSafeLoop();                                // safe mode: performs a requested restart
