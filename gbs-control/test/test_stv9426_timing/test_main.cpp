#include <stdio.h>
#include <unity.h>
#include "stv9426-timing.h"
void setUp(void) {}
void tearDown(void) {}

void test_fm_for_8mhz_is_datasheet_value(void) { TEST_ASSERT_EQUAL_UINT8(10, stvFm(8000000UL)); }

// On a TV: 45.3-45.4 MHz steady and sharp (1200p LD 50, 1080p LD 55), shimmering edges at 49.8 MHz
// (1200p LD 55); the pixel clock stops at 45.5 MHz (the VCO > 6x pixel rule held 1080p at LD 41)
void test_ld_stops_at_the_tested_pixel_clock(void) {
    TEST_ASSERT_EQUAL_UINT8(55, stvLdFor(67500, 8000000UL, 10));   // 12*56*67500 = 45.36 MHz
    TEST_ASSERT_EQUAL_UINT8(50, stvLdFor(74040, 8000000UL, 10));   // 1200p60: 45.31 MHz
    TEST_ASSERT_EQUAL_UINT8(62, stvLdFor(60000, 8000000UL, 10));   // 960p60: 45.36 MHz
    TEST_ASSERT_EQUAL_UINT8(63, stvLdFor(45000, 8000000UL, 10));   // 34.56 MHz: LD's own limit
    TEST_ASSERT_EQUAL_UINT8(58, stvLdFor(63981, 8000000UL, 10));   // 45.30 MHz
    TEST_ASSERT_EQUAL_UINT8(63, stvLdFor(15734, 8000000UL, 10));
    TEST_ASSERT_EQUAL_UINT8(32, stvLdFor(200000, 8000000UL, 10));  // never below the capture minimum
}

void test_pp_formula(void) {
    TEST_ASSERT_EQUAL_UINT8(45, stvPp(10, 8000000UL, 63, 31469));  // 1.664e9 / 24168192 - 24 = 44.85
    TEST_ASSERT_EQUAL_UINT8(25, stvPp(10, 8000000UL, 41, 67500));  // 48.91 - 24
}

void test_dd_keeps_todays_left_edge(void) {
    TEST_ASSERT_EQUAL_UINT8(38, stvDdFor(63));
    TEST_ASSERT_EQUAL_UINT8(22, stvDdFor(41));
    TEST_ASSERT_TRUE(stvDdFor(32) >= 4);
}

void test_preset_id_mapping(void) {
    TEST_ASSERT_EQUAL_UINT8(OUT_1080P60, stvOutStdOf(0x05));
    TEST_ASSERT_EQUAL_UINT8(OUT_1080P50, stvOutStdOf(0x15));
    TEST_ASSERT_EQUAL_UINT8(OUT_576P, stvOutStdOf(0x14));
    TEST_ASSERT_EQUAL_UINT8(OUT_BYPASS, stvOutStdOf(0x21));
    TEST_ASSERT_EQUAL_UINT8(OUT_BYPASS, stvOutStdOf(0x22));
    TEST_ASSERT_EQUAL_UINT8(OUT_480P, stvOutStdOf(0x00));
    TEST_ASSERT_EQUAL_UINT8(OUT_1200P60, stvOutStdOf(0x07));      // issue #35
    TEST_ASSERT_EQUAL_UINT8(OUT_1200P50, stvOutStdOf(0x17));
    TEST_ASSERT_EQUAL_UINT16(1200, stvActiveLines(OUT_1200P60));
    TEST_ASSERT_EQUAL_UINT32(74040, stvLineHz(OUT_1200P60));
    TEST_ASSERT_EQUAL_UINT32(61750, stvLineHz(OUT_1200P50));
}

void test_timing_all_standards_within_pll_limit(void) {
    uint8_t fm = stvFm(STV_XTAL_HZ);
    for (uint8_t s = 0; s < OUT_STD_COUNT; s++) {
        StvTiming t = stvTimingFor((StvOutStd)s, STV_XTAL_HZ);
        TEST_ASSERT_TRUE(t.ld >= 32 && t.ld <= 63);
        TEST_ASSERT_TRUE(stvWithinPll(t.ld, stvLineHz((StvOutStd)s), STV_XTAL_HZ, fm));
        TEST_ASSERT_TRUE(t.dd >= 4);
        TEST_ASSERT_TRUE(t.chMenu >= 1 && t.chMenu <= 63);
    }
    TEST_ASSERT_TRUE(stvWithinPll(STV_LD_IDLE, 67500, STV_XTAL_HZ, fm));
}

// A menu row shorter than the 18-line ROM glyph shows a strip of stray pixels under every row (720p had CH 16, fixed
// with CH 18). The 15 kHz outputs are left out: 240/288 lines, not checked at a TV yet.
void test_menu_rows_hold_the_whole_glyph(void) {
    for (uint8_t s = 0; s < OUT_STD_COUNT; s++) {
        if (s == OUT_15K60 || s == OUT_15K50) continue;
        char what[24];
        snprintf(what, sizeof(what), "output standard %u", s);
        TEST_ASSERT_TRUE_MESSAGE(stvTimingFor((StvOutStd)s, STV_XTAL_HZ).chMenu >= 18, what);
    }
}

// No zoom (it doubles the height only) and the 480p share of the height at every output
void test_menu_is_a_third_of_the_height_unzoomed(void) {
    for (uint8_t s = 0; s < OUT_STD_COUNT; s++) {
        if (s == OUT_15K60 || s == OUT_15K50 || s == OUT_BYPASS || s == OUT_576P) continue;
        StvTiming t = stvTimingFor((StvOutStd)s, STV_XTAL_HZ);
        char what[24];
        snprintf(what, sizeof(what), "output standard %u", s);
        TEST_ASSERT_FALSE_MESSAGE(t.zoomMenu, what);
        unsigned share = 900u * t.chMenu / stvActiveLines((StvOutStd)s);   // 9 rows, per cent
        TEST_ASSERT_TRUE_MESSAGE(share >= 33 && share <= 34, what);
    }
}

void test_bypass_is_safe_up_to_87khz(void) {
    StvTiming t = stvTimingFor(OUT_BYPASS, STV_XTAL_HZ);
    TEST_ASSERT_EQUAL_UINT8(32, t.ld);                                   // SXGA@75 RGBHV bypass is ~80 kHz
    TEST_ASSERT_TRUE(stvWithinPll(t.ld, 80000, STV_XTAL_HZ, stvFm(STV_XTAL_HZ)));
}

void test_pll_time_constants_valid_for_every_ld(void) {
    TEST_ASSERT_FALSE(stvTimeConstantsOk(0, 3, 41));   // the old capture constants (AF 0, BF 3): only LD 63
    TEST_ASSERT_FALSE(stvTimeConstantsOk(0, 3, 63));   // Table 2 says Y, but B - A < 3 (datasheet summary)
    TEST_ASSERT_TRUE(stvTimeConstantsOk(0, 2, 63));
    TEST_ASSERT_FALSE(stvTimeConstantsOk(0, 2, 16));
    uint8_t lockA = STV_LOCK_TC >> 4, lockB = STV_LOCK_TC & 7, capA = STV_CAPTURE_TC >> 4, capB = STV_CAPTURE_TC & 7;
    for (uint8_t s = 0; s < OUT_STD_COUNT; s++) {
        uint8_t ld = stvTimingFor((StvOutStd)s, STV_XTAL_HZ).ld;
        TEST_ASSERT_TRUE(stvTimeConstantsOk(lockA, lockB, ld));
        TEST_ASSERT_TRUE(stvTimeConstantsOk(capA, capB, ld));
    }
    TEST_ASSERT_TRUE(stvTimeConstantsOk(capA, capB, STV_LD_IDLE));
    TEST_ASSERT_TRUE(stvTimeConstantsOk(lockA, lockB, STV_LD_IDLE));
}

void test_compat_menu_fits_the_line_for_every_ld(void) {
    for (uint8_t ld = 32; ld <= 63; ld++) {
        unsigned start = (stvDdFor(ld) + 8u) * 3u, line = 12u * (ld + 1u);
        TEST_ASSERT_TRUE_MESSAGE(start + 28u * 12u <= line * 95u / 100u, "28 cells past the line end");
    }
}

void test_bypass_uses_the_measured_rate(void) {
    TEST_ASSERT_EQUAL_UINT8(63, stvTimingForRate(OUT_BYPASS, 45000, STV_XTAL_HZ).ld);   // 720p HD bypass
    TEST_ASSERT_EQUAL_UINT8(32, stvTimingForRate(OUT_BYPASS, 0, STV_XTAL_HZ).ld);       // unknown: conservative
}

void test_menu_fits_the_line_for_ld_35_to_63(void) {
    for (uint8_t ld = 35; ld <= 63; ld++) {
        unsigned start = (stvDdFor(ld) + 8u) * 3u, line = 12u * (ld + 1u);
        TEST_ASSERT_TRUE_MESSAGE(start + 30u * 12u <= line * 95u / 100u, "30 cells past the line end");
    }
}

// Horizontal placement from the TV5725 output window: the panel's first pixel is (DD + 8) * 3 STV pixels after the
// H-SYNC falling edge, which is VDS_HS_SP
static float panelStartFrac(int dd, uint8_t ld) { return (float)((dd + 8) * 3.0 / (12.0 * (ld + 1))); }
static const float kPanelFrac63 = 360.0f / 768.0f;

void test_dd_centres_the_panel_on_the_active_window(void) {
    struct P { StvWindow w; } presets[] = {
        {{2704, 160, 612, 2594, true}},   // ntsc_240p.h (960p)
        {{2573, 12, 510, 2490, true}},    // ntsc_720x480.h (sync low-active: falling edge still at HS_SP)
        {{1601, 72, 348, 1356, true}},    // ntsc_1920x1080.h
        {{2166, 152, 494, 2074, true}},   // pal_240p.h
    };
    for (const P& p : presets) {
        const StvWindow& w = p.w;
        int dd = stvDdForWindow(63, w, STV_ALIGN_CENTER, 0);
        double s = (double)((w.act0 - w.hsSp + w.htotal) % w.htotal) / w.htotal;
        double a = (double)((w.act1 - w.act0 + w.htotal) % w.htotal) / w.htotal;
        double want = s + (a - kPanelFrac63) / 2;
        TEST_ASSERT_FLOAT_WITHIN(3.0f / 768.0f, (float)want, panelStartFrac(dd, 63));   // within one DD unit
    }
}

void test_dd_left_and_right_keep_a_margin(void) {
    StvWindow w = {2704, 160, 612, 2594, true};
    int l = stvDdForWindow(63, w, STV_ALIGN_LEFT, 0), c = stvDdForWindow(63, w, STV_ALIGN_CENTER, 0);
    int r = stvDdForWindow(63, w, STV_ALIGN_RIGHT, 0);
    TEST_ASSERT_TRUE(l < c && c < r);
    double s = 452.0 / 2704, a = 1982.0 / 2704;
    TEST_ASSERT_FLOAT_WITHIN(3.0f / 768.0f, (float)(s + a * 0.05), panelStartFrac(l, 63));
    TEST_ASSERT_FLOAT_WITHIN(3.0f / 768.0f, (float)(s + a * 0.95 - kPanelFrac63), panelStartFrac(r, 63));
}

void test_dd_narrow_window_keeps_left_centre_right_order(void) {
    StvWindow w = {1601, 72, 348, 1356, true};                    // ntsc_1920x1080.h at LD 41: picture < panel
    int l = stvDdForWindow(41, w, STV_ALIGN_LEFT, 0), c = stvDdForWindow(41, w, STV_ALIGN_CENTER, 0);
    int r = stvDdForWindow(41, w, STV_ALIGN_RIGHT, 0);
    TEST_ASSERT_TRUE(l <= c && c <= r);
}

void test_dd_without_a_window_is_the_legacy_edge(void) {
    StvWindow none = {0, 0, 0, 0, false};
    TEST_ASSERT_EQUAL_INT(stvDdFor(63), stvDdForWindow(63, none, STV_ALIGN_CENTER, 0));
    StvWindow zero = {0, 10, 20, 30, true};                        // htotal 0: not usable
    TEST_ASSERT_EQUAL_INT(stvDdFor(40), stvDdForWindow(40, zero, STV_ALIGN_LEFT, 0));
}

void test_dd_stays_inside_the_line(void) {
    StvWindow w = {1601, 72, 348, 1356, true};
    for (uint8_t ld = 40; ld <= 63; ld++)                         // the panel fits 95 % of the line from LD 40
        for (uint8_t al = 0; al < 3; al++) {
            int dd = stvDdForWindow(ld, w, al, 0);
            TEST_ASSERT_TRUE(dd >= 4 && dd <= stvDdFor(ld) + 400);
            TEST_ASSERT_TRUE(((dd + 8) * 3 + 360) * 100 <= 12 * (ld + 1) * 95 + 300);   // ends within 95 % (+1 unit)
        }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fm_for_8mhz_is_datasheet_value);
    RUN_TEST(test_ld_stops_at_the_tested_pixel_clock);
    RUN_TEST(test_menu_is_a_third_of_the_height_unzoomed);
    RUN_TEST(test_pp_formula);
    RUN_TEST(test_dd_keeps_todays_left_edge);
    RUN_TEST(test_preset_id_mapping);
    RUN_TEST(test_timing_all_standards_within_pll_limit);
    RUN_TEST(test_menu_rows_hold_the_whole_glyph);
    RUN_TEST(test_bypass_is_safe_up_to_87khz);
    RUN_TEST(test_pll_time_constants_valid_for_every_ld);
    RUN_TEST(test_compat_menu_fits_the_line_for_every_ld);
    RUN_TEST(test_bypass_uses_the_measured_rate);
    RUN_TEST(test_menu_fits_the_line_for_ld_35_to_63);
    RUN_TEST(test_dd_centres_the_panel_on_the_active_window);
    RUN_TEST(test_dd_left_and_right_keep_a_margin);
    RUN_TEST(test_dd_narrow_window_keeps_left_centre_right_order);
    RUN_TEST(test_dd_without_a_window_is_the_legacy_edge);
    RUN_TEST(test_dd_stays_inside_the_line);
    return UNITY_END();
}
