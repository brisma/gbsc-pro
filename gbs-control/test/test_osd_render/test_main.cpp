#include <unity.h>
#include <string>
#include "osd-theme.h"
#include "osd-renderer.h"
#include "menu-fake.h"
#include "menu-engine.h"
#include "stv9426-driver.h"
#include "stv-fake.h"
#include "stv9426-udc.h"
#include "profile-names.h"
#include "golden.h"
#include "menu-fit.h"
#include "settings-registry.h"
#include "api-actions.h"
#include "menu-tree.h"
#include "menu-model.h"
#include "menu-strings.h"
#include <vector>

void setUp(void) {}
void tearDown(void) {}

// ---- Theme roles ----
void test_classic_matches_v2_colors(void) {
    TEST_ASSERT_EQUAL_HEX8(0x17, osdAttr(1, R_N));   // white on blue (v2 OSD_TEXT_NORMAL)
    TEST_ASSERT_EQUAL_HEX8(0x60, osdAttr(1, R_C));   // black on yellow (v2 OSD_CURSOR_ACTIVE)
    TEST_ASSERT_EQUAL_HEX8(0x14, osdAttr(1, R_D));   // red on blue (v2 OSD_TEXT_DISABLED)
    TEST_ASSERT_EQUAL_HEX8(0x30, osdAttr(1, R_T));   // black on cyan (title)
}

// GBSC PRO theme: black and yellow, no blue (hard to read on black); yellow = accent and cursor bar
void test_gbsc_theme_black_and_yellow(void) {
    TEST_ASSERT_EQUAL_HEX8(0x07, osdAttr(0, R_N));    // white on black
    TEST_ASSERT_EQUAL_HEX8(0x60, osdAttr(0, R_C));    // black on yellow: the highlighted row
    TEST_ASSERT_EQUAL_HEX8(0x70, osdAttr(0, R_T));    // title: black on a white bar, never mistaken for the cursor
    TEST_ASSERT_EQUAL_HEX8(0x70, osdAttr(0, R_H));    // help row: black on a white bar like the title, not a menu item
    TEST_ASSERT_EQUAL_HEX8(0x04, osdAttr(0, R_D));    // disabled: red on black
    TEST_ASSERT_EQUAL_HEX8(0x64, osdAttr(0, R_DC));   // disabled under the cursor: red on yellow
    TEST_ASSERT_EQUAL_HEX8(0x06, osdAttr(0, R_A));    // accent: yellow on black
    TEST_ASSERT_EQUAL_HEX8(0x06, osdAttr(0, R_F));
    TEST_ASSERT_EQUAL_HEX8(0x06, osdAttr(0, R_BF));
    for (uint8_t r = 0; r < R_COUNT; r++) {
        uint8_t a = osdAttr(0, r);
        TEST_ASSERT_NOT_EQUAL(1, a & 7);
        TEST_ASSERT_NOT_EQUAL(1, (a >> 4) & 7);
    }
}

// A short message on the help row is a yellow bar with black text (GBSC PRO); the other themes
// keep their accent colour there
void test_message_role_per_theme(void) {
    TEST_ASSERT_EQUAL_HEX8(0x60, osdAttr(0, R_MSG));
    for (uint8_t t = 1; t < 4; t++) TEST_ASSERT_EQUAL_HEX8(osdAttr(t, R_A), osdAttr(t, R_MSG));
}

// The help row never looks like a menu row; Retro draws it on the title's dark bar
void test_help_row_differs_from_menu_rows(void) {
    for (uint8_t t = 0; t < 4; t++) TEST_ASSERT_NOT_EQUAL_HEX8(osdAttr(t, R_N), osdAttr(t, R_H));
    TEST_ASSERT_EQUAL_HEX8(osdAttr(3, R_T), osdAttr(3, R_H));   // Retro: green on black, like the title
}

void test_theme_roles_visible(void) {
    for (uint8_t t = 0; t < 4; t++)
        for (uint8_t r = 0; r < R_COUNT; r++) {
            uint8_t a = osdAttr(t, r);
            TEST_ASSERT_NOT_EQUAL((a >> 4) & 7, a & 7);
        }
}

// The selected n/a row is highlighted: disabled text on the cursor background
void test_disabled_cursor_role_per_theme(void) {
    TEST_ASSERT_EQUAL_HEX8(0x64, osdAttr(0, R_DC));   // GBSC PRO: red on yellow
    TEST_ASSERT_EQUAL_HEX8(0x64, osdAttr(1, R_DC));   // Classic: red on yellow
    TEST_ASSERT_EQUAL_HEX8(0x34, osdAttr(2, R_DC));   // Light: red on cyan
    TEST_ASSERT_EQUAL_HEX8(0x64, osdAttr(3, R_DC));   // Retro: red on yellow
}

void test_out_of_range_theme_falls_back_to_the_default(void) {
    TEST_ASSERT_EQUAL_HEX8(osdAttr(0, R_N), osdAttr(9, R_N));
    TEST_ASSERT_EQUAL_HEX8(osdAttr(0, R_N), osdAttr(4, R_N));   // the old GBSC PRO value
    TEST_ASSERT_EQUAL_HEX8(0x70, osdAttr(2, R_N));   // light: black on white
    TEST_ASSERT_EQUAL_HEX8(0x20, osdAttr(3, R_N));   // retro: black on green
}

// ---- List renderer ----
struct Scene {
    FakeBackend be;
    MenuEngine e;
    FakeStv f;
    Stv9426 d;
    OsdRenderer r;
    uint32_t now = 1000;
    uint8_t theme = 0;
    explicit Scene(StvOutStd out = OUT_1080P60) : e(be), d(f), r(d) { d.begin(STV_XTAL_HZ); d.setOutput(out); }
    void draw() { r.render(e.view(), theme, now); d.service(now); now += 50; d.service(now); r.render(e.view(), theme, now); d.service(now); }
    void key(uint8_t k) { now += 100; e.key(k, false, now); e.tick(now); draw(); }
    void wait(uint32_t ms) { now += ms; e.tick(now); draw(); }
    bool select(const std::string& l) {
        for (int i = 0; i < 40; i++) { if (std::string(e.view().rows[e.view().cursorRow].label) == l) return true; key(K_DOWN); }
        return false;
    }
    void enter(const std::string& l) { select(l); key(K_OK); }
    std::string screen() { return f.decode(stvActiveLines(d.output()) + 60); }
};

static void checkGolden(Scene& s, const char* name) {
    std::string out = s.screen();
    TEST_ASSERT_TRUE_MESSAGE(out.find("NO-EOL") == std::string::npos, name);
    TEST_ASSERT_TRUE_MESSAGE(goldenCheck(name, out), name);
}

void test_golden_root(void) { Scene s; s.key(K_MENU); checkGolden(s, "menu-root"); }   // the default theme, GBSC PRO
void test_golden_root_classic(void) { Scene s; s.theme = 1; s.key(K_MENU); checkGolden(s, "menu-root-classic"); }

void test_golden_picture_top(void) {
    Scene s;
    s.be.apply(SID_pic_blue, 121); s.be.settle();
    s.key(K_MENU); s.enter("Picture");
    checkGolden(s, "menu-picture-top");
}

void test_golden_confirm_yes_selected(void) {
    Scene s;
    s.key(K_MENU); s.enter("System"); s.select("Restart"); s.key(K_OK); s.key(K_LEFT);
    checkGolden(s, "menu-confirm-yes");
}

// The status pop-up in the pop-up place, with two profiles on this signal
void test_golden_status_popup(void) {
    Scene s;
    s.be.sig.srcLines = 262; s.be.sig.srcHzX10 = 599; s.be.sig.presetId = 0x01; s.be.sig.outHzX10 = 600;
    ProfileEntry& e = s.be.list.at(0);
    e.used = true; e.hasFp = true; strcpy(e.name, "SNES"); e.region = PROFILE_REGION_NTSC_J;
    ProfileEntry& f = s.be.list.at(1);
    f.used = true; f.hasFp = true; strcpy(f.name, "SNES"); f.region = PROFILE_REGION_NTSC_U;
    s.be.active = 0; s.be.matches = {0, 1};
    s.e.statusPopup(s.now, true); s.wait(10);
    checkGolden(s, "overlay-status");
}

void test_golden_quick_page(void) { Scene s; s.key(K_OK); checkGolden(s, "quick-page"); }

static std::string rowText(Scene& s, const std::string& label) {
    std::string out = s.screen();
    size_t p = out.find("|" + std::string(" ") + label);
    if (p == std::string::npos) return "";
    size_t e = out.find('|', p + 1);
    return out.substr(p + 1, e - p - 1);
}

static int glyphCount(Scene& s, uint8_t row, uint8_t g) {
    int n = 0;
    for (uint8_t c = 0; c < 30; c++) if (s.d.shadow().code(row, c) == s.d.glyph(g)) n++;
    return n;
}

void test_selected_enum_arrows_dropped_when_too_long(void) {
    Scene s;
    s.key(K_MENU); s.enter("Picture"); s.select("Deinterlace");
    uint8_t row0 = s.e.view().cursorRow + 1;
    TEST_ASSERT_EQUAL_INT(1, glyphCount(s, row0, UDC_CHEV_L));      // "Adaptive" leaves room for the arrows
    MenuView v = s.e.view();                                       // no v3 value is this long: a synthetic row
    snprintf(v.rows[v.cursorRow].value, sizeof(v.rows[0].value), "Motion adaptive");
    v.rev = (uint16_t)(v.rev + 0x4000);
    s.r.render(v, 0, s.now); s.d.service(s.now); s.now += 50; s.d.service(s.now);
    s.r.render(v, 0, s.now); s.d.service(s.now);
    std::string row = rowText(s, "Deinterlace");
    TEST_ASSERT_TRUE(row.find("Motion adaptive") != std::string::npos);
    TEST_ASSERT_EQUAL_INT(0, glyphCount(s, row0, UDC_CHEV_L));
}

void test_slider_cells_shrink_for_long_labels(void) {
    Scene s;
    s.key(K_MENU); s.enter("Picture"); s.select("Scanline strength");
    std::string row = rowText(s, "Scanline strength");
    int cells = 0;
    for (size_t i = 0; i < row.size();) {
        if (row.compare(i, 3, "⑦") == 0 || row.compare(i, 3, "⑧") == 0 || row.compare(i, 3, "⑨") == 0) cells++;
        unsigned char c = (unsigned char)row[i];
        i += c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    }
    TEST_ASSERT_EQUAL_INT(6, cells);
}

void test_render_skips_unchanged_view(void) {
    Scene s;
    s.key(K_MENU);
    s.f.log.clear();
    s.r.render(s.e.view(), s.theme, s.now);
    s.d.service(s.now);
    TEST_ASSERT_EQUAL_UINT32(0, s.f.log.size());
}

// ---- Overlays and power ----
static void infoSignal(Scene& s) {
    s.be.sig.srcLines = 262; s.be.sig.srcHzX10 = 599; s.be.sig.presetId = 0x01; s.be.sig.outHzX10 = 600;
    s.be.sig.frameLocked = true; s.be.sig.sogLevel = 5;
    s.be.texts[LT_PROFILE_ACTIVE] = "Mega Drive"; s.be.texts[LT_WIFI_SSID] = "home";
    s.be.texts[LT_WIFI_IP] = "192.168.0.99";
}

static int countEntries(const FakeStv& f, const char* e) {
    int n = 0;
    for (auto& l : f.log) if (l == e) n++;
    return n;
}

void test_golden_info_rgb_scart_240p(void) { Scene s; infoSignal(s); s.key(K_INFO); checkGolden(s, "overlay-info"); }

// The info panel shows the network name after the Wi-Fi icon and the IP address
void test_info_shows_ssid_and_ip(void) {
    Scene s; infoSignal(s); s.key(K_INFO);
    std::string sc = s.screen();
    TEST_ASSERT_TRUE(sc.find("| WiFi                  \u2463 home |") != std::string::npos);   // "④ home" on the right
    TEST_ASSERT_TRUE(sc.find("| IP              192.168.0.99 |") != std::string::npos);
    TEST_ASSERT_TRUE(sc.find("SYNC") == std::string::npos && sc.find("STRP") == std::string::npos);
}

// Whole-word labels on the left, values on the right like the menu rows; the
// lock is its icon at the end of the Output row
void test_info_rows_whole_labels_values_right(void) {
    Scene s; infoSignal(s); s.key(K_INFO);
    std::string sc = s.screen();
    TEST_ASSERT_TRUE(sc.find("| Input  RGB SCART 240p 59.9Hz |") != std::string::npos);
    TEST_ASSERT_TRUE(sc.find("| Output     1280x960 60.0Hz \u2462 |") != std::string::npos);   // "③" lock
    TEST_ASSERT_TRUE(sc.find("| Profile           Mega Drive |") != std::string::npos);
}

// The lock icon belongs to the Output row only: " LOCK" in a profile name is text
void test_info_lock_only_on_the_output_row(void) {
    Scene s; infoSignal(s);
    s.be.texts[LT_PROFILE_ACTIVE] = "ZELDA LOCK ON";
    s.key(K_INFO);
    std::string sc = s.screen();
    TEST_ASSERT_TRUE_MESSAGE(sc.find("| Profile        ZELDA LOCK ON |") != std::string::npos, sc.c_str());
}

// A value longer than the room beside its label ends with a dot (24-letter profile, 32-letter SSID)
void test_info_long_values_end_with_a_dot(void) {
    Scene s; infoSignal(s);
    s.be.texts[LT_PROFILE_ACTIVE] = "Super Nintendo Mini 240p";
    s.be.texts[LT_WIFI_SSID] = "Vodafone-Casa-Rossi-5GHz-Extende";
    s.key(K_INFO);
    std::string sc = s.screen();
    TEST_ASSERT_TRUE(sc.find("| Profile Super Nintendo Mini. |") != std::string::npos);
    TEST_ASSERT_TRUE(sc.find("| WiFi \u2463 Vodafone-Casa-Rossi-. |") != std::string::npos);
}

void test_golden_volume_pill(void) { Scene s; s.key(K_VOLUP); checkGolden(s, "overlay-volume"); }

void test_golden_adjust_strip_1080p(void) {
    Scene s;
    s.be.lives[LV_POSITION] = (12L << 16) | 4;
    s.key(K_MENU); s.enter("Screen"); s.select("Position"); s.key(K_OK);
    checkGolden(s, "overlay-adjust-1080p");
}

void test_adjust_strip_1080p_spacing_split(void) {
    Scene s;
    s.key(K_MENU); s.enter("Screen"); s.select("Position"); s.key(K_OK);
    TEST_ASSERT_TRUE(s.screen().find("S 255x3\nS 130\nR 1D2") != std::string::npos);   // 1080*94% - 3*40 = 895 lines
}

void test_toast_over_the_menu_uses_the_message_role(void) {
    Scene s;
    s.key(K_MENU); s.enter("System");
    s.be.apply(SID_sys_menu_mode, 0); s.be.settle();     // Advanced -> Easy inside a page: "Advanced changes stay active"
    s.wait(50);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, s.e.view().overlay);
    TEST_ASSERT_EQUAL_HEX8(osdAttr(0, R_MSG), s.d.shadow().attr(8, 1));
}

void test_golden_toast_alone(void) {
    Scene s;
    s.key(K_MENU); s.enter("Profiles"); s.enter("New profile");
    s.key(K_OK);                                        // menu-issued profile operation (Signal name)
    s.key(K_MENU);                                      // menu closed before the result
    s.be.resultSeq++;
    s.now += 300; s.e.tick(s.now); s.draw();            // result toast with no menu
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, s.e.view().overlay);
    TEST_ASSERT_FALSE(s.e.view().overMenu);
    checkGolden(s, "overlay-toast");
}

// Boot prompt: a confirmation with the menu closed still shows its Yes/No choice
void test_adv_controller_prompt_shows_its_choice(void) {
    Scene s;
    s.be.sig.advControllerUpdateAvailable = true;
    TEST_ASSERT_TRUE(s.e.promptAdvController(s.now));
    s.draw();
    TEST_ASSERT_FALSE(s.e.view().overMenu);
    std::string scr = s.screen();
    size_t q = scr.find("Update the ADV Controller"), yes = scr.find("Yes"), no = scr.find("No", yes);
    TEST_ASSERT_TRUE(q != std::string::npos && yes != std::string::npos && no != std::string::npos);
    TEST_ASSERT_TRUE(scr.find("\n", q) < yes);                  // the choice is on the row below the question
}

// Over a menu the question takes the last list row, the choice the help row
static std::string shadowRow(Scene& s, uint8_t row) {
    std::string r;
    for (uint8_t c = 1; c <= 28; c++) { uint8_t k = s.d.shadow().code(row, c); r += k == 0x9A ? ' ' : (char)k; }
    return r;
}

void test_confirm_over_a_menu_uses_two_rows(void) {
    Scene s;
    s.key(K_MENU); s.enter("System"); s.select("Restart"); s.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, s.e.view().overlay);
    TEST_ASSERT_TRUE(shadowRow(s, 7).find("Restart the GBSC PRO") != std::string::npos);
    TEST_ASSERT_EQUAL_HEX8(s.d.glyph(UDC_QUESTION), s.d.shadow().code(7, 21));   // '?' is a UDC (not in the ROM)
    std::string choice = shadowRow(s, 8);
    TEST_ASSERT_TRUE(choice.find("Yes") != std::string::npos && choice.find("No") != std::string::npos);
    TEST_ASSERT_TRUE(choice.find("Restart") == std::string::npos);
}

// Dynamic glyph slots: no screen needs more glyphs than there are slots (a miss would draw a space)
void test_no_screen_runs_out_of_glyph_slots(void) {
    Scene s;
    s.key(K_VOLUP); s.wait(4000);
    s.key(K_MUTE); s.key(K_MUTE); s.wait(4000);
    s.key(K_INFO); s.key(K_BACK);
    s.key(K_MENU); s.enter("Picture"); s.key(K_MENU);
    s.be.sig.updatePct = 30; s.wait(10); s.be.sig.updatePct = -1; s.wait(10);
    TEST_ASSERT_EQUAL_UINT16(0, s.d.glyphMisses());
}

// Scrollbar: column 29 of rows 1..7, thumb proportional to the window
static char barCell(Scene& s, uint8_t row) {
    uint8_t c = s.d.shadow().code(row, 29);
    if (c == s.d.glyph(UDC_BAR_THUMB)) return 'T';
    if (c == s.d.glyph(UDC_BAR_TRACK)) return '|';
    return c == OSD_BLANK ? ' ' : '?';
}
static std::string bar(Scene& s) {
    std::string b;
    for (uint8_t r = 1; r <= 7; r++) b += barCell(s, r);
    return b;
}

void test_scrollbar_top_middle_bottom(void) {
    Scene s;
    s.key(K_MENU); s.enter("Picture");
    const MenuView& v = s.e.view();
    TEST_ASSERT_TRUE(v.listTotal > 7);
    TEST_ASSERT_EQUAL_UINT8(0, v.listTop);
    std::string top = bar(s);
    TEST_ASSERT_EQUAL_CHAR('T', top[0]);                        // the thumb touches the top only at the top
    TEST_ASSERT_EQUAL_CHAR('|', top[6]);
    for (int i = 0; i < 12 && s.e.view().listTop < 2; i++) s.key(K_DOWN);
    TEST_ASSERT_TRUE(s.e.view().listTop > 0);
    std::string mid = bar(s);
    TEST_ASSERT_EQUAL_CHAR('|', mid[0]);                        // scrolled: track above the thumb
    TEST_ASSERT_TRUE(mid.find('T') != std::string::npos);
    Scene b;
    b.key(K_MENU); b.enter("Picture"); b.key(K_UP);             // wraps to the last row
    std::string bottom = bar(b);
    TEST_ASSERT_EQUAL_CHAR('|', bottom[0]);
    TEST_ASSERT_EQUAL_CHAR('T', bottom[6]);
    size_t len = top.find_last_of('T') + 1;
    TEST_ASSERT_EQUAL_UINT((unsigned)((7 * 7 * 2 + v.listTotal) / (2 * v.listTotal)), (unsigned)len);   // round(49 / total)
}

void test_no_scrollbar_when_the_list_fits(void) {
    Scene s;
    s.key(K_MENU); s.enter("System"); s.enter("WiFi");
    TEST_ASSERT_TRUE(s.e.view().listTotal <= 7);
    TEST_ASSERT_EQUAL_STRING("       ", bar(s).c_str());
}

void test_scrollbar_colours_follow_the_row(void) {
    Scene s;
    s.key(K_MENU); s.enter("Picture");
    uint8_t cur = s.e.view().cursorRow + 1;
    uint8_t sel = osdAttr(0, R_C), nor = osdAttr(0, R_N);
    TEST_ASSERT_EQUAL_HEX8(sel, s.d.shadow().attr(cur, 29));    // inside the selection band
    for (uint8_t r = 1; r <= 7; r++) {
        if (r == cur) continue;
        uint8_t at = s.d.shadow().attr(r, 29);
        TEST_ASSERT_EQUAL_HEX8(nor & 0x07, at & 0x07);           // normal text colour on any row background
        TEST_ASSERT_EQUAL_HEX8(s.d.shadow().attr(r, 1) & 0xF0, at & 0xF0);
    }
}

// A value that differs from its default ("On" of Developer mode) stayed yellow on a row that is
// not selected and looked like a stuck cursor: values take the row's colour, yellow marks only the cursor
void test_changed_value_keeps_the_row_colour(void) {
    Scene s;
    s.be.apply(SID_pic_saturation, 70); s.be.settle();
    s.key(K_MENU); s.enter("Picture");
    std::string out = s.screen();
    uint8_t satRow = 0;
    for (uint8_t r = 1; r <= 7; r++)
        if (s.d.shadow().code(r, 1) == 'S' && s.d.shadow().code(r, 2) == 'a') satRow = r;
    TEST_ASSERT_TRUE(satRow > 0);
    TEST_ASSERT_EQUAL_HEX8(osdAttr(0, R_N), s.d.shadow().attr(satRow, 27));   // the "7" of 70
}

// Nine positions (osd.position / osd.popup_position): vertical from the strips, horizontal from DD and the pill
static unsigned leadingSpacing(const std::string& screen);
static int ddOf(Scene& s) { return s.f.reg[1]; }

void test_menu_positions_move_the_panel(void) {
    Scene c; c.key(K_MENU);
    unsigned centre = leadingSpacing(c.screen());
    int ddc = ddOf(c);
    Scene tl; tl.r.setPlacement(0, 7); tl.key(K_MENU);
    Scene br; br.r.setPlacement(8, 7); br.key(K_MENU);
    TEST_ASSERT_TRUE(leadingSpacing(tl.screen()) < centre);
    TEST_ASSERT_TRUE(leadingSpacing(br.screen()) > centre);
    TEST_ASSERT_TRUE(ddOf(tl) <= ddc && ddOf(br) >= ddc);          // no window in the fake: same legacy DD
}

void test_menu_redraws_when_only_the_column_changes(void) {
    Scene s(OUT_960P60); s.key(K_MENU);
    s.d.setWindow({2704, 160, 612, 2594, true});                    // ntsc_240p.h (960p)
    s.r.setPlacement(3, 7); s.draw();                               // Center Left
    int left = ddOf(s);
    s.r.setPlacement(5, 7); s.draw();                               // Center Right
    char msg[64]; snprintf(msg, sizeof(msg), "left %d right %d ld %u", left, ddOf(s), s.d.timing().ld);
    TEST_ASSERT_TRUE_MESSAGE(ddOf(s) > left, msg);
}

void test_pill_hugs_its_side(void) {
    Scene l; l.r.setPlacement(4, 6); l.key(K_VOLUP);               // Bottom Left
    TEST_ASSERT_EQUAL_HEX8(l.d.glyph(UDC_SPEAKER), l.d.shadow().code(0, 2));
    Scene r; r.r.setPlacement(4, 8); r.key(K_MUTE);                // Bottom Right
    TEST_ASSERT_TRUE((r.d.shadow().attr(0, 28) & 0x80) == 0);       // the pill's last padding cell is opaque
    TEST_ASSERT_TRUE((r.d.shadow().attr(0, 29) & 0x80) != 0);
}

// A toast with the menu closed ("Loaded MD NTSC-U" from auto-load) showed in the middle of the
// screen while the volume pill and the status pop-up sit at the pop-up position: it follows osd.popup_position too
void test_toast_alone_uses_the_popup_position(void) {
    Scene low; low.r.setPlacement(4, 7); low.e.message("Loaded MD PAL", 1000); low.wait(50);   // Bottom Center
    Scene high; high.r.setPlacement(4, 1); high.e.message("Loaded MD PAL", 1000); high.wait(50);   // Top Center
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, low.e.view().overlay);
    TEST_ASSERT_FALSE(low.e.view().overMenu);
    TEST_ASSERT_TRUE(leadingSpacing(low.screen()) > leadingSpacing(high.screen()));
    Scene moved; moved.r.setPlacement(0, 7); moved.e.message("Loaded MD PAL", 1000); moved.wait(50);   // menu Top Left
    TEST_ASSERT_EQUAL_UINT(leadingSpacing(low.screen()), leadingSpacing(moved.screen()));
}

// HD bypass: the output follows the source, so the menu is centred on the measured active lines (720p here)
void test_bypass_centres_on_the_measured_lines(void) {
    Scene s(OUT_BYPASS);
    s.d.setOutput(OUT_BYPASS, 45000, 720);
    s.key(K_MENU);
    const StvTiming& t = s.d.timing();
    unsigned rowLines = t.chMenu * (t.zoomMenu ? 2 : 1);
    TEST_ASSERT_UINT_WITHIN(2, 720 / 2 - 9 * rowLines / 2, leadingSpacing(s.screen()));
}

// The MUTE/ON box was wider than its word with the word left-aligned: the box is padding,
// icon, space, word, padding, centred in the panel
static int opaqueFrom(Scene& s, int& first) {
    int n = 0; first = -1;
    for (uint8_t c = 0; c < 30; c++)
        if (!(s.d.shadow().attr(0, c) & 0x80)) { if (first < 0) first = c; n++; }
    return n;
}

void test_mute_box_fits_its_word(void) {
    Scene m; m.key(K_MUTE);                                       // "MUTE"
    int first, n = opaqueFrom(m, first);
    TEST_ASSERT_EQUAL_INT(4 + 4, n);
    TEST_ASSERT_EQUAL_INT(11, first);                              // centred: 11..18
    Scene o; o.key(K_MUTE); o.key(K_MUTE);                         // "ON"
    n = opaqueFrom(o, first);
    TEST_ASSERT_EQUAL_INT(2 + 4, n);
    TEST_ASSERT_EQUAL_INT(12, first);                              // 12..17
}

// The toast alone ("Loaded MD PAL") was a 30-cell bar, mostly empty: one cell each side of the
// text, centred or on the pop-up position's side like the mute box
void test_toast_box_fits_its_text(void) {
    Scene c; c.e.message("Loaded MD PAL", 1000); c.wait(50);       // 13 characters, Bottom Center
    int first, n = opaqueFrom(c, first);
    TEST_ASSERT_EQUAL_INT(13 + 2, n);
    TEST_ASSERT_EQUAL_INT(8, first);                               // 15 - 15 / 2: 8..22
    TEST_ASSERT_EQUAL_UINT8('L', c.d.shadow().code(0, 9));
    Scene l; l.r.setPlacement(4, 6); l.e.message("Done", 1000); l.wait(50);   // Bottom Left
    n = opaqueFrom(l, first);
    TEST_ASSERT_EQUAL_INT(4 + 2, n);
    TEST_ASSERT_EQUAL_INT(1, first);
    Scene r; r.r.setPlacement(4, 8); r.e.message("Done", 1000); r.wait(50);   // Bottom Right: ends at 28
    n = opaqueFrom(r, first);
    TEST_ASSERT_EQUAL_INT(23, first);
    Scene w; w.e.message("Failed: profile storage full", 1000); w.wait(50);   // 28 characters: the whole row
    n = opaqueFrom(w, first);
    TEST_ASSERT_EQUAL_INT(30, n);
}

// Descriptions longer than the help row scroll: still for 1.2 s, then one cell every 150 ms (1.5 s / 300 ms was too
// slow), and again from the start when the selection changes
static std::string helpRow(Scene& s) {
    std::string r;
    for (uint8_t c = 1; c <= 28; c++) {                          // ROM codes back to ASCII (stvCharCode)
        uint8_t k = s.d.shadow().code(8, c);
        r += k == 0x9A ? ' ' : k == 0x3E ? '-' : k == 0x3C ? '+' : k == 0x3D ? '=' : (char)k;
    }
    return r;
}

void test_long_description_scrolls_on_the_help_row(void) {
    Scene s;                                                      // v3 hints fit one row: a synthetic long help
    s.key(K_MENU); s.enter("Input");
    const std::string help = "Raise if dim - lower if whites clip - stops Auto gain";
    auto drawLong = [&](uint32_t ms) {
        s.now += ms; s.e.tick(s.now);
        MenuView v = s.e.view();
        snprintf(v.help, sizeof(v.help), "%s", help.c_str());
        v.rev = (uint16_t)(v.rev + 0x4000);                          // not the engine's revision: drawn again
        s.r.render(v, 0, s.now); s.d.service(s.now); s.now += 50; s.d.service(s.now);
        s.r.render(v, 0, s.now); s.d.service(s.now);
    };
    drawLong(0);
    TEST_ASSERT_EQUAL_STRING(help.substr(0, 28).c_str(), helpRow(s).c_str());
    drawLong(1000);
    TEST_ASSERT_EQUAL_STRING(help.substr(0, 28).c_str(), helpRow(s).c_str());   // still during the hold
    drawLong(800);                                                // 1.8 s: 0.6 s past the hold, 4 cells at 150 ms
    size_t at = help.find(helpRow(s));
    TEST_ASSERT_TRUE_MESSAGE(at != std::string::npos && at >= 3, helpRow(s).c_str());   // moved along the same text
    s.key(K_UP);                                                  // another row: from its start again
    std::string other = std::string(s.e.view().help).substr(0, 28);
    TEST_ASSERT_EQUAL_STRING(other.c_str(), helpRow(s).substr(0, other.size()).c_str());
}

// Load rows: the tag always shows, the selected long label ("<name> <region>") scrolls
void test_profile_row_keeps_the_tag_and_scrolls_the_name(void) {
    Scene s;
    ProfileEntry& e = s.be.list.at(0);
    e.used = true; strcpy(e.name, "Super Famicom Jr Turbo X"); e.region = PROFILE_REGION_NTSC_J;   // 24 chars
    s.be.active = 0;
    s.key(K_MENU); s.enter("Profiles"); s.enter("Load");
    TEST_ASSERT_TRUE(s.select("Super Famicom Jr NTSC-J"));
    std::string row = rowText(s, "Super");
    TEST_ASSERT_TRUE(row.find("Active") != std::string::npos);
    TEST_ASSERT_TRUE(row.find("Super Famicom") != std::string::npos);
    s.wait(1200 + 150 * 3);
    std::string out = s.screen();
    TEST_ASSERT_TRUE_MESSAGE(out.find("Famicom Jr") != std::string::npos, out.c_str());   // scrolled
    TEST_ASSERT_TRUE(out.find("Super Famicom") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("Active") != std::string::npos);
}

// In the Profiles screens the long name did not scroll: the Save changes value (active profile)
// and the profile sub-menu title scroll when they do not fit
void test_profiles_save_value_and_title_scroll(void) {
    Scene s;
    ProfileEntry& e = s.be.list.at(0);
    e.used = true; strcpy(e.name, "Super Famicom Jr Turbo X"); e.region = PROFILE_REGION_NTSC_J;
    s.be.active = 0;
    s.key(K_MENU); s.enter("Profiles");
    TEST_ASSERT_TRUE(s.select("Save changes"));
    TEST_ASSERT_EQUAL_STRING("Super Famicom Jr NTSC-J", s.e.view().rows[s.e.view().cursorRow].value);   // whole label
    std::string a = s.screen();
    s.wait(2000);                                                 // past the 1.2 s hold, inside the first pass
    TEST_ASSERT_TRUE(a != s.screen());                            // the value scrolled
    Scene m;
    ProfileEntry& f = m.be.list.at(0);
    f.used = true; strcpy(f.name, "Super Famicom Jr Turbo X");
    m.key(K_MENU); m.enter("Profiles"); m.enter("Manage"); m.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Super Famicom Jr Turbo X", m.e.view().title);
    std::string t0 = m.screen();                                  // 22 cells next to "1/3": it scrolls
    m.wait(2000);                                                 // inside the 2-step cycle (hold 1.5 s)
    TEST_ASSERT_TRUE(t0 != m.screen());
}

// Brightness: a plain slider of picture.brightness 0..100 (the centred bar is gone)
void test_brightness_plain_slider(void) {
    Scene p; p.be.apply(SID_pic_brightness, 100); p.be.settle();
    p.key(K_MENU); p.enter("Picture"); p.select("Brightness");
    uint8_t row = p.e.view().cursorRow + 1;
    TEST_ASSERT_TRUE(glyphCount(p, row, UDC_SLIDER_FULL) > 0);
    TEST_ASSERT_EQUAL_INT(0, glyphCount(p, row, UDC_SLIDER_TRACK));
    Scene n; n.be.apply(SID_pic_brightness, 0); n.be.settle();
    n.key(K_MENU); n.enter("Picture"); n.select("Brightness");
    row = n.e.view().cursorRow + 1;
    TEST_ASSERT_EQUAL_INT(0, glyphCount(n, row, UDC_SLIDER_FULL));
    TEST_ASSERT_TRUE(glyphCount(n, row, UDC_SLIDER_TRACK) > 0);
}

void test_selected_disabled_row_highlighted(void) {
    for (uint8_t t = 0; t < 4; t++) {
        Scene s; s.theme = t;
        s.be.apply(SID_input_source, 2); s.be.settle();            // RGB SoG: 75 ohm sync is n/a
        s.key(K_MENU); s.enter("Input"); TEST_ASSERT_TRUE(s.select("75 ohm sync"));
        uint8_t row = s.e.view().cursorRow + 1;
        for (uint8_t c = 0; c < 29; c++) TEST_ASSERT_EQUAL_HEX8(osdAttr(t, R_DC), s.d.shadow().attr(row, c));
        s.key(K_UP);                                                 // not selected: the plain disabled colour
        TEST_ASSERT_EQUAL_HEX8(osdAttr(t, R_D), s.d.shadow().attr(row, 1));
    }
}

// The ROM arrows have a box drawn in them: menus use the UDC chevrons
static bool hasRomArrow(Scene& s) {
    for (uint8_t r = 0; r < stv::kRows; r++)
        for (uint8_t c = 0; c < stv::kRowWidth; c++) {
            uint8_t k = s.d.shadow().code(r, c);
            if (k == 0x05 || k == 0x06 || k == 0x15 || k == 0x16) return true;
        }
    return false;
}

void test_menus_use_chevrons_not_rom_arrows(void) {
    Scene s;
    s.key(K_MENU);
    TEST_ASSERT_FALSE(hasRomArrow(s));                          // submenu arrows
    s.enter("System"); s.enter("OSD settings"); s.select("Theme");
    TEST_ASSERT_FALSE(hasRomArrow(s));                          // title back arrow, cycle arrows
    TEST_ASSERT_EQUAL_HEX8(s.d.glyph(UDC_CHEV_L), s.d.shadow().code(0, 1));
}

void test_deep_list_with_bar_fits_the_glyph_slots(void) {
    Scene s;
    s.key(K_MENU); s.enter("System"); s.enter("Sound");          // volume slider and %
    s.key(K_BACK); s.enter("Picture");                          // sliders, bar, back chevron
    s.select("Deinterlace");                                    // cycle chevrons
    TEST_ASSERT_EQUAL_UINT16(0, s.d.glyphMisses());
}

// The update overlay sits in the middle of the picture, not at the menu's top
static unsigned leadingSpacing(const std::string& screen) {
    unsigned sum = 0;
    size_t pos = screen.find('\n') + 1;                          // skip the header line
    while (pos < screen.size() && screen.compare(pos, 2, "S ") == 0) {
        unsigned n = 0, k = 1;
        sscanf(screen.c_str() + pos, "S %u", &n);
        const char* x = strchr(screen.c_str() + pos, 'x');
        const char* eol = strchr(screen.c_str() + pos, '\n');
        if (x && eol && x < eol) sscanf(x + 1, "%u", &k);
        sum += n * k;
        pos = eol ? (size_t)(eol - screen.c_str()) + 1 : screen.size();
    }
    return sum;
}

void test_update_overlay_is_centred(void) {
    for (StvOutStd out : {OUT_1080P60, OUT_480P}) {
        Scene s(out);
        s.be.sig.updatePct = 42; s.wait(10);
        const StvTiming& t = s.d.timing();
        unsigned rowLines = t.chMenu * (t.zoomMenu ? 2 : 1);
        unsigned want = stvActiveLines(out) / 2 - rowLines;   // two rows, centred
        unsigned top = leadingSpacing(s.screen());
        TEST_ASSERT_UINT_WITHIN(2, want, top);
    }
}

// The OSD is centred on the picture; only the volume/mute pill sits low, centred horizontally
void test_menu_list_is_centred(void) {
    for (StvOutStd out : {OUT_1080P60, OUT_480P}) {
        Scene s(out);
        s.key(K_MENU);
        const StvTiming& t = s.d.timing();
        unsigned rowLines = t.chMenu * (t.zoomMenu ? 2 : 1);
        unsigned total = 9 * rowLines;                          // title, 7 rows, help
        TEST_ASSERT_UINT_WITHIN(2, stvActiveLines(out) / 2 - total / 2, leadingSpacing(s.screen()));
    }
}

// No transparent lines between the title, the list and the help row (the picture showed through)
void test_menu_list_has_no_gaps(void) {
    Scene s;
    s.key(K_MENU);
    std::string scr = s.screen();
    size_t first = scr.find("\nR "), last = scr.rfind("\nR ");
    TEST_ASSERT_TRUE(first != std::string::npos && last > first);
    TEST_ASSERT_TRUE(scr.substr(first, last - first).find("\nS ") == std::string::npos);
}

void test_volume_pill_is_low_and_centred(void) {
    Scene s;
    s.key(K_VOLUP);
    const StvTiming& t = s.d.timing();
    unsigned rowLines = t.chMenu * (t.zoomMenu ? 2 : 1);
    TEST_ASSERT_UINT_WITHIN(2, stvActiveLines(s.d.output()) * 94 / 100 - rowLines, leadingSpacing(s.screen()));
    std::string sc = s.screen();
    size_t r = sc.find("R ");
    std::string row = sc.substr(sc.find('|', r) + 1, 200);
    size_t speaker = row.find("\u24ea");                          // ⓪ = speaker glyph
    TEST_ASSERT_TRUE(speaker != std::string::npos);
    TEST_ASSERT_EQUAL_UINT(9, speaker);                            // 12-cell pill centred in 30 columns
    // one coloured padding cell each side: cells 8 and 21 carry the pill attribute
    size_t a = sc.find('\n', sc.find('|', r)) + 1;
    std::string attrs = sc.substr(sc.find('|', a) + 1, 30);
    TEST_ASSERT_TRUE(attrs[8] == attrs[9] && attrs[21] == attrs[9]);
    TEST_ASSERT_TRUE(attrs[7] != attrs[9] && attrs[22] != attrs[9]);
}

void test_mute_pill_is_padded(void) {
    Scene s;
    s.key(K_MUTE);
    std::string sc = s.screen();
    size_t r = sc.find("R ");
    size_t a = sc.find('\n', sc.find('|', r)) + 1;
    std::string attrs = sc.substr(sc.find('|', a) + 1, 30);
    TEST_ASSERT_TRUE(attrs[11] == attrs[12] && attrs[18] == attrs[12]);   // icon at 12, MUTE at 14..17
    TEST_ASSERT_TRUE(attrs[10] != attrs[12] && attrs[19] != attrs[12]);
}

void test_golden_update(void) {
    Scene s;
    s.be.sig.updatePct = 42;
    s.now += 300; s.e.tick(s.now); s.draw();
    checkGolden(s, "overlay-update");
}

void test_layout_switch_clears_rows(void) {
    Scene s;
    s.key(K_MENU);
    s.f.log.clear();
    s.key(K_INFO);
    TEST_ASSERT_EQUAL_INT(1, countEntries(s.f, "W 000+38"));
    TEST_ASSERT_TRUE(s.screen().find("Signal info") != std::string::npos);
    s.key(K_BACK);
    TEST_ASSERT_EQUAL_INT(2, countEntries(s.f, "W 000+38"));
    std::string out = s.screen();
    TEST_ASSERT_TRUE(out.find("Signal info") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("GBSC PRO") != std::string::npos);
}

void test_power_follows_view(void) {
    Scene s;
    s.key(K_MENU);
    TEST_ASSERT_EQUAL_HEX8(0x80, s.f.reg[3]);
    s.key(K_MENU);
    TEST_ASSERT_EQUAL_HEX8(0x00, s.f.reg[3]);
    s.key(K_MENU);
    TEST_ASSERT_EQUAL_HEX8(0x80, s.f.reg[3]);
}

// ---- Fit checks over the whole tree (walker in test/sim/menu-fit.h) ----
static std::string mstr(uint16_t id) { return menuFitStr(id); }

struct OsdFit : MenuFitRule {
    bool fits(const std::string& l, const std::string& v, uint8_t kind) override {
        if (kind == ROWK_LIVE) return true;                 // OSD LIVE values: free text, clipped at runtime
        if (kind == ROWK_SLIDER && !v.empty()) kind = ROWK_CYCLE;
        return OsdRenderer::rowFits(l.c_str(), v.c_str(), kind);
    }
    void extra(const MenuNode& n, const std::string& l, std::vector<std::string>& out) override {
        if (n.type == N_ACTION && !n.opts) {
            bool confirm = (n.flags & NF_CONFIRM) != 0;
            if (!(n.flags & NF_FN)) { ActionDef d; actionsDefAt(n.ref, d); confirm = confirm || (d.flags & ACTION_CONFIRM); }
            if (confirm) {                                  // a real question, one row of 28 cells
                uint16_t q = MenuEngine::confirmQuestion(n);
                std::string t = q ? mstr(q) : "";
                if (t.empty() || t.size() > 28 || t.back() != '?') out.push_back("confirm: " + l);
            }
        }
        if (n.type == N_ADJUST2D && l.size() + 1 + strlen(menuFitLiveMax(n.ref2)) > 27) out.push_back("adjust title: " + l);
    }
};

void test_row_fit_rule_rejects_long_rows(void) {
    TEST_ASSERT_FALSE(OsdRenderer::rowFits("Very long label here", "PAL Cmb -N w/ p", ROWK_CYCLE));
    TEST_ASSERT_TRUE(OsdRenderer::rowFits("Video format", "PAL Cmb -N w/ p", ROWK_CYCLE));   // exactly 29 cells
    TEST_ASSERT_FALSE(OsdRenderer::rowFits("A label of twenty one", "", ROWK_SLIDER));
}

void test_every_row_fits_30_columns(void) {
    OsdFit rule;
    std::vector<std::string> gMisfits = menuFitAll(rule);
    for (uint16_t id = S_T_BUSY; id <= S_T_NO_SIGNAL; id++)
        if (mstr(id).size() > 28) gMisfits.push_back("toast: " + mstr(id));
    std::string all;
    for (auto& m : gMisfits) all += m + "\n";
    TEST_ASSERT_TRUE_MESSAGE(gMisfits.empty(), all.c_str());
}

void test_info_lines_fit(void) {
    const char* worst[] = {"IN   YPbPr 576i  99.9Hz", "SYNC CSync  1125 lines", "OUT  1920x1080  99.9Hz LOCK",
                           "STRP Off  SOG 31"};
    for (const char* l : worst) TEST_ASSERT_TRUE_MESSAGE(strlen(l) + 1 <= 28, l);   // + 1: the lock tag's glyph
}

// ---- Redraw after other writers, exclusive access ----
void test_external_draw_triggers_redraw(void) {
    Scene s;
    s.key(K_MENU);
    for (uint8_t c = 0; c < 4; c++) s.d.shadow().set(0, c, 'X', 0x17);   // another writer draws on the same chip
    s.d.touch();
    s.r.render(s.e.view(), s.theme, s.now); s.d.service(s.now);
    TEST_ASSERT_TRUE(s.screen().find("XXXX") == std::string::npos);
    TEST_ASSERT_TRUE(s.screen().find(" GBSC PRO ") != std::string::npos);
}

void test_exclusive_owner_pauses_renderer(void) {
    Scene s;
    s.key(K_MENU);
    s.d.setExclusive(true);                             // e.g. the Developer test screen owns the chip
    s.f.log.clear();
    s.key(K_DOWN);
    TEST_ASSERT_EQUAL_UINT32(0, s.f.log.size());
    s.d.setExclusive(false);
    s.draw();
    TEST_ASSERT_TRUE(s.f.log.size() > 0);
}

void test_close_clears_rows_in_always_on_mode(void) {
    Scene s;
    s.d.setAlwaysOn(true);
    s.key(K_MENU);
    s.key(K_MENU);                                      // close: the display bit stays on (A/B mode)
    TEST_ASSERT_EQUAL_HEX8(0x80, s.f.reg[3]);
    TEST_ASSERT_TRUE(s.screen().find("Input") == std::string::npos);
}

// ---- 4d: keyboard, quick slider, names list ----
void test_golden_keyboard_name(void) {
    Scene s;
    s.key(K_MENU); s.enter("Profiles"); s.enter("New profile");
    s.key(K_DOWN); s.key(K_OK);                         // Edit name...
    s.key(K_DOWN); s.key(K_RIGHT); s.key(K_RIGHT);      // selected key: P
    checkGolden(s, "kbd-name");
}

// The comma (Extra hosts keyboard) showed as a yellow '.', unreadable next to the real dot. The ROM has no comma: a
// UDC draws it, in the field and on its key, in the keys' own colour
void test_keyboard_comma_is_a_drawn_glyph(void) {
    Scene s;
    MenuView v = {};
    v.mode = VIEW_OVERLAY; v.rev = 1; v.overlay = OV_KEYBOARD; v.lineCount = 6; v.caret = 27; v.value = 0;   // key: A
    snprintf(v.lines[0], sizeof(v.lines[0]), "Extra hosts");
    snprintf(v.lines[1], sizeof(v.lines[1]), "tv.lan,gbs");
    snprintf(v.lines[2], sizeof(v.lines[2]), "ABCDEFGHIJKLM");
    snprintf(v.lines[3], sizeof(v.lines[3]), "NOPQRSTUVWXYZ");
    snprintf(v.lines[4], sizeof(v.lines[4]), "0123456789-./,");
    snprintf(v.lines[5], sizeof(v.lines[5]), "aA < > Space Del Done");
    s.r.render(v, 0, s.now); s.d.service(s.now); s.now += 50; s.d.service(s.now); s.r.render(v, 0, s.now); s.d.service(s.now);
    uint8_t comma = s.d.glyph(UDC_COMMA);
    TEST_ASSERT_EQUAL_HEX8(comma, s.d.shadow().code(1, 7));            // "tv.lan," in the field
    TEST_ASSERT_EQUAL_HEX8(comma, s.d.shadow().code(5, 27));           // the 14th digit-row key (column 1 + 2 x 13)
    TEST_ASSERT_EQUAL_HEX8(s.d.shadow().attr(5, 25), s.d.shadow().attr(5, 27));   // same colour as '/'
}

// Volume in percent: the STV9426 ROM has no '%', a UDC glyph (shown as ⑩ by the simulator) draws it
void test_volume_percent_uses_the_percent_glyph(void) {
    Scene s;
    s.key(K_MENU); s.enter("System"); s.enter("Sound");
    TEST_ASSERT_TRUE(s.screen().find("80\u2469") != std::string::npos);   // "80⑩"
    s.key(K_MENU);
    s.key(K_VOLUP);
    TEST_ASSERT_TRUE(s.screen().find("82\u2469") != std::string::npos);   // the volume pill
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_classic_matches_v2_colors);
    RUN_TEST(test_volume_percent_uses_the_percent_glyph);
    RUN_TEST(test_gbsc_theme_black_and_yellow);
    RUN_TEST(test_message_role_per_theme);
    RUN_TEST(test_toast_over_the_menu_uses_the_message_role);
    RUN_TEST(test_help_row_differs_from_menu_rows);
    RUN_TEST(test_theme_roles_visible);
    RUN_TEST(test_out_of_range_theme_falls_back_to_the_default);
    RUN_TEST(test_disabled_cursor_role_per_theme);
    RUN_TEST(test_selected_disabled_row_highlighted);
    RUN_TEST(test_golden_root);
    RUN_TEST(test_golden_root_classic);
    RUN_TEST(test_golden_picture_top);
    RUN_TEST(test_golden_confirm_yes_selected);
    RUN_TEST(test_confirm_over_a_menu_uses_two_rows);
    RUN_TEST(test_golden_quick_page);
    RUN_TEST(test_golden_status_popup);
    RUN_TEST(test_selected_enum_arrows_dropped_when_too_long);
    RUN_TEST(test_slider_cells_shrink_for_long_labels);
    RUN_TEST(test_render_skips_unchanged_view);
    RUN_TEST(test_golden_info_rgb_scart_240p);
    RUN_TEST(test_info_shows_ssid_and_ip);
    RUN_TEST(test_info_rows_whole_labels_values_right);
    RUN_TEST(test_info_lock_only_on_the_output_row);
    RUN_TEST(test_info_long_values_end_with_a_dot);
    RUN_TEST(test_golden_volume_pill);
    RUN_TEST(test_golden_adjust_strip_1080p);
    RUN_TEST(test_adjust_strip_1080p_spacing_split);
    RUN_TEST(test_golden_toast_alone);
    RUN_TEST(test_golden_update);
    RUN_TEST(test_update_overlay_is_centred);
    RUN_TEST(test_menu_list_is_centred);
    RUN_TEST(test_menu_list_has_no_gaps);
    RUN_TEST(test_volume_pill_is_low_and_centred);
    RUN_TEST(test_mute_pill_is_padded);
    RUN_TEST(test_layout_switch_clears_rows);
    RUN_TEST(test_power_follows_view);
    RUN_TEST(test_row_fit_rule_rejects_long_rows);
    RUN_TEST(test_every_row_fits_30_columns);
    RUN_TEST(test_info_lines_fit);
    RUN_TEST(test_external_draw_triggers_redraw);
    RUN_TEST(test_exclusive_owner_pauses_renderer);
    RUN_TEST(test_close_clears_rows_in_always_on_mode);
    RUN_TEST(test_golden_keyboard_name);
    RUN_TEST(test_keyboard_comma_is_a_drawn_glyph);
    RUN_TEST(test_adv_controller_prompt_shows_its_choice);
    RUN_TEST(test_no_screen_runs_out_of_glyph_slots);
    RUN_TEST(test_scrollbar_top_middle_bottom);
    RUN_TEST(test_brightness_plain_slider);
    RUN_TEST(test_profiles_save_value_and_title_scroll);
    RUN_TEST(test_long_description_scrolls_on_the_help_row);
    RUN_TEST(test_profile_row_keeps_the_tag_and_scrolls_the_name);
    RUN_TEST(test_mute_box_fits_its_word);
    RUN_TEST(test_toast_box_fits_its_text);
    RUN_TEST(test_bypass_centres_on_the_measured_lines);
    RUN_TEST(test_menu_positions_move_the_panel);
    RUN_TEST(test_menu_redraws_when_only_the_column_changes);
    RUN_TEST(test_pill_hugs_its_side);
    RUN_TEST(test_toast_alone_uses_the_popup_position);
    RUN_TEST(test_changed_value_keeps_the_row_colour);
    RUN_TEST(test_no_scrollbar_when_the_list_fits);
    RUN_TEST(test_scrollbar_colours_follow_the_row);
    RUN_TEST(test_menus_use_chevrons_not_rom_arrows);
    RUN_TEST(test_deep_list_with_bar_fits_the_glyph_slots);
    return UNITY_END();
}
