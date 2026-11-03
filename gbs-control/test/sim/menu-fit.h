// Test helper: walk every visible row of the tree (inputs 1-6 x NTSC/PAL, developer mode, a profile active)
// and ask a renderer-specific rule whether label + widest value fit. Shared by the OSD and OLED fit tests.
#pragma once
#include <string>
#include <vector>
#include <string.h>
#include "menu-fake.h"
#include "menu-tree.h"
#include "menu-model.h"
#include "menu-strings.h"
#include "settings-registry.h"
#include "api-actions.h"
#include "menu-catalog.h"

struct MenuFitRule {
    virtual ~MenuFitRule() {}
    virtual bool fits(const std::string& label, const std::string& value, uint8_t kind) = 0;
    virtual void extra(const MenuNode&, const std::string&, std::vector<std::string>&) {}   // renderer-only checks
};

inline std::string menuFitStr(uint16_t id) { char b[64]; menuStrCopy(id, b, sizeof(b)); return b; }

inline const char* menuFitLiveMax(uint8_t lv) {
    switch (lv) {
        case LV_ADC_GAIN: return "255";
        case LV_HTOTAL: case LV_PLL_DIV: return "65535";
        case LV_POSITION: case LV_SIZE: case LV_BORDERS: return "65535/65535";
        case LV_BORDER_POS: return "4095/2047";   // VDS_DIS_HB_SP 12 bits, VDS_DIS_VB_SP 11
        case LV_SDRAM: return "216MHz";
        case LV_OSR: return "4x";
        case LV_SOG: return "31";
        case LV_SYNC_POLARITY: return "Inverted";
        case LV_OUT_RATE: return "60Hz";
        default: return "Off";
    }
}

// Every value label a SETTING row can show
inline std::vector<std::string> menuFitSettingValues(const MenuNode& n) {
    std::vector<std::string> out;
    SettingDef d; settingsDefAt(n.ref, d);
    char name[32]; uint16_t v;
    if (n.opts && n.opts < 100) { for (uint8_t i = 0; menuOptAt(menuOptList(n.opts), i, name, sizeof(name), v); i++) out.push_back(name); }
    else if (d.type == SettingType::Bool) { out.push_back("Off"); out.push_back("On"); }
    else if (d.type == SettingType::Enum) { for (uint8_t i = 0; settingsEnumAt(d, i, name, sizeof(name), v); i++) out.push_back(name); }
    else out.push_back(std::to_string(d.max));
    return out;
}

inline void menuFitWalk(MenuFitRule& rule, const MenuNode* kids, uint8_t count, MenuBackend& be, const std::string& path,
                        std::vector<std::string>& misfits) {
    for (uint8_t i = 0; i < count; i++) {
        MenuNode n; menuNodeRead(&kids[i], n);
        if (!menuVisible(n, be)) continue;
        std::string l = menuFitStr(n.label);
        auto check = [&](const std::string& value, uint8_t kind) {
            if (!rule.fits(l, value, kind)) misfits.push_back(path + "/" + l + " = " + value);
        };
        switch (n.type) {
            case N_SUB:
                check("", ROWK_SUB);
                if (!(n.flags & NF_FN)) menuFitWalk(rule, n.children, n.count, be, path + "/" + l, misfits);
                if ((n.flags & NF_FN) && n.ref == FN_PROFILE_NAME_LIST) {   // quick names: catalog + region
                    for (uint8_t c = 0; c < menuCatalogCount(); c++) {
                        char name[32]; menuCatalogName(c, name, sizeof(name));
                        if (!rule.fits(name, "NTSC", ROWK_CYCLE)) misfits.push_back(path + "/" + l + "/" + name);
                    }
                    if (!rule.fits(menuFitStr(S_EDIT_NAME), "", ROWK_ACTION)) misfits.push_back("Edit name...");
                }
                break;
            case N_HEADER: check("", ROWK_HEADER); break;
            case N_SETTING:
                if (menuSliderHalves(n, be) >= 0) check("", ROWK_SLIDER);
                for (auto& v : menuFitSettingValues(n)) check(v, menuSliderHalves(n, be) >= 0 ? ROWK_SLIDER : ROWK_CYCLE);
                break;
            case N_CHOICE: { MenuChoice c; menuChoiceRead(n.ref, c); for (uint8_t k = 0; k < c.count; k++) { MenuChoiceOpt o; menuChoiceOptRead(c, k, o); check(menuFitStr(o.label), ROWK_CYCLE); } break; }
            case N_STEPPER: check(n.ref2 ? menuFitLiveMax(n.ref2) : "-/+", ROWK_VALUE); break;
            case N_ADJUST2D: check(menuFitLiveMax(n.ref2), ROWK_ACTION); break;
            case N_TOGGLE_FN: check("Off", ROWK_VALUE); break;
            case N_ACTION:
                if (n.opts) break;                              // free text (clipped at runtime)
                check(n.ref2 ? menuFitLiveMax(n.ref2) : "", ROWK_ACTION);
                break;
            case N_LIVE: check("", ROWK_LIVE); break;          // the value is free text, clipped at runtime
            default: break;
        }
        rule.extra(n, l, misfits);
    }
}

// All boards' views: inputs 1-6, NTSC and PAL, an update available, a profile active, developer mode on
inline std::vector<std::string> menuFitAll(MenuFitRule& rule) {
    std::vector<std::string> misfits;
    MenuNode root; menuNodeRead(menuRoot(), root);
    for (uint8_t in = 1; in <= 6; in++)
        for (int ntsc = 0; ntsc < 2; ntsc++) {
            FakeBackend be;
            be.apply(SID_input_source, in); be.settle();
            be.sig.advNtsc = ntsc != 0; be.sig.updateAvailable = true; be.active = 0; be.dev = true;
            menuFitWalk(rule, root.children, root.count, be, "Menu", misfits);
        }
    return misfits;
}
