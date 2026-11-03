// ADV Controller status texts for the menu and /api/info (pure; the version string comes off the wire: escaped/filtered)
#pragma once
#include <stddef.h>
#include "adv-link-client.h"

void advControllerMenuText(const AdvControllerInfo& h, char* out, size_t n);          // OSD charset only
// update: ADV Controller update state for the web ("idle", "running", "done", "failed:<step>"; not JSON-escaped)
// sig: the last SIGNAL report, appended as "signal":{...} while the ADV Controller is OK; nullptr: no field
bool advControllerStatusJson(const AdvControllerInfo& h, bool updateAvailable, const char* update, char* out, size_t n,
                    const AdvSignal* sig = nullptr);   // false: too small
#define ADV_CONTROLLER_JSON_MAX 512   // worst case of advControllerStatusJson (host test), the /api/info buffer
// Decoder standard for the menu (AD_RESULT 0..7) or -1 when not locked: SIGNAL when fresh, else INFO
// GET /api/adv/read body; announcedGen != r.gen: the read is still queued ({"state":"pending","gen":announced}).
// Returns the length, 0 when it does not fit
#define ADV_REG_READ_JSON_MAX 256
size_t advRegReadJson(const AdvRegRead& r, uint8_t announcedGen, char* out, size_t n);
int8_t advControllerStandard(const AdvControllerInfo& h, const AdvSignal* sig, bool signalFresh);
