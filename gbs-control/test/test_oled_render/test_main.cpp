#include <unity.h>
#include <string>
#include <string.h>
#include "oled-fake.h"
#include "golden.h"
#include "oled-renderer.h"
#include "platform-caps.h"
#include "menu-rig.h"
#include "menu-fit.h"
#include "oled-splash.h"

void setUp(void) {}
void tearDown(void) {}

void test_real_library_draws_text(void) {
    FakeOled d;
    d.clear();
    d.setFont(ArialMT_Plain_10);
    TEST_ASSERT_EQUAL_INT(49, d.getStringWidth("Resolution"));
    d.drawString(0, 0, "H");
    TEST_ASSERT_TRUE(d.pixel(1, 3));      // left stem of the H, first glyph row
    TEST_ASSERT_FALSE(d.pixel(1, 2));
    d.display();
    TEST_ASSERT_EQUAL_INT(1, d.frames);
}

void test_pbm_format(void) {
    FakeOled d;
    d.clear();
    d.setPixel(0, 0);
    d.setPixel(127, 63);
    std::string p = d.pbm();
    TEST_ASSERT_EQUAL_STRING("P1\n128 64\n", p.substr(0, 10).c_str());
    TEST_ASSERT_EQUAL_CHAR('1', p[10]);                 // first pixel
    TEST_ASSERT_EQUAL_CHAR('1', p[p.size() - 2]);      // last pixel, before the final newline
}

// ---- Renderer ----
struct OScene {
    MenuRig r;
    FakeOled d;
    OledRenderer o{d};
    OScene() {                                      // a 240p source at 1280x960, AP mode
        r.be.sig.presetId = 0x01; r.be.sig.srcLines = 262; r.be.sig.srcHzX10 = 599; r.be.sig.outHzX10 = 600;
        r.be.texts[LT_WIFI_IP] = "192.168.4.1"; r.be.texts[LT_WIFI_SSID] = "gbscontrol"; r.be.texts[LT_WIFI_MODE] = "AP";
    }
    std::string shot() { MenuIdle i; r.e.idle(i); o.draw(r.v(), i, false, r.now); return d.pbm(); }
    void clickEnter(const char* l) { TEST_ASSERT_TRUE_MESSAGE(r.select(l), l); r.key(K_CLICK); }
};

static void oledGolden(OScene& s, const char* name) {
    TEST_ASSERT_TRUE_MESSAGE(goldenCheckFile("oled", name, "pbm", s.shot()), name);
}

// The changed-from-default dot is not drawn on the editing row, where it touched the outline
void test_oled_no_changed_dot_on_the_editing_row(void) {
    OScene s;
    s.r.be.apply(SID_osd_theme, 1); s.r.be.settle();             // differs from the default
    s.r.openFull(); s.clickEnter("System"); s.clickEnter("OSD settings");
    s.clickEnter("Theme");                                       // edit mode
    s.shot();
    int y = 14 + s.r.v().cursorRow * 10;
    TEST_ASSERT_TRUE(s.r.v().rows[s.r.v().cursorRow].flags & ROWF_EDITING);
    TEST_ASSERT_FALSE(s.d.pixel(1, y + 4) || s.d.pixel(2, y + 4) || s.d.pixel(1, y + 5) || s.d.pixel(2, y + 5));
}

// Scrolling text on the OLED: the band shows the selected row's description once after the cursor rests 1.5 s, then
// the title again
void test_oled_band_shows_the_description_once(void) {
    OScene s;
    s.r.be.apply(SID_osd_timeout, 4); s.r.be.settle();           // 60 s: the pass is not cut by the menu timeout
    s.r.openFull();                                              // root, Input selected
    std::string title = s.shot();
    s.r.wait(1000);
    TEST_ASSERT_TRUE(title == s.shot());                         // resting less than 1.5 s
    s.r.wait(1000);
    std::string desc = s.shot();
    TEST_ASSERT_TRUE(desc != title);                             // the description replaced the title
    s.r.wait(3000);
    TEST_ASSERT_TRUE(s.shot() != desc);                          // it scrolls
    s.r.wait(30000);
    TEST_ASSERT_TRUE(s.shot() == title);                         // one pass, then the title stays
}

void test_oled_selected_long_name_scrolls(void) {
    OScene s;
    ProfileEntry& e = s.r.be.list.at(0);
    e.used = true; strcpy(e.name, "Super Famicom Jr Turbo X"); e.region = PROFILE_REGION_NTSC_J;
    s.r.be.active = 0;                                            // the "Active" tag leaves no room: it scrolls
    s.r.openFull(); s.clickEnter("Profiles"); s.clickEnter("Load");
    TEST_ASSERT_TRUE(s.r.select("Super Famicom Jr T NTSC-J"));
    std::string a = s.shot();
    s.r.wait(3500);
    std::string b = s.shot();
    TEST_ASSERT_TRUE(a != b);                                    // the label moved (or the band shows help)
}

void test_oled_home_long_profile_scrolls_once(void) {
    OScene s;
    ProfileEntry& e = s.r.be.list.at(0);
    e.used = true; strcpy(e.name, "Super Famicom Jr Turbo X"); e.region = PROFILE_REGION_NTSC_J;
    s.r.be.active = 0;
    std::string a = s.shot();
    s.r.wait(3000);
    TEST_ASSERT_TRUE(a != s.shot());                             // scrolling
    s.r.wait(30000);
    TEST_ASSERT_TRUE(a == s.shot());                             // one pass, then at rest
}

// Profiles screens on the OLED: the Save row's long value and a long profile title scroll
void test_oled_profiles_value_and_title_scroll(void) {
    OScene s;
    ProfileEntry& e = s.r.be.list.at(0);
    e.used = true; strcpy(e.name, "Super Famicom Jr Turbo X");
    s.r.be.active = 0;
    s.r.openFull(); s.clickEnter("Profiles");
    TEST_ASSERT_TRUE(s.r.select("Save changes"));
    std::string v0 = s.shot();
    s.r.wait(2000);
    TEST_ASSERT_TRUE(v0 != s.shot());                            // the long value scrolls
    OScene m;
    ProfileEntry& f = m.r.be.list.at(0);
    f.used = true; strcpy(f.name, "Super Famicom Jr Turbo X");
    m.r.openFull(); m.clickEnter("Profiles"); m.clickEnter("Manage"); m.r.key(K_CLICK);
    std::string t0 = m.shot();
    m.r.wait(2000);                                               // after the 1.5 s hold
    TEST_ASSERT_TRUE(t0 != m.shot());                            // the long title scrolls in the band
}

void test_oled_brightness_slider(void) {
    OScene s;
    s.r.be.apply(SID_pic_brightness, 30); s.r.be.settle();
    s.r.openFull(); s.clickEnter("Picture");
    TEST_ASSERT_TRUE(s.r.select("Brightness"));
    oledGolden(s, "picture-brightness");
}

void test_oled_quick_page(void) { OScene s; s.r.key(K_CLICK); oledGolden(s, "quick-page"); }

void test_oled_tv_osd(void) {                       // standard board: the TV5725 icon mode
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER);
    OScene s; s.r.e.setEncoderBackRow(true);
    s.r.key(K_CLICK);
    TEST_ASSERT_TRUE(s.r.select("TV OSD"));
    s.r.key(K_CLICK);
    platformSetCaps(CAPS_DEFAULT);
}
// No signal with an active profile that has a region: the hint "OK: choose the input" keeps the whole line (the
// region cut it to "OK: choose the."), the same picture as with no profile
void test_oled_idle_no_signal_hides_the_region(void) {
    OScene plain; plain.r.be.sig.noSignal = true;
    OScene s; s.r.be.sig.noSignal = true;
    s.r.be.list.at(0).used = true; strcpy(s.r.be.list.at(0).name, "Mega Drive"); s.r.be.list.at(0).region = PROFILE_REGION_NTSC_U;
    s.r.be.active = 0;
    TEST_ASSERT_TRUE(plain.shot() == s.shot());
    plain.r.be.sig.searching = true; s.r.be.sig.searching = true;
    TEST_ASSERT_TRUE(plain.shot() == s.shot());
}
void test_oled_root(void) { OScene s; s.r.openFull(); oledGolden(s, "root"); }
void test_oled_info(void) { OScene s; s.r.key(K_INFO); oledGolden(s, "ov-info"); }
void test_oled_confirm(void) {
    OScene s; s.r.openFull(); s.clickEnter("System"); s.clickEnter("Restart");
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, s.r.v().overlay);
    oledGolden(s, "ov-confirm");
}
void test_oled_revert(void) {
    OScene s; s.r.openFull(); s.clickEnter("Output"); s.clickEnter("Resolution");
    s.r.key(K_ROT_R); s.r.key(K_CLICK);                            // pick the next resolution and apply it
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, s.r.v().overlay);
}
void test_oled_toast(void) {
    OScene s; s.r.openFull(); s.clickEnter("Profiles"); s.clickEnter("New profile");
    s.r.key(K_CLICK);                                              // Signal name row
    s.r.be.result = ""; s.r.be.resultSeq++;
    s.r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, s.r.v().overlay);
}
void test_oled_adjust(void) {
    OScene s; s.r.openFull(); s.clickEnter("Screen"); s.clickEnter("Position");
    TEST_ASSERT_EQUAL_UINT8(OV_ADJUST, s.r.v().overlay);
}
void test_oled_update(void) {
    OScene s; s.r.be.sig.updatePct = 42; s.r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, s.r.v().overlay);
    oledGolden(s, "ov-update");
}

// Cycle rows whose longest value leaves no room for the whole label: the renderer clips the label, the value
// always stays whole; the v3 labels and values all fit, only the catalog row is left
static const char* const kLabelClipped[] = {
    "Menu/Profiles/New profile/SNES/Super Famicom",            // catalog row + region
};
// The decoder row reads "Video standard" everywhere; with its long values ("PAL-M 7.5 IRE") the
// OLED clips the label ("Video stan") and keeps the value whole, as for the catalog row
static const char* const kLabelClippedPrefix[] = {
    "Menu/Input/Video standard = ",
};

struct OledFit : MenuFitRule {
    FakeOled d;
    bool fits(const std::string& l, const std::string& v, uint8_t kind) override {
        return OledRenderer::rowFits(d, l.c_str(), v.c_str(), kind);
    }
};

void test_oled_fit_rule(void) {
    FakeOled d;
    TEST_ASSERT_TRUE(OledRenderer::rowFits(d, "Resolution", "1920x1080", ROWK_CYCLE));
    TEST_ASSERT_FALSE(OledRenderer::rowFits(d, "A label that is far too long", "1920x1080", ROWK_CYCLE));
    TEST_ASSERT_TRUE(OledRenderer::rowFits(d, "Back", "", ROWK_ACTION));
    TEST_ASSERT_TRUE(OledRenderer::rowFits(d, "Menu position", "Bottom right", ROWK_CYCLE));   // kGap 2
}

void test_oled_every_row_fits(void) {
    OledFit rule;
    std::vector<std::string> bad;
    for (auto& m : menuFitAll(rule)) {
        bool allowed = false;
        for (const char* k : kLabelClipped) allowed = allowed || m == k;
        for (const char* k : kLabelClippedPrefix) allowed = allowed || m.rfind(k, 0) == 0;
        if (!allowed) bad.push_back(m);
    }
    std::string all;
    for (auto& m : bad) all += m + "\n";
    TEST_ASSERT_TRUE_MESSAGE(bad.empty(), all.c_str());
}

void test_render_sends_frames_only_on_change(void) {
    OScene s;
    MenuIdle i; s.r.e.idle(i);
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, 1000));
    TEST_ASSERT_FALSE(s.o.render(s.r.v(), i, 1100));        // nothing changed
    s.r.key(K_CLICK);
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, 1200));
    s.r.key(K_ROT_R);
    TEST_ASSERT_FALSE(s.o.render(s.r.v(), i, 1220));        // within 50 ms of the last frame
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, 1260));
    TEST_ASSERT_EQUAL_INT(3, s.d.frames);
}

void test_idle_redraws_only_when_its_text_changes(void) {
    OScene s;
    MenuIdle i; s.r.e.idle(i);
    s.o.render(s.r.v(), i, 1000);
    MenuIdle j = i;
    TEST_ASSERT_FALSE(s.o.render(s.r.v(), j, 2000));
    snprintf(j.net, sizeof(j.net), "10.0.0.2");
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), j, 3000));
}

void test_saver_starts_after_180s_and_steps(void) {
    OScene s;
    MenuIdle i; s.r.e.idle(i);
    s.o.activity(0);
    s.o.render(s.r.v(), i, 1000);
    TEST_ASSERT_FALSE(s.o.render(s.r.v(), i, 179000));
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, 180000));       // saver on
    TEST_ASSERT_FALSE(s.o.render(s.r.v(), i, 200000));
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, 210000));       // next position after 30 s
    s.r.be.sig.presetId = 0x05;                             // the information changed: redrawn at once
    MenuIdle j; s.r.e.idle(j);
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), j, 211000));
    s.o.activity(212000);
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), j, 212000));       // back to the home screen
}

// Screensaver: the resolution path large with profile and region, at a position that moves
void test_saver_shows_the_path_and_moves(void) {
    OScene s;
    s.r.be.list.at(0).used = true; strcpy(s.r.be.list.at(0).name, "Mega Drive"); s.r.be.list.at(0).region = PROFILE_REGION_NTSC_U;
    s.r.be.active = 0;
    MenuIdle i; s.r.e.idle(i);
    s.o.activity(0);
    s.o.render(s.r.v(), i, 180000);
    std::string a = s.d.pbm();
    TEST_ASSERT_TRUE(goldenCheckFile("oled", "saver-path", "pbm", a));
    s.o.render(s.r.v(), i, 210000);
    TEST_ASSERT_TRUE(a != s.d.pbm());                       // another place on the panel
}

void test_saver_never_over_an_open_menu(void) {
    OScene s;
    MenuIdle i; s.r.e.idle(i);
    s.o.activity(0);
    s.r.key(K_CLICK);
    s.o.render(s.r.v(), i, 1000);
    s.o.render(s.r.v(), i, 200000);                         // may redraw (scrolling text), never the saver
    TEST_ASSERT_TRUE(s.d.pixel(0, 0) && s.d.pixel(127, 5));  // the list's inverted band is still there
}

// ---- 4d ----
void test_oled_keyboard(void) {
    OScene s; s.r.openFull(); s.clickEnter("Profiles"); s.clickEnter("New profile");
    s.r.key(K_ROT_R); s.r.key(K_CLICK);                            // Edit name...
    for (int i = 0; i < 15; i++) s.r.key(K_ROT_R);                // second row, 'P'
    oledGolden(s, "kbd-name");
}

// ---- OLED layout details ----
static MenuView listView(uint8_t rows) {
    MenuView v; memset(&v, 0, sizeof(v));
    v.mode = VIEW_LIST;
    strcpy(v.title, "Picture");
    v.posIndex = 1; v.posCount = 12;
    v.rowCount = rows;
    for (uint8_t i = 0; i < rows; i++) {
        snprintf(v.rows[i].label, sizeof(v.rows[i].label), "Row %u", i);
        strcpy(v.rows[i].value, "On");
        v.rows[i].slider = -1;
    }
    return v;
}
static void drawView(FakeOled& d, const MenuView& v) { OledRenderer o(d); MenuIdle i = {}; o.draw(v, i, false, 0); }
static int litIn(const FakeOled& d, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) n += d.pixel(x, y);
    return n;
}

void test_title_band_inverted(void) {
    FakeOled d; MenuView v = listView(3); drawView(d, v);
    for (int x = 0; x < 128; x++) TEST_ASSERT_TRUE(d.pixel(x, 0) && d.pixel(x, 12));   // band rows 0 and 12
    TEST_ASSERT_TRUE(litIn(d, 2, 3, 40, 9) < 33 * 7);                                  // black title text inside
    TEST_ASSERT_EQUAL_INT(0, litIn(d, 0, 13, 127, 13));                                // 1 px gap under the band
}

void test_slider_track_and_fill(void) {
    FakeOled d; MenuView v = listView(1);
    strcpy(v.rows[0].value, "128"); v.rows[0].flags = ROWF_SLIDER; v.rows[0].kind = ROWK_SLIDER; v.rows[0].slider = 10;
    drawView(d, v);
    int y = 14;
    for (int x = 72; x <= 101; x++) TEST_ASSERT_TRUE(d.pixel(x, y + 4));               // 1 px track, 30 px
    TEST_ASSERT_FALSE(d.pixel(102, y + 4));
    for (int x = 72; x <= 86; x++) for (int r = 2; r <= 6; r++) TEST_ASSERT_TRUE(d.pixel(x, y + r));   // fill 15 px
    TEST_ASSERT_FALSE(d.pixel(87, y + 2));
}

// The "more below" triangle covered the last row's value ("48"); the title's n/m already says where the cursor is,
// so rows below draw nothing extra
void test_more_rows_below_draw_no_marker(void) {
    FakeOled a, b; MenuView v = listView(5);
    v.moreBelow = false; drawView(a, v);
    v.moreBelow = true; drawView(b, v);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) TEST_ASSERT_EQUAL(a.pixel(x, y), b.pixel(x, y));
}

static std::string infoAt(OScene& s, uint32_t t) { MenuIdle i; s.r.e.idle(i); s.o.draw(s.r.v(), i, false, t); return s.d.pbm(); }
static void infoAt0(OScene& s) { infoAt(s, s.r.now); }
static const int kInfoOutY = 14 + 10;                  // second row of the panel (title band 14 px, rows 10 px)

// Info panel: Input, Output (the lock as an icon), Profile, WiFi (network name), IP
void test_info_rows(void) {
    MenuView v; memset(&v, 0, sizeof(v));
    v.mode = VIEW_OVERLAY; v.overlay = OV_INFO; v.lineCount = 5;
    const char* l[5] = {"IN   RGBs 240p  59.9Hz", "OUT  1280x960  60.0Hz LOCK", "PROF Mega Drive", "NET  home",
                        "IP   192.168.0.99"};
    for (int i = 0; i < 5; i++) strcpy(v.lines[i], l[i]);
    OledInfoRow r[5];
    TEST_ASSERT_EQUAL_UINT8(5, oledInfoRows(v, r));
    TEST_ASSERT_EQUAL_STRING("Input", r[0].label);   TEST_ASSERT_EQUAL_STRING("RGBs 240p", r[0].value);
    TEST_ASSERT_EQUAL_STRING("Output", r[1].label);  TEST_ASSERT_EQUAL_STRING("1280x960 60Hz", r[1].value);
    TEST_ASSERT_TRUE(r[1].lock);
    TEST_ASSERT_EQUAL_STRING("Profile", r[2].label); TEST_ASSERT_EQUAL_STRING("Mega Drive", r[2].value);
    TEST_ASSERT_EQUAL_STRING("WiFi", r[3].label);    TEST_ASSERT_EQUAL_STRING("home", r[3].value);
    TEST_ASSERT_EQUAL_STRING("IP", r[4].label);      TEST_ASSERT_EQUAL_STRING("192.168.0.99", r[4].value);
    strcpy(v.lines[1], "OUT  1280x960  60.0Hz");
    oledInfoRows(v, r);
    TEST_ASSERT_FALSE(r[1].lock);
    strcpy(v.lines[2], "PROF Mega  Drive");                                                // a double space in the name
    oledInfoRows(v, r);
    TEST_ASSERT_EQUAL_STRING("Mega  Drive", r[2].value);
    v.lineCount = 4;                                                                       // no signal (engine lines)
    strcpy(v.lines[0], "IN   No signal"); strcpy(v.lines[1], "PROF Defaults");
    strcpy(v.lines[2], "NET  home"); strcpy(v.lines[3], "IP   192.168.4.1");
    TEST_ASSERT_EQUAL_UINT8(4, oledInfoRows(v, r));
    TEST_ASSERT_EQUAL_STRING("No signal", r[0].value);
    TEST_ASSERT_EQUAL_STRING("Profile", r[1].label);
    TEST_ASSERT_EQUAL_STRING("WiFi", r[2].label);
    TEST_ASSERT_EQUAL_STRING("IP", r[3].label);
}

// The lock icon closes the Output row only when the output is locked; 1920x1200 59.9Hz plus the icon falls back
// to "1200p 59.9Hz"
void test_oled_info_lock_icon(void) {
    auto lockBar = [](FakeOled& d) {                   // the lock's full-width row: 7 lit pixels at x 120..126
        for (int x = 120; x <= 126; x++) if (!d.pixel(x, kInfoOutY + 5)) return false;
        return true;
    };
    OScene s; s.r.be.sig.frameLocked = true; s.r.key(K_INFO);
    infoAt0(s);
    TEST_ASSERT_TRUE(lockBar(s.d));
    OScene u; u.r.key(K_INFO);
    infoAt0(u);
    TEST_ASSERT_FALSE(lockBar(u.d));
    MenuView v; memset(&v, 0, sizeof(v));
    v.mode = VIEW_OVERLAY; v.overlay = OV_INFO; v.lineCount = 1;
    strcpy(v.lines[0], "OUT  1920x1200  59.9Hz LOCK");
    OledInfoRow r[5];
    oledInfoRows(v, r);
    TEST_ASSERT_EQUAL_STRING("1920x1200 59.9Hz", r[0].value);
    FakeOled d;
    char sv[32];
    oledInfoShownValue(d, r[0], sv, sizeof(sv));
    TEST_ASSERT_EQUAL_STRING("1200p 59.9Hz", sv);
}

// "Profile" in full; a name longer than the room beside it scrolls (loops), a short one rests

void test_oled_info_long_profile_scrolls(void) {
    OScene s;
    s.r.be.texts[LT_PROFILE_ACTIVE] = "Super Nintendo Mini 240p";
    s.r.key(K_INFO);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, s.r.v().overlay);
    std::string a = infoAt(s, s.r.now), b = infoAt(s, s.r.now + 1000), c = infoAt(s, s.r.now + 1500 + 300 * 3);
    TEST_ASSERT_TRUE(a == b);                          // the start holds
    TEST_ASSERT_TRUE(a != c);                          // then it moves
    char line[40]; snprintf(line, sizeof(line), "PROF %s", "Super Nintendo Mini 240p");
    bool whole = false;                                // the engine hands over all 24 letters
    for (uint8_t k = 0; k < s.r.v().lineCount; k++) whole = whole || !strcmp(s.r.v().lines[k], line);
    TEST_ASSERT_TRUE(whole);
    MenuIdle i; s.r.e.idle(i);
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, s.r.now + 5000));
    TEST_ASSERT_TRUE(s.o.render(s.r.v(), i, s.r.now + 5300));   // redrawn while it scrolls
}

void test_oled_info_long_ssid_scrolls(void) {
    OScene s;
    s.r.be.texts[LT_WIFI_SSID] = "Vodafone-Casa-Rossi-5GHz-Extende";
    s.r.key(K_INFO);
    std::string a = infoAt(s, s.r.now), b = infoAt(s, s.r.now + 1000), c = infoAt(s, s.r.now + 1500 + 300 * 3);
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_TRUE(a != c);
}

void test_oled_info_short_profile_rests(void) {
    OScene s;
    s.r.be.texts[LT_PROFILE_ACTIVE] = "SNES";
    s.r.key(K_INFO);
    TEST_ASSERT_TRUE(infoAt(s, s.r.now) == infoAt(s, s.r.now + 1500 + 300 * 3));
}

void test_keyboard_field_below_band(void) {
    OScene s; s.r.openFull(); s.clickEnter("Profiles"); s.clickEnter("New profile"); s.r.key(K_ROT_R); s.r.key(K_CLICK);
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, s.r.v().overlay);
    s.shot();
    for (int x = 0; x < 128; x++) TEST_ASSERT_TRUE(s.d.pixel(x, 0));
    TEST_ASSERT_EQUAL_INT(0, litIn(s.d, 0, 13, 127, 13));
}

void test_idle_band_and_rows(void) {
    OScene s; s.shot();
    for (int x = 0; x < 128; x++) TEST_ASSERT_TRUE(s.d.pixel(x, 0));
    TEST_ASSERT_EQUAL_INT(0, litIn(s.d, 0, 13, 127, 13));
}

void test_big_overlay_bar(void) {
    MenuView v; memset(&v, 0, sizeof(v));
    v.mode = VIEW_OVERLAY; v.overlay = OV_VOLUME; v.value = 50; strcpy(v.lines[0], "Volume");   // percent
    FakeOled d; drawView(d, v);
    for (int x = 0; x < 128; x++) TEST_ASSERT_TRUE(d.pixel(x, 0));
    TEST_ASSERT_TRUE(d.pixel(8, 44) && d.pixel(119, 44) && d.pixel(8, 53) && d.pixel(119, 53));   // frame
    TEST_ASSERT_TRUE(d.pixel(10, 46) && d.pixel(63, 51));                                         // fill 54 px
    TEST_ASSERT_FALSE(d.pixel(65, 48));
}

void test_confirm_label_below_band(void) {
    MenuView v; memset(&v, 0, sizeof(v));
    v.mode = VIEW_OVERLAY; v.overlay = OV_CONFIRM; strcpy(v.lines[0], "Erase settings and profiles?");
    FakeOled d; drawView(d, v);
    for (int x = 0; x < 128; x++) TEST_ASSERT_TRUE(d.pixel(x, 0));
    TEST_ASSERT_EQUAL_INT(0, litIn(d, 0, 13, 127, 13));
    TEST_ASSERT_TRUE(litIn(d, 0, 16, 127, 40) > 20);                                   // the label under the band
}

// Boot splash: artwork + version, author and link in ArialMT_Plain_10 (the "/" kept 1 px clear of the "m")
void test_oled_splash(void) {
    FakeOled o;
    oledDrawSplash(o, "3.0.0");
    TEST_ASSERT_TRUE(goldenCheckFile("oled", "splash", "pbm", o.pbm()));
    TEST_ASSERT_EQUAL_INT(1, o.frames);               // sent to the display once
    for (int x = 0; x < 128; x++) TEST_ASSERT_FALSE(o.pixel(x, 63));   // bottom row stays clear
}

// Home screen: RGBs 240p CSync -> 960p locked, Mega Drive NTSC
void test_oled_home_info_block(void) {
    OScene s;
    s.r.be.sig.srcLines = 262; s.r.be.sig.srcHzX10 = 599; s.r.be.sig.presetId = 0x01; s.r.be.sig.outHzX10 = 600;
    s.r.be.sig.frameLocked = true; s.r.be.sig.syncType = 1;
    s.r.be.texts[LT_WIFI_IP] = "192.168.0.99"; s.r.be.texts[LT_WIFI_MODE] = "Client"; s.r.be.lives[LV_WIFI_RSSI] = -54;
    s.r.be.list.at(0).used = true; strcpy(s.r.be.list.at(0).name, "Mega Drive"); s.r.be.list.at(0).region = PROFILE_REGION_NTSC_U;
    s.r.be.active = 0;
    oledGolden(s, "home-c");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_oled_brightness_slider);
    RUN_TEST(test_oled_profiles_value_and_title_scroll);
    RUN_TEST(test_oled_band_shows_the_description_once);
    RUN_TEST(test_oled_selected_long_name_scrolls);
    RUN_TEST(test_oled_home_long_profile_scrolls_once);
    RUN_TEST(test_oled_no_changed_dot_on_the_editing_row);
    RUN_TEST(test_real_library_draws_text);
    RUN_TEST(test_pbm_format);
    RUN_TEST(test_oled_idle_no_signal_hides_the_region);
    RUN_TEST(test_oled_tv_osd);
    RUN_TEST(test_oled_root);
    RUN_TEST(test_oled_info);
    RUN_TEST(test_oled_confirm);
    RUN_TEST(test_oled_quick_page);
    RUN_TEST(test_oled_revert);
    RUN_TEST(test_oled_toast);
    RUN_TEST(test_oled_adjust);
    RUN_TEST(test_oled_update);
    RUN_TEST(test_oled_fit_rule);
    RUN_TEST(test_oled_every_row_fits);
    RUN_TEST(test_render_sends_frames_only_on_change);
    RUN_TEST(test_idle_redraws_only_when_its_text_changes);
    RUN_TEST(test_saver_starts_after_180s_and_steps);
    RUN_TEST(test_saver_shows_the_path_and_moves);
    RUN_TEST(test_saver_never_over_an_open_menu);
    RUN_TEST(test_oled_keyboard);
    RUN_TEST(test_title_band_inverted);
    RUN_TEST(test_slider_track_and_fill);
    RUN_TEST(test_more_rows_below_draw_no_marker);
    RUN_TEST(test_info_rows);
    RUN_TEST(test_oled_info_long_profile_scrolls);
    RUN_TEST(test_oled_info_short_profile_rests);
    RUN_TEST(test_oled_info_long_ssid_scrolls);
    RUN_TEST(test_oled_info_lock_icon);
    RUN_TEST(test_keyboard_field_below_band);
    RUN_TEST(test_idle_band_and_rows);
    RUN_TEST(test_big_overlay_bar);
    RUN_TEST(test_confirm_label_below_band);
    RUN_TEST(test_oled_splash);
    RUN_TEST(test_oled_home_info_block);
    return UNITY_END();
}
