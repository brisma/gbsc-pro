#pragma once
#include <stdint.h>
#include <stddef.h>
#include "update-source.h"
class AsyncWebServerRequest;

static const uint32_t UPDATE_MAX_SKETCH = 1044464;   // eagle.flash.4m1m sketch cap

void updateUploadChunk(AsyncWebServerRequest* req, size_t index, uint8_t* data, size_t len, bool final);
int updateUploadFinish(AsyncWebServerRequest* req);     // HTTP status: 202 accepted (verified by the loop), 4xx otherwise
void updateLoop();            // Update.end() + reboot or error report; progress screens
bool updateInProgress();      // any source, verifying and the restart wait included
bool updateUploadActive();    // a web upload (other web requests get 503 meanwhile; an OTA pull does not)
UpdateSource updateSource();  // US_NONE when idle
const char* updateLastError();   // the last failure reason ("format", "memory", "space", "write", "size", "sha", ...)

// Shared Updater core (web upload and OTA pull). Begin runs from the loop (pull) or the first upload chunk; writes
// run in the network context; Finish only moves to VERIFY, updateLoop() checks the size and sha256 (pull), the image
// trailer, then Update.end() and the restart. false: busy (another update or the ADV Controller update), memory or
// space (the reason in updateLastError(), reported by updateLoop())
bool updateCoreBegin(UpdateSource src, uint32_t total, const uint8_t* expectSha);
bool updateCoreWrite(const uint8_t* d, size_t n);   // false: stop feeding (format, write); the state is FAILED
void updateCoreFinish(UpdateSource src);            // only for that source's run
void updateCoreFail(UpdateSource src, const char* reason);
uint8_t updateProgress();     // 0-100
uint8_t updateStage();        // 0 receiving, 1 verifying, 2 verified (restart in 2.5 s): UPD_* of menu-types.h
// Last web upload for /api/info "upload": {"state":"idle|receiving|verifying|done|failed","error":..,"received":n,"total":n}
int updateUploadJson(char* out, size_t n);
