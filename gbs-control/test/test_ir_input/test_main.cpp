#include <unity.h>
#include "ir-input.h"
#include "menu-types.h"
#include "menu-signal.h"

void setUp(void) {}
void tearDown(void) {}

void test_ir_key_map(void) {
    const uint32_t codes[12] = {IR_CODE_UP, IR_CODE_DOWN, IR_CODE_LEFT, IR_CODE_RIGHT, IR_CODE_OK, IR_CODE_MENU,
                                IR_CODE_EXIT, IR_CODE_INFO, IR_CODE_SAVE, IR_CODE_MUTE, IR_CODE_VOLUP, IR_CODE_VOLDN};
    const int keys[12] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_MENU, K_BACK, K_INFO, K_SAVE, K_MUTE, K_VOLUP, K_VOLDOWN};
    for (int i = 0; i < 12; i++) TEST_ASSERT_EQUAL_INT(keys[i], irKeyFor(codes[i]));
    TEST_ASSERT_EQUAL_INT(-1, irKeyFor(0x12345678));
}

void test_ir_repeat_acceleration(void) {
    IrRepeater r;
    bool rep = true;
    TEST_ASSERT_EQUAL_INT(-1, r.feed(0, true, 1000, rep));          // repeat with no key yet
    TEST_ASSERT_EQUAL_INT(K_RIGHT, r.feed(IR_CODE_RIGHT, false, 1000, rep));
    TEST_ASSERT_FALSE(rep);
    int sent = 0; uint32_t last = 1000, gapEarly = 0, gapMid = 0, gapLate = 0;
    for (uint32_t t = 1050; t <= 6000; t += 10) {
        if (r.feed(0, true, t, rep) == K_RIGHT) {
            TEST_ASSERT_TRUE(rep);
            uint32_t gap = t - last; last = t; sent++;
            if (t < 3000) gapEarly = gap;
            else if (t > 3600 && t < 4900) gapMid = gap;
            else if (t > 5100) gapLate = gap;
        }
    }
    TEST_ASSERT_TRUE(sent > 20);
    TEST_ASSERT_TRUE(gapEarly >= 125 && gapEarly < 140);
    TEST_ASSERT_TRUE(gapMid >= 62 && gapMid < 75);
    TEST_ASSERT_TRUE(gapLate >= 31 && gapLate < 45);
    r.clear();
    TEST_ASSERT_EQUAL_INT(-1, r.feed(0, true, 7000, rep));
    TEST_ASSERT_EQUAL_INT(-1, r.feed(0x1234, false, 7000, rep));    // unknown code
}

void test_source_lines_halved_for_sd_modes(void) {
    TEST_ASSERT_EQUAL_UINT16(262, menuSourceLines(1, 525));
    TEST_ASSERT_EQUAL_UINT16(312, menuSourceLines(2, 625));
    TEST_ASSERT_EQUAL_UINT16(525, menuSourceLines(3, 525));
    TEST_ASSERT_EQUAL_UINT16(625, menuSourceLines(4, 625));
}

void test_adv_ntsc_detection(void) {
    TEST_ASSERT_TRUE(menuAdvIsNtsc(0, 0x00));
    TEST_ASSERT_FALSE(menuAdvIsNtsc(0, 0x80 | 0x40));
    TEST_ASSERT_TRUE(menuAdvIsNtsc(0, 0x80 | 0x10));
    TEST_ASSERT_TRUE(menuAdvIsNtsc(2, 0x80 | 0x40));
    TEST_ASSERT_FALSE(menuAdvIsNtsc(1, 0x00));
    TEST_ASSERT_TRUE(menuAdvIsNtsc(8, 0x00));
    TEST_ASSERT_TRUE(menuAdvIsNtsc(0, 0x80));                 // valid signal, no standard bit: default NTSC
}

void test_ir_gap_forgets_key(void) {
    IrRepeater r; bool rep;
    r.feed(IR_CODE_VOLUP, false, 1000, rep);
    TEST_ASSERT_EQUAL_INT(-1, r.feed(0, true, 1200, rep));     // 200 ms without a frame: another remote's repeat
}

void test_ir_unknown_code_clears(void) {
    IrRepeater r; bool rep;
    r.feed(IR_CODE_OK, false, 1000, rep);
    r.feed(0x20DF10EF, false, 1050, rep);                      // a TV remote key
    TEST_ASSERT_EQUAL_INT(-1, r.feed(0, true, 1100, rep));
}

void test_ir_real_nec_spacing(void) {
    IrRepeater r; bool rep;
    r.feed(IR_CODE_RIGHT, false, 0, rep);
    int sent = 0;
    for (uint32_t t = 108; t <= 3000; t += 108) if (r.feed(0, true, t, rep) == K_RIGHT) sent++;
    TEST_ASSERT_TRUE(sent >= 13);                              // every second frame first, every frame later
}

void test_sync_type_sog_first(void) {
    TEST_ASSERT_EQUAL_UINT8(2, menuSyncType(true, true));
    TEST_ASSERT_EQUAL_UINT8(2, menuSyncType(true, false));
    TEST_ASSERT_EQUAL_UINT8(1, menuSyncType(false, true));
    TEST_ASSERT_EQUAL_UINT8(0, menuSyncType(false, false));
}

void test_source_lines_not_halved_below_400(void) {
    TEST_ASSERT_EQUAL_UINT16(300, menuSourceLines(1, 300));
}

void test_signal_change_detector(void) {
    MenuSignalKey a = {1, 1, false, 0, 525};
    MenuSignalKey b = a;
    b.lines = 527;
    TEST_ASSERT_FALSE(menuSignalChanged(a, b));                // jitter within 2 lines
    b.lines = 530;
    TEST_ASSERT_TRUE(menuSignalChanged(a, b));
    b = a; b.standard = 2;
    TEST_ASSERT_TRUE(menuSignalChanged(a, b));
    b = a; b.deint = 1;
    TEST_ASSERT_TRUE(menuSignalChanged(a, b));
    // Force 60Hz or a new resolution changes only the output preset; the OLED home and /api/info
    // kept the old output rate (49.7 for a 59.9 Hz output) until the next input change
    b = a; b.preset = 0x05; a.preset = 0x15;
    TEST_ASSERT_TRUE(menuSignalChanged(a, b));
}

void test_only_arrows_and_volume_repeat(void) {
    const uint32_t once[6] = {IR_CODE_MENU, IR_CODE_SAVE, IR_CODE_OK, IR_CODE_EXIT, IR_CODE_INFO, IR_CODE_MUTE};
    for (uint32_t code : once) {
        IrRepeater r; bool rep;
        TEST_ASSERT_TRUE(r.feed(code, false, 1000, rep) >= 0);
        for (uint32_t t = 1108; t <= 2000; t += 108) TEST_ASSERT_EQUAL_INT(-1, r.feed(0, true, t, rep));
    }
    const uint32_t held[6] = {IR_CODE_UP, IR_CODE_DOWN, IR_CODE_LEFT, IR_CODE_RIGHT, IR_CODE_VOLUP, IR_CODE_VOLDN};
    for (uint32_t code : held) {
        IrRepeater r; bool rep; int n = 0;
        r.feed(code, false, 1000, rep);
        for (uint32_t t = 1108; t <= 2000; t += 108) if (r.feed(0, true, t, rep) >= 0) n++;
        TEST_ASSERT_TRUE(n > 0);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_only_arrows_and_volume_repeat);
    RUN_TEST(test_ir_gap_forgets_key);
    RUN_TEST(test_ir_unknown_code_clears);
    RUN_TEST(test_ir_real_nec_spacing);
    RUN_TEST(test_sync_type_sog_first);
    RUN_TEST(test_source_lines_not_halved_below_400);
    RUN_TEST(test_signal_change_detector);
    RUN_TEST(test_ir_key_map);
    RUN_TEST(test_ir_repeat_acceleration);
    RUN_TEST(test_source_lines_halved_for_sd_modes);
    RUN_TEST(test_adv_ntsc_detection);
    return UNITY_END();
}
