// OTA over plain HTTP (device only): the check of the chosen channel (system.update_channel) and the more stable
// ones, the install through the shared Updater core, the daily check. One job at a time, driven by otaLoop()
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ota-plan.h"

#ifndef GBSC_OTA_BASE
#define GBSC_OTA_BASE "http://gbsc.brisma.it/v1/"
#endif

void otaCheck(bool fromUser);                 // fromUser false = the daily check (no toast unless a newer version)
// Installs the offer: the latest one, when the request confirmed its kind (olderConfirmed: the user was asked about
// an older build). false: nothing offered or it changed, busy, no Wi-Fi, or the Updater refused (memory, space);
// every refusal is shown and logged
bool otaInstall(bool olderConfirmed);
void otaLoop();                               // loop(), every pass (also during an install)
bool otaBusy();                               // a check or an install runs
// The offer and the last result count for the channel they were worked out for: nothing / "" once the setting changed
const OtaOffer& otaOffer();
const char* otaLastResult();                  // "" before any check, "none", "<version>" or "error:<code>"
// Result text for the menus: fromUser false for the daily check (the menu shows only a newer version then)
void otaSetMenuHook(void (*hook)(const char* text, bool fromUser));
int otaInfoJson(char* out, size_t n);         // /api/info "update" object
