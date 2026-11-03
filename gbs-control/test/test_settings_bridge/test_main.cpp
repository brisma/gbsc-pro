#include <string.h>
#include <unity.h>
#include "options.h"
#include "settings-registry.h"
#include "settings-bridge.h"

void setUp(void) {}
void tearDown(void) {}

static SettingDef defOf(const char* key) {
    int i = settingsFind(key); TEST_ASSERT_TRUE_MESSAGE(i >= 0, key);
    SettingDef d; settingsDefAt((uint8_t)i, d); return d;
}

void test_toggle_fires_only_on_change(void) {
    SettingsBridgeHit h;
    TEST_ASSERT_FALSE(settingsBridgeLookup(defOf("picture.scanlines"), 1, 1, h));
    TEST_ASSERT_TRUE(settingsBridgeLookup(defOf("picture.scanlines"), 0, 1, h));
    TEST_ASSERT_EQUAL_UINT8(BRIDGE_USER, h.queue);
    TEST_ASSERT_EQUAL_CHAR('7', h.ch);
    TEST_ASSERT_TRUE(settingsBridgeLookup(defOf("input.auto_gain"), 1, 0, h));
    TEST_ASSERT_EQUAL_UINT8(BRIDGE_SERIAL, h.queue);
    TEST_ASSERT_EQUAL_CHAR('T', h.ch);
}

void test_value_map_fires_for_mapped_values(void) {
    SettingsBridgeHit h;
    TEST_ASSERT_TRUE(settingsBridgeLookup(defOf("output.preset"), 5, 5, h));   // re-select re-applies
    TEST_ASSERT_EQUAL_CHAR('s', h.ch);
    TEST_ASSERT_TRUE(settingsBridgeLookup(defOf("output.preset"), 0, 1, h));
    TEST_ASSERT_EQUAL_CHAR('h', h.ch);
    TEST_ASSERT_FALSE(settingsBridgeLookup(defOf("output.preset"), 0, 2, h));  // Custom: normal path
    TEST_ASSERT_TRUE(settingsBridgeLookup(defOf("picture.deinterlace"), 1, 0, h));
    TEST_ASSERT_EQUAL_CHAR('r', h.ch);
}

void test_unbridged_setting_has_no_hit(void) {
    SettingsBridgeHit h;
    TEST_ASSERT_FALSE(settingsBridgeLookup(defOf("adv.brightness"), 1, 2, h));
}

void test_pending_bridge_tracks_queued_toggles(void) {
    // POST scanlines=1 then scanlines=0 before the queued legacy char ran: two toggles, final value 0
    SettingDef d = defOf("picture.scanlines");
    uint8_t idx = (uint8_t)settingsFind("picture.scanlines");
    BridgePending p;
    SettingsBridgeHit h;
    uint16_t current = 0;
    TEST_ASSERT_TRUE(settingsBridgeLookup(d, p.effective(idx, current, 0), 1, h));
    p.set(idx, 1, 0);
    TEST_ASSERT_TRUE(settingsBridgeLookup(d, p.effective(idx, current, 10), 0, h));   // second toggle queued
    p.set(idx, 0, 10);
    TEST_ASSERT_EQUAL_UINT16(0, p.effective(idx, 1, 20));   // first char ran: still pending 0
    TEST_ASSERT_EQUAL_UINT16(0, p.effective(idx, 0, 30));   // second char ran: cleared
    TEST_ASSERT_FALSE(settingsBridgeLookup(d, p.effective(idx, 0, 40), 0, h));
}

void test_pending_bridge_times_out(void) {
    BridgePending p;
    p.set(3, 1, 1000);
    TEST_ASSERT_EQUAL_UINT16(1, p.effective(3, 0, 2000));
    TEST_ASSERT_EQUAL_UINT16(0, p.effective(3, 0, 1000 + BridgePending::kTimeoutMs));   // char never ran
}

void test_pending_value_from_queue_and_bridge(void) {
    BridgePending p;
    uint16_t t = 0;
    TEST_ASSERT_FALSE(p.peek(3, 0, 1000, t));
    p.set(3, 5, 1000);
    TEST_ASSERT_TRUE(p.peek(3, 0, 1100, t));
    TEST_ASSERT_EQUAL_UINT16(5, t);
    TEST_ASSERT_FALSE(p.peek(3, 5, 1100, t));                                // already applied
    TEST_ASSERT_FALSE(p.peek(3, 0, 1000 + BridgePending::kTimeoutMs, t));    // expired
    TEST_ASSERT_TRUE(p.peek(3, 0, 1100, t));                                 // peek never clears the slot
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pending_value_from_queue_and_bridge);
    RUN_TEST(test_toggle_fires_only_on_change);
    RUN_TEST(test_value_map_fires_for_mapped_values);
    RUN_TEST(test_unbridged_setting_has_no_hit);
    RUN_TEST(test_pending_bridge_tracks_queued_toggles);
    RUN_TEST(test_pending_bridge_times_out);
    return UNITY_END();
}
