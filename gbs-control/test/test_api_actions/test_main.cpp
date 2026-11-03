#include <string.h>
#include <string>
#include <unity.h>
#include "platform-caps.h"
#include "api-actions.h"
#include "ota-plan.h"
#include "api-queue.h"

static void sink(const char* t, void* c) { ((std::string*)c)->append(t); }
void setUp(void) { platformSetCaps(CAPS_DEFAULT); }
void tearDown(void) {}

static ActionDef def(const char* name) {
    int i = actionsFind(name); TEST_ASSERT_TRUE_MESSAGE(i >= 0, name);
    ActionDef d; actionsDefAt((uint8_t)i, d); return d;
}

void test_names_unique(void) {
    for (uint8_t i = 0; i < actionsCount(); i++)
        for (uint8_t j = i + 1; j < actionsCount(); j++) {
            ActionDef d;
            actionsDefAt(i, d); std::string a = d.name;   // name points to a shared buffer: copy it
            actionsDefAt(j, d);
            TEST_ASSERT_TRUE_MESSAGE(a != d.name, a.c_str());
        }
}

void test_legacy_mapping(void) {
    ActionDef d = def("screen.move_left");
    TEST_ASSERT_EQUAL_UINT8(ACT_SERIAL, d.kind);
    TEST_ASSERT_EQUAL_CHAR('7', (char)d.arg);
    d = def("system.restart");
    TEST_ASSERT_EQUAL_UINT8(ACT_USER, d.kind);
    TEST_ASSERT_TRUE(d.flags & ACTION_CONFIRM);
    TEST_ASSERT_EQUAL_INT(-1, actionsFind("nope"));
}

void test_availability(void) {
    TEST_ASSERT_FALSE(actionsAvailable(def("dev.debug_view"), false));
    TEST_ASSERT_TRUE(actionsAvailable(def("dev.debug_view"), true));
    platformSetCaps(CAP_ORIGINAL | CAP_LED);
    TEST_ASSERT_FALSE(actionsAvailable(def("adv.ace_defaults"), true));
    TEST_ASSERT_TRUE(actionsAvailable(def("screen.move_left"), false));
}

void test_crash_action_is_developer_only(void) {
    ActionDef d = def("dev.crash");
    TEST_ASSERT_EQUAL_UINT8(ACT_FUNC, d.kind);
    TEST_ASSERT_EQUAL_UINT8(AF_CRASH, d.arg);
    TEST_ASSERT_FALSE(actionsAvailable(d, false));
    TEST_ASSERT_TRUE(actionsAvailable(d, true));
}

void test_list_json(void) {
    std::string out;
    actionsListJson(false, sink, &out);
    TEST_ASSERT_TRUE(out.find("{\"name\":\"system.restart\",\"caps\":0,\"confirm\":1}") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("dev.debug_view") == std::string::npos);
    out.clear();
    actionsListJson(true, sink, &out);
    TEST_ASSERT_TRUE(out.find("\"name\":\"dev.adv_i2c\"") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("\"params\":[{\"name\":\"data\",\"type\":\"hex_triplets\",\"max\":10}]") != std::string::npos);
    TEST_ASSERT_EQUAL_CHAR('[', out[0]);
    TEST_ASSERT_EQUAL_CHAR(']', out[out.size() - 1]);
}

void test_hex_triplets(void) {
    uint8_t b[30]; uint8_t n;
    TEST_ASSERT_TRUE(actionsParseHexTriplets("42,0E,00,56,17,02", b, sizeof(b), n));
    TEST_ASSERT_EQUAL_UINT8(6, n);
    TEST_ASSERT_EQUAL_HEX8(0x0E, b[1]);
    TEST_ASSERT_FALSE(actionsParseHexTriplets("42,0E", b, sizeof(b), n));      // not a triplet
    TEST_ASSERT_FALSE(actionsParseHexTriplets("42,0G,00", b, sizeof(b), n));   // bad hex
    TEST_ASSERT_FALSE(actionsParseHexTriplets("142,00,00", b, sizeof(b), n));  // > 0xFF
    TEST_ASSERT_FALSE(actionsParseHexTriplets("", b, sizeof(b), n));
    std::string many;
    for (int i = 0; i < 11; i++) many += (i ? "," : "") + std::string("42,00,00");
    TEST_ASSERT_FALSE(actionsParseHexTriplets(many.c_str(), b, sizeof(b), n));  // > 10 triplets
}

// Any count of 1-2 digit hex bytes (dev.menu_key: a key sequence)
void test_hex_bytes(void) {
    uint8_t b[30]; uint8_t n;
    TEST_ASSERT_TRUE(actionsParseHexBytes("06,1,4", b, sizeof(b), n));
    TEST_ASSERT_EQUAL_UINT8(3, n);
    TEST_ASSERT_EQUAL_HEX8(0x04, b[2]);
    TEST_ASSERT_TRUE(actionsParseHexBytes("0A", b, sizeof(b), n));
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_FALSE(actionsParseHexBytes("", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseHexBytes("01,", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseHexBytes("01,,02", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseHexBytes("1G", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseHexBytes("01,02,03", b, 2, n));                    // does not fit
    ActionDef d = def("dev.menu_key");
    TEST_ASSERT_TRUE((d.flags & ACTION_DEV) && (d.flags & ACTION_PARAM_BYTES));
    d = def("dev.screen_snapshot");
    TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
}

void test_update_actions(void) {
    ActionDef d = def("system.check_update");
    TEST_ASSERT_FALSE(d.flags & ACTION_DEV);
    TEST_ASSERT_EQUAL_INT(-1, actionsFind("system.check_update_reboot"));   // the update mode is gone
    d = def("system.install_update");
    TEST_ASSERT_TRUE(d.flags & ACTION_CONFIRM);
    TEST_ASSERT_FALSE(d.flags & ACTION_DEV);
}

// OTA channels: data is the channel name as text, frozen for the mobile app
void test_check_channel_action_takes_text(void) {
    ActionDef d = def("system.check_channel");
    TEST_ASSERT_TRUE(d.flags & ACTION_PARAM_TEXT);
    TEST_ASSERT_FALSE(d.flags & ACTION_DEV);
    uint8_t b[30], n = 0;
    TEST_ASSERT_TRUE(actionsParseText("beta", b, sizeof(b), n));
    TEST_ASSERT_EQUAL_UINT8(4, n);
    TEST_ASSERT_EQUAL_INT(OTA_BETA, otaChannelParse((const char*)b));
    TEST_ASSERT_FALSE(actionsParseText("", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseText("be ta", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseText("Beta;x", b, sizeof(b), n));
    TEST_ASSERT_FALSE(actionsParseText("abcdefgh", b, 8, n));                         // 7 characters + NUL at most
    TEST_ASSERT_TRUE(actionsParseText("stable", b, sizeof(b), n));
    TEST_ASSERT_EQUAL_INT(-1, otaChannelParse("nightly"));
    d = def("system.install_update");
    TEST_ASSERT_TRUE(d.flags & ACTION_PARAM_OPT_TEXT);                                 // "manual": the Other channels offer
}

// The action list tells an app the text parameters (mobile app discovery, frozen from 3.0.0)
void test_action_list_describes_text_params(void) {
    std::string out;
    actionsListJson(false, sink, &out);
    TEST_ASSERT_TRUE(out.find("{\"name\":\"system.check_channel\",\"caps\":0,\"params\":[{\"name\":\"data\","
                              "\"type\":\"text\",\"values\":[\"stable\",\"beta\",\"dev\"]}]}") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("{\"name\":\"system.install_update\",\"caps\":0,\"confirm\":1,\"params\":[{\"name\":"
                              "\"data\",\"type\":\"text\",\"optional\":1,\"values\":[\"manual\"]}]}") != std::string::npos);
}

void test_osd_developer_actions(void) {
    const char* names[4] = {"dev.osd_test", "dev.osd_rom_dump", "dev.osd_always_on", "dev.osd_timing"};
    for (uint8_t i = 0; i < 4; i++) {
        int idx = actionsFind(names[i]);
        TEST_ASSERT_TRUE_MESSAGE(idx >= 0, names[i]);
        ActionDef d; actionsDefAt((uint8_t)idx, d);
        TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
        TEST_ASSERT_EQUAL_UINT8(CAP_OSD, d.caps);
        TEST_ASSERT_EQUAL_UINT8(ACT_FUNC, d.kind);
    }
    ActionDef t; actionsDefAt((uint8_t)actionsFind("dev.osd_timing"), t);
    TEST_ASSERT_TRUE(t.flags & ACTION_PARAM_HEX);
}

void test_action_ids_match_table_order(void) {
    TEST_ASSERT_EQUAL_INT(actionsFind("system.restart"), AID_sys_restart);
    TEST_ASSERT_EQUAL_INT(actionsFind("picture.color_up"), AID_pic_color_up);
    TEST_ASSERT_EQUAL_INT(actionsFind("picture.color_down"), AID_pic_color_down);
    TEST_ASSERT_EQUAL_INT(actionsFind("dev.pll_div_down"), AID_dev_pll_down);
    TEST_ASSERT_EQUAL_INT(actionsCount(), AID_COUNT);
}

// SOG level is a two-way user action pair (no Developer mode, no serial character)
void test_sog_actions_not_dev(void) {
    const char* names[2] = {"input.sog_up", "input.sog_down"};
    for (const char* n : names) {
        int idx = actionsFind(n);
        TEST_ASSERT_TRUE_MESSAGE(idx >= 0, n);
        ActionDef d; actionsDefAt((uint8_t)idx, d);
        TEST_ASSERT_FALSE(d.flags & ACTION_DEV);
        TEST_ASSERT_EQUAL_UINT8(ACT_FUNC, d.kind);
        TEST_ASSERT_TRUE(actionsAvailable(d, false));
    }
    TEST_ASSERT_TRUE(actionsFind("dev.sog_down") < 0);
}

// Actions that change captured scaler registers mark the active profile Unsaved
void test_regs_actions_flagged(void) {
    const char* regs[] = {"screen.move_left", "screen.move_right", "screen.move_up", "screen.move_down",
                          "screen.scale_left", "screen.scale_right", "screen.scale_up", "screen.scale_down",
                          "screen.border_left", "screen.border_right", "screen.border_up", "screen.border_down",
                          "screen.border_move_left", "screen.border_move_right", "screen.border_move_up",
                          "screen.border_move_down", "dev.mem_left", "dev.mem_right", "dev.hs_left", "dev.hs_right",
                          "dev.htotal_up", "dev.htotal_down", "dev.htotal_resync", "dev.pll_div_up",
                          "dev.pll_div_down", "dev.snap_frame_rate", "input.gain_up", "input.gain_down"};
    for (const char* n : regs) {
        int idx = actionsFind(n);
        TEST_ASSERT_TRUE_MESSAGE(idx >= 0, n);
        ActionDef d; actionsDefAt((uint8_t)idx, d);
        TEST_ASSERT_TRUE_MESSAGE(d.flags & ACTION_REGS, n);
    }
    const char* plain[] = {"picture.brightness_up", "dev.invert_sync", "dev.if_auto_offset", "system.restart"};
    for (const char* n : plain) {
        ActionDef d; actionsDefAt((uint8_t)actionsFind(n), d);
        TEST_ASSERT_FALSE_MESSAGE(d.flags & ACTION_REGS, n);
    }
}

// Menu functions the web app calls: none needs Developer mode
void test_action_names_for_menu_functions(void) {
    const char* names[] = {"picture.reset", "screen.reset", "adv.decoder_reset", "input.termination"};
    for (const char* n : names) {
        ActionDef d = def(n);
        TEST_ASSERT_FALSE_MESSAGE(d.flags & ACTION_DEV, n);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ACT_FUNC, d.kind, n);
    }
    TEST_ASSERT_TRUE(def("picture.reset").flags & ACTION_CONFIRM);
    TEST_ASSERT_TRUE(def("screen.reset").flags & ACTION_CONFIRM);
    TEST_ASSERT_TRUE(def("screen.reset").flags & ACTION_REGS);
    TEST_ASSERT_TRUE(def("adv.decoder_reset").flags & ACTION_CONFIRM);
    TEST_ASSERT_EQUAL_UINT8(CAP_ADV, def("adv.decoder_reset").caps);
    TEST_ASSERT_EQUAL_UINT8(CAP_ADV, def("input.termination").caps);
    TEST_ASSERT_TRUE(def("input.termination").flags & ACTION_PARAM_BYTES);
}

// input.termination takes exactly one byte, 0 (off) or 1 (on)
void test_termination_param_validation(void) {
    ActionDef d = def("input.termination");
    uint8_t one[] = {1}, zero[] = {0}, two[] = {2}, pair[] = {1, 0};
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, one, 0));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, pair, 2));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, two, 1));
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, one, 1));
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, zero, 1));
    uint8_t keys[] = {6, 1, 4};                                      // a key sequence keeps any length
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(def("dev.menu_key"), keys, 3));
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(def("system.restart"), nullptr, 0));
}

void test_actions_list_shows_bytes_param(void) {
    std::string out;
    actionsListJson(false, sink, &out);
    size_t at = out.find("\"name\":\"input.termination\"");
    TEST_ASSERT_TRUE(at != std::string::npos);
    size_t end = out.find('}', out.find("\"params\"", at));
    std::string row = out.substr(at, end - at);
    TEST_ASSERT_TRUE_MESSAGE(row.find("\"params\":[{\"name\":\"data\",\"type\":\"bytes\",\"len\":1") != std::string::npos, row.c_str());
    out.clear();
    actionsListJson(true, sink, &out);
    TEST_ASSERT_TRUE(out.find("\"type\":\"bytes\",\"max_len\":30") != std::string::npos);   // dev.menu_key
}

// The action queue (9 slots, one kept free) refuses the 9th pending action: the web answers 409 busy
void test_action_queue_full_is_busy(void) {
    RingQueue<int, 9> q;
    for (int i = 0; i < 8; i++) TEST_ASSERT_TRUE(q.push(i));
    TEST_ASSERT_FALSE(q.push(8));
    int v; TEST_ASSERT_TRUE(q.pop(v));
    TEST_ASSERT_TRUE(q.push(8));
}

// The web remote works in every mode; dev.menu_key stays for tools
void test_menu_key_available_without_devmode(void) {
    TEST_ASSERT_TRUE(actionsAvailable(def("menu.key"), false));
    TEST_ASSERT_TRUE(def("menu.key").flags & ACTION_PARAM_BYTES);
    TEST_ASSERT_EQUAL_UINT8(AF_MENU_KEY, def("menu.key").arg);
    TEST_ASSERT_FALSE(actionsAvailable(def("dev.menu_key"), false));
}

// dev.tv5725_read: one byte, the segment 0..5; Developer only
void test_tv5725_read_param(void) {
    ActionDef d = def("dev.tv5725_read");
    TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
    TEST_ASSERT_FALSE(actionsAvailable(d, false));
    uint8_t s5[] = {5}, s6[] = {6}, two[] = {1, 2};
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, s5, 1));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, s6, 1));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, two, 2));
}

// dev.clockgen: one byte, 0 = Si5351 output off, 1 = on (no-signal WiFi experiments); Developer only
void test_clockgen_param(void) {
    ActionDef d = def("dev.clockgen");
    TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
    TEST_ASSERT_FALSE(actionsAvailable(d, false));
    uint8_t off[] = {0}, on[] = {1}, two[] = {2};
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, off, 1));
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, on, 1));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, two, 1));
}

// dev.loop_pause: one byte, a pause in ms at every loop pass (0 off, at most 100); Developer only
void test_loop_pause_param(void) {
    ActionDef d = def("dev.loop_pause");
    TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
    uint8_t zero[] = {0}, hundred[] = {100}, over[] = {101};
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, zero, 1));
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, hundred, 1));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, over, 1));
}

// dev.si5351: op (0 status, 1 vco648 rule, 2 crystal load, 3 lock timing, 4 output frame rate) + up to 2 bytes
void test_si5351_param(void) {
    ActionDef d = def("dev.si5351");
    TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
    uint8_t one[] = {0}, three[] = {3, 4, 20}, four[] = {3, 4, 20, 1};
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, one, 1));
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, three, 3));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, four, 4));
}

// dev.adv_read: dev, map, reg, count (the link request; checked again by advRegReadReqUnpack)
void test_adv_read_param_len_4(void) {
    ActionDef d = def("dev.adv_read");
    TEST_ASSERT_TRUE(d.flags & ACTION_DEV);
    TEST_ASSERT_EQUAL_UINT8(CAP_ADV, d.caps);
    uint8_t ok[] = {0x42, 0x40, 0x80, 4}, three[] = {0x42, 0, 0}, bad[] = {0x42, 0x10, 0, 1};
    TEST_ASSERT_EQUAL_INT(0, actionsValidateParams(d, ok, 4));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, three, 3));
    TEST_ASSERT_NOT_EQUAL(0, actionsValidateParams(d, bad, 4));   // not a decoder map
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_action_ids_match_table_order);
    RUN_TEST(test_names_unique);
    RUN_TEST(test_osd_developer_actions);
    RUN_TEST(test_legacy_mapping);
    RUN_TEST(test_availability);
    RUN_TEST(test_crash_action_is_developer_only);
    RUN_TEST(test_list_json);
    RUN_TEST(test_hex_triplets);
    RUN_TEST(test_hex_bytes);
    RUN_TEST(test_update_actions);
    RUN_TEST(test_check_channel_action_takes_text);
    RUN_TEST(test_action_list_describes_text_params);
    RUN_TEST(test_sog_actions_not_dev);
    RUN_TEST(test_regs_actions_flagged);
    RUN_TEST(test_action_names_for_menu_functions);
    RUN_TEST(test_termination_param_validation);
    RUN_TEST(test_actions_list_shows_bytes_param);
    RUN_TEST(test_action_queue_full_is_busy);
    RUN_TEST(test_menu_key_available_without_devmode);
    RUN_TEST(test_tv5725_read_param);
    RUN_TEST(test_clockgen_param);
    RUN_TEST(test_loop_pause_param);
    RUN_TEST(test_si5351_param);
    RUN_TEST(test_adv_read_param_len_4);
    return UNITY_END();
}
