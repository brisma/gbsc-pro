// Picture geometry: width, display clock and vertical steps on the scaler's output registers
#include <unity.h>
#include <stdio.h>
#include "geometry.h"

void setUp(void) {}
void tearDown(void) {}

// 1080p from a 60 Hz source, as read from a running unit
static GeoRegs start1080p60() {
    GeoRegs r = {};
    r.hTotal = 1614; r.hsSt = 20; r.hsSp = 72;
    r.hbSt = 1304; r.hbSp = 270; r.disHbSt = 1362; r.disHbSp = 354;
    r.hScale = 1023; r.hScaleByps = true;
    r.vTotal = 1125; r.vScale = 455; r.vbSp = 30; r.disVbSt = 1125; r.disVbSp = 40;
    r.clock = 0x85;
    return r;
}

static bool sameRegs(const GeoRegs& a, const GeoRegs& b) {   // field by field: the struct has padding
    return a.hTotal == b.hTotal && a.hsSt == b.hsSt && a.hsSp == b.hsSp && a.hbSt == b.hbSt && a.hbSp == b.hbSp &&
           a.disHbSt == b.disHbSt && a.disHbSp == b.disHbSp && a.hScale == b.hScale && a.hScaleByps == b.hScaleByps &&
           a.vTotal == b.vTotal && a.vScale == b.vScale && a.vbSp == b.vbSp && a.disVbSt == b.disVbSt &&
           a.disVbSp == b.disVbSp && a.ifVbSt == b.ifVbSt && a.ifVbSp == b.ifVbSp && a.clock == b.clock &&
           a.mark == b.mark && a.bar == b.bar && a.clamps == b.clamps;
}

// The memory blank is a pair of positions on a line that wraps (Screen > Position moves both and wraps them at the
// line total): pixels are read from hbSp forward to hbSt
static uint16_t readLength(const GeoRegs& r) { return (uint16_t)((r.hbSt + r.hTotal - r.hbSp) % r.hTotal); }
static void positionLeft(GeoRegs& r, uint16_t clocks) {   // as Screen > Position does
    r.hbSt = (uint16_t)((r.hbSt + r.hTotal - clocks) % r.hTotal);
    r.hbSp = (uint16_t)((r.hbSp + r.hTotal - clocks) % r.hTotal);
}

void test_clock_step_keeps_the_picture(void) {
    GeoRegs r = start1080p60();
    TEST_ASSERT_TRUE(geoClockStep(r));
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.clock);
    TEST_ASSERT_EQUAL_UINT16(2421, r.hTotal);
    TEST_ASSERT_EQUAL_UINT16(30, r.hsSt);
    TEST_ASSERT_EQUAL_UINT16(108, r.hsSp);
    TEST_ASSERT_EQUAL_UINT16(531, r.disHbSp);
    TEST_ASSERT_EQUAL_UINT16(2043, r.disHbSt);
    TEST_ASSERT_EQUAL_UINT16(433, r.hbSp);
    TEST_ASSERT_EQUAL_UINT16(1984, r.hbSt);
    TEST_ASSERT_EQUAL_UINT16(682, r.hScale);
    TEST_ASSERT_FALSE(r.hScaleByps);
}

void test_clock_step_from_81(void) {   // 81 -> 108 MHz: x4/3
    GeoRegs r = start1080p60();
    r.clock = 0x65;
    r.hTotal = 1446;
    TEST_ASSERT_TRUE(geoClockStep(r));
    TEST_ASSERT_EQUAL_HEX8(0x85, r.clock);
    TEST_ASSERT_EQUAL_UINT16(1928, r.hTotal);
    TEST_ASSERT_EQUAL_UINT16(767, r.hScale);
}

void test_no_step_from_162_or_unknown(void) {
    GeoRegs r = start1080p60();
    r.clock = 0xA5;
    GeoRegs k = r;
    TEST_ASSERT_FALSE(geoClockStep(r));
    TEST_ASSERT_TRUE(sameRegs(k, r));
    r.clock = 0x45;
    k = r;
    TEST_ASSERT_FALSE(geoClockStep(r));
    TEST_ASSERT_TRUE(sameRegs(k, r));
}

void test_narrower_at_1080p_steps_the_clock(void) {   // what Integer scale asks for there: x8/9
    GeoRegs r = start1080p60();
    TEST_ASSERT_TRUE(geoWidth(r, 8.0f / 9.0f));
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.clock);
    TEST_ASSERT_UINT16_WITHIN(1, 767, r.hScale);
    TEST_ASSERT_UINT16_WITHIN(1, 615, r.disHbSp);
    TEST_ASSERT_UINT16_WITHIN(1, 1959, r.disHbSt);
    TEST_ASSERT_UINT16_WITHIN(1, 522, r.hbSp);
    TEST_ASSERT_UINT16_WITHIN(1, 1900, r.hbSt);
}

void test_force43_at_960p(void) {   // the 960p 60 Hz table: window 612..2594, memory blank 480..2512, scale 512, line 2704
    GeoRegs r = start1080p60();
    r.clock = 0xA5; r.hTotal = 2704; r.hScale = 512; r.hScaleByps = false;
    r.disHbSp = 612; r.disHbSt = 2594; r.hbSp = 480; r.hbSt = 2512;
    TEST_ASSERT_TRUE(geoWidth(r, 0.75f));
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.clock);
    TEST_ASSERT_UINT16_WITHIN(1, 683, r.hScale);
    TEST_ASSERT_UINT16_WITHIN(1, 860, r.disHbSp);
    TEST_ASSERT_UINT16_WITHIN(1, 2346, r.disHbSt);
    TEST_ASSERT_UINT16_WITHIN(1, 747, r.hbSp);
    TEST_ASSERT_UINT16_WITHIN(1, 2271, r.hbSt);
}

void test_wider_then_back(void) {
    GeoRegs r = start1080p60(), k = r;
    TEST_ASSERT_TRUE(geoWidth(r, 1.4f));
    TEST_ASSERT_UINT16_WITHIN(1, 731, r.hScale);
    TEST_ASSERT_FALSE(r.hScaleByps);
    TEST_ASSERT_TRUE(r.hbSt <= r.disHbSt);
    TEST_ASSERT_TRUE(geoWidth(r, 1.0f / 1.4f));
    TEST_ASSERT_UINT16_WITHIN(1, k.disHbSp, r.disHbSp);
    TEST_ASSERT_UINT16_WITHIN(1, k.disHbSt, r.disHbSt);
    TEST_ASSERT_UINT16_WITHIN(1, k.hbSp, r.hbSp);
    TEST_ASSERT_UINT16_WITHIN(1, k.hbSt, r.hbSt);
    TEST_ASSERT_UINT16_WITHIN(1, 1023, r.hScale);
}

void test_factor_one_changes_nothing(void) {
    GeoRegs r = start1080p60(), k = r;
    TEST_ASSERT_TRUE(geoWidth(r, 1.0f));
    TEST_ASSERT_TRUE(sameRegs(k, r));   // the scaler's bypass stays as it was
}

void test_unreachable_width_changes_nothing(void) {
    GeoRegs r = start1080p60();
    r.clock = 0xA5;   // scale 1023 at 162 MHz: no room left
    GeoRegs k = r;
    TEST_ASSERT_FALSE(geoWidth(r, 0.5f));
    TEST_ASSERT_TRUE(sameRegs(k, r));
}

void test_a_read_longer_than_the_line_is_held(void) {
    GeoRegs r = start1080p60();
    r.hbSp = 10;   // 1294 clocks read: x1.4 would be 1812 of a 1614 line
    TEST_ASSERT_TRUE(geoWidth(r, 1.4f));
    TEST_ASSERT_TRUE(readLength(r) > 1500 && readLength(r) < r.hTotal);
    TEST_ASSERT_TRUE(r.hbSt <= r.disHbSt);
    TEST_ASSERT_TRUE(r.disHbSp < r.disHbSt && r.disHbSt < r.hTotal);
    TEST_ASSERT_TRUE(r.clamps > 0);
}

void test_memory_blank_may_start_before_the_line(void) {   // 1200p Full: the read starts 13 clocks before 0
    GeoRegs r = {};
    r.hTotal = 2188; r.hsSt = 16; r.hsSp = 50; r.hbSt = 1897; r.hbSp = 170; r.disHbSt = 1985; r.disHbSp = 302;
    r.hScale = 602; r.vTotal = 1235; r.vScale = 410; r.vbSp = 23; r.disVbSt = 1233; r.disVbSp = 33; r.clock = 0xA5;
    TEST_ASSERT_TRUE(geoWidth(r, 1.2f));
    TEST_ASSERT_UINT16_WITHIN(2, 2072, readLength(r));    // 1727 x 1.2: nothing cut
    TEST_ASSERT_TRUE(r.hbSp > 2100);                      // wrapped, as Screen > Position leaves it
    TEST_ASSERT_EQUAL_UINT8(0, r.clamps);
}

void test_position_moved_picture_keeps_its_read(void) {   // moved left past the line start, then another width
    GeoRegs r = start1080p60();
    r.hbSp = 20; r.hbSt = 1054;                           // 1034 clocks read, as the table, 250 to the left
    positionLeft(r, 36);                                  // hbSp wraps to 1598
    TEST_ASSERT_TRUE(r.hbSp > r.hbSt);
    const float pixels = 1034.0f * 1023.0f;               // the read in source pixels (x 1023)
    TEST_ASSERT_TRUE(geoWidth(r, 0.75f));                 // narrower at 1080p: the clock steps to 162 MHz
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.clock);
    TEST_ASSERT_UINT16_WITHIN(2, 1163, readLength(r));    // 1034 x 1.5 x 0.75, not a one-pixel read
    TEST_ASSERT_FLOAT_WITHIN(pixels * 0.004f, pixels, (float)readLength(r) * r.hScale);
    TEST_ASSERT_TRUE(geoWidth(r, 1.0f / 0.75f));
    TEST_ASSERT_UINT16_WITHIN(2, 1551, readLength(r));
    TEST_ASSERT_FLOAT_WITHIN(pixels * 0.004f, pixels, (float)readLength(r) * r.hScale);
    TEST_ASSERT_UINT16_WITHIN(2, 4, r.hbSp);              // 16 clocks before the line at 108 MHz, where it was
}

void test_clock_step_with_a_wrapped_memory_blank(void) {
    GeoRegs r = start1080p60();
    r.hbSp = 1598; r.hbSt = 1018;                         // 1034 clocks read across the line end
    TEST_ASSERT_TRUE(geoClockStep(r));
    TEST_ASSERT_UINT16_WITHIN(2, 1551, readLength(r));    // x1.5
    TEST_ASSERT_TRUE(r.hbSp < r.hTotal && r.hbSt < r.hTotal);
}

void test_line_total_stays_in_its_register(void) {       // 12 bits: a second clock step would need 4330
    GeoRegs r = {};
    r.hTotal = 2165; r.hsSt = 24; r.hsSp = 128; r.hbSt = 1772; r.hbSp = 464; r.disHbSt = 1772; r.disHbSp = 548;
    r.hScale = 1023; r.vTotal = 750; r.vScale = 819; r.disVbSt = 748; r.disVbSp = 26; r.clock = 0x65;
    GeoRegs k = r;
    TEST_ASSERT_FALSE(geoWidth(r, 0.5f));
    TEST_ASSERT_TRUE(sameRegs(k, r));
    r.clock = 0x85; r.hTotal = 2887; k = r;
    TEST_ASSERT_FALSE(geoClockStep(r));
    TEST_ASSERT_TRUE(sameRegs(k, r));
}

// ---- States: Aspect, Integer scale, 50Hz style ----

static GeoState st(uint8_t aspect, bool integer, uint8_t style50) {
    GeoState s = {aspect, integer, style50};
    return s;
}

// The geometry registers of every table the step handles, as the preset headers hold them. rawStyle: the 50Hz style
// the untouched table is (the 60 Hz tables and 576p have one picture only)
struct TableRegs { uint8_t id; const char* name; uint8_t rawStyle; GeoRegs regs; };
static const TableRegs kTables[] = {
    {0x01, "960p 60", GEO_PAL_TV, {2704, 8, 160, 2512, 480, 2594, 612, 512, false, 1000, 512, 26, 1000, 38, 6, 8, 0xA5, 0, 0, 0}},
    {0x02, "1024p 60", GEO_PAL_TV, {2535, 16, 144, 2408, 384, 2496, 516, 512, false, 1066, 485, 30, 1066, 40, 4, 6, 0xA5, 0, 0, 0}},
    {0x03, "720p 60", GEO_PAL_TV, {2402, 16, 144, 2040, 458, 2040, 550, 683, false, 750, 683, 16, 750, 26, 4, 6, 0x85, 0, 0, 0}},
    {0x04, "480p", GEO_PAL_TV, {2573, 180, 12, 2404, 396, 2490, 510, 512, false, 525, 1023, 24, 520, 32, 4, 6, 0x65, 0, 0, 0}},
    {0x05, "1080p 60", GEO_PAL_TV, {1601, 20, 72, 1298, 264, 1356, 348, 1023, true, 1125, 455, 30, 1125, 40, 8, 10, 0x85, 0, 0, 0}},
    {0x07, "1200p 60", GEO_PAL_TV, {2188, 16, 50, 1897, 170, 1985, 302, 602, false, 1235, 410, 23, 1233, 33, 4, 6, 0xA5, 0, 0, 0}},
    {0x11, "960p 50", GEO_FULL_HEIGHT, {2166, 24, 152, 2004, 356, 2074, 494, 607, false, 1000, 585, 16, 996, 19, 40, 42, 0x85, 0, 0, 0}},
    {0x12, "1024p 50", GEO_FULL_HEIGHT, {2032, 0, 132, 1944, 222, 2004, 360, 585, false, 1066, 512, 20, 1064, 23, 44, 46, 0x85, 0, 0, 0}},
    {0x13, "720p 50", GEO_FULL_HEIGHT, {2165, 24, 128, 1772, 464, 1772, 548, 835, false, 750, 819, 16, 748, 26, 30, 32, 0x65, 0, 0, 0}},
    {0x14, "576p", GEO_PAL_TV, {2599, 196, 28, 2400, 400, 2492, 564, 512, false, 625, 1023, 4, 590, 12, 40, 42, 0x65, 0, 0, 0}},
    {0x15, "1080p 50", GEO_PAL_TV, {1444, 8, 56, 1204, 170, 1262, 256, 1023, true, 1125, 512, 16, 1125, 28, 38, 40, 0x65, 0, 0, 0}},
    {0x17, "1200p 50", GEO_FULL_HEIGHT, {1749, 0, 27, 1514, 90, 1574, 228, 714, false, 1235, 437, 30, 1233, 33, 44, 46, 0x85, 0, 0, 0}},
};
static GeoRegs table(uint8_t id) {
    for (const TableRegs& t : kTables)
        if (t.id == id) return t.regs;
    TEST_FAIL_MESSAGE("no such table");
    return GeoRegs();
}

void test_integer_at_1080p_60(void) {   // the picture a TV showed clean: x4, 960 lines, the width x8/9 on 162 MHz
    GeoRegs r = start1080p60();
    GeoResult g = geometryApply(r, 0x05, st(GEO_AUTO, true, GEO_PAL_TV));
    TEST_ASSERT_TRUE(g.acted && g.changed && !g.failed);
    TEST_ASSERT_EQUAL_HEX8(0x85, g.clockFrom);
    TEST_ASSERT_EQUAL_HEX8(0xA5, g.clockTo);
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.clock);
    TEST_ASSERT_EQUAL_UINT16(2421, r.hTotal);
    TEST_ASSERT_EQUAL_UINT16(512, r.vScale);
    TEST_ASSERT_EQUAL_UINT16(100, r.disVbSp);
    TEST_ASSERT_EQUAL_UINT16(1065, r.disVbSt);
    TEST_ASSERT_EQUAL_UINT16(90, r.vbSp);
    TEST_ASSERT_EQUAL_UINT8(60, r.bar);
    TEST_ASSERT_UINT16_WITHIN(1, 767, r.hScale);
    TEST_ASSERT_UINT16_WITHIN(1, 615, r.disHbSp);
    TEST_ASSERT_UINT16_WITHIN(1, 1959, r.disHbSt);
    TEST_ASSERT_UINT16_WITHIN(1, 522, r.hbSp);
    TEST_ASSERT_UINT16_WITHIN(1, 1900, r.hbSt);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.mark);
}

void test_integer_off_again(void) {
    GeoRegs r = start1080p60();
    geometryApply(r, 0x05, st(GEO_AUTO, true, GEO_PAL_TV));
    GeoResult g = geometryApply(r, 0x05, st(GEO_AUTO, false, GEO_PAL_TV));
    TEST_ASSERT_TRUE(g.changed);
    TEST_ASSERT_EQUAL_UINT16(455, r.vScale);
    TEST_ASSERT_EQUAL_UINT16(40, r.disVbSp);
    TEST_ASSERT_EQUAL_UINT16(1125, r.disVbSt);
    TEST_ASSERT_EQUAL_UINT16(30, r.vbSp);
    TEST_ASSERT_EQUAL_UINT8(0, r.bar);
    TEST_ASSERT_EQUAL_HEX8(0xA5, r.clock);   // a stepped clock stays: the same picture
    TEST_ASSERT_UINT16_WITHIN(1, 682, r.hScale);
    TEST_ASSERT_UINT16_WITHIN(2, 531, r.disHbSp);
    TEST_ASSERT_UINT16_WITHIN(2, 2043, r.disHbSt);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.mark);
}

void test_same_state_twice_changes_nothing(void) {
    GeoRegs r = start1080p60();
    geometryApply(r, 0x05, st(GEO_FULL, true, GEO_PAL_TV));
    GeoRegs k = r;
    GeoResult g = geometryApply(r, 0x05, st(GEO_FULL, true, GEO_PAL_TV));
    TEST_ASSERT_TRUE(g.acted);
    TEST_ASSERT_FALSE(g.changed);
    TEST_ASSERT_TRUE(sameRegs(k, r));
}

void test_hand_made_vertical_scale_survives_integer(void) {
    GeoRegs r = start1080p60();
    r.vScale = 470;   // the user's Size
    geometryApply(r, 0x05, st(GEO_AUTO, true, GEO_PAL_TV));
    TEST_ASSERT_TRUE(r.vScale > 512);
    geometryApply(r, 0x05, st(GEO_AUTO, false, GEO_PAL_TV));
    TEST_ASSERT_UINT16_WITHIN(1, 470, r.vScale);
    TEST_ASSERT_EQUAL_UINT16(40, r.disVbSp);
}

void test_full_height_at_1080p_50(void) {   // upstream's picture there: scale 455, the window from line 42, IF_VB + 22
    GeoRegs r = table(0x15);
    GeoResult g = geometryApply(r, 0x15, st(GEO_AUTO, false, GEO_FULL_HEIGHT));
    TEST_ASSERT_TRUE(g.changed);
    TEST_ASSERT_EQUAL_UINT16(455, r.vScale);
    TEST_ASSERT_EQUAL_UINT16(42, r.disVbSp);
    TEST_ASSERT_EQUAL_UINT16(1125, r.disVbSt);
    TEST_ASSERT_EQUAL_UINT16(60, r.ifVbSt);
    TEST_ASSERT_EQUAL_UINT16(62, r.ifVbSp);
    TEST_ASSERT_EQUAL_UINT16(16, r.vbSp);
    TEST_ASSERT_EQUAL_UINT16(1023, r.hScale);   // the width is not touched
    TEST_ASSERT_TRUE(r.hScaleByps);
    TEST_ASSERT_EQUAL_HEX8(0x65, r.clock);
    TEST_ASSERT_EQUAL_HEX8(0x10, r.mark);       // (Full height + 1) << 3
}

void test_pal_tv_at_1080p_50_is_the_table(void) {
    GeoRegs r = table(0x15), k = r;
    GeoResult g = geometryApply(r, 0x15, st(GEO_AUTO, false, GEO_PAL_TV));
    TEST_ASSERT_TRUE(g.acted);
    k.mark = 0x08;   // only the mark is written
    TEST_ASSERT_TRUE(sameRegs(k, r));
}

void test_as_60hz_at_1080p_50_steps_the_clock(void) {
    GeoRegs r = table(0x15);
    geometryApply(r, 0x15, st(GEO_AUTO, false, GEO_AS_60HZ));
    TEST_ASSERT_EQUAL_UINT16(455, r.vScale);
    TEST_ASSERT_EQUAL_UINT16(42, r.disVbSp);
    TEST_ASSERT_EQUAL_HEX8(0x85, r.clock);
    TEST_ASSERT_UINT16_WITHIN(1, 843, r.hScale);   // 1023 / (4/3) / 0.910
    TEST_ASSERT_EQUAL_HEX8(0x18, r.mark);
}

void test_pal_tv_at_960p_50_shows_the_whole_frame(void) {   // x3.5 cropped -> 960 lines, the width as the table's
    GeoRegs r = table(0x11);
    geometryApply(r, 0x11, st(GEO_AUTO, false, GEO_PAL_TV));
    TEST_ASSERT_EQUAL_UINT16(614, r.vScale);
    TEST_ASSERT_EQUAL_UINT16(607, r.hScale);
    TEST_ASSERT_EQUAL_UINT8(8, r.bar);
    TEST_ASSERT_EQUAL_UINT16(27, r.disVbSp);
    TEST_ASSERT_EQUAL_UINT16(988, r.disVbSt);
}

void test_full_height_with_integer_is_pal_tv(void) {
    GeoState n = geoNormalise(0x15, st(GEO_AUTO, true, GEO_FULL_HEIGHT));
    TEST_ASSERT_EQUAL_UINT8(GEO_PAL_TV, n.style50);
    TEST_ASSERT_TRUE(n.integer);
    TEST_ASSERT_EQUAL_HEX8(0x0C, geoMark(0x15, n));
}

void test_style_only_on_50hz_tables(void) {   // a 50 Hz source sent at 60 Hz runs a 60 Hz table
    GeoRegs r = start1080p60(), k = r;
    geometryApply(r, 0x05, st(GEO_AUTO, false, GEO_AS_60HZ));
    TEST_ASSERT_TRUE(sameRegs(k, r));          // nothing, the mark stays 0
    GeoTarget t;
    TEST_ASSERT_TRUE(geoTarget(0x01, st(GEO_AUTO, true, GEO_PAL_TV), t));   // 960p is x4 already
    TEST_ASSERT_EQUAL_UINT16(0, t.vert.vScale);
    TEST_ASSERT_EQUAL_UINT16(1000, t.widthPm);
}

void test_full_ignores_style_and_integer_width(void) {
    GeoTarget a, b;
    TEST_ASSERT_TRUE(geoTarget(0x13, st(GEO_FULL, false, GEO_PAL_TV), a));
    TEST_ASSERT_TRUE(geoTarget(0x13, st(GEO_FULL, true, GEO_AS_60HZ), b));
    TEST_ASSERT_EQUAL_UINT16(1333, a.widthPm);
    TEST_ASSERT_EQUAL_UINT16(1333, b.widthPm);
    TEST_ASSERT_EQUAL_UINT16(683, b.vert.vScale);   // the height still follows style and Integer
}

void test_force43_multiplies(void) {
    GeoTarget t;
    TEST_ASSERT_TRUE(geoTarget(0x12, st(GEO_FORCE43, false, GEO_PAL_TV), t));
    TEST_ASSERT_UINT16_WITHIN(1, 724, t.widthPm);   // 965 x 0.75
    TEST_ASSERT_TRUE(geoTarget(0x05, st(GEO_FORCE43, false, GEO_PAL_TV), t));
    TEST_ASSERT_EQUAL_UINT16(1000, t.widthPm);      // a 16:9 frame holds the 4:3 picture already
}

void test_unknown_tables_untouched(void) {
    const uint8_t ids[] = {0x00, 0x06, 0x16, 0x21, 0x22};
    for (uint8_t id : ids) {
        GeoRegs r = start1080p60(), k = r;
        GeoResult g = geometryApply(r, id, st(GEO_FULL, true, GEO_AS_60HZ));
        TEST_ASSERT_FALSE(g.acted);
        TEST_ASSERT_TRUE(sameRegs(k, r));
    }
}

void test_failed_step_keeps_mark(void) {
    GeoRegs r = start1080p60();
    r.disHbSp = 1400; r.disHbSt = 1362;   // a window the width step refuses
    GeoRegs k = r;
    GeoResult g = geometryApply(r, 0x05, st(GEO_FULL, true, GEO_PAL_TV));
    TEST_ASSERT_TRUE(g.acted && g.failed);
    TEST_ASSERT_FALSE(g.changed);
    TEST_ASSERT_TRUE(sameRegs(k, r));     // the vertical step is not left half done either
}

void test_user_window_is_scaled_about_its_centre(void) {
    GeoRegs r = start1080p60();
    r.disHbSp = 400; r.disHbSt = 1300; r.hbSp = 316; r.hbSt = 1242;   // moved and narrowed by hand
    geometryApply(r, 0x05, st(GEO_FULL, false, GEO_PAL_TV));
    TEST_ASSERT_UINT16_WITHIN(2, 850, (uint16_t)((r.disHbSp + r.disHbSt) / 2));
    TEST_ASSERT_UINT16_WITHIN(2, 1260, (uint16_t)(r.disHbSt - r.disHbSp));   // 900 x 1.4
}

void test_saved_state_to_another(void) {   // registers saved on Force 4:3 loaded with Aspect = Full: one factor of 4/3
    GeoRegs r = table(0x01);
    geometryApply(r, 0x01, st(GEO_FORCE43, false, GEO_PAL_TV));
    TEST_ASSERT_UINT16_WITHIN(1, 683, r.hScale);
    geometryApply(r, 0x01, st(GEO_FULL, false, GEO_PAL_TV));
    TEST_ASSERT_UINT16_WITHIN(1, 512, r.hScale);
    TEST_ASSERT_UINT16_WITHIN(2, 612, r.disHbSp);
    TEST_ASSERT_UINT16_WITHIN(2, 2594, r.disHbSt);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.mark);
}

void test_every_state_there_and_back(void) {
    char where[48];
    for (const TableRegs& t : kTables)
        for (uint8_t a = 0; a < 3; a++)
            for (uint8_t i = 0; i < 2; i++)
                for (uint8_t s = 0; s < 3; s++) {
                    snprintf(where, sizeof(where), "%s aspect %u integer %u style %u", t.name, a, i, s);
                    GeoRegs r = t.regs;
                    GeoResult g = geometryApply(r, t.id, st(a, i != 0, s));
                    TEST_ASSERT_TRUE_MESSAGE(g.acted && !g.failed, where);
                    TEST_ASSERT_TRUE_MESSAGE(r.disHbSp < r.disHbSt && r.disHbSt < r.hTotal, where);
                    TEST_ASSERT_TRUE_MESSAGE(r.hbSp < r.hTotal && r.hbSt <= r.disHbSt && readLength(r) > 0, where);
                    TEST_ASSERT_TRUE_MESSAGE(r.hScale >= 256 && r.hScale <= 1023 && r.vScale >= 256 && r.vScale <= 1023, where);
                    TEST_ASSERT_TRUE_MESSAGE(r.disVbSp < r.disVbSt && r.disVbSt <= r.vTotal, where);
                    geometryApply(r, t.id, st(GEO_AUTO, false, t.rawStyle));   // back to the picture the table is
                    TEST_ASSERT_UINT16_WITHIN_MESSAGE(1, t.regs.vScale, r.vScale, where);
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(t.regs.disVbSp, r.disVbSp, where);
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(t.regs.disVbSt, r.disVbSt, where);
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(t.regs.vbSp, r.vbSp, where);
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(t.regs.ifVbSp, r.ifVbSp, where);
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(t.regs.ifVbSt, r.ifVbSt, where);
                    // the width in source pixels, whatever the clock ended on
                    float before = (float)(t.regs.disHbSt - t.regs.disHbSp) * t.regs.hScale;
                    float after = (float)(r.disHbSt - r.disHbSp) * r.hScale;
                    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(before * 0.006f, before, after, where);
                    float readBefore = (float)readLength(t.regs) * t.regs.hScale, readAfter = (float)readLength(r) * r.hScale;
                    // the read comes back too. One known trim: where the table reads up to the window's end (720p) a
                    // wider picture's read is held at the window's end, and comes back the fixed delay's share shorter
                    TEST_ASSERT_TRUE_MESSAGE(readAfter <= readBefore * 1.006f && readAfter >= readBefore * 0.985f, where);
                }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clock_step_keeps_the_picture);
    RUN_TEST(test_clock_step_from_81);
    RUN_TEST(test_no_step_from_162_or_unknown);
    RUN_TEST(test_narrower_at_1080p_steps_the_clock);
    RUN_TEST(test_force43_at_960p);
    RUN_TEST(test_wider_then_back);
    RUN_TEST(test_factor_one_changes_nothing);
    RUN_TEST(test_unreachable_width_changes_nothing);
    RUN_TEST(test_a_read_longer_than_the_line_is_held);
    RUN_TEST(test_memory_blank_may_start_before_the_line);
    RUN_TEST(test_position_moved_picture_keeps_its_read);
    RUN_TEST(test_clock_step_with_a_wrapped_memory_blank);
    RUN_TEST(test_line_total_stays_in_its_register);
    RUN_TEST(test_integer_at_1080p_60);
    RUN_TEST(test_integer_off_again);
    RUN_TEST(test_same_state_twice_changes_nothing);
    RUN_TEST(test_hand_made_vertical_scale_survives_integer);
    RUN_TEST(test_full_height_at_1080p_50);
    RUN_TEST(test_pal_tv_at_1080p_50_is_the_table);
    RUN_TEST(test_as_60hz_at_1080p_50_steps_the_clock);
    RUN_TEST(test_pal_tv_at_960p_50_shows_the_whole_frame);
    RUN_TEST(test_full_height_with_integer_is_pal_tv);
    RUN_TEST(test_style_only_on_50hz_tables);
    RUN_TEST(test_full_ignores_style_and_integer_width);
    RUN_TEST(test_force43_multiplies);
    RUN_TEST(test_unknown_tables_untouched);
    RUN_TEST(test_failed_step_keeps_mark);
    RUN_TEST(test_user_window_is_scaled_about_its_centre);
    RUN_TEST(test_saved_state_to_another);
    RUN_TEST(test_every_state_there_and_back);
    return UNITY_END();
}
