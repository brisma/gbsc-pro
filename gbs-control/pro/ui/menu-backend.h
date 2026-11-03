// Everything the menu engine reads or changes goes through this interface.
// Device: menu-backend-device.cpp; host: test/sim/menu-fake.*
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "menu-types.h"
#include "../profiles/profile-names.h"

class ProfileList;
struct MenuWrite { uint8_t sid; uint16_t v; };

class MenuBackend {
public:
    virtual ~MenuBackend() {}
    virtual uint16_t value(uint8_t sid) = 0;                  // effective value (a queued write included)
    virtual bool settled(uint8_t sid) = 0;                     // no write pending for this setting
    virtual bool applyMany(const MenuWrite* w, uint8_t n) = 0; // all or nothing (up to 12); false: queue full
    bool apply(uint8_t sid, uint16_t v) { MenuWrite w = {sid, v}; return applyMany(&w, 1); }
    virtual bool run(uint8_t aid) = 0;                         // false: queue full
    virtual bool actionsIdle() = 0;                            // every queued action has run (legacy commands too)
    virtual bool fn(uint8_t menuFn) = 0;                       // FN_DECODER_RESET, FN_GEOMETRY_RESET, FN_WIFI_RESET
    virtual bool toggleGet(uint8_t menuFn) = 0;                // FN_TERMINATION
    virtual void toggleSet(uint8_t menuFn, bool on) = 0;
    virtual int32_t live(uint8_t lv) = 0;                      // MenuLive readbacks (raw values)
    virtual void text(uint8_t lt, char* out, size_t n) = 0;    // MenuText strings
    virtual bool setText(uint8_t lt, const char* v) = 0;       // LT_OTA_PASSWORD / LT_EXTRA_HOSTS; false: busy
    virtual const MenuSignal& signal() = 0;
    virtual uint8_t caps() = 0;
    virtual bool devMode() = 0;
    virtual uint8_t menuMode() = 0;                            // system.menu_mode: 0 Easy, 1 Advanced
    virtual uint8_t activeInput() = 0;                         // the input the hardware is on (1..6); input.source is the choice
    virtual uint8_t basePreset() = 0;                          // output preset behind a profile's Custom
    virtual void saverHold(bool on) = 0;
    virtual void noSignalEnter() = 0;
    // Ends the 720x480 free-run (detection resumes); the saved blanking comes back only when restoreBlanking
    // (not after a Source change: it was saved under the old preset)
    virtual void noSignalExit(bool restoreBlanking) = 0;
    virtual void tvOsd(uint8_t nav) = 0;                       // TVOSD_*: the TV5725 icon OSD on a standard board
    virtual const ProfileList& profiles() = 0;
    virtual int profileActive() = 0;                           // -1: none
    // Profiles of the current signal, ranked as auto-load ranks them; 0 without a stable signal
    virtual uint8_t profileMatches(uint8_t* out, uint8_t max) = 0;
    virtual bool profileModified() = 0;
    // ProfileOpKind; name for SAVEAS/RENAME (nullptr otherwise); region index for SAVEAS; false: queue full
    virtual bool profileOp(uint8_t popKind, int8_t index, const char* name = nullptr, uint8_t region = PROFILE_REGION_NONE) = 0;
    virtual uint8_t profileOpsPending() = 0;                   // operations queued before the next one
    virtual uint16_t profileResultSeq() = 0;                   // bumps when an operation finished
    virtual bool profileResultFor(uint16_t seq, char* out, size_t n) = 0;   // result of operation number seq
};
