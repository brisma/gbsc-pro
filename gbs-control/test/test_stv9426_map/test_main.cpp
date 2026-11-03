#include <unity.h>
#include "stv9426-map.h"
using namespace stv;
void setUp(void) {}
void tearDown(void) {}

// List scrollbar: no arrow strips; table on page 0 inside character
// number 0's bytes, 11 dynamic UDC slots from character number 1, then the 9 text rows ending at 0x3FF
void test_map_regions_do_not_overlap(void) {
    TEST_ASSERT_EQUAL_HEX16(0x000, kPageAddr);
    TEST_ASSERT_EQUAL_HEX16(kPageAddr, kPageP * 64);
    TEST_ASSERT_TRUE(kPageAddr + kDescSlots * 2 <= kUdcBytes * kUdcFirst);    // table ends before the first UDC
    TEST_ASSERT_EQUAL_UINT8(11, kUdcSlots);
    TEST_ASSERT_TRUE(kUdcBytes * (kUdcFirst + kUdcSlots) <= kTextAddr);     // slots end before the text
    TEST_ASSERT_EQUAL_HEX16(0x1D2, kTextAddr);
    TEST_ASSERT_EQUAL_HEX16(0x400, rowAddr(kRows - 1) + kRowCells * 2);     // the last row ends the RAM
    TEST_ASSERT_TRUE(kDescSlots - kMaxContent >= 4);                        // >= 1020 filler lines after any layout
}

void test_descriptor_encoding_matches_datasheet(void) {
    TEST_ASSERT_EQUAL_HEX16(0x003A, spacingDesc(58));
    TEST_ASSERT_EQUAL_HEX16(0xD000, charDesc(0x000, true));    // today's row 1 descriptor (bytes 00 D0)
    TEST_ASSERT_EQUAL_HEX16(0xC1C0, charDesc(0x1C0, false));
    TEST_ASSERT_EQUAL_HEX16(0xD3EE, charDesc(0x3EE, true));
    TEST_ASSERT_EQUAL_HEX16(0xC1C0, charDesc(0x1C1, false));   // odd address rounded down to even
}

void test_build_compat_layout_bytes(void) {
    uint8_t d[kDescSlots * 2];
    TEST_ASSERT_TRUE(buildDescriptors(compatLayout(), d));
    const uint8_t expect[8] = {0x3A, 0x00, 0xD2, 0xD1, 0x10, 0xD2, 0x4E, 0xD2};   // spacing 58, rows 0x1D2/0x210/0x24E zoomed
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, d, 8);
}

void test_layout_fillers_cover_1080p_frame(void) {
    uint8_t d[kDescSlots * 2];
    Layout l = compatLayout();
    TEST_ASSERT_TRUE(buildDescriptors(l, d));
    unsigned lines = 0;
    for (uint8_t i = l.count; i < kDescSlots; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, d[2 * i]);       // SL = 255
        TEST_ASSERT_EQUAL_HEX8(0x00, d[2 * i + 1]);   // spacing strip
        lines += 255;
    }
    TEST_ASSERT_TRUE(lines >= 1125);
}

void test_build_rejects_bad_layouts(void) {
    uint8_t d[kDescSlots * 2];
    Layout l; l.count = 16;
    TEST_ASSERT_FALSE(buildDescriptors(l, d));
    l.count = 1; l.strips[0] = {STRIP_ROW, 9, false};
    TEST_ASSERT_FALSE(buildDescriptors(l, d));
    l.strips[0] = {STRIP_SPACE, 0, false};
    TEST_ASSERT_FALSE(buildDescriptors(l, d));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_map_regions_do_not_overlap);
    RUN_TEST(test_descriptor_encoding_matches_datasheet);
    RUN_TEST(test_build_compat_layout_bytes);
    RUN_TEST(test_layout_fillers_cover_1080p_frame);
    RUN_TEST(test_build_rejects_bad_layouts);
    return UNITY_END();
}
