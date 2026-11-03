// TV5725 icon OSD on the standard board: icon mapping, visibility rule, indicator timer, re-arm after a preset load
#include <unity.h>
#include "tv-osd.h"
#include "settings-registry.h"
#include "api-actions.h"

void setUp(void) {}
void tearDown(void) {}

void test_icon_mapping(void) {
    TEST_ASSERT_EQUAL_UINT8(TVO_BRIGHTNESS, tvOsdIconForSetting(SID_pic_brightness));
    TEST_ASSERT_EQUAL_UINT8(TVO_CONTRAST, tvOsdIconForSetting(SID_pic_contrast));
    TEST_ASSERT_EQUAL_UINT8(TVO_NONE, tvOsdIconForSetting(SID_pic_saturation));
    TEST_ASSERT_EQUAL_UINT8(TVO_MOVE_X, tvOsdIconForAction(AID_scr_move_left));
    TEST_ASSERT_EQUAL_UINT8(TVO_MOVE_X, tvOsdIconForAction(AID_scr_move_right));
    TEST_ASSERT_EQUAL_UINT8(TVO_MOVE_Y, tvOsdIconForAction(AID_scr_move_up));
    TEST_ASSERT_EQUAL_UINT8(TVO_MOVE_Y, tvOsdIconForAction(AID_scr_move_down));
    TEST_ASSERT_EQUAL_UINT8(TVO_SCALE_X, tvOsdIconForAction(AID_scr_scale_left));
    TEST_ASSERT_EQUAL_UINT8(TVO_SCALE_X, tvOsdIconForAction(AID_scr_scale_right));
    TEST_ASSERT_EQUAL_UINT8(TVO_SCALE_Y, tvOsdIconForAction(AID_scr_scale_up));
    TEST_ASSERT_EQUAL_UINT8(TVO_SCALE_Y, tvOsdIconForAction(AID_scr_scale_down));
    TEST_ASSERT_EQUAL_UINT8(TVO_NONE, tvOsdIconForAction(AID_sys_restart));
}

void test_action_direction(void) {                 // the move/scale indicator shows which way it went
    TEST_ASSERT_EQUAL_INT8(1, tvOsdActionDir(AID_scr_move_right));
    TEST_ASSERT_EQUAL_INT8(-1, tvOsdActionDir(AID_scr_move_left));
    TEST_ASSERT_EQUAL_INT8(1, tvOsdActionDir(AID_scr_move_down));
    TEST_ASSERT_EQUAL_INT8(-1, tvOsdActionDir(AID_scr_move_up));
    TEST_ASSERT_EQUAL_INT8(1, tvOsdActionDir(AID_scr_scale_right));
    TEST_ASSERT_EQUAL_INT8(-1, tvOsdActionDir(AID_scr_scale_left));
    TEST_ASSERT_EQUAL_INT8(1, tvOsdActionDir(AID_scr_scale_up));
    TEST_ASSERT_EQUAL_INT8(-1, tvOsdActionDir(AID_scr_scale_down));
    TEST_ASSERT_EQUAL_INT8(0, tvOsdActionDir(AID_sys_restart));
}

void test_indicator_only_for_single_writes(void) {
    SettingsPair one = {SID_pic_brightness, 55};
    TEST_ASSERT_EQUAL_UINT8(TVO_BRIGHTNESS, tvOsdIconForWrite(&one, 1));
    SettingsPair reset[2] = {{SID_pic_brightness, 50}, {SID_pic_contrast, 50}};   // Reset picture writes many
    TEST_ASSERT_EQUAL_UINT8(TVO_NONE, tvOsdIconForWrite(reset, 2));
    SettingsPair other = {SID_pic_red, 128};
    TEST_ASSERT_EQUAL_UINT8(TVO_NONE, tvOsdIconForWrite(&other, 1));
}

void test_allowed_rule(void) {
    TEST_ASSERT_TRUE(tvOsdAllowed(false, true, 0x01));            // standard board, 960p
    TEST_ASSERT_FALSE(tvOsdAllowed(true, true, 0x01));            // the STV9426 menu covers the Pro
    TEST_ASSERT_FALSE(tvOsdAllowed(false, false, 0x01));          // no source
    TEST_ASSERT_FALSE(tvOsdAllowed(false, true, 0x06));           // Downscale (15 kHz)
    TEST_ASSERT_FALSE(tvOsdAllowed(false, true, 0x16));           // Downscale, PAL
    TEST_ASSERT_FALSE(tvOsdAllowed(false, true, 0x21));           // HD bypass
    TEST_ASSERT_FALSE(tvOsdAllowed(false, true, 0x22));           // RGBHV bypass
    TEST_ASSERT_TRUE(tvOsdAllowed(false, true, 0x15));            // 1080p PAL
}

void test_indicator_timer(void) {
    TvOsdIndicator t;
    TEST_ASSERT_FALSE(t.shown());
    t.show(TVO_CONTRAST, 0);
    TEST_ASSERT_TRUE(t.shown());
    TEST_ASSERT_EQUAL_UINT8(TVO_CONTRAST, t.icon());
    TEST_ASSERT_FALSE(t.due(TvOsdIndicator::kShowMs - 1));
    TEST_ASSERT_TRUE(t.due(TvOsdIndicator::kShowMs));
    t.show(TVO_CONTRAST, 2000);                                  // every step restarts it
    TEST_ASSERT_FALSE(t.due(TvOsdIndicator::kShowMs));
    TEST_ASSERT_TRUE(t.due(2000 + TvOsdIndicator::kShowMs));
    t.hide();
    TEST_ASSERT_FALSE(t.shown());
    TEST_ASSERT_FALSE(t.due(10000));
}

void test_rearm_after_a_preset_load(void) {
    uint16_t seen = 3;
    TEST_ASSERT_FALSE(tvOsdNeedsRearm(seen, 3));
    TEST_ASSERT_TRUE(tvOsdNeedsRearm(seen, 4));                  // tv5725OsdOff() ran: draw the layout again
    TEST_ASSERT_EQUAL_UINT16(4, seen);
    TEST_ASSERT_FALSE(tvOsdNeedsRearm(seen, 4));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_icon_mapping);
    RUN_TEST(test_action_direction);
    RUN_TEST(test_indicator_only_for_single_writes);
    RUN_TEST(test_allowed_rule);
    RUN_TEST(test_indicator_timer);
    RUN_TEST(test_rearm_after_a_preset_load);
    return UNITY_END();
}
