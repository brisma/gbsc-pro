#include <stdio.h>
#include <string.h>
#include "menu-fake.h"
#include "settings-registry.h"
#include "api-actions.h"
#include "platform-caps.h"
#include "profile-store.h"

uint8_t FakeBackend::menuMode() { return (uint8_t)value(SID_sys_menu_mode); }

FakeBackend::FakeBackend() {
    memset(&u, 0, sizeof(u));
    settingsApplyDefaults(u);
    u.menuMode = 1;   // Advanced: tests see the whole tree unless they set Easy (0)
    memset(&sig, 0, sizeof(sig));
    sig.updatePct = -1;
    sig.advStandard = -1;
    sig.scanlinesAllowed = true;
    memset(lives, 0, sizeof(lives));
    texts[LT_WIFI_PASS] = "qqqqqqqq";
    texts[LT_WIFI_HOST] = "gbscontrol.local";
    texts[LT_SOURCE_NAME] = "RGBs 240p";
    texts[LT_ADV_CONTROLLER_NEW] = "3.0.1";
    texts[LT_FW_GBS] = "3.0.0";
    list.clear();
}

void FakeBackend::settle() {
    for (auto& p : pending) {
        SettingDef d; settingsDefAt(p.first, d);
        settingsPut(u, d, p.second);
    }
    pending.clear();
}

uint16_t FakeBackend::value(uint8_t sid) {
    auto it = pending.find(sid);
    if (it != pending.end()) return it->second;
    SettingDef d; settingsDefAt(sid, d);
    return settingsGet(u, d);
}

bool FakeBackend::settled(uint8_t sid) { return pending.find(sid) == pending.end(); }

bool FakeBackend::applyMany(const MenuWrite* w, uint8_t n) {
    if (queueFull || (queueRoom >= 0 && n > queueRoom)) return false;
    for (uint8_t i = 0; i < n; i++) {
        SettingDef d; settingsDefAt(w[i].sid, d);
        char key[48]; settingsKeyCopy(d, key, sizeof(key));
        char line[80]; snprintf(line, sizeof(line), "apply %s=%u", key, w[i].v);
        log.push_back(line);
        pending[w[i].sid] = w[i].v;
    }
    return true;
}

uint8_t FakeBackend::activeInput() {
    uint16_t c = value(SID_input_source);
    return c >= 1 && c <= 6 ? (uint8_t)c : activeIn;
}

bool FakeBackend::run(uint8_t aid) {
    if (queueFull) return false;
    ActionDef d; actionsDefAt(aid, d);
    log.push_back(std::string("run ") + d.name);
    return true;
}

bool FakeBackend::fn(uint8_t menuFn) {
    if (queueFull) return false;
    char line[16]; snprintf(line, sizeof(line), "fn %u", menuFn);
    log.push_back(line);
    return true;
}

bool FakeBackend::toggleGet(uint8_t) { return termination; }

void FakeBackend::toggleSet(uint8_t menuFn, bool on) {
    termination = on;
    char line[24]; snprintf(line, sizeof(line), "toggle %u %u", menuFn, on ? 1 : 0);
    log.push_back(line);
}

int32_t FakeBackend::live(uint8_t lv) { return lv < LV_COUNT ? lives[lv] : 0; }

void FakeBackend::text(uint8_t lt, char* out, size_t n) {
    if (!n) return;
    if (lt == LT_PROFILE_ACTIVE && texts[lt].empty() && active >= 0) {   // as the device: the active profile's name
        snprintf(out, n, "%s", list.at((uint8_t)active).name);
        return;
    }
    const std::string& s = lt < LT_COUNT ? texts[lt] : texts[0];
    strncpy(out, s.c_str(), n - 1);
    out[n - 1] = '\0';
}

uint8_t FakeBackend::caps() { return platformCaps(); }

void FakeBackend::saverHold(bool on) { log.push_back(on ? "hold 1" : "hold 0"); }

// Log: "profile <kind> <index>", plus " <name> <region>" when a name or a region is given
bool FakeBackend::profileOp(uint8_t popKind, int8_t index, const char* name, uint8_t region) {
    if (queueFull) return false;
    if (popKind == POP_LOAD && index >= 0) {             // as the device: the loaded profile becomes active, last used
        active = index;
        list.at((uint8_t)index).lastused = list.nextLastused();
    }
    char line[64];
    if (name || region != PROFILE_REGION_NONE) snprintf(line, sizeof(line), "profile %u %d %s %u", popKind, index, name ? name : "-", region);
    else snprintf(line, sizeof(line), "profile %u %d", popKind, index);
    log.push_back(line);
    return true;
}

bool FakeBackend::profileResultFor(uint16_t seq, char* out, size_t n) {
    auto it = results.find(seq);
    const std::string* r = it != results.end() ? &it->second : seq && seq == resultSeq ? &result : nullptr;
    if (!r) return false;
    if (n) { strncpy(out, r->c_str(), n - 1); out[n - 1] = '\0'; }
    return true;
}

bool FakeBackend::setText(uint8_t lt, const char* v) {
    if (textBusy || lt >= LT_COUNT) return false;
    texts[lt] = v;
    log.push_back(std::string("text ") + std::to_string(lt) + " " + v);
    return true;
}
