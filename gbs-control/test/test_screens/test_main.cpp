// Screen catalogue: walks the whole menu tree and the overlays on both displays and writes every OSD screen
// (<dir>/osd/*.txt, golden format) and OLED screen (<dir>/oled/*.pbm). With SCREENS_OUT=<dir> the files are kept
// for review (render with tools/osd-golden-report.py / tools/oled-golden-report.py); without it the walk runs in a
// temporary directory as a check that every page renders.
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "host-fs.h"
#include "osd-renderer.h"
#include "oled-renderer.h"
#include "oled-fake.h"
#include "menu-fake.h"
#include "menu-engine.h"
#include "menu-tree.h"
#include "menu-strings.h"
#include "stv9426-driver.h"
#include "stv-fake.h"
#include "settings-registry.h"

void setUp(void) {}
void tearDown(void) {}

static std::string gOut;
static int gCount = 0;

static void writeFile(const std::string& path, const std::string& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    gCount++;
}

static std::string slug(const std::string& s) {
    std::string o;
    for (char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o += c;
        else if (c >= 'A' && c <= 'Z') o += (char)(c - 'A' + 'a');
        else if (!o.empty() && o.back() != '-') o += '-';
    }
    while (!o.empty() && o.back() == '-') o.pop_back();
    return o;
}

// One engine per display: the OSD one is driven with the remote keys, the OLED one with the encoder
struct Osd {
    FakeBackend be;
    MenuEngine e;
    FakeStv f;
    Stv9426 d;
    OsdRenderer r;
    uint32_t now = 1000;
    Osd() : e(be), d(f), r(d) { d.begin(STV_XTAL_HZ); d.setOutput(OUT_1080P60); }
    void draw() { r.render(e.view(), 0, now); d.service(now); now += 50; d.service(now); r.render(e.view(), 0, now); d.service(now); }
    void key(uint8_t k) { now += 100; e.key(k, false, now); e.tick(now); draw(); }
    void wait(uint32_t ms) { now += ms; e.tick(now); draw(); }
    std::string shot() { return f.decode(stvActiveLines(d.output()) + 60); }
};

struct Oled {
    FakeBackend be;
    MenuEngine e;
    FakeOled d;
    OledRenderer o{d};
    uint32_t now = 1000;
    Oled() : e(be) {
        e.setWindow(5);
        e.setEncoderBackRow(true);
        be.sig.presetId = 0x01; be.sig.srcLines = 262; be.sig.srcHzX10 = 599; be.sig.outHzX10 = 600;
        be.texts[LT_WIFI_IP] = "192.168.4.1"; be.texts[LT_WIFI_SSID] = "gbscontrol"; be.texts[LT_WIFI_MODE] = "AP";
    }
    void key(uint8_t k) { now += 100; e.key(k, false, now); e.tick(now); }
    void wait(uint32_t ms) { now += ms; e.tick(now); }
    std::string shot() { MenuIdle i; e.idle(i); o.draw(e.view(), i, false, now); return d.pbm(); }
};

static void setupBackend(FakeBackend& be, uint8_t input) {
    be.dev = true;                                   // the Developer menu is part of the catalogue
    be.sig.updateAvailable = true;
    be.sig.advControllerUpdateAvailable = true;
    be.texts[LT_UPDATE_VERSION] = "3.0.1";
    be.sig.srcLines = 262; be.sig.srcHzX10 = 599; be.sig.presetId = 0x01; be.sig.outHzX10 = 600;   // 240p source
    be.sig.frameLocked = true; be.sig.sogLevel = 5;
    be.texts[LT_PROFILE_ACTIVE] = "Mega Drive"; be.texts[LT_WIFI_SSID] = "home";
    be.texts[LT_WIFI_MODE] = "STA"; be.texts[LT_WIFI_IP] = "192.168.1.20";
    const char* names[3] = {"Mega Drive", "SNES PAL", "PlayStation"};
    for (uint8_t i = 0; i < 3; i++) {
        ProfileEntry& e = be.list.at(i);
        e.used = true;
        strncpy(e.name, names[i], sizeof(e.name) - 1);
        e.hasFp = i != 2;
    }
    be.apply(SID_input_source, input);
    be.settle();
    be.log.clear();
}

template <class S> static bool selectRow(S& s, const std::string& l, uint8_t down, uint8_t up) {
    for (int i = 0; i < 48; i++) {
        const MenuView& v = s.e.view();
        if (std::string(v.rows[v.cursorRow].label) == l) return true;
        s.key(down);
    }
    for (int i = 0; i < 48; i++) {
        const MenuView& v = s.e.view();
        if (std::string(v.rows[v.cursorRow].label) == l) return true;
        s.key(up);
    }
    return false;
}

static void save(const std::string& name, Osd* osd, Oled* oled) {
    if (osd) writeFile(gOut + "/osd/" + name + ".txt", osd->shot());
    if (oled) writeFile(gOut + "/oled/" + name + ".pbm", oled->shot());
}

// Opens the menu and enters `path` (labels) on both displays; false when a label is not visible
static bool openPath(Osd& osd, Oled& oled, const std::vector<std::string>& path) {
    osd.key(K_MENU);
    oled.key(K_CLICK); oled.key(K_ROT_L); oled.key(K_CLICK);   // click opens the quick menu, "All settings" the full one
    for (const std::string& l : path) {
        if (!selectRow(osd, l, K_DOWN, K_UP) || !selectRow(oled, l, K_ROT_R, K_ROT_L)) return false;
        osd.key(K_OK);
        oled.key(K_CLICK);
    }
    return true;
}

static void closeAll(Osd& osd, Oled& oled) {
    osd.key(K_MENU);
    if (osd.e.busy()) osd.key(K_MENU);
    oled.wait(20000);
    osd.wait(20000);
}

// Captures one list level: top page, and the last page when the list scrolls on that display
static void captureLevel(Osd& osd, Oled& oled, const std::string& name) {
    save(name, &osd, &oled);
    const MenuView& v = osd.e.view();
    if (v.moreBelow) {
        for (int i = 0; i < 40 && osd.e.view().moreBelow; i++) osd.key(K_DOWN);
        save(name + "-end", &osd, nullptr);
    }
    if (oled.e.view().moreBelow) {
        for (int i = 0; i < 40 && oled.e.view().moreBelow; i++) oled.key(K_ROT_R);
        save(name + "-end", nullptr, &oled);
    }
}

static void walk(uint8_t input, const char* prefix, const MenuNode* pgm, std::vector<std::string>& path, int onlyFirst) {
    MenuNode n;
    menuNodeRead(pgm, n);
    for (uint8_t i = 0; i < n.count; i++) {
        MenuNode k;
        menuNodeRead(&n.children[i], k);
        if (k.type != N_SUB) continue;
        char label[21];
        menuStrCopy(k.label, label, sizeof(label));
        if (onlyFirst && path.empty() && std::string(label) != "Input" && std::string(label) != "Picture") continue;
        path.push_back(label);
        Osd osd; Oled oled;
        setupBackend(osd.be, input);
        setupBackend(oled.be, input);
        if (openPath(osd, oled, path)) {
            std::string name = prefix;
            for (const std::string& p : path) name += "-" + slug(p);
            std::string label;
            for (const std::string& p : path) label += (label.empty() ? "" : " > ") + p;
            FILE* ix = fopen((gOut + "/index.txt").c_str(), "ab");
            if (ix) { fprintf(ix, "%s\t%s\n", name.c_str(), label.c_str()); fclose(ix); }
            captureLevel(osd, oled, name);
            if (!(k.flags & NF_FN)) walk(input, prefix, &n.children[i], path, onlyFirst);
        }
        path.pop_back();
    }
}

void test_catalogue(void) {
    const char* env = getenv("SCREENS_OUT");
    bool keep = env && *env;
    gOut = keep ? std::string(env) : hostTempDir("gbsc-screens-");
    TEST_ASSERT_FALSE(gOut.empty());
    hostMakeDir(gOut);
    hostMakeDir(gOut + "/osd");
    hostMakeDir(gOut + "/oled");

    // Closed menu (OLED idle block) and the root on both displays
    {
        Osd osd; Oled oled;
        setupBackend(osd.be, 1); setupBackend(oled.be, 1);
        save("00-idle", nullptr, &oled);
        osd.key(K_MENU); oled.key(K_CLICK); oled.key(K_ROT_L); oled.key(K_CLICK);
        captureLevel(osd, oled, "01-root");
    }
    {                                                    // the quick page
        Osd osd; Oled oled;
        setupBackend(osd.be, 1); setupBackend(oled.be, 1);
        osd.key(K_OK); oled.key(K_CLICK);
        save("02-quick", &osd, &oled);
    }
    std::vector<std::string> path;
    walk(1, "10", menuRoot(), path, 0);            // RGBs: the whole tree
    walk(5, "20-sv", menuRoot(), path, 1);         // S-Video: Input and Picture with the decoder rows
    walk(6, "30-av", menuRoot(), path, 1);         // Composite

    // Overlays
    Osd osd; Oled oled;
    setupBackend(osd.be, 1); setupBackend(oled.be, 1);
    osd.key(K_VOLUP); oled.key(K_VOLUP); save("40-ov-volume", &osd, &oled); closeAll(osd, oled);
    osd.key(K_MUTE); oled.key(K_MUTE); save("41-ov-mute", &osd, &oled); osd.key(K_MUTE); oled.key(K_MUTE); closeAll(osd, oled);
    osd.key(K_INFO); oled.key(K_INFO); save("42-ov-info", &osd, &oled); closeAll(osd, oled);
    if (openPath(osd, oled, {"System"}) && selectRow(osd, "Restart", K_DOWN, K_UP) && selectRow(oled, "Restart", K_ROT_R, K_ROT_L)) {
        osd.key(K_OK); oled.key(K_CLICK); save("43-ov-confirm", &osd, &oled);
        osd.key(K_LEFT); oled.key(K_ROT_L); save("44-ov-confirm-yes", &osd, &oled);
    }
    closeAll(osd, oled);
    if (openPath(osd, oled, {"Output"})) {
        osd.key(K_RIGHT); oled.key(K_CLICK); oled.key(K_ROT_R); oled.key(K_CLICK);
        osd.wait(2000); oled.wait(2000);
        save("45-ov-revert", &osd, &oled);
    }
    closeAll(osd, oled);
    if (openPath(osd, oled, {"Screen"})) {
        osd.key(K_OK); oled.key(K_CLICK); save("46-ov-adjust", &osd, &oled);
    }
    closeAll(osd, oled);
    osd.e.notice(S_T_ADV_CONTROLLER_DONE, osd.now); osd.draw(); oled.e.notice(S_T_ADV_CONTROLLER_DONE, oled.now);
    save("47-ov-toast", &osd, &oled); closeAll(osd, oled);
    osd.be.sig.updatePct = 42; oled.be.sig.updatePct = 42; osd.wait(10); oled.wait(10);
    save("48-ov-update", &osd, &oled);
    osd.be.sig.updateAdvController = true; oled.be.sig.updateAdvController = true; osd.wait(300); oled.wait(300);
    save("49-ov-update-adv-controller", &osd, &oled);
    osd.be.sig.updatePct = -1; oled.be.sig.updatePct = -1; osd.be.sig.updateAdvController = false; oled.be.sig.updateAdvController = false;
    osd.wait(10); oled.wait(10);
    closeAll(osd, oled);
    if (osd.e.promptAdvController(osd.now) && oled.e.promptAdvController(oled.now)) { osd.draw(); save("50-ov-adv-controller-prompt", &osd, &oled); }
    closeAll(osd, oled);
    // OTA: a recommended channel switch, an older build from Other channels, their
    // confirms, and the longest check and install toasts (ota-update.cpp texts, <= 28 characters)
    {
        Osd o; Oled l;
        for (FakeBackend* be : {&o.be, &l.be}) {
            setupBackend(*be, 1);
            be->apply(SID_sys_menu_mode, 1); be->settle();   // Last result is an Advanced row
            be->texts[LT_FW_CHANNEL] = "Dev";
            be->texts[LT_UPDATE_VERSION] = "3.0.1-beta12"; be->texts[LT_UPDATE_OFFER] = "Beta switch";
            be->texts[LT_UPDATE_CONFIRM] = "Move to the Beta channel?"; be->texts[LT_UPDATE_RESULT] = "No connection";
            be->sig.updateManual = true; be->texts[LT_MANUAL_VERSION] = "2.4.1";
            be->texts[LT_MANUAL_OFFER] = "Stable older switch"; be->texts[LT_MANUAL_CONFIRM] = "Install an older version?";
        }
        if (openPath(o, l, {"System", "Update"})) {
            captureLevel(o, l, "60-ota-update");
            if (selectRow(o, "Install", K_DOWN, K_UP) && selectRow(l, "Install", K_ROT_R, K_ROT_L)) {
                o.key(K_OK); l.key(K_CLICK); save("61-ota-confirm-switch", &o, &l);
            }
        }
        closeAll(o, l);
        if (openPath(o, l, {"System", "Update", "Other channels"})) {
            captureLevel(o, l, "62-ota-other-channels");
            if (selectRow(o, "Install", K_DOWN, K_UP) && selectRow(l, "Install", K_ROT_R, K_ROT_L)) {
                o.key(K_OK); l.key(K_CLICK); save("63-ota-confirm-older", &o, &l);
            }
        }
        closeAll(o, l);
        const char* toasts[] = {"Checking...", "3.0.1-beta12 available", "Check failed: no connection",
                                "Not installed: offer changed", "Beta up to date"};
        for (const char* t : toasts) {
            o.e.message(t, o.now); o.draw(); l.e.message(t, l.now); l.wait(10);
            save("64-ota-toast-" + slug(t), &o, &l);
            closeAll(o, l);
        }
    }
    if (keep) printf("screens: %d files -> %s\n", gCount, gOut.c_str());
    else hostRemoveTree(gOut);
    TEST_ASSERT_TRUE_MESSAGE(gCount >= 100, "the catalogue walk stopped early");   // 115 files (OSD + OLED)
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_catalogue);
    return UNITY_END();
}
