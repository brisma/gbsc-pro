// OTA over plain HTTP (device only): the
// recommended check (the build channel and the more stable ones), Other channels, the install through the shared
// Updater core, the daily check. One job at a time, driven by otaLoop()
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ota-plan.h"

#ifndef GBSC_OTA_BASE
#define GBSC_OTA_BASE "http://gbsc.brisma.it/v1/"
#endif

void otaCheck(bool fromUser);                 // the recommended update; fromUser false = the daily check
void otaCheckChannel(uint8_t ch);             // Other channels (always from the user)
// Installs the recommended (manual false) or the Other channels offer: the latest one (no version token, a check
// finishing between the confirm and the install is accepted). false: nothing offered, busy,
// no Wi-Fi, or the Updater refused (memory, space); every refusal is shown and logged
bool otaInstall(bool manual);
void otaLoop();                               // loop(), every pass (also during an install)
bool otaBusy();                               // a check or an install runs
const OtaOffer& otaRecommended();
const OtaOffer& otaManualOffer();
const char* otaLastResult();                  // "" before any check, "none", "<version>" or "error:<code>"
// Result text for the menus: fromUser false for the daily check (the menu shows only "available" then)
void otaSetMenuHook(void (*hook)(const char* text, bool fromUser));
int otaInfoJson(char* out, size_t n);         // /api/info "update" object
