#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "menu-fake.h"
#include "platform-caps.h"
#include "settings-registry.h"
#include "api-actions.h"
#include "menu-types.h"
#include "menu-strings.h"
#include "menu-tree.h"
#include "menu-charset.h"
#include "menu-model.h"

void setUp(void) {}
void tearDown(void) { platformSetCaps(CAPS_DEFAULT); }

static std::string str(uint16_t id) { char b[256]; menuStrCopy(id, b, sizeof(b)); return b; }   // wider than any string: a 64-byte copy hid the 96-character limit
static bool charsetOk(const std::string& s) { for (char c : s) if (!menuCharOk(c)) return false; return true; }

struct Visit { std::string path; MenuNode n; };
static void walk(const MenuNode* kids, uint8_t count, const std::string& path, std::vector<Visit>& out) {
    for (uint8_t i = 0; i < count; i++) {
        MenuNode n; menuNodeRead(&kids[i], n);
        out.push_back({path, n});
        if (n.type == N_SUB && !(n.flags & NF_FN)) walk(n.children, n.count, path.empty() ? str(n.label) : path + "/" + str(n.label), out);
    }
}
static std::vector<Visit> allNodes() {
    std::vector<Visit> v;
    MenuNode root; menuNodeRead(menuRoot(), root);
    walk(root.children, root.count, "", v);
    return v;
}

// Option list "Label=value|..." straight from the (host) string
static bool optEntries(const char* list, std::vector<std::pair<std::string, int>>& out) {
    if (!list) return false;
    std::string s(list);
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t bar = s.find('|', pos);
        std::string tok = s.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
        size_t eq = tok.find('=');
        if (eq == std::string::npos) return false;
        out.push_back({tok.substr(0, eq), atoi(tok.c_str() + eq + 1)});
        if (bar == std::string::npos) break;
        pos = bar + 1;
    }
    return true;
}

void test_menu_strings_all_in_charset(void) {
    for (uint16_t i = 0; i < S_COUNT; i++) TEST_ASSERT_TRUE_MESSAGE(charsetOk(str(i)), str(i).c_str());
}

// Enum rows without their own option list show the registry labels: they must be ROM characters too
void test_registry_enum_labels_use_rom_charset(void) {
    for (auto& v : allNodes()) {
        if (v.n.type != N_SETTING || v.n.opts) continue;
        SettingDef d; settingsDefAt((uint8_t)v.n.ref, d);
        for (uint8_t i = 0; i < settingsEnumCount(d); i++) {
            char name[40]; uint16_t value;
            TEST_ASSERT_TRUE(settingsEnumAt(d, i, name, sizeof(name), value));
            TEST_ASSERT_TRUE_MESSAGE(charsetOk(name) && strlen(name) <= 20, name);
        }
    }
}

// n/a reasons replace the hint on the help row: one row, and they name the row as it is labelled now
void test_reasons_fit_the_help_row(void) {
    const uint16_t reasons[] = {S_R_NEEDS_BOB, S_R_2X_FIRST, S_R_2X_ON, S_R_FORCE60, S_R_FTL_OFF, S_R_WY_MANUAL,
                                S_R_RGBS, S_R_ACE, S_R_SCANLINES, S_R_NO_FP, S_R_SYNC_MANUAL};
    for (uint16_t r : reasons) TEST_ASSERT_TRUE_MESSAGE(str(r).size() <= 28, str(r).c_str());
    TEST_ASSERT_TRUE(str(S_R_WY_MANUAL).find(str(S_Y_FILTER_MODE)) != std::string::npos);
}

void test_keyboard_help_fits_its_row(void) {
    TEST_ASSERT_TRUE(str(S_T_KB_HELP).size() <= 28);                  // drawKeyboard row 7 does not scroll
}

void test_tree_strings_use_rom_charset(void) {
    for (auto& v : allNodes()) {
        std::string l = str(v.n.label), h = str(v.n.help);
        TEST_ASSERT_TRUE_MESSAGE(l.size() <= 20, l.c_str());
        TEST_ASSERT_TRUE_MESSAGE(h.size() <= 96, h.c_str());       // descriptions scroll
        if (v.n.type != N_HEADER) TEST_ASSERT_TRUE_MESSAGE(!h.empty(), l.c_str());   // every item explains itself
        TEST_ASSERT_TRUE_MESSAGE(charsetOk(l) && charsetOk(h), l.c_str());
        if (v.n.type == N_SETTING && v.n.opts && v.n.opts < 100) {
            std::vector<std::pair<std::string, int>> e;
            TEST_ASSERT_TRUE_MESSAGE(optEntries(menuOptList(v.n.opts), e), l.c_str());
            for (auto& p : e) TEST_ASSERT_TRUE_MESSAGE(charsetOk(p.first) && p.first.size() <= 20, p.first.c_str());
        }
    }
}

void test_tree_settings_and_actions_exist(void) {
    for (auto& v : allNodes()) {
        const MenuNode& n = v.n;
        std::string l = str(n.label);
        if (n.type == N_SETTING) {
            TEST_ASSERT_TRUE_MESSAGE(n.ref < SID_COUNT, l.c_str());
            if (n.opts && n.opts < 100) {
                SettingDef d; settingsDefAt(n.ref, d);
                std::vector<std::pair<std::string, int>> e;
                TEST_ASSERT_TRUE(optEntries(menuOptList(n.opts), e));
                for (auto& p : e) TEST_ASSERT_TRUE_MESSAGE(p.second >= d.min && p.second <= d.max, p.first.c_str());
            }
        }
        if (n.type == N_ACTION && !(n.flags & NF_FN)) TEST_ASSERT_TRUE_MESSAGE(n.ref < AID_COUNT, l.c_str());
        if (n.type == N_STEPPER) TEST_ASSERT_TRUE_MESSAGE(n.ref < AID_COUNT && n.opts < AID_COUNT, l.c_str());
        if (n.type == N_ADJUST2D) {
            MenuAdjust a; menuAdjustRead(n.ref, a);
            TEST_ASSERT_TRUE(a.left < AID_COUNT && a.right < AID_COUNT && a.up < AID_COUNT && a.down < AID_COUNT);
        }
        if (n.type == N_CHOICE) {
            MenuChoice c; menuChoiceRead(n.ref, c);
            TEST_ASSERT_TRUE(c.count >= 2 && c.count <= 10);
            for (uint8_t i = 0; i < c.count; i++) {
                MenuChoiceOpt o; menuChoiceOptRead(c, i, o);
                SettingDef d; settingsDefAt(o.sid1, d);
                TEST_ASSERT_TRUE(settingsValid(d, o.v1));
                if (o.sid2 != 0xFF) { settingsDefAt(o.sid2, d); TEST_ASSERT_TRUE(settingsValid(d, o.v2)); }
                TEST_ASSERT_TRUE(charsetOk(str(o.label)));
            }
        }
    }
}

// Visibility for the structural checks: the real rules on a fake backend, worst case for the optional rows
struct Cfg { uint8_t input; bool ntsc; bool dev; uint8_t mode; };
static FakeBackend* gBe = nullptr;
static FakeBackend& beFor(const Cfg& c) {
    static FakeBackend be;
    be.apply(SID_input_source, c.input);
    be.apply(SID_sys_menu_mode, c.mode);
    be.settle();
    be.sig.advNtsc = c.ntsc;
    be.sig.updateAvailable = true;
    be.sig.advControllerUpdateAvailable = true;
    be.texts[LT_WIFI_MODE] = "AP";
    be.active = 0;
    be.dev = c.dev;
    gBe = &be;
    return be;
}
static bool nodeVisible(const MenuNode& n, const Cfg& c) { return menuVisible(n, beFor(c)); }
static bool anyVisibleChild(const MenuNode& n, const Cfg& c) {
    for (uint8_t i = 0; i < n.count; i++) { MenuNode k; menuNodeRead(&n.children[i], k); if (nodeVisible(k, c)) return true; }
    return false;
}
static void checkLists(const MenuNode& sub, const Cfg& c, const std::string& path) {
    std::vector<std::string> seen;
    for (uint8_t i = 0; i < sub.count; i++) {
        MenuNode k; menuNodeRead(&sub.children[i], k);
        if (!nodeVisible(k, c)) continue;
        std::string l = str(k.label);
        if (k.type == N_HEADER) { seen.clear(); continue; }   // a section may repeat a label (Firmware: Version)
        for (auto& s : seen) TEST_ASSERT_TRUE_MESSAGE(s != l, (path + "/" + l).c_str());
        seen.push_back(l);
        if (k.type == N_SUB && !(k.flags & NF_FN)) checkLists(k, c, path + "/" + l);
    }
}

void test_tree_no_duplicate_visible_labels(void) {
    MenuNode root; menuNodeRead(menuRoot(), root);
    for (uint8_t in = 1; in <= 6; in++)
        for (int ntsc = 0; ntsc < 2; ntsc++)
            for (int dev = 0; dev < 2; dev++)
                for (uint8_t mode = 0; mode < 2; mode++) checkLists(root, {in, ntsc != 0, dev != 0, mode}, "Menu");
}

void test_tree_every_sub_has_a_visible_child_somewhere(void) {
    for (auto& v : allNodes()) {
        if (v.n.type != N_SUB || (v.n.flags & NF_FN)) continue;
        bool ever = false;
        for (uint8_t in = 1; in <= 6 && !ever; in++)
            for (int ntsc = 0; ntsc < 2 && !ever; ntsc++) ever = anyVisibleChild(v.n, {in, ntsc != 0, true, 1});
        TEST_ASSERT_TRUE_MESSAGE(ever, str(v.n.label).c_str());
    }
}

// ---- Easy pages ----
static bool subAt(const char* path, MenuNode& out) {
    menuNodeRead(menuRoot(), out);
    std::string rest(path);
    while (!rest.empty()) {
        size_t slash = rest.find('/');
        std::string name = rest.substr(0, slash);
        rest = slash == std::string::npos ? "" : rest.substr(slash + 1);
        bool found = false;
        for (uint8_t i = 0; i < out.count && !found; i++) {
            MenuNode k; menuNodeRead(&out.children[i], k);
            if (k.type == N_SUB && str(k.label) == name) { out = k; found = true; }
        }
        if (!found) return false;
    }
    return true;
}
// Visible labels of a page in order; a header counts only when an item after it (before the next header) shows
static std::string rows(FakeBackend& be, const char* path) {
    MenuNode sub;
    if (!subAt(path, sub)) return std::string("missing ") + path;
    std::string out;
    for (uint8_t i = 0; i < sub.count; i++) {
        MenuNode k; menuNodeRead(&sub.children[i], k);
        if (k.type == N_HEADER) {
            bool any = false;
            for (uint8_t j = i + 1; j < sub.count && !any; j++) {
                MenuNode m; menuNodeRead(&sub.children[j], m);
                if (m.type == N_HEADER) break;
                any = menuVisible(m, be);
            }
            if (!any) continue;
        } else if (!menuVisible(k, be)) continue;
        if (!out.empty()) out += ", ";
        out += str(k.label);
    }
    return out;
}
static FakeBackend& easy(uint8_t input, bool interlaced, bool ntsc) {
    static FakeBackend be;
    be = FakeBackend();
    be.apply(SID_input_source, input);
    be.apply(SID_sys_menu_mode, 0);
    be.settle();
    be.sig.interlaced = interlaced;
    be.sig.advNtsc = ntsc;
    return be;
}
static void expectCommonEasyPages(FakeBackend& be) {
    TEST_ASSERT_EQUAL_STRING("Input, Output, Picture, Screen, Profiles, System, Menu mode", rows(be, "").c_str());
    TEST_ASSERT_EQUAL_STRING("Resolution, Aspect, Integer scale, 50Hz style, Always 60Hz, Smooth scrolling, HDMI range",
                             rows(be, "Output").c_str());
    TEST_ASSERT_EQUAL_STRING("Filter, Scanlines, Brightness, Contrast, Saturation, Deinterlace, Reset picture",
                             rows(be, "Picture").c_str());
    TEST_ASSERT_EQUAL_STRING("Position, Size, Edge mask, Reset screen", rows(be, "Screen").c_str());
    TEST_ASSERT_EQUAL_STRING("Load, New profile, Save changes, Discard changes, Manage, Auto-load", rows(be, "Profiles").c_str());
    TEST_ASSERT_EQUAL_STRING("WiFi, Device, TV and OLED menu, Update, Signal info, Restart, Factory reset", rows(be, "System").c_str());
    TEST_ASSERT_EQUAL_STRING("Volume, Mute, Knob direction", rows(be, "System/Device").c_str());
    TEST_ASSERT_EQUAL_STRING("Status pop-up, OLED idle, OLED idle after", rows(be, "System/TV and OLED menu").c_str());
    TEST_ASSERT_EQUAL_STRING("Mode, Network, Address, Hostname, Reset to hotspot", rows(be, "System/WiFi").c_str());
    TEST_ASSERT_EQUAL_STRING("GBS-Control, ADV Controller, Channel, Check now", rows(be, "System/Update").c_str());
}

// The 50 Hz picture is chosen on the Output page (50Hz style): the Screen page has no row for it, in Advanced either
void test_screen_page_has_no_50hz_row(void) {
    FakeBackend& be = easy(1, false, false);
    be.apply(SID_sys_menu_mode, 1);
    be.settle();
    TEST_ASSERT_EQUAL_STRING("Position, Size, Edge mask, Mask position, Reset screen", rows(be, "Screen").c_str());
}

void test_easy_pages_rgb_scart_240p(void) {
    FakeBackend& be = easy(1, false, false);
    TEST_ASSERT_EQUAL_STRING("Source, Auto search, Adaptive Sync, Sync cleanup", rows(be, "Input").c_str());
    expectCommonEasyPages(be);
}
void test_easy_pages_component_480i(void) {
    FakeBackend& be = easy(4, true, false);
    TEST_ASSERT_EQUAL_STRING("Source, Auto search, Adaptive Sync", rows(be, "Input").c_str());
    expectCommonEasyPages(be);
}
void test_easy_pages_svideo_ntsc(void) {
    FakeBackend& be = easy(5, true, true);
    TEST_ASSERT_EQUAL_STRING("Source, Auto search, Video standard, 2X, Adaptive Sync", rows(be, "Input").c_str());
    expectCommonEasyPages(be);
}
void test_easy_pages_composite_pal(void) {
    FakeBackend& be = easy(6, true, false);
    TEST_ASSERT_EQUAL_STRING("Source, Auto search, Video standard, 2X, Adaptive Sync", rows(be, "Input").c_str());
    expectCommonEasyPages(be);
}
void test_easy_pages_vga(void) {
    FakeBackend& be = easy(3, false, false);
    TEST_ASSERT_EQUAL_STRING("Source, Auto search, Adaptive Sync, VGA upscaling", rows(be, "Input").c_str());
    expectCommonEasyPages(be);
}

// The decoder standard row reads "Video standard" on the TV, the OLED and the web, and fits the 30 columns with its
// widest value
void test_video_standard_label(void) {
    FakeBackend& be = easy(5, true, true);
    MenuNode in; TEST_ASSERT_TRUE(subAt("Input", in));
    uint8_t found = 0;
    for (uint8_t i = 0; i < in.count; i++) {
        MenuNode k; menuNodeRead(&in.children[i], k);
        if (k.type != N_SETTING || (k.ref != SID_adv_format_sv && k.ref != SID_adv_format_av)) continue;
        TEST_ASSERT_EQUAL_STRING("Video standard", str(k.label).c_str());
        found++;
    }
    TEST_ASSERT_EQUAL_UINT8(2, found);
    (void)be;
}

// Digital sync: an Advanced row of Input on the boards with the sync separator, for the RGB inputs
void test_digital_sync_row(void) {
    const uint8_t rgb[] = {1, 2, 3};                   // RGB SCART, RGB SoG, VGA
    for (uint8_t in : rgb) {
        FakeBackend& be = easy(in, false, false);
        TEST_ASSERT_TRUE(rows(be, "Input").find("Digital sync") == std::string::npos);   // not in Easy
        be.apply(SID_sys_menu_mode, 1); be.settle();
        TEST_ASSERT_TRUE_MESSAGE(rows(be, "Input").find("Digital sync") != std::string::npos, rows(be, "Input").c_str());
    }
    FakeBackend& yuv = easy(4, false, false);          // Component: the sync is in the picture
    yuv.apply(SID_sys_menu_mode, 1); yuv.settle();
    TEST_ASSERT_TRUE(rows(yuv, "Input").find("Digital sync") == std::string::npos);
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER | CAP_LED);
    FakeBackend& standard = easy(1, false, false);
    standard.apply(SID_sys_menu_mode, 1); standard.settle();
    bool there = rows(standard, "Input").find("Digital sync") != std::string::npos;
    platformSetCaps(CAPS_DEFAULT);
    TEST_ASSERT_FALSE(there);
}

// Sync dejitter: an Advanced row of Input wherever the HS/VS pins carry the sync: VGA on both boards, the other RGB
// inputs of the Pro while Digital sync reads the HS pin
void test_sync_dejitter_row(void) {
    auto shown = [](FakeBackend& be) { return rows(be, "Input").find("Sync dejitter") != std::string::npos; };
    FakeBackend& vga = easy(3, false, false);
    TEST_ASSERT_FALSE(shown(vga));                                       // not in Easy
    vga.apply(SID_sys_menu_mode, 1); vga.settle();
    TEST_ASSERT_TRUE_MESSAGE(shown(vga), rows(vga, "Input").c_str());
    FakeBackend& scart = easy(1, false, false);
    scart.apply(SID_sys_menu_mode, 1); scart.settle();
    TEST_ASSERT_FALSE(shown(scart));                                     // the slicer reads the sync
    scart.apply(SID_input_digital_sync, 1); scart.settle();
    TEST_ASSERT_TRUE(shown(scart));
    FakeBackend& yuv = easy(4, false, false);
    yuv.apply(SID_sys_menu_mode, 1); yuv.apply(SID_input_digital_sync, 1); yuv.settle();
    TEST_ASSERT_FALSE(shown(yuv));                                       // Component: the sync is in the picture
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER | CAP_LED);
    FakeBackend& standard = easy(3, false, false);
    standard.apply(SID_sys_menu_mode, 1); standard.settle();
    bool there = shown(standard);
    platformSetCaps(CAPS_DEFAULT);
    TEST_ASSERT_TRUE(there);                                             // the standard board's VGA input too
}

// The HDMI levels tune the compression: shown only with HDMI range = Limited, Advanced only, on both boards
void test_hdmi_level_rows(void) {
    auto shown = [](FakeBackend& be, const char* label) { return rows(be, "Output").find(label) != std::string::npos; };
    FakeBackend& be = easy(1, false, false);
    be.apply(SID_output_hdmi, 1); be.settle();
    TEST_ASSERT_FALSE(shown(be, "HDMI black"));                          // not in Easy
    be.apply(SID_sys_menu_mode, 1); be.settle();
    TEST_ASSERT_TRUE_MESSAGE(shown(be, "HDMI black"), rows(be, "Output").c_str());
    TEST_ASSERT_TRUE(shown(be, "HDMI white"));
    be.apply(SID_output_hdmi, 0); be.settle();                           // Full: nothing to tune
    TEST_ASSERT_FALSE(shown(be, "HDMI black"));
    TEST_ASSERT_FALSE(shown(be, "HDMI white"));
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER | CAP_LED);    // the standard board: the same rule
    FakeBackend& std = easy(1, false, false);
    std.apply(SID_sys_menu_mode, 1); std.apply(SID_output_hdmi, 1); std.settle();
    bool limitedShown = shown(std, "HDMI black");
    platformSetCaps(CAPS_DEFAULT);
    TEST_ASSERT_TRUE(limitedShown);
}

// Reset screen closes the Screen page in both modes: Mask position (Advanced) sits above it, with the other masks
void test_reset_screen_is_the_last_screen_row(void) {
    for (uint8_t mode = 0; mode < 2; mode++) {
        FakeBackend& be = easy(1, false, false);
        be.apply(SID_sys_menu_mode, mode); be.settle();
        std::string r = rows(be, "Screen");
        const std::string last = "Reset screen";
        TEST_ASSERT_TRUE_MESSAGE(r.size() >= last.size() && r.compare(r.size() - last.size(), last.size(), last) == 0, r.c_str());
    }
}

// Settings with registry options take them from the registry: no menu-only list for them
void test_converted_rows_use_registry_options(void) {
    const uint8_t sids[] = {SID_pic_deint, SID_adv_filter_y, SID_adv_filter_wy, SID_adv_filter_c, SID_adv_cl_ntsc,
                            SID_adv_cl_pal, SID_adv_cc_ntsc, SID_adv_cc_pal, SID_dev_syncwatcher};
    for (auto& v : allNodes())
        if (v.n.type == N_SETTING)
            for (uint8_t sid : sids) if (v.n.ref == sid) TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, v.n.opts, str(v.n.label).c_str());
}

// Knob direction is an Easy row (TV menu too) of System > Device; the knob's steps stay Advanced, right under it.
// Restart and Factory reset close the System page in both modes
void test_knob_direction_in_easy(void) {
    FakeBackend& be = easy(1, false, false);
    std::string dev = rows(be, "System/Device");
    TEST_ASSERT_TRUE_MESSAGE(dev.find("Knob direction") != std::string::npos, dev.c_str());
    TEST_ASSERT_TRUE_MESSAGE(dev.find("Knob steps") == std::string::npos, dev.c_str());
    std::string sys = rows(be, "System");
    TEST_ASSERT_TRUE_MESSAGE(sys.size() > 22 && sys.compare(sys.size() - 22, 22, "Restart, Factory reset") == 0, sys.c_str());
    be.apply(SID_sys_menu_mode, 1);
    be.settle();
    sys = rows(be, "System");
    TEST_ASSERT_TRUE_MESSAGE(sys.compare(sys.size() - 22, 22, "Restart, Factory reset") == 0, sys.c_str());
    TEST_ASSERT_EQUAL_STRING("Volume, Mute, Knob direction, Knob steps, Board type", rows(be, "System/Device").c_str());
}

// The hotspot password shows only in hotspot mode; the installed ADV Controller version always shows (both firmware
// versions stay visible), a bundled newer build adds its own install row
void test_wifi_password_and_adv_controller_rows(void) {
    FakeBackend& be = easy(1, false, false);
    be.texts[LT_WIFI_MODE] = "AP";
    TEST_ASSERT_EQUAL_STRING("Mode, Network, Address, Hostname, Password, Reset to hotspot", rows(be, "System/WiFi").c_str());
    be.sig.advControllerUpdateAvailable = true;
    be.sig.updateAvailable = true;
    TEST_ASSERT_EQUAL_STRING("GBS-Control, ADV Controller, Channel, Check now, Found on, Install, Install ADV",
                             rows(be, "System/Update").c_str());
}

// Standard board: no STV9426 settings, the TV5725 icon switch instead
void test_standard_board_system_rows(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER | CAP_LED);
    FakeBackend& be = easy(1, false, false);
    TEST_ASSERT_EQUAL_STRING("WiFi, Device, TV and OLED menu, Update, Signal info, Restart, Factory reset", rows(be, "System").c_str());
    TEST_ASSERT_EQUAL_STRING("Knob direction", rows(be, "System/Device").c_str());   // no audio there
    TEST_ASSERT_EQUAL_STRING("Changes on TV, OLED idle, OLED idle after", rows(be, "System/TV and OLED menu").c_str());
    platformSetCaps(CAPS_DEFAULT);
}

// Knob steps is a System > Device row; every dev.* key is hidden without Developer mode, so the key must not be one
void test_knob_steps_in_system_without_devmode(void) {
    FakeBackend& be = easy(1, false, false);
    be.apply(SID_sys_menu_mode, 1);
    be.settle();
    be.dev = false;
    TEST_ASSERT_TRUE(rows(be, "System/Device").find("Knob steps") != std::string::npos);
}

void test_advanced_main_menu_rows(void) {
    FakeBackend& be = easy(1, false, false);
    be.apply(SID_sys_menu_mode, 1);
    be.settle();
    TEST_ASSERT_EQUAL_STRING("Input, Output, Picture, Screen, Profiles, System, Menu mode", rows(be, "").c_str());
    be.dev = true;
    TEST_ASSERT_EQUAL_STRING("Input, Output, Picture, Screen, Profiles, System, Developer, Menu mode", rows(be, "").c_str());
    TEST_ASSERT_EQUAL_STRING("Sync, Clock / ADC, H capture shift, Debug, Serial log, Arduino upload, OTA password",
                             rows(be, "Developer").c_str());
    TEST_ASSERT_EQUAL_STRING("Filter, Scanlines, Brightness, Contrast, Saturation, Deinterlace, Reset picture, Detail, "
                             "Sharpness, Line filter, Color sharpness, Scanline strength, Color, Red, Green, Blue, "
                             "Reset colors", rows(be, "Picture").c_str());
    TEST_ASSERT_EQUAL_STRING("WiFi, Device, TV and OLED menu, Update, Signal info, Developer menu, Restart, Factory reset",
                             rows(be, "System").c_str());
    TEST_ASSERT_EQUAL_STRING("Status pop-up, OLED idle, OLED idle after, Theme, Menu position, Pop-ups, Auto close",
                             rows(be, "System/TV and OLED menu").c_str());
    TEST_ASSERT_EQUAL_STRING("Mode, Network, Address, Hostname, Optimize WiFi, Extra hosts, Reset to hotspot", rows(be, "System/WiFi").c_str());
    TEST_ASSERT_EQUAL_STRING("Load, New profile, Save changes, Discard changes, Manage, Auto-load, Load sets input, Startup",
                             rows(be, "Profiles").c_str());
}

// UG-637 "NTSC Comb Filter Settings: used for NTSC M/NTSC J CVBS inputs": the comb rows show on Composite only
void test_decoder_comb_rows_composite_only(void) {
    FakeBackend& be = easy(5, false, true);
    be.apply(SID_sys_menu_mode, 1);
    be.settle();
    TEST_ASSERT_EQUAL_STRING("Levels, Brightness, Contrast, Saturation, Hue, Filters, Luma filter mode, Luma filter, "
                             "Enhance, Noise reduction, Color transients, Adaptive contrast, Reset comb/filters, Reset decoder",
                             rows(be, "Input/Decoder").c_str());
    be.apply(SID_input_source, 6);
    be.settle();
    TEST_ASSERT_EQUAL_STRING("Levels, Brightness, Contrast, Saturation, Hue, Comb, Luma comb, Chroma comb, Comb split, "
                             "Comb taps, Filters, Luma filter, Chroma filter, Enhance, Noise reduction, Color transients, "
                             "Adaptive contrast, Reset comb/filters, Reset decoder", rows(be, "Input/Decoder").c_str());
}

// SOG level: Advanced, no Developer menu needed, RGB SoG and Component only
void test_sog_row_in_sampling(void) {
    for (uint8_t in = 1; in <= 6; in++) {
        FakeBackend& be = easy(in, false, true);
        be.apply(SID_sys_menu_mode, 1);
        be.settle();
        bool sog = rows(be, "Input/ADC calibration").find("SOG level") != std::string::npos;
        TEST_ASSERT_EQUAL_MESSAGE(in == 2 || in == 4, sog, "input");
    }
    FakeBackend& be = easy(2, false, true);                         // Easy: the whole ADC calibration page is Advanced
    TEST_ASSERT_EQUAL_STRING("Source, Auto search, Adaptive Sync, Sync cleanup", rows(be, "Input").c_str());
}

// The quick page (and Quick setup) reuse tree nodes: same charset, length and binding rules
static void checkExtraRoot(const MenuNode* rootPgm) {
    MenuNode root; menuNodeRead(rootPgm, root);
    TEST_ASSERT_TRUE(charsetOk(str(root.label)));
    for (uint8_t i = 0; i < root.count; i++) {
        MenuNode k; menuNodeRead(&root.children[i], k);
        std::string l = str(k.label), h = str(k.help);
        TEST_ASSERT_TRUE_MESSAGE(l.size() <= 20 && charsetOk(l), l.c_str());
        TEST_ASSERT_TRUE_MESSAGE(!h.empty() && h.size() <= 96 && charsetOk(h), l.c_str());   // the help row scrolls
        if (k.type == N_SETTING) TEST_ASSERT_TRUE_MESSAGE(k.ref < SID_COUNT, l.c_str());
    }
}
void test_quick_root_strings_and_bindings(void) { checkExtraRoot(menuQuickRoot()); }

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_menu_strings_all_in_charset);
    RUN_TEST(test_tree_strings_use_rom_charset);
    RUN_TEST(test_keyboard_help_fits_its_row);
    RUN_TEST(test_reasons_fit_the_help_row);
    RUN_TEST(test_registry_enum_labels_use_rom_charset);
    RUN_TEST(test_tree_settings_and_actions_exist);
    RUN_TEST(test_tree_no_duplicate_visible_labels);
    RUN_TEST(test_tree_every_sub_has_a_visible_child_somewhere);
    RUN_TEST(test_screen_page_has_no_50hz_row);
    RUN_TEST(test_digital_sync_row);
    RUN_TEST(test_sync_dejitter_row);
    RUN_TEST(test_hdmi_level_rows);
    RUN_TEST(test_reset_screen_is_the_last_screen_row);
    RUN_TEST(test_converted_rows_use_registry_options);
    RUN_TEST(test_easy_pages_rgb_scart_240p);
    RUN_TEST(test_easy_pages_component_480i);
    RUN_TEST(test_easy_pages_svideo_ntsc);
    RUN_TEST(test_easy_pages_composite_pal);
    RUN_TEST(test_easy_pages_vga);
    RUN_TEST(test_video_standard_label);
    RUN_TEST(test_knob_direction_in_easy);
    RUN_TEST(test_wifi_password_and_adv_controller_rows);
    RUN_TEST(test_standard_board_system_rows);
    RUN_TEST(test_advanced_main_menu_rows);
    RUN_TEST(test_knob_steps_in_system_without_devmode);
    RUN_TEST(test_decoder_comb_rows_composite_only);
    RUN_TEST(test_sog_row_in_sampling);
    RUN_TEST(test_quick_root_strings_and_bindings);
    return UNITY_END();
}
