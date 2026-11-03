// Host fake of MenuBackend: the real settings registry on a local userOptions, a log of every call,
// a pending-write model (apply is visible through value() at once, stored only after settle())
#pragma once
#include <map>
#include <string>
#include <vector>
#include "options.h"
#include "menu-backend.h"
#include "profile-list.h"

class FakeBackend : public MenuBackend {
public:
    userOptions u;
    MenuSignal sig;
    int32_t lives[LV_COUNT];
    std::string texts[LT_COUNT];
    std::vector<std::string> log;
    std::map<uint8_t, uint16_t> pending;
    bool queueFull = false;
    int queueRoom = -1;                                       // >= 0: applyMany of more pairs fails (device queue)
    bool dev = false;
    bool termination = false;
    ProfileList list;
    int active = -1;
    bool modified = false;
    uint16_t resultSeq = 0;
    std::string result;                                       // result of operation number resultSeq
    std::map<uint16_t, std::string> results;                  // earlier results by sequence number
    uint8_t opsPending = 0;
    bool textBusy = false;
    bool actionsBusy = false;                                 // queued actions not yet run (legacy commands)
    std::vector<uint8_t> matches;                             // profileMatches(): profiles of the current signal

    FakeBackend();
    void settle();                                            // store every pending write

    uint16_t value(uint8_t sid) override;
    bool settled(uint8_t sid) override;
    bool applyMany(const MenuWrite* w, uint8_t n) override;
    bool run(uint8_t aid) override;
    bool fn(uint8_t menuFn) override;
    bool toggleGet(uint8_t menuFn) override;
    void toggleSet(uint8_t menuFn, bool on) override;
    int32_t live(uint8_t lv) override;
    void text(uint8_t lt, char* out, size_t n) override;
    bool setText(uint8_t lt, const char* v) override;
    bool actionsIdle() override { return !actionsBusy; }
    const MenuSignal& signal() override { return sig; }
    uint8_t caps() override;
    bool devMode() override { return dev; }
    uint8_t menuMode() override;
    uint8_t activeIn = 1;                                     // what Auto found; a manual choice (1..6) wins
    uint8_t activeInput() override;
    uint8_t base = 0;                                         // basePreset(): the active profile's base preset
    uint8_t basePreset() override { return base; }
    void saverHold(bool on) override;
    void noSignalEnter() override { log.push_back("nosignal enter"); }
    void noSignalExit(bool restore) override { log.push_back(restore ? "nosignal exit 1" : "nosignal exit 0"); }
    void tvOsd(uint8_t nav) override { log.push_back(std::string("tvosd ") + (char)('0' + nav)); }
    const ProfileList& profiles() override { return list; }
    int profileActive() override { return active; }
    uint8_t profileMatches(uint8_t* out, uint8_t max) override {
        uint8_t n = 0;
        for (uint8_t m : matches) if (n < max) out[n++] = m;
        return n;
    }
    bool profileModified() override { return modified; }
    bool profileOp(uint8_t popKind, int8_t index, const char* name = nullptr, uint8_t region = PROFILE_REGION_NONE) override;
    uint8_t profileOpsPending() override { return opsPending; }
    uint16_t profileResultSeq() override { return resultSeq; }
    bool profileResultFor(uint16_t seq, char* out, size_t n) override;
    void finishOp(const char* res) { resultSeq++; result = res; results[resultSeq] = res; if (opsPending) opsPending--; }
};
