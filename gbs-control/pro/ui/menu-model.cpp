#include <stdio.h>
#include <string.h>
#include "../settings/settings-pgm.h"
#include "../settings/settings-registry.h"
#include "../api/api-actions.h"
#include "menu-model.h"
#include "menu-strings.h"
#include "menu-tree.h"
#include "menu-charset.h"
#include "../input/input-types.h"
#include "tv-osd.h"
#include "../../options.h"
#include "../util/str-copy.h"
#include "../profiles/profile-list.h"
#include "../profiles/profile-names.h"


const char* menuRateStandard(uint16_t srcHzX10) { return !srcHzX10 ? "" : srcHzX10 < 550 ? "PAL" : "NTSC"; }

uint8_t menuInput(MenuBackend& be) { return be.activeInput(); }   // not input.source: that is the choice (Auto = 0)

static bool predVisible(uint8_t pred, MenuBackend& be) {
    uint8_t in = menuInput(be);
    bool svav = inputIsDecoder(in);
    switch (pred) {
        case P_DEVMODE: return be.devMode() && be.menuMode() == 1;   // the Developer page lives inside Advanced
        case P_SVAV: return svav;
        case P_SV: case P_SV_WY: return in == InputTypeSV;
        case P_AV: return in == InputTypeAV;
        case P_AV_NTSC: return in == InputTypeAV && be.signal().advNtsc;    // UG-637: the comb settings act on CVBS only
        case P_AV_PAL: return in == InputTypeAV && !be.signal().advNtsc;
        case P_RGB_SYNC: return (be.caps() & CAP_ADV) != 0 && (in == InputTypeRGBs || in == InputTypeRGsB);   // the stripper acts on RGBs/RGsB only
        case P_RGBS: return (be.caps() & CAP_ADV) != 0;
        case P_PROFILE_ACTIVE: return be.profileActive() >= 0;
        case P_UPDATE_AVAILABLE: return be.signal().updateAvailable;
        case P_UPDATE_MANUAL: return be.signal().updateManual;
        case P_ADV_CONTROLLER_UPDATE: return be.signal().advControllerUpdateAvailable;
        case P_VGA: return in == InputTypeVGA;
        case P_SOG: return in == InputTypeRGsB || in == InputTypeYUV;
        case P_TV_OSD: return tvOsdAllowed((be.caps() & CAP_OSD) != 0, !be.signal().noSignal, be.signal().presetId);
        case P_I2P_ON: return svav;
        case P_WIFI_AP: { char m[8]; be.text(LT_WIFI_MODE, m, sizeof(m)); return strcmp(m, "AP") == 0; }
        default: return true;
    }
}

bool menuVisible(const MenuNode& n, MenuBackend& be) {
    if ((n.flags & NF_ADVANCED) && be.menuMode() == 0) return false;
    if (!predVisible(n.pred, be)) return false;
    switch (n.type) {
        case N_SETTING: {
            SettingDef d; settingsDefAt(n.ref, d);
            return settingsAvailable(d, be.devMode());
        }
        case N_ACTION:
        case N_STEPPER: {
            if (n.flags & NF_FN) return true;
            ActionDef d; actionsDefAt(n.ref, d);
            return actionsAvailable(d, be.devMode());
        }
        case N_ADJUST2D: {
            MenuAdjust a; menuAdjustRead(n.ref, a);
            ActionDef d; actionsDefAt(a.left, d);
            return actionsAvailable(d, be.devMode());
        }
        case N_SUB:
            if (n.flags & NF_FN) return true;   // dynamic lists show a "No profiles" row when empty
            for (uint8_t i = 0; i < n.count; i++) {
                MenuNode k; menuNodeRead(&n.children[i], k);
                if (k.type != N_HEADER && menuVisible(k, be)) return true;
            }
            return false;
        default:
            return true;
    }
}

bool menuEnabled(const MenuNode& n, MenuBackend& be, uint16_t* reason) {
    uint16_t r = S_NONE;
    uint8_t in = menuInput(be);
    switch (n.pred) {
        case P_SV_WY: if (be.value(SID_adv_filter_wy_ovr) != 1) r = S_R_WY_MANUAL; break;
        case P_RGBS: if (in != InputTypeRGBs) r = S_R_RGBS; break;
        case P_ACE_ON: if (be.value(SID_adv_ace) != 1) r = S_R_ACE; break;
        case P_SCANLINES_OK:   // interlaced sources need Bob (motion adaptive blocks the scanlines)
            if (!be.signal().scanlinesAllowed)
                r = be.signal().interlaced && be.value(SID_pic_deint) == 0 ? S_R_NEEDS_BOB : S_R_SCANLINES;
            break;
        case P_I2P_ON: if (be.value(SID_adv_i2p) != 1) r = S_R_2X_FIRST; break;
        case P_DEINT_OK: if (inputIsDecoder(in) && be.value(SID_adv_i2p) == 1) r = S_R_2X_ON; break;
        case P_FTL_OK:   // a 50 Hz source forced to 60 Hz cannot lock its frames to the output
            if (be.value(SID_output_force60) == 1 && !be.signal().noSignal && be.signal().srcHzX10 && be.signal().srcHzX10 < 550)
                r = S_R_FORCE60;
            break;
        case P_FTL_ON: if (be.value(SID_output_ftl) != 1) r = S_R_FTL_OFF; break;
        default: break;
    }
    if (reason) *reason = r;
    return r == S_NONE;
}

bool menuOptAt(const char* list, uint8_t i, char* name, size_t n, uint16_t& value) {
    if (!list || !n) return false;
    uint8_t idx = 0;
    size_t o = 0;
    bool inValue = false;
    uint16_t v = 0;
    for (const char* p = list;; p++) {
        char c = (char)pgm_read_byte(p);
        if (c == '|' || c == '\0') {
            if (idx == i) {
                name[o < n ? o : n - 1] = '\0';
                value = v;
                return true;
            }
            if (c == '\0') return false;
            idx++; o = 0; v = 0; inValue = false;
            continue;
        }
        if (idx != i) continue;
        if (c == '=') { inValue = true; continue; }
        if (inValue) v = (uint16_t)(v * 10 + (c - '0'));
        else if (o + 1 < n) name[o++] = c;
    }
}


static const char* onOff(bool on) { return on ? "On" : "Off"; }

static void formatLive(uint8_t lv, int32_t v, char* out, size_t n) {
    static const char* const kSdram[8] = {"108MHz", "81MHz", "FB", "162MHz", "144MHz", "185MHz", "216MHz", "129MHz"};
    switch (lv) {
        case LV_ADC_GAIN: case LV_HTOTAL: case LV_PLL_DIV:
            snprintf(out, n, "%ld", (long)v); break;
        case LV_DEBUG_VIEW: case LV_FREEZE: case LV_IF_OFFSET: case LV_SYNCWATCHER:
        case LV_OSD_ALWAYS_ON: case LV_OSD_TEST: case LV_PRINT_INFO: case LV_IR_LOG: case LV_OTA:
            copyStr(out, n, onOff(v != 0)); break;
        case LV_POSITION: case LV_SIZE: case LV_BORDERS: case LV_BORDER_POS:
            snprintf(out, n, "%ld/%ld", (long)((v >> 16) & 0xFFFF), (long)(v & 0xFFFF)); break;
        case LV_SDRAM: copyStr(out, n, (v >= 0 && v < 8) ? kSdram[v] : "-"); break;
        case LV_OSR: snprintf(out, n, "%ldx", (long)v); break;
        case LV_ADC_FILTER: copyStr(out, n, onOff(v > 0)); break;
        case LV_SOG: snprintf(out, n, "%ld", (long)v); break;
        case LV_SYNC_POLARITY: copyStr(out, n, v ? "Inverted" : "Normal"); break;
        case LV_OUT_RATE: copyStr(out, n, (v >= 565 && v <= 645) ? "60Hz" : (v >= 465 && v <= 545) ? "50Hz" : "-"); break;
        default: copyStr(out, n, ""); break;
    }
}

static bool optLabel(const char* list, uint16_t value, char* out, size_t n) {
    char name[24]; uint16_t v;
    for (uint8_t i = 0; menuOptAt(list, i, name, sizeof(name), v); i++)
        if (v == value) { copyStr(out, n, name); return true; }
    return false;
}

void menuSanitize(char* s) {
    for (; *s; s++) {
        if (*s == '_') *s = ' ';
        else if (!menuCharOk(*s)) *s = '.';
    }
}

void menuFormatSettingValue(const MenuNode& n, uint16_t v, char* out, size_t len) {
    if (!len) return;
    SettingDef d; settingsDefAt(n.ref, d);
    if (n.opts && n.opts < 100) { if (!optLabel(menuOptList(n.opts), v, out, len)) copyStr(out, len, "-"); }
    else if (d.type == SettingType::Bool) copyStr(out, len, onOff(v != 0));
    else settingsFormat(d, v, out, len);
    if (n.flags & NF_PERCENT) { size_t l = strlen(out); if (l + 1 < len) { out[l] = '%'; out[l + 1] = '\0'; } }
}

void menuProfileEntryLabel(const ProfileEntry& e, char* out, size_t n) {
    profileLabel(e.name, e.region, out, n);          // within 24: the name is shortened, the region kept
    menuSanitize(out);
}

void menuProfileLabel(MenuBackend& be, char* out, size_t n) {
    int a = be.profileActive();
    if (a < 0) { menuStrCopy(S_DEFAULTS, out, n); return; }
    menuProfileEntryLabel(be.profiles().at((uint8_t)a), out, n);
}

void menuFormatValue(const MenuNode& n, MenuBackend& be, char* out, size_t len) {
    if (!len) return;
    out[0] = '\0';
    if ((n.type == N_ACTION || n.type == N_SUB) && n.opts == LT_PROFILE_LABEL) { menuProfileLabel(be, out, len); return; }
    if (n.type == N_ACTION && n.opts == LT_PROFILE_SAVE) {   // no profile: nothing to save into
        if (be.profileActive() >= 0) menuProfileLabel(be, out, len);
        return;
    }
    switch (n.type) {
        case N_SETTING:
            if (n.ref == SID_input_source && be.value(n.ref) == 0) {   // "Auto: <found input>"
                char in[24] = "";
                if (!be.signal().searching && !be.signal().noSignal) menuFormatSettingValue(n, be.activeInput(), in, sizeof(in));
                snprintf(out, len, in[0] ? "Auto: %s" : "Auto", in);
                break;
            }
            menuFormatSettingValue(n, be.value(n.ref), out, len);
            break;
        case N_CHOICE: menuFormatChoice(n.ref, menuChoiceCurrent(n.ref, be), out, len); break;
        case N_TOGGLE_FN: copyStr(out, len, onOff(be.toggleGet(n.ref))); break;
        case N_LIVE: be.text(n.ref, out, len); menuSanitize(out); break;
        case N_ACTION:
            if (n.opts) { be.text(n.opts, out, len); menuSanitize(out); }
            else if (n.ref2) formatLive(n.ref2, be.live(n.ref2), out, len);
            break;
        case N_STEPPER:
            if (n.ref2) formatLive(n.ref2, be.live(n.ref2), out, len);
            else copyStr(out, len, "-/+");   // no cheap readback: Left/Right step the value
            break;
        case N_ADJUST2D:
            if (n.ref2) formatLive(n.ref2, be.live(n.ref2), out, len);
            break;
        default: break;
    }
}

bool menuIsChanged(const MenuNode& n, MenuBackend& be) {
    if (n.type != N_SETTING) return false;
    SettingDef d; settingsDefAt(n.ref, d);
    return be.value(n.ref) != d.def;
}

int menuSliderHalves(const MenuNode& n, MenuBackend& be) {
    if (n.type != N_SETTING || (n.opts && n.opts < 100)) return -1;
    SettingDef d; settingsDefAt(n.ref, d);
    if (d.type == SettingType::Bool || d.type == SettingType::Enum || d.max <= d.min) return -1;
    uint16_t v = be.value(n.ref);
    if (v < d.min) v = d.min;
    if (v > d.max) v = d.max;
    return (int)((uint32_t)(v - d.min) * 20 / (d.max - d.min));
}

// Next/previous entry of a list of `count` values given by `at`, wrapping; skip() filters entries out
template <typename At, typename Skip>
static bool stepList(uint8_t count, uint16_t cur, int dir, At at, Skip skip, uint16_t& out) {
    if (!count) return false;
    int pos = -1;
    for (uint8_t i = 0; i < count; i++) if (at(i) == cur) { pos = i; break; }
    for (uint8_t tries = 0; tries < count; tries++) {
        pos = (pos < 0 && dir < 0) ? count - 1 : (pos + dir + count) % count;
        if (!skip(at((uint8_t)pos))) { out = at((uint8_t)pos); return true; }
    }
    return false;
}

bool menuStepValue(const MenuNode& n, MenuBackend& be, int dir, uint16_t& out) {
    return n.type == N_SETTING && menuStepFrom(n, be, be.value(n.ref), dir, out);
}

bool menuStepFrom(const MenuNode& n, MenuBackend& be, uint16_t cur, int dir, uint16_t& out) {
    if (n.type != N_SETTING) return false;
    SettingDef d; settingsDefAt(n.ref, d);
    if (n.opts && n.opts < 100) {
        const char* list = menuOptList(n.opts);
        uint8_t count = 0; char name[24]; uint16_t v;
        while (menuOptAt(list, count, name, sizeof(name), v)) count++;
        return stepList(count, cur, dir, [&](uint8_t i) { menuOptAt(list, i, name, sizeof(name), v); return v; },
                        [](uint16_t) { return false; }, out);
    }
    if (d.type == SettingType::Bool) { out = cur ? 0 : 1; return true; }
    if (d.type == SettingType::Enum) {
        uint8_t count = settingsEnumCount(d);
        char name[24];
        bool profile = be.profileActive() >= 0;
        bool preset = n.ref == SID_output_preset;
        bool easy = be.menuMode() == 0;
        return stepList(count, cur, dir,
                        [&](uint8_t i) { uint16_t v = 0; settingsEnumAt(d, i, name, sizeof(name), v); return v; },
                        [&](uint16_t v) { return !settingsOptionAvailable(d, v) || (preset && v == OutputCustomized && !profile) ||
                                                 (easy && v != cur && settingsOptionAdvanced(d, v)); },
                        out);
    }
    int step = n.opts >= 100 ? n.opts - 100 : 1;
    int v = (int)cur + dir * step;
    if (v < d.min) v = d.min;
    if (v > d.max) v = d.max;
    out = (uint16_t)v;
    return true;
}

const char* menuAdvStandardName(int8_t s) {
    static const char* const k[8] = {"NTSC", "NTSC443", "PAL-M", "PAL60", "PAL", "SECAM", "PAL-N", "SECAM"};
    return s >= 0 && s < 8 ? k[s] : "";
}

// ---- Choices ----

// With a profile loaded the preset reads Custom: the choice shows the profile's base preset
static uint16_t choiceValue(MenuBackend& be, uint8_t sid) {
    uint16_t v = be.value(sid);
    if (sid == SID_output_preset && v == OutputCustomized) return be.basePreset();
    return v;
}

static bool choiceMatches(const MenuChoiceOpt& o, MenuBackend& be) {
    if (choiceValue(be, o.sid1) != o.v1) return false;
    return o.sid2 == 0xFF || (o.flags & CHO_SID1_ONLY) || choiceValue(be, o.sid2) == o.v2;
}

int menuChoiceCurrent(uint8_t choice, MenuBackend& be) {
    MenuChoice c; menuChoiceRead(choice, c);
    MenuChoiceOpt o;
    for (uint8_t i = 0; i < c.count; i++) {
        menuChoiceOptRead(c, i, o);
        if (!choiceMatches(o, be)) continue;
        if (!(o.flags & CHO_ALIAS)) return i;
        MenuChoiceOpt k;
        for (uint8_t j = 0; j < c.count; j++) {
            menuChoiceOptRead(c, j, k);
            if (k.label == o.label && !(k.flags & CHO_ALIAS)) return j;
        }
    }
    return -1;
}

static bool choiceOffered(const MenuChoiceOpt& o, MenuBackend& be) {
    if (o.flags & CHO_ALIAS) return false;
    if ((o.flags & CHO_ORIGINAL) && !(be.caps() & CAP_ORIGINAL)) return false;
    if ((o.flags & CHO_ADVANCED) && be.menuMode() == 0) return false;
    return true;
}

bool menuChoiceStep(uint8_t choice, MenuBackend& be, int cur, int dir, uint8_t& next) {
    MenuChoice c; menuChoiceRead(choice, c);
    if (!c.count) return false;
    int pos = cur;
    MenuChoiceOpt o;
    for (uint8_t tries = 0; tries < c.count; tries++) {
        pos = pos < 0 ? (dir > 0 ? 0 : c.count - 1) : (pos + dir + c.count) % c.count;   // from Custom: an end
        menuChoiceOptRead(c, (uint8_t)pos, o);
        if (choiceOffered(o, be) || pos == cur) { next = (uint8_t)pos; return pos != cur; }
    }
    return false;
}

uint8_t menuChoiceWrites(uint8_t choice, uint8_t opt, MenuWrite* w) {
    MenuChoice c; menuChoiceRead(choice, c);
    MenuChoiceOpt o; menuChoiceOptRead(c, opt, o);
    w[0] = {o.sid1, o.v1};
    if (o.sid2 == 0xFF || (o.flags & CHO_SID1_ONLY)) return 1;
    w[1] = {o.sid2, o.v2};
    return 2;
}

void menuFormatChoice(uint8_t choice, int opt, char* out, size_t len) {
    if (opt < 0) { menuStrCopy(S_CUSTOM, out, len); return; }
    MenuChoice c; menuChoiceRead(choice, c);
    MenuChoiceOpt o; menuChoiceOptRead(c, (uint8_t)opt, o);
    menuStrCopy(o.label, out, len);
}
