// Test rig for the menu engine: fake backend + engine + a clock advanced by every key
#pragma once
#include <string>
#include "menu-fake.h"
#include "menu-engine.h"
#include "settings-registry.h"

struct MenuRig {
    FakeBackend be;
    MenuEngine e;
    uint32_t now = 1000;
    MenuRig() : e(be) {}
    void key(uint8_t k, bool repeat = false) { now += 100; e.key(k, repeat, now); e.tick(now); }
    void wait(uint32_t ms) { now += ms; e.tick(now); }
    // The full menu from an encoder session: click opens the Quick menu, then its "All settings" row (one notch
    // up, the list wraps) when the session shows it, else MENU (a board without the encoder option)
    // The full menu in an encoder session. The rig has no signal, so the Quick menu is the no-signal free-run one, which
    // has no All settings row: MENU opens the full menu then. The All settings row itself is pinned by its own tests
    // (test_quick_all_settings_row_opens_the_full_menu and the free-run ones), not by this helper.
    void openFull() {
        key(K_CLICK); key(K_ROT_L);
        if (selected() == "All settings") key(K_CLICK);
        else key(K_MENU);
    }
    void setInput(uint8_t in) { be.apply(SID_input_source, in); be.settle(); be.log.clear(); }
    const MenuView& v() const { return e.view(); }
    std::string title() const { return v().title; }
    std::string label(uint8_t row) const { return v().rows[row].label; }
    std::string value(uint8_t row) const { return v().rows[row].value; }
    std::string selected() const { return v().rows[v().cursorRow].label; }
    std::string selectedValue() const { return v().rows[v().cursorRow].value; }
    // Move the cursor down until the selected row has this label (at most 40 steps)
    bool select(const std::string& l) {
        for (int i = 0; i < 40; i++) { if (selected() == l) return true; key(K_DOWN); }
        return false;
    }
    bool enter(const std::string& l) { if (!select(l)) return false; key(K_OK); return title() == l; }
    std::string lastLog() const { return be.log.empty() ? "" : be.log.back(); }
};
