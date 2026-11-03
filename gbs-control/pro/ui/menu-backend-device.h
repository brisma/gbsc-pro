// MenuBackend on the device: reads uopt/rto/GBS registers, writes through the web queues
#pragma once
#include "menu-backend.h"

class DeviceMenuBackend : public MenuBackend {
public:
    void refreshSignal();        // cheap fields (no blocking reads): before every key and every 250 ms
    void refreshRates();         // blocking vsync measurements: only while the info panel is open with a signal

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
    bool actionsIdle() override;
    const MenuSignal& signal() override;
    uint8_t caps() override;
    bool devMode() override;
    uint8_t menuMode() override;
    uint8_t activeInput() override;
    uint8_t basePreset() override;
    void saverHold(bool on) override;
    void noSignalEnter() override;
    void noSignalExit(bool restoreBlanking) override;
    void tvOsd(uint8_t nav) override;
    const ProfileList& profiles() override;
    int profileActive() override;
    uint8_t profileMatches(uint8_t* out, uint8_t max) override;
    bool profileModified() override;
    bool profileOp(uint8_t popKind, int8_t index, const char* name, uint8_t region) override;
    uint8_t profileOpsPending() override;
    uint16_t profileResultSeq() override;
    bool profileResultFor(uint16_t seq, char* out, size_t n) override;

private:
    uint16_t vperiod_ = 0;       // VPERIOD_IF of the last refreshSignal (profileMatches reads no I2C)
    char srcName_[PROFILE_NAME_BUF] = "Profile";   // LT_SOURCE_NAME, refreshed with the signal outside preset stages
    MenuSignal sig_ = {};
};
