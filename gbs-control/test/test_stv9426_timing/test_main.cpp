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

// VGA pass-through: the line rate from the IF's line period (a quarter of the 27 MHz oscillator's counts)
void test_line_rate_from_if_period(void) {
    TEST_ASSERT_UINT32_WITHIN(100, 31469, stvLineHzFromIfPeriod(214));   // 640x480@60, read on the unit
    TEST_ASSERT_UINT32_WITHIN(100, 15734, stvLineHzFromIfPeriod(429));   // 15 kHz NTSC
    TEST_ASSERT_UINT32_WITHIN(1000, 80000, stvLineHzFromIfPeriod(84));   // SXGA@75
    TEST_ASSERT_EQUAL_UINT32(0, stvLineHzFromIfPeriod(0));
    TEST_ASSERT_EQUAL_UINT32(0, stvLineHzFromIfPeriod(511));             // the counter's top: overflowed
    TEST_ASSERT_EQUAL_UINT32(0, stvLineHzFromIfPeriod(60));              // over 100 kHz: not a line rate
    // 640x480 with the rate it gives: LD 63, the menu as on the scaled 480p output
    TEST_ASSERT_EQUAL_UINT8(stvTimingFor(OUT_480P, STV_XTAL_HZ).ld,
                            stvTimingForRate(OUT_BYPASS, stvLineHzFromIfPeriod(214), STV_XTAL_HZ, 504).ld);
}

// Bypass: the rows follow the visible lines of the source (18 lines at 480, as the scaled outputs), not 480p's
void test_bypass_rows_follow_the_visible_lines(void) {
    StvTiming t = stvTimingForRate(OUT_BYPASS, 45000, STV_XTAL_HZ, 720);     // 720p
    StvTiming p = stvTimingFor(OUT_720P60, STV_XTAL_HZ);
    TEST_ASSERT_EQUAL_UINT8(p.chMenu, t.chMenu);
    TEST_ASSERT_EQUAL_UINT8(p.topMenu, t.topMenu);
    TEST_ASSERT_EQUAL_UINT8(p.ld, t.ld);
    t = stvTimingForRate(OUT_BYPASS, 67500, STV_XTAL_HZ, 1080);             // 1080p
    TEST_ASSERT_EQUAL_UINT8(stvTimingFor(OUT_1080P60, STV_XTAL_HZ).chMenu, t.chMenu);
    t = stvTimingForRate(OUT_BYPASS, 33750, STV_XTAL_HZ, 540);              // 1080i: a field has 540
    TEST_ASSERT_EQUAL_UINT8(20, t.chMenu);
    TEST_ASSERT_EQUAL_UINT8(40, t.topMenu);
    t = stvTimingForRate(OUT_BYPASS, 15734, STV_XTAL_HZ, 240);              // a 15 kHz source passed through
    TEST_ASSERT_EQUAL_UINT8(stvTimingFor(OUT_15K60, STV_XTAL_HZ).chMenu, t.chMenu);
    TEST_ASSERT_EQUAL_UINT8(stvTimingFor(OUT_15K60, STV_XTAL_HZ).topMenu, t.topMenu);
    t = stvTimingForRate(OUT_BYPASS, 0, STV_XTAL_HZ);                       // nothing known: 480p rows, as before
    TEST_ASSERT_EQUAL_UINT8(18, t.chMenu);
    TEST_ASSERT_EQUAL_UINT8(40, t.topMenu);
    t = stvTimingForRate(OUT_1080P60, 0, STV_XTAL_HZ, 720);                 // a scaled output keeps its own rows
    TEST_ASSERT_EQUAL_UINT8(stvTimingFor(OUT_1080P60, STV_XTAL_HZ).chMenu, t.chMenu);
}

// An HD source passed through has no scaler window: the standard timing of its format stands for it (the output is
// the source: the picture starts a back porch after the end of the sync pulse). Seen on a TV at 720p60: with the
// fixed DD 38 the panel started about 6 % into the picture and ended at two thirds of it
void test_bypass_window_of_hd_formats(void) {
    StvWindow w;
    TEST_ASSERT_TRUE(stvBypassWindow(749, 44895, w));                    // 720p60 as the scaler counts it
    TEST_ASSERT_TRUE(w.valid);
    TEST_ASSERT_EQUAL_UINT16(1650, w.htotal);
    TEST_ASSERT_EQUAL_UINT16(220, w.act0 - w.hsSp);
    TEST_ASSERT_EQUAL_UINT16(1280, w.act1 - w.act0);
    TEST_ASSERT_TRUE(stvBypassWindow(750, 37500, w));                    // 720p50
    TEST_ASSERT_EQUAL_UINT16(1980, w.htotal);
    TEST_ASSERT_TRUE(stvBypassWindow(562, 33750, w));                    // 1080i60: the lines of a field
    TEST_ASSERT_EQUAL_UINT16(2200, w.htotal);
    TEST_ASSERT_EQUAL_UINT16(148, w.act0 - w.hsSp);
    TEST_ASSERT_EQUAL_UINT16(1920, w.act1 - w.act0);
    TEST_ASSERT_TRUE(stvBypassWindow(562, 28125, w));                    // 1080i50
    TEST_ASSERT_EQUAL_UINT16(2640, w.htotal);
    TEST_ASSERT_TRUE(stvBypassWindow(1125, 67500, w));                   // 1080p60
    TEST_ASSERT_EQUAL_UINT16(2200, w.htotal);
    TEST_ASSERT_TRUE(stvBypassWindow(1125, 56250, w));                   // 1080p50
    TEST_ASSERT_EQUAL_UINT16(2640, w.htotal);
    TEST_ASSERT_FALSE(stvBypassWindow(749, 0, w));                       // no rate: nothing to stand on
    TEST_ASSERT_FALSE(w.valid);
    TEST_ASSERT_FALSE(stvBypassWindow(806, 48363, w));                   // 1024x768: not an HD video format
    TEST_ASSERT_FALSE(stvBypassWindow(525, 31469, w));                   // 480p: left as it is (not measured)
}

// VGA pass-through (VGA upscaling off): 640x480 at 60 Hz has the VESA timing (800 pixels, sync 96, back porch 48, sync
// low). The chip locks on the falling edge, the start of a negative sync. Seen on a TV with a Dreamcast: with the
// fixed DD 38 the panel's centre sat about 140 STV pixels left of the picture's
void test_vga_pass_window_of_640x480(void) {
    StvWindow w;
    TEST_ASSERT_TRUE(stvVgaPassWindow(525, 31469, 120, w));                  // sync 96 of 800
    TEST_ASSERT_TRUE(w.valid);
    TEST_ASSERT_EQUAL_UINT16(800, w.htotal);
    TEST_ASSERT_EQUAL_UINT16(144 + 21, w.act0 - w.hsSp);   // + the pass-through's delay, 2.6 % of 800
    TEST_ASSERT_EQUAL_UINT16(640, w.act1 - w.act0);
    TEST_ASSERT_TRUE(stvVgaPassWindow(525, stvLineHzFromIfPeriod(214), 0, w)); // the rate as measured, no pulse
    uint8_t ld = stvTimingForRate(OUT_BYPASS, 31469, STV_XTAL_HZ).ld;
    TEST_ASSERT_EQUAL_INT(87, stvDdForWindow(ld, w, STV_ALIGN_CENTER, 0));    // was 38 (fixed), 80 without the delay
    TEST_ASSERT_FALSE(stvVgaPassWindow(525, 0, 120, w));                      // no rate
    TEST_ASSERT_FALSE(w.valid);
    TEST_ASSERT_FALSE(stvVgaPassWindow(525, 15734, 120, w));                  // 30 Hz: no VGA mode
    // vertically: the picture's 480 lines start 35 lines after the sync's start (sync 2 + back porch 33)
    StvVgaMode m;
    TEST_ASSERT_TRUE(stvVgaPassMode(525, 31469, 120, m));
    TEST_ASSERT_EQUAL_UINT16(35, m.vstart);
    TEST_ASSERT_EQUAL_UINT16(480, m.vactive);
    TEST_ASSERT_FALSE(stvVgaPassMode(525, 0, 120, m));
}

// The Dreamcast's own 640x480 (its BootROM: 858 clocks of 27 MHz, picture at 168, first line 40) has the lines and the
// rate of the VESA one, and a shorter sync: 64 of 858, measured on the unit as 141 of 1857 (76 thousandths). With the
// VESA window its menu sat 1.5 % of the picture left of the centre
void test_vga_pass_dreamcast_640x480_by_its_sync(void) {
    StvVgaMode m;
    TEST_ASSERT_TRUE(stvVgaPassMode(525, stvLineHzFromIfPeriod(214), 76, m));
    TEST_ASSERT_EQUAL_UINT16(858, m.htotal);
    TEST_ASSERT_EQUAL_UINT16(168, m.hstart);
    TEST_ASSERT_EQUAL_UINT16(40, m.vstart);
    TEST_ASSERT_EQUAL_UINT16(480, m.vactive);
    TEST_ASSERT_TRUE(stvVgaPassMode(525, 31469, 118, m));   // a VESA source
    TEST_ASSERT_EQUAL_UINT16(800, m.htotal);
    TEST_ASSERT_TRUE(stvVgaPassMode(525, 31469, 0, m));     // not measured: VESA
    TEST_ASSERT_EQUAL_UINT16(800, m.htotal);
    StvWindow vesa, dc;
    TEST_ASSERT_TRUE(stvVgaPassWindow(525, 31469, 120, vesa));
    TEST_ASSERT_TRUE(stvVgaPassWindow(525, 31469, 76, dc));
    uint8_t ld = stvTimingForRate(OUT_BYPASS, 31469, STV_XTAL_HZ).ld;
    TEST_ASSERT_TRUE(stvDdForWindow(ld, dc, STV_ALIGN_CENTER, 0) < stvDdForWindow(ld, vesa, STV_ALIGN_CENTER, 0));
}

// Every VGA mode of the table is found from its own lines and line rate (as measured: one line off the standard's
// count, the rate rounded), and no other mode is found instead
void test_vga_pass_modes_are_found_and_unambiguous(void) {
    for (uint8_t i = 0; i < stvVgaPassModeCount(); i++) {
        StvVgaMode want = stvVgaPassModeAt(i), got;
        uint32_t fh = (uint32_t)want.lines * want.hz;
        for (int dl = -1; dl <= 1; dl++) {
            TEST_ASSERT_TRUE(stvVgaPassMode((uint16_t)(want.lines + dl), fh, stvVgaSyncPermille(want), got));
            TEST_ASSERT_EQUAL_UINT16(want.lines, got.lines);
            TEST_ASSERT_EQUAL_UINT8(want.hz, got.hz);
            TEST_ASSERT_EQUAL_UINT16(want.htotal, got.htotal);
        }
        TEST_ASSERT_TRUE(want.hstart + want.hactive < want.htotal);
        TEST_ASSERT_TRUE(want.vstart + want.vactive < want.lines);
        for (uint8_t j = 0; j < stvVgaPassModeCount(); j++) {   // no two modes both near one measurement
            StvVgaMode o = stvVgaPassModeAt(j);
            if (j == i) continue;
            int dl = (int)o.lines - want.lines, dr = (int)o.hz - want.hz;
            int ds = (int)stvVgaSyncPermille(o) - stvVgaSyncPermille(want);   // or pulses 3 % apart
            TEST_ASSERT_FALSE(dl >= -6 && dl <= 6 && dr >= -2 && dr <= 2 && ds > -30 && ds < 30);
        }
    }
}

// The Dreamcast moved from 640x480 to 800x600 under the same preset and the menu stayed placed for 640x480: the source
// is read again, and a change seen twice in a row places it again
void test_source_watch_needs_two_agreeing_readings(void) {
    StvSourceWatch w;
    TEST_ASSERT_FALSE(w.changed(525, 31469, 525, 31469));   // as placed
    TEST_ASSERT_FALSE(w.changed(526, 31500, 525, 31469));   // within a count
    TEST_ASSERT_FALSE(w.changed(628, 37879, 525, 31469));   // new, once
    TEST_ASSERT_TRUE(w.changed(628, 37868, 525, 31469));    // the same again: place it
    TEST_ASSERT_FALSE(w.changed(628, 37879, 628, 37879));   // placed
    TEST_ASSERT_FALSE(w.changed(806, 48363, 628, 37879));   // a reading during the switch...
    TEST_ASSERT_FALSE(w.changed(500, 37500, 628, 37879));   // ... and another one: not the same
    TEST_ASSERT_FALSE(w.changed(500, 0, 628, 37879));       // no rate: skipped
    TEST_ASSERT_TRUE(w.changed(500, 37500, 628, 37879));    // 640x480 75 Hz settled
    StvSourceWatch t;
    TEST_ASSERT_FALSE(t.due(0));
    TEST_ASSERT_FALSE(t.due(500));
    TEST_ASSERT_TRUE(t.due(1000));
    TEST_ASSERT_FALSE(t.due(1999));
    TEST_ASSERT_TRUE(t.due(2000));
}

// A scaled output's picture lines from the scaler's vertical registers, counted from the end of its vertical sync (the
// edge the chip counts from). 480p: sync 4 -> 0 (the short span is lines 0..4), picture 32..520; the menu centred on
// 480 lines from the sync sat 31 lines above the picture's centre on a TV (model: 32). 960p: sync 1..4, picture 38..1000
void test_vds_lines_from_the_sync_end(void) {
    uint16_t first = 0, active = 0;
    TEST_ASSERT_TRUE(stvVdsLines(4, 0, 520, 32, 525, first, active));
    TEST_ASSERT_EQUAL_UINT16(28, first);
    TEST_ASSERT_EQUAL_UINT16(488, active);
    TEST_ASSERT_TRUE(stvVdsLines(1, 4, 1000, 38, 1000, first, active));
    TEST_ASSERT_EQUAL_UINT16(34, first);
    TEST_ASSERT_EQUAL_UINT16(962, active);
    TEST_ASSERT_FALSE(stvVdsLines(1, 4, 1000, 38, 0, first, active));     // no frame
    TEST_ASSERT_FALSE(stvVdsLines(1, 4, 60, 38, 1000, first, active));    // 22 lines: not a picture
}

// Other VGA modes, from the VESA DMT: 800x600 60 Hz (1056 pixels, sync 128 + back porch 88, 4 + 23 lines), and 720x400
// 70 Hz (DOS text). The Dreamcast (DreamHAL timings) puts its 800x600 picture at 146 of 713 clocks = 216 of 1056
static void checkWindow(uint16_t lines, uint32_t fh, uint16_t htotal, uint16_t start, uint16_t width) {
    StvWindow w;
    TEST_ASSERT_TRUE(stvVgaPassWindow(lines, fh, 0, w));
    TEST_ASSERT_EQUAL_UINT16(htotal, w.htotal);
    TEST_ASSERT_EQUAL_UINT16(start + (htotal * 26 + 500) / 1000, w.act0 - w.hsSp);   // + the pass-through's delay
    TEST_ASSERT_EQUAL_UINT16(width, w.act1 - w.act0);
}

void test_vga_pass_other_modes(void) {
    checkWindow(628, 37879, 1056, 216, 800);
    checkWindow(449, 31469, 900, 162, 720);
    checkWindow(806, 48363, 1344, 296, 1024);   // 1024x768 60 Hz
    checkWindow(806, 56476, 1328, 280, 1024);   // 1024x768 70 Hz: same lines, another rate
    StvVgaMode m;
    TEST_ASSERT_TRUE(stvVgaPassMode(628, 37879, 0, m));
    TEST_ASSERT_EQUAL_UINT16(27, m.vstart);
    TEST_ASSERT_EQUAL_UINT16(600, m.vactive);
    TEST_ASSERT_FALSE(stvVgaPassMode(600, 36000, 0, m));      // not a standard mode: fixed place
    TEST_ASSERT_FALSE(stvVgaPassMode(1125, 67500, 0, m));     // 1080p: not in the table
    StvWindow w;
    TEST_ASSERT_FALSE(stvVgaPassWindow(600, 36000, 0, w));
    TEST_ASSERT_FALSE(w.valid);
}

// ... and the panel is then centred in the picture (and follows Left / Right) as on a scaled output
void test_bypass_panel_is_centred_in_the_picture(void) {
    struct { uint16_t lines; uint32_t fh; } k[] = {{749, 44895}, {750, 37500}, {562, 33750}, {562, 28125}, {1125, 67500}};
    for (auto& m : k) {
        StvWindow w;
        TEST_ASSERT_TRUE(stvBypassWindow(m.lines, m.fh, w));
        uint8_t ld = stvTimingForRate(OUT_BYPASS, m.fh, STV_XTAL_HZ).ld;
        int px = 12 * (ld + 1);                                          // STV pixels a line
        int act0 = (w.act0 - w.hsSp) * px / w.htotal, act1 = (w.act1 - w.hsSp) * px / w.htotal;
        int left = (stvDdForWindow(ld, w, STV_ALIGN_CENTER, 0) + 8) * 3, right = left + 30 * 12;
        TEST_ASSERT_TRUE_MESSAGE(left >= act0 && right <= act1, "the panel leaves the picture");
        TEST_ASSERT_INT_WITHIN_MESSAGE(8, left - act0, act1 - right, "not centred");
        int l = (stvDdForWindow(ld, w, STV_ALIGN_LEFT, 0) + 8) * 3, r = (stvDdForWindow(ld, w, STV_ALIGN_RIGHT, 0) + 8) * 3;
        TEST_ASSERT_TRUE(l <= left && left <= r);
    }
    StvWindow w;
    stvBypassWindow(749, 44895, w);                                      // the photo: DD 38 was far left of centre
    TEST_ASSERT_TRUE(stvDdForWindow(63, w, STV_ALIGN_CENTER, 0) > 38 + 10);
}

// The line rate of a bypass is asked again while it is unknown, a few times
void test_bypass_rate_is_asked_again(void) {
    StvRateRetry r;
    TEST_ASSERT_FALSE(r.due(1000));                       // nothing to ask
    r.start(1000);
    TEST_ASSERT_FALSE(r.due(1000 + StvRateRetry::kEveryMs - 1));
    TEST_ASSERT_TRUE(r.due(1000 + StvRateRetry::kEveryMs));
    TEST_ASSERT_FALSE(r.due(1001 + StvRateRetry::kEveryMs));
    uint32_t t = 1000 + StvRateRetry::kEveryMs;
    for (uint8_t i = 1; i < StvRateRetry::kTries; i++) { t += StvRateRetry::kEveryMs; TEST_ASSERT_TRUE(r.due(t)); }
    TEST_ASSERT_FALSE(r.due(t + 10 * StvRateRetry::kEveryMs));   // given up until the next output change
    r.start(t);
    r.stop();                                             // the rate came
    TEST_ASSERT_FALSE(r.due(t + StvRateRetry::kEveryMs));
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
    RUN_TEST(test_bypass_rows_follow_the_visible_lines);
    RUN_TEST(test_bypass_window_of_hd_formats);
    RUN_TEST(test_bypass_panel_is_centred_in_the_picture);
    RUN_TEST(test_bypass_rate_is_asked_again);
    RUN_TEST(test_line_rate_from_if_period);
    RUN_TEST(test_vga_pass_window_of_640x480);
    RUN_TEST(test_vga_pass_dreamcast_640x480_by_its_sync);
    RUN_TEST(test_vga_pass_modes_are_found_and_unambiguous);
    RUN_TEST(test_vga_pass_other_modes);
    RUN_TEST(test_source_watch_needs_two_agreeing_readings);
    RUN_TEST(test_vds_lines_from_the_sync_end);
    RUN_TEST(test_menu_fits_the_line_for_ld_35_to_63);
    RUN_TEST(test_dd_centres_the_panel_on_the_active_window);
    RUN_TEST(test_dd_left_and_right_keep_a_margin);
    RUN_TEST(test_dd_narrow_window_keeps_left_centre_right_order);
    RUN_TEST(test_dd_without_a_window_is_the_legacy_edge);
    RUN_TEST(test_dd_stays_inside_the_line);
    return UNITY_END();
}
