#include <unity.h>
#include <string.h>
#include "menu-fake.h"
#include "settings-registry.h"
#include "api-actions.h"
#include "menu-strings.h"
#include "menu-backend.h"
#include "menu-model.h"
#include "menu-tree.h"
#include "menu-find.h"
#include "menu-signal.h"
#include "menu-signal.h"
#include "platform-caps.h"
#include <string>

void setUp(void) {}
void tearDown(void) { platformSetCaps(CAPS_DEFAULT); }

void test_strings_copy_and_terminate(void) {
    char buf[4];
    menuStrCopy(S_INPUT, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Inp", buf);
    TEST_ASSERT_EQUAL_UINT8(5, menuStrLen(S_INPUT));
    menuStrCopy(S_NONE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_fake_apply_is_pending_until_settle(void) {
    FakeBackend be;
    SettingDef d; settingsDefAt(SID_output_preset, d);
    TEST_ASSERT_TRUE(be.apply(SID_output_preset, 5));
    TEST_ASSERT_EQUAL_UINT16(5, be.value(SID_output_preset));
    TEST_ASSERT_FALSE(be.settled(SID_output_preset));
    TEST_ASSERT_EQUAL_UINT16(0, settingsGet(be.u, d));
    be.settle();
    TEST_ASSERT_TRUE(be.settled(SID_output_preset));
    TEST_ASSERT_EQUAL_UINT16(5, settingsGet(be.u, d));
    TEST_ASSERT_EQUAL_STRING("apply output.preset=5", be.log.back().c_str());
}

void test_fake_queue_full(void) {
    FakeBackend be;
    be.queueFull = true;
    TEST_ASSERT_FALSE(be.apply(SID_output_preset, 5));
    TEST_ASSERT_FALSE(be.run(AID_sys_restart));
    TEST_ASSERT_EQUAL_UINT32(0, be.log.size());
}

void test_fake_apply_many_is_all_or_nothing(void) {
    FakeBackend be;
    MenuWrite w[2] = {{SID_adv_i2p, 1}, {SID_adv_smooth, 1}};
    TEST_ASSERT_TRUE(be.applyMany(w, 2));
    TEST_ASSERT_EQUAL_UINT32(2, be.log.size());
    TEST_ASSERT_EQUAL_STRING("apply adv.smooth=1", be.log[1].c_str());
}

static std::string fmt(const MenuNode& n, MenuBackend& be) { char b[32]; menuFormatValue(n, be, b, sizeof(b)); return b; }

void test_visibility_follows_the_input_setting(void) {
    FakeBackend be;
    MenuNode sv = menuFind("Input/Video standard", SID_adv_format_sv), av = menuFind("Input/Video standard", SID_adv_format_av);
    MenuNode dec = menuFind("Input/Decoder");
    be.u.activeInputType = 1;
    TEST_ASSERT_FALSE(menuVisible(sv, be)); TEST_ASSERT_FALSE(menuVisible(av, be)); TEST_ASSERT_FALSE(menuVisible(dec, be));
    be.apply(SID_input_source, 5);                       // pending write already counts
    TEST_ASSERT_TRUE(menuVisible(sv, be)); TEST_ASSERT_FALSE(menuVisible(av, be)); TEST_ASSERT_TRUE(menuVisible(dec, be));
}

// With Source = Auto the pages follow the input the hardware is on, not the choice
void test_visibility_follows_the_active_input_in_auto(void) {
    FakeBackend be;
    MenuNode sv = menuFind("Input/Video standard", SID_adv_format_sv), dec = menuFind("Input/Decoder");
    TEST_ASSERT_EQUAL_UINT16(0, be.value(SID_input_source));
    be.activeIn = 5;                                     // Auto found S-Video
    TEST_ASSERT_TRUE(menuVisible(sv, be)); TEST_ASSERT_TRUE(menuVisible(dec, be));
    TEST_ASSERT_EQUAL_UINT8(5, menuInput(be));
}

void test_source_value_shows_auto_and_the_found_input(void) {
    FakeBackend be;
    MenuNode src = menuFind("Input/Source");
    be.activeIn = 4;
    TEST_ASSERT_EQUAL_STRING("Auto: Component", fmt(src, be).c_str());
    be.sig.searching = true;
    TEST_ASSERT_EQUAL_STRING("Auto", fmt(src, be).c_str());
    be.sig.searching = false; be.sig.noSignal = true;
    TEST_ASSERT_EQUAL_STRING("Auto", fmt(src, be).c_str());
    be.sig.noSignal = false;
    be.apply(SID_input_source, 6); be.settle();
    TEST_ASSERT_EQUAL_STRING("Composite", fmt(src, be).c_str());         // a manual choice reads as before
}

void test_ntsc_pal_comb_pair(void) {
    FakeBackend be; be.apply(SID_input_source, 6); be.settle();
    MenuNode n = menuFind("Input/Decoder/Comb split", SID_adv_comb_ntsc), p = menuFind("Input/Decoder/Comb split", SID_adv_comb_pal);
    be.sig.advNtsc = true;
    TEST_ASSERT_TRUE(menuVisible(n, be)); TEST_ASSERT_FALSE(menuVisible(p, be));
    be.sig.advNtsc = false;
    TEST_ASSERT_FALSE(menuVisible(n, be)); TEST_ASSERT_TRUE(menuVisible(p, be));
}

// The sync stripper only acts on RGBs and RGsB (advSwitchesFor): hidden on the other inputs
void test_sync_stripper_only_on_rgbs_and_rgsb(void) {
    FakeBackend be;
    MenuNode n = menuFind("Input/Sync cleanup");
    for (uint8_t in = 1; in <= 6; in++) {
        be.apply(SID_input_source, in); be.settle();
        TEST_ASSERT_EQUAL_MESSAGE(in <= 2, menuVisible(n, be), "input");
    }
}

void test_disabled_with_reason(void) {
    FakeBackend be; be.apply(SID_input_source, 6); be.settle();
    uint16_t reason = S_NONE;
    TEST_ASSERT_FALSE(menuEnabled(menuFind("Input/75 ohm sync"), be, &reason));
    TEST_ASSERT_EQUAL_UINT16(S_R_RGBS, reason);
    TEST_ASSERT_TRUE(menuEnabled(menuFind("Input/Source"), be, &reason));
}

// Bandwidth names per standard (UG-637 Tables 64/68: NTSC 01 and 10 are both Medium), no "None"
// chroma taps (the guide: do not use), in the registry so the web app and the API are checked too; no 6-tap setting
static std::string enumList(int sid) {
    SettingDef d; settingsDefAt((uint8_t)sid, d);
    std::string out; char name[32]; uint16_t v;
    for (uint8_t i = 0; settingsEnumAt(d, i, name, sizeof(name), v); i++) out += (out.empty() ? "" : "|") + std::string(name) + "=" + std::to_string(v);
    return out;
}
// Issue #46: "Auto 7.5 IRE" right after Auto, value 12 so stored indexes keep their meaning
void test_output_resolution_has_1200p(void) {   // issue #35
    SettingDef d; settingsDefAt(SID_output_preset, d);
    TEST_ASSERT_TRUE(enumList(SID_output_preset).find("1200p=7") != std::string::npos);
}

void test_video_format_auto_with_setup(void) {
    std::string l = enumList(SID_adv_format_sv);
    TEST_ASSERT_EQUAL_STRING("Auto=0|Auto 7.5 IRE=12|PAL=1|NTSC 7.5 IRE=2|PAL-60=3|NTSC 4.43=4|NTSC-J=5|PAL-N 7.5 IRE=6|"
                             "PAL-M 0 IRE=7|PAL-M 7.5 IRE=8|PAL-Nc=9|PAL-Nc 7.5IRE=10|SECAM=11", l.c_str());
    TEST_ASSERT_TRUE(l.find("SECAM=11") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING(l.c_str(), enumList(SID_adv_format_av).c_str());
    TEST_ASSERT_TRUE(menuAdvIsNtsc(12, 0x00));                       // autodetect: the status decides
    TEST_ASSERT_FALSE(menuAdvIsNtsc(12, 0xC0));
}

void test_decoder_option_lists_follow_the_guide(void) {
    TEST_ASSERT_EQUAL_STRING("Narrow=0|Medium=1|Wide=3", enumList(SID_adv_comb_ntsc).c_str());
    TEST_ASSERT_EQUAL_STRING("Narrow=0|Medium=1|Wide=2|Widest=3", enumList(SID_adv_comb_pal).c_str());
    TEST_ASSERT_EQUAL_STRING("3-2 lines=1|5-3 lines=2|5-4 lines=3", enumList(SID_adv_ct_ntsc).c_str());
    TEST_ASSERT_EQUAL_STRING("5-2 lines=1|5-3 lines=2|5-4 lines=3", enumList(SID_adv_ct_pal).c_str());
    TEST_ASSERT_EQUAL_STRING("Auto=1|Full=0|Limited=3|Limited SD=2", enumList(SID_output_hdmi).c_str());
    TEST_ASSERT_EQUAL_STRING("Auto=0|RGB SCART=1|RGB SoG=2|VGA=3|Component=4|S-Video=5|Composite=6", enumList(SID_input_source).c_str());
    SettingDef d; settingsDefAt(SID_adv_comb_ntsc, d);
    TEST_ASSERT_FALSE(settingsValid(d, 2));                         // a web or old-profile value outside the list
    settingsDefAt(SID_adv_ct_pal, d);
    TEST_ASSERT_FALSE(settingsValid(d, 0));
    TEST_ASSERT_TRUE(settingsFind("picture.tap6") < 0);
}

void test_every_enable_rule_has_a_reason(void) {
    FakeBackend be;
    be.apply(SID_input_source, 6); be.apply(SID_adv_filter_wy_ovr, 0); be.apply(SID_adv_ace, 0); be.settle();
    be.sig.scanlinesAllowed = false;
    const MenuNode nodes[] = {menuFind("Input/75 ohm sync"),
                              menuFind("Input/Decoder/Adaptive contrast/Contrast boost"), menuFind("Picture/Scanlines")};
    for (const MenuNode& n : nodes) {
        uint16_t reason = S_NONE;
        TEST_ASSERT_FALSE(menuEnabled(n, be, &reason));
        TEST_ASSERT_NOT_EQUAL(S_NONE, reason);
    }
    be.apply(SID_input_source, 5); be.settle();
    uint16_t reason = S_NONE;
    TEST_ASSERT_FALSE(menuEnabled(menuFind("Input/Decoder/Luma filter", SID_adv_filter_wy), be, &reason));
    TEST_ASSERT_EQUAL_UINT16(S_R_WY_MANUAL, reason);
}

void test_caps_and_devmode_hide_nodes(void) {
    FakeBackend be;
    TEST_ASSERT_FALSE(menuVisible(menuFind("Developer"), be));
    be.dev = true;
    TEST_ASSERT_TRUE(menuVisible(menuFind("Developer"), be));
    platformSetCaps(CAP_OLED | CAP_ENCODER);
    TEST_ASSERT_FALSE(menuVisible(menuFind("Input/75 ohm sync"), be));
    TEST_ASSERT_FALSE(menuVisible(menuFind("System/Sound"), be));
}

void test_streaming_option_parser_beyond_160_chars(void) {
    char name[24]; uint16_t v = 0;
    TEST_ASSERT_TRUE(menuOptAt(menuOptList(OL_FILTER_Y), 30, name, sizeof(name), v));
    TEST_ASSERT_EQUAL_STRING("NTSC-WN3", name);
    TEST_ASSERT_EQUAL_UINT16(30, v);
    TEST_ASSERT_FALSE(menuOptAt(menuOptList(OL_FILTER_Y), 31, name, sizeof(name), v));
}

void test_value_formats(void) {
    FakeBackend be;
    be.apply(SID_output_preset, 5); be.apply(SID_adv_i2p, 1); be.apply(SID_adv_smooth, 1); be.settle();
    TEST_ASSERT_EQUAL_STRING("1080p", fmt(menuFind("Output/Resolution"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("On", fmt(menuFind("Output/External clock"), be).c_str());   // registry inversion
    TEST_ASSERT_EQUAL_STRING("On", fmt(menuFind("Input/2X"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("Adaptive", fmt(menuFind("Picture/Deinterlace"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("50", fmt(menuFind("Picture/Saturation"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("Auto", fmt(menuFind("Output/HDMI range"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("Method 1", fmt(menuFind("Output/Lock method"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("Auto: RGB SCART", fmt(menuFind("Input/Source"), be).c_str());   // Auto + the found input
    TEST_ASSERT_EQUAL_STRING("Center", fmt(menuFind("System/OSD settings/Menu position"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("15s", fmt(menuFind("System/OSD settings/Auto close"), be).c_str());
    be.dev = true;
    be.lives[LV_SDRAM] = 0;
    TEST_ASSERT_EQUAL_STRING("108MHz", fmt(menuFind("Developer/Clock / ADC/SDRAM clock"), be).c_str());
    be.lives[LV_SDRAM] = 2;
    TEST_ASSERT_EQUAL_STRING("FB", fmt(menuFind("Developer/Clock / ADC/SDRAM clock"), be).c_str());
    be.lives[LV_OUT_RATE] = 599;
    TEST_ASSERT_EQUAL_STRING("60Hz", fmt(menuFindChild("Developer/Clock / ADC", "Snap 60/50Hz"), be).c_str());
    TEST_ASSERT_EQUAL_STRING("50", fmt(menuFind("Picture/Brightness"), be).c_str());    // a 0..100 setting, 50 neutral
    be.texts[LT_FW_GBS] = "3.0.0_dev";
    TEST_ASSERT_EQUAL_STRING("3.0.0 dev", fmt(menuFind("System/Update/GBS-Control"), be).c_str());   // sanitized
}

// Developer toggles show their state; SOG level shows the level alone
void test_developer_readbacks(void) {
    FakeBackend be;
    be.dev = true;
    be.lives[LV_SYNC_POLARITY] = 0;
    TEST_ASSERT_EQUAL_STRING("Normal", fmt(menuFind("Developer/Sync/Sync polarity"), be).c_str());
    be.lives[LV_SYNC_POLARITY] = 1;
    TEST_ASSERT_EQUAL_STRING("Inverted", fmt(menuFind("Developer/Sync/Sync polarity"), be).c_str());
    be.lives[LV_OSD_TEST] = 1;
    TEST_ASSERT_EQUAL_STRING("On", fmt(menuFind("Developer/Debug/OSD test screen"), be).c_str());
    be.lives[LV_PRINT_INFO] = 0;
    TEST_ASSERT_EQUAL_STRING("Off", fmt(menuFind("Developer/Serial log/Print info"), be).c_str());
    be.lives[LV_IR_LOG] = 1;
    TEST_ASSERT_EQUAL_STRING("On", fmt(menuFind("Developer/Serial log/IR codes log"), be).c_str());
    be.lives[LV_OTA] = 1;
    TEST_ASSERT_EQUAL_STRING("On", fmt(menuFind("Developer/Arduino upload"), be).c_str());
    be.apply(SID_input_source, 2); be.settle();
    be.lives[LV_SOG] = 7;
    TEST_ASSERT_EQUAL_STRING("7", fmt(menuFind("Input/Sampling/SOG level"), be).c_str());
}

void test_step_values(void) {
    FakeBackend be;
    MenuNode luma = menuFind("Input/Decoder/Luma comb", SID_adv_cl_ntsc);
    uint16_t v = 0, seq[5];
    for (int i = 0; i < 5; i++) { TEST_ASSERT_TRUE(menuStepValue(luma, be, +1, v)); be.apply(SID_adv_cl_ntsc, v); seq[i] = v; }
    const uint16_t expect[5] = {4, 5, 6, 7, 0};
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expect, seq, 5);
    MenuNode strength = menuFind("Picture/Scanline strength");
    be.apply(SID_pic_scan_strength, 0x50);
    TEST_ASSERT_TRUE(menuStepValue(strength, be, +1, v)); TEST_ASSERT_EQUAL_UINT16(0x60, v);
    be.apply(SID_pic_scan_strength, 0x60);
    TEST_ASSERT_TRUE(menuStepValue(strength, be, +1, v)); TEST_ASSERT_EQUAL_UINT16(0x60, v);   // clamped
    MenuNode src = menuFind("Input/Source");
    platformSetCaps(CAP_OLED | CAP_ENCODER);
    be.apply(SID_input_source, 4);
    TEST_ASSERT_TRUE(menuStepValue(src, be, +1, v)); TEST_ASSERT_EQUAL_UINT16(0, v);   // S-Video/Composite need CAP_ADV: wraps to Auto
    platformSetCaps(CAPS_DEFAULT);
    MenuNode theme = menuFind("System/OSD settings/Theme");
    be.apply(SID_osd_theme, 0);
    TEST_ASSERT_TRUE(menuStepValue(theme, be, -1, v)); TEST_ASSERT_EQUAL_UINT16(3, v);   // wraps: GBSC PRO -> Retro
}

void test_slider_and_changed(void) {
    FakeBackend be;
    MenuNode vol = menuFind("System/Sound/Volume");
    be.apply(SID_audio_volume, 50);                           // percent: half way
    TEST_ASSERT_EQUAL_INT(10, menuSliderHalves(vol, be));
    TEST_ASSERT_TRUE(menuIsChanged(vol, be));
    be.apply(SID_audio_volume, 80);                           // the default
    TEST_ASSERT_FALSE(menuIsChanged(vol, be));
    TEST_ASSERT_EQUAL_INT(-1, menuSliderHalves(menuFind("Output/Resolution"), be));
}

void test_sanitize_dynamic_text(void) {
    char s[] = "my_ssid,2*";
    menuSanitize(s);
    TEST_ASSERT_EQUAL_STRING("my ssid.2.", s);
}

void test_reset_writes_use_registry_defaults(void) {
    MenuWrite w[16];
    uint8_t n = menuDecoderResetWrites(w);
    // "Resets the whole decoder": the Adaptive contrast gains too (they were left out)
    const uint8_t expect[12] = {SID_adv_brightness, SID_adv_contrast, SID_adv_saturation, SID_adv_hue, SID_adv_ace,
                                SID_adv_i2p, SID_adv_smooth, SID_adv_ace_luma, SID_adv_ace_chroma, SID_adv_ace_cmax,
                                SID_adv_ace_gamma, SID_adv_ace_speed};
    TEST_ASSERT_EQUAL_UINT8(12, n);
    for (uint8_t i = 0; i < n; i++) {
        SettingDef d; settingsDefAt(w[i].sid, d);
        TEST_ASSERT_EQUAL_UINT8(expect[i], w[i].sid);
        TEST_ASSERT_EQUAL_UINT16(d.def, w[i].v);
    }
}

// Reset picture: every Picture page setting back to its registry default, 12 writes
void test_picture_reset_writes(void) {
    MenuWrite w[16];
    TEST_ASSERT_EQUAL_UINT8(12, menuPictureResetWrites(w));
    const uint8_t expect[12] = {SID_pic_sharpness, SID_pic_line_filter, SID_pic_scanlines, SID_pic_scan_strength,
                                SID_pic_brightness, SID_pic_contrast, SID_pic_saturation, SID_pic_deint,
                                SID_pic_step_resp, SID_pic_red, SID_pic_green, SID_pic_blue};
    for (uint8_t i = 0; i < 12; i++) {
        SettingDef d; settingsDefAt(w[i].sid, d);
        TEST_ASSERT_EQUAL_UINT8(expect[i], w[i].sid);
        TEST_ASSERT_EQUAL_UINT16(d.def, w[i].v);
    }
}

// n/a reasons of the v3 tree
static uint16_t reasonOf(const MenuNode& n, FakeBackend& be) { uint16_t r = 0; menuEnabled(n, be, &r); return r; }
void test_na_reasons(void) {
    FakeBackend be;
    be.apply(SID_input_source, 5); be.settle();
    MenuNode smooth = menuFind("Input/2X smooth"), deint = menuFind("Picture/Deinterlace");
    TEST_ASSERT_EQUAL_UINT16(S_R_2X_FIRST, reasonOf(smooth, be));
    TEST_ASSERT_EQUAL_UINT16(S_NONE, reasonOf(deint, be));
    be.apply(SID_adv_i2p, 1); be.settle();
    TEST_ASSERT_EQUAL_UINT16(S_NONE, reasonOf(smooth, be));
    TEST_ASSERT_EQUAL_UINT16(S_R_2X_ON, reasonOf(deint, be));
    be.apply(SID_input_source, 1); be.settle();                       // 2X acts on S-Video/Composite only
    TEST_ASSERT_EQUAL_UINT16(S_NONE, reasonOf(deint, be));
    MenuNode ftl = menuFind("Output/Smooth scrolling"), method = menuFind("Output/Lock method");
    TEST_ASSERT_EQUAL_UINT16(S_R_FTL_OFF, reasonOf(method, be));
    be.apply(SID_output_ftl, 1); be.settle();
    TEST_ASSERT_EQUAL_UINT16(S_NONE, reasonOf(method, be));
    be.sig.srcHzX10 = 500;
    TEST_ASSERT_EQUAL_UINT16(S_NONE, reasonOf(ftl, be));
    be.apply(SID_output_force60, 1); be.settle();
    TEST_ASSERT_EQUAL_UINT16(S_R_FORCE60, reasonOf(ftl, be));
    be.sig.srcHzX10 = 599;                                             // a 60 Hz source still locks
    TEST_ASSERT_EQUAL_UINT16(S_NONE, reasonOf(ftl, be));
    MenuNode scan = menuFind("Picture/Scanlines");
    be.sig.scanlinesAllowed = false; be.sig.interlaced = true;
    TEST_ASSERT_EQUAL_UINT16(S_R_NEEDS_BOB, reasonOf(scan, be));
    be.apply(SID_pic_deint, 1); be.settle();
    TEST_ASSERT_EQUAL_UINT16(S_R_SCANLINES, reasonOf(scan, be));
    be.sig.interlaced = false; be.apply(SID_pic_deint, 0); be.settle();
    TEST_ASSERT_EQUAL_UINT16(S_R_SCANLINES, reasonOf(scan, be));
}

// Easy hides NF_ADVANCED nodes; the Developer page needs Advanced and the Developer menu
void test_advanced_node_hidden_in_easy(void) {
    FakeBackend be;
    MenuNode dec = menuFind("Input/Decoder");
    be.apply(SID_input_source, 5); be.settle();
    be.apply(SID_sys_menu_mode, 1); be.settle();
    TEST_ASSERT_TRUE(menuVisible(dec, be));
    be.apply(SID_sys_menu_mode, 0); be.settle();
    TEST_ASSERT_FALSE(menuVisible(dec, be));
    TEST_ASSERT_TRUE(menuVisible(menuFind("Input/Source"), be));   // Easy items stay
}

void test_developer_needs_advanced_and_devmode(void) {
    FakeBackend be;
    MenuNode dev = menuFind("Developer");
    const bool expect[2][2] = {{false, false}, {false, true}};    // [mode][devmode]
    for (int mode = 0; mode < 2; mode++)
        for (int d = 0; d < 2; d++) {
            be.apply(SID_sys_menu_mode, (uint16_t)mode); be.settle();
            be.dev = d != 0;
            TEST_ASSERT_EQUAL(expect[mode][d], menuVisible(dev, be));
        }
}

// Easy steps only the options without the '#a' mark, but keeps the current one in the cycle
void test_easy_skips_advanced_options(void) {
    FakeBackend be;
    be.apply(SID_input_source, 5); be.apply(SID_sys_menu_mode, 0); be.settle();
    MenuNode n = menuFind("Input/Video standard", SID_adv_format_sv);
    const uint16_t expect[] = {12, 1, 2, 3, 5, 0};
    uint16_t cur = 0, next = 0;
    for (uint16_t e : expect) {
        TEST_ASSERT_TRUE(menuStepFrom(n, be, cur, +1, next));
        TEST_ASSERT_EQUAL_UINT16(e, next);
        cur = next;
    }
}

void test_easy_keeps_current_advanced_option(void) {
    FakeBackend be;
    be.apply(SID_input_source, 5); be.apply(SID_sys_menu_mode, 0); be.settle();
    MenuNode n = menuFind("Input/Video standard", SID_adv_format_sv);
    uint16_t next = 0;
    TEST_ASSERT_TRUE(menuStepFrom(n, be, 11, -1, next));   // from SECAM (advanced) back to the Easy NTSC-J
    TEST_ASSERT_EQUAL_UINT16(5, next);
    be.apply(SID_sys_menu_mode, 1); be.settle();          // Advanced steps every option
    TEST_ASSERT_TRUE(menuStepFrom(n, be, 3, +1, next));
    TEST_ASSERT_EQUAL_UINT16(4, next);
}
// One rule for the PAL/NTSC word everywhere: nothing until the source rate is measured
void test_rate_standard_word(void) {
    TEST_ASSERT_EQUAL_STRING("", menuRateStandard(0));
    TEST_ASSERT_EQUAL_STRING("PAL", menuRateStandard(500));
    TEST_ASSERT_EQUAL_STRING("NTSC", menuRateStandard(599));
}

// Status pop-up: Off never; Input only on a new input or profile (not 240p <-> 480i on the same game); Always on
// any change
void test_status_popup_rules(void) {
    // VPERIOD_IF counts about two fields in modes 1/2: 240p reads 525 (odd), 480i 524/526 (even)
    StatusKey a = {1, 1, 525, 0, false}, flip = {1, 1, 524, 0, true}, input = {2, 1, 525, 0, false},
              prof = {1, 1, 525, 3, false}, jitter = {1, 1, 523, 0, false};
    TEST_ASSERT_FALSE(menuStatusPopupDue(0, a, input));
    TEST_ASSERT_TRUE(menuStatusPopupDue(1, a, input));
    TEST_ASSERT_TRUE(menuStatusPopupDue(1, a, prof));
    TEST_ASSERT_FALSE(menuStatusPopupDue(1, a, flip));
    TEST_ASSERT_TRUE(menuStatusPopupDue(2, a, flip));
    TEST_ASSERT_FALSE(menuStatusPopupDue(2, a, jitter));                 // line jitter is not a change
    TEST_ASSERT_FALSE(menuStatusKeyChanged(a, jitter));
    TEST_ASSERT_TRUE(menuStatusKeyChanged(a, flip));
}

// The source line count shown in the menus: only a value that stayed within 2 lines for a second; 0 ("--") while it
// moves (an unstable sync made "1220L" and similar flash by)
void test_line_filter_shows_only_stable_counts(void) {
    MenuLineFilter f;
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(312, 0));
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(313, MenuLineFilter::kStableMs - 1));
    TEST_ASSERT_EQUAL_UINT16(313, f.feed(313, MenuLineFilter::kStableMs));
    TEST_ASSERT_EQUAL_UINT16(313, f.feed(311, 1500));                    // jitter of 2 keeps it
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(1220, 1600));                     // a jump: hidden at once
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(640, 1700));
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(312, 1800));
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(312, 1800 + MenuLineFilter::kStableMs - 1));
    TEST_ASSERT_EQUAL_UINT16(312, f.feed(312, 1800 + MenuLineFilter::kStableMs));
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(0, 5000));                        // no line counter: nothing
}

// A count no video uses is never shown, even when it stays: a console switching off gave "218L"
void test_line_filter_hides_implausible_counts(void) {
    MenuLineFilter f;
    f.feed(218, 0);
    TEST_ASSERT_EQUAL_UINT16(0, f.feed(218, 5000));
    MenuLineFilter g;
    g.feed(1300, 0);
    TEST_ASSERT_EQUAL_UINT16(0, g.feed(1300, 5000));
    MenuLineFilter h;                                                    // XGA (806 lines) and 1200p (1235) are real
    h.feed(806, 0);
    TEST_ASSERT_EQUAL_UINT16(806, h.feed(806, 2000));
    MenuLineFilter k;
    k.feed(1235, 0);
    TEST_ASSERT_EQUAL_UINT16(1235, k.feed(1235, 2000));
    MenuLineFilter m;
    m.feed(262, 0);
    TEST_ASSERT_EQUAL_UINT16(262, m.feed(262, 2000));
}

// Rates are measured again every kRetryMs while a source is shown and they are still unknown: a measurement planned
// while the RGB SCART judge ran (no signal for the menu) was skipped for good and the OLED kept "--.-Hz"
void test_rates_retry_while_shown_and_unknown(void) {
    MenuRatesRetry r;
    TEST_ASSERT_FALSE(r.due(true, 0, 0));                                // no signal: nothing to measure
    TEST_ASSERT_FALSE(r.due(false, 0, 100));                             // just shown: let the signal settle first
    TEST_ASSERT_FALSE(r.due(false, 0, 100 + MenuRatesRetry::kRetryMs - 1));
    TEST_ASSERT_TRUE(r.due(false, 0, 100 + MenuRatesRetry::kRetryMs));   // still unknown: measure
    TEST_ASSERT_FALSE(r.due(false, 0, 100 + 2 * MenuRatesRetry::kRetryMs - 1));
    TEST_ASSERT_TRUE(r.due(false, 0, 100 + 2 * MenuRatesRetry::kRetryMs));   // again
    TEST_ASSERT_FALSE(r.due(false, 497, 5000));                          // known: no more
}

// Each measurement blocks the loop while it waits for a pulse (about 2 s when none comes, as in HD bypass before the
// fix: the menu turned to one step every 2 s): after kMaxTries failed tries it stops until the signal
// goes away or the rates are known
void test_rates_retry_gives_up_after_failed_tries(void) {
    MenuRatesRetry r;
    uint32_t t = 0;
    r.due(false, 0, t);                                                  // shown
    for (uint8_t i = 0; i < MenuRatesRetry::kMaxTries; i++) {
        t += MenuRatesRetry::kRetryMs;
        TEST_ASSERT_TRUE(r.due(false, 0, t));
    }
    t += MenuRatesRetry::kRetryMs;
    TEST_ASSERT_FALSE(r.due(false, 0, t));                               // given up
    TEST_ASSERT_FALSE(r.due(false, 0, t + 60000));
    TEST_ASSERT_FALSE(r.due(true, 0, t + 60001));                        // the signal went away
    r.due(false, 0, t + 60002);                                          // a new one: tries again
    TEST_ASSERT_TRUE(r.due(false, 0, t + 60002 + MenuRatesRetry::kRetryMs));
}

// A signal change without a "no signal" phase (240p -> 480i, a new output preset) tries again too
void test_rates_retry_starts_over_on_a_signal_change(void) {
    MenuRatesRetry r;
    uint32_t t = 0;
    r.due(false, 0, t);
    for (uint8_t i = 0; i <= MenuRatesRetry::kMaxTries; i++) { t += MenuRatesRetry::kRetryMs; r.due(false, 0, t); }
    TEST_ASSERT_FALSE(r.due(false, 0, t + MenuRatesRetry::kRetryMs));   // given up
    r.restart();                                                         // the menu saw the signal change
    t += 2 * MenuRatesRetry::kRetryMs;
    TEST_ASSERT_FALSE(r.due(false, 0, t));                               // settles first
    TEST_ASSERT_TRUE(r.due(false, 0, t + MenuRatesRetry::kRetryMs));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rates_retry_while_shown_and_unknown);
    RUN_TEST(test_rates_retry_gives_up_after_failed_tries);
    RUN_TEST(test_rates_retry_starts_over_on_a_signal_change);
    RUN_TEST(test_line_filter_shows_only_stable_counts);
    RUN_TEST(test_line_filter_hides_implausible_counts);
    RUN_TEST(test_rate_standard_word);
    RUN_TEST(test_status_popup_rules);
    RUN_TEST(test_picture_reset_writes);
    RUN_TEST(test_na_reasons);
    RUN_TEST(test_reset_writes_use_registry_defaults);
    RUN_TEST(test_strings_copy_and_terminate);
    RUN_TEST(test_fake_apply_is_pending_until_settle);
    RUN_TEST(test_fake_queue_full);
    RUN_TEST(test_fake_apply_many_is_all_or_nothing);
    RUN_TEST(test_visibility_follows_the_input_setting);
    RUN_TEST(test_visibility_follows_the_active_input_in_auto);
    RUN_TEST(test_source_value_shows_auto_and_the_found_input);
    RUN_TEST(test_ntsc_pal_comb_pair);
    RUN_TEST(test_sync_stripper_only_on_rgbs_and_rgsb);
    RUN_TEST(test_decoder_option_lists_follow_the_guide);
    RUN_TEST(test_video_format_auto_with_setup);
    RUN_TEST(test_output_resolution_has_1200p);
    RUN_TEST(test_disabled_with_reason);
    RUN_TEST(test_every_enable_rule_has_a_reason);
    RUN_TEST(test_caps_and_devmode_hide_nodes);
    RUN_TEST(test_streaming_option_parser_beyond_160_chars);
    RUN_TEST(test_value_formats);
    RUN_TEST(test_developer_readbacks);
    RUN_TEST(test_step_values);
    RUN_TEST(test_slider_and_changed);
    RUN_TEST(test_sanitize_dynamic_text);
    RUN_TEST(test_advanced_node_hidden_in_easy);
    RUN_TEST(test_developer_needs_advanced_and_devmode);
    RUN_TEST(test_easy_skips_advanced_options);
    RUN_TEST(test_easy_keeps_current_advanced_option);
    return UNITY_END();
}
