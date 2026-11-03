#include <unity.h>
#include <string.h>
#include "menu-rig.h"
#include "menu-keyboard.h"
#include "menu-strings.h"
#include "platform-caps.h"
#include "profile-store.h"

void setUp(void) {}
void tearDown(void) { platformSetCaps(CAPS_DEFAULT); }

// ---- Navigation ----
void test_menu_opens_at_root_with_titles(void) {
    MenuRig r;
    TEST_ASSERT_FALSE(r.e.busy());
    r.key(K_MENU);
    TEST_ASSERT_TRUE(r.e.busy());
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
    TEST_ASSERT_EQUAL_UINT8(7, r.v().rowCount);
    const char* expect[7] = {"Input", "Output", "Picture", "Screen", "Profiles", "System", "Menu mode"};
    for (uint8_t i = 0; i < 7; i++) TEST_ASSERT_EQUAL_STRING(expect[i], r.label(i).c_str());
    TEST_ASSERT_FALSE(r.v().moreAbove);
    TEST_ASSERT_FALSE(r.v().moreBelow);
    TEST_ASSERT_EQUAL_STRING("Input", r.selected().c_str());
    TEST_ASSERT_EQUAL_STRING("Choose the input cable and tune how its signal is read.", r.v().help);
    TEST_ASSERT_TRUE(r.v().rows[0].flags & ROWF_SUB);
}

void test_cursor_wraps_and_skips_headers(void) {
    MenuRig r;
    r.key(K_MENU);
    TEST_ASSERT_TRUE(r.enter("Picture"));
    TEST_ASSERT_EQUAL_STRING("Filter", r.selected().c_str());
    r.key(K_DOWN);
    TEST_ASSERT_EQUAL_STRING("Scanlines", r.selected().c_str());
    r.key(K_UP); r.key(K_UP);
    TEST_ASSERT_EQUAL_STRING("Reset colors", r.selected().c_str());   // wraps
    r.key(K_DOWN);
    TEST_ASSERT_EQUAL_STRING("Filter", r.selected().c_str());
    TEST_ASSERT_TRUE(r.select("Reset picture"));
    r.key(K_DOWN);
    TEST_ASSERT_EQUAL_STRING("Sharpness", r.selected().c_str());  // the Detail header is not selectable
    r.key(K_ROT_L);
    TEST_ASSERT_EQUAL_STRING("Reset picture", r.selected().c_str());
}

void test_window_scrolls_with_cursor(void) {
    MenuRig r;
    r.key(K_MENU);
    r.enter("Picture");
    TEST_ASSERT_FALSE(r.v().moreAbove);
    TEST_ASSERT_TRUE(r.v().moreBelow);
    r.key(K_UP);
    TEST_ASSERT_TRUE(r.v().moreAbove);
    TEST_ASSERT_FALSE(r.v().moreBelow);
    TEST_ASSERT_TRUE(r.v().rows[2].flags & ROWF_HEADER);                       // Color
    TEST_ASSERT_EQUAL_STRING("Reset colors", r.label(r.v().rowCount - 1).c_str());
}

void test_back_pops_and_closes_at_root(void) {
    MenuRig r;
    r.key(K_MENU);
    r.enter("Output");
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Output", r.selected().c_str());   // cursor kept on the parent row
    r.key(K_BACK);
    TEST_ASSERT_FALSE(r.e.busy());
}

void test_menu_key_closes_everything(void) {
    MenuRig r;
    r.key(K_MENU);
    r.enter("Input");
    r.enter("Sampling");
    r.key(K_MENU);
    TEST_ASSERT_FALSE(r.e.busy());
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());     // reopens at the root
}

void test_sync_cleanup_only_on_rgb_scart_and_sog(void) {   // it acts on RGBs/RGsB only
    MenuRig r;
    r.setInput(6);
    r.key(K_MENU); r.enter("Input");
    TEST_ASSERT_FALSE(r.select("Sync cleanup"));
    r.key(K_MENU);
    r.setInput(1);
    r.key(K_MENU); r.enter("Input");
    TEST_ASSERT_TRUE(r.select("Sync cleanup"));
}

void test_disabled_row_shows_reason_in_help(void) {
    MenuRig r;
    r.setInput(2);                                                // RGB SoG: termination RGB SCART only
    r.key(K_MENU);
    r.enter("Input");
    TEST_ASSERT_TRUE(r.select("75 ohm sync"));
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_DISABLED);
    TEST_ASSERT_EQUAL_STRING("RGB SCART input only", r.v().help);
}

void test_hidden_submenu_pops_to_visible_parent(void) {
    MenuRig r;
    r.setInput(5);
    r.key(K_MENU);
    r.enter("Input");
    TEST_ASSERT_TRUE(r.enter("Decoder"));
    TEST_ASSERT_TRUE(r.enter("Adaptive contrast"));
    r.setInput(1);
    r.e.onSignalChange();
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Sampling", r.selected().c_str());
}

void test_cursor_keeps_child_when_rows_change(void) {
    MenuRig r;
    r.key(K_MENU);
    r.enter("Input");
    r.select("Sampling");
    r.setInput(5);                          // "Video standard" and 2X appear above
    r.e.onSignalChange();
    TEST_ASSERT_EQUAL_STRING("Sampling", r.selected().c_str());
}

void test_window_size_five(void) {
    MenuRig r;
    r.e.setWindow(5);
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(5, r.v().rowCount);
    TEST_ASSERT_TRUE(r.v().moreBelow);
}

void test_menu_times_out_after_15s(void) {   // osd.timeout default
    MenuRig r;
    r.key(K_MENU);
    r.wait(14800);
    TEST_ASSERT_TRUE(r.e.busy());
    r.wait(300);
    TEST_ASSERT_FALSE(r.e.busy());
}

// ---- Edits, confirmations, revert, encoder ----
static int countLog(const MenuRig& r, const std::string& entry) {
    int n = 0;
    for (auto& l : r.be.log) if (l == entry) n++;
    return n;
}
static std::string line0(const MenuRig& r) { return r.v().lines[0]; }

void test_bool_setting_toggles_through_apply(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Output"); r.select("Always 60Hz");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("apply output.force_60hz=1", r.lastLog().c_str());
    TEST_ASSERT_EQUAL_STRING("On", r.selectedValue().c_str());
}

void test_enum_left_right_wrap(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("OSD settings"); r.select("Theme");
    r.key(K_LEFT);
    TEST_ASSERT_EQUAL_STRING("apply osd.theme=3", r.lastLog().c_str());
}

// OK on Sound > Volume opened the quick slider in the middle of the screen, which looked like
// another control than the remote's volume pill: OK/click opens that same pill, the arrows/encoder adjust it
void test_volume_row_ok_opens_the_volume_pill(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("Sound"); r.select("Volume");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_VOLUME, r.v().overlay);
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply audio.volume=82"));
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Sound", r.title().c_str());
}

void test_numeric_repeat_allowed_toggle_repeat_ignored(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("Sound"); r.select("Volume");
    TEST_ASSERT_EQUAL_STRING("80%", r.selectedValue().c_str());   // percent, 2 % per step
    r.key(K_LEFT, true); r.key(K_LEFT, true);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply audio.volume=78"));
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply audio.volume=76"));
    r.key(K_MENU); r.key(K_MENU); r.enter("Output"); r.select("Always 60Hz");
    r.be.log.clear();
    r.key(K_OK, true);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
}

// The line filter is on by default (scanlines at a non-integer scale look even) and every Filter
// choice keeps it on; the factory state reads Normal
void test_filter_default_is_normal_with_the_line_filter(void) {
    MenuRig r;
    TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_pic_line_filter));
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Filter"));
    TEST_ASSERT_EQUAL_STRING("Normal", r.selectedValue().c_str());
    for (int i = 0; i < 3; i++) {
        r.key(K_RIGHT); r.be.settle(); r.wait(300);
        TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_pic_line_filter));
    }
}

void test_choice_applies_both_pairs(void) {
    MenuRig r;
    r.be.apply(SID_pic_sharpness, 31); r.be.apply(SID_pic_line_filter, 0); r.be.settle();   // line filter off in Advanced
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Filter"));
    TEST_ASSERT_EQUAL_STRING("Custom", r.selectedValue().c_str());
    r.be.log.clear();
    r.key(K_LEFT);                                                     // Smooth: both settings change
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply picture.sharpness=0"));
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply picture.line_filter=1"));
    TEST_ASSERT_EQUAL_STRING("Smooth", r.selectedValue().c_str());
}

void test_choice_is_atomic(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Picture"); r.select("Filter");
    r.be.queueFull = true;
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Busy - try again", line0(r).c_str());
}

void test_stepper_runs_actions(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Input"); r.enter("Sampling"); TEST_ASSERT_TRUE(r.select("Gain"));
    r.key(K_LEFT);
    TEST_ASSERT_EQUAL_STRING("run input.gain_down", r.lastLog().c_str());
    r.key(K_RIGHT, true);
    TEST_ASSERT_EQUAL_STRING("run input.gain_up", r.lastLog().c_str());
}

void test_confirm_default_no(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.select("Restart");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_UINT8(0, r.v().choice);
    TEST_ASSERT_EQUAL_STRING("Restart the GBSC PRO?", line0(r).c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    r.key(K_OK); r.key(K_LEFT);
    TEST_ASSERT_EQUAL_UINT8(1, r.v().choice);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("run system.restart", r.lastLog().c_str());
}

void test_queue_full_shows_busy_toast(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Output"); r.select("Force 60Hz");
    r.be.queueFull = true;
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    r.wait(2100);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
}

void test_commit_on_ok_for_resolution(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    TEST_ASSERT_EQUAL_STRING("1024p fixed", r.selectedValue().c_str());   // pending: 960p/1024p -> 1024p fixed
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("1080p", r.selectedValue().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply output.preset=5"));
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "hold 1"));
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(15, r.v().value);
}

static void openRevert(MenuRig& r) {
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    r.key(K_RIGHT); r.key(K_OK);       // 960p/1024p -> 1024p fixed, revert countdown open
}

void test_revert_keeps_first_old_value(void) {
    MenuRig r;
    openRevert(r);
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("apply output.preset=5", r.lastLog().c_str());   // 1080p
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply output.preset=0"));          // both first old values come back
    TEST_ASSERT_EQUAL_STRING("apply output.match_preset=1", r.lastLog().c_str());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "hold 1"));
}

void test_revert_releases_hold_only_after_settle(void) {
    MenuRig r;
    openRevert(r);
    r.key(K_BACK);
    r.wait(300);
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "hold 0"));
    r.be.settle();
    r.wait(300);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "hold 0"));
}

void test_revert_ok_keeps_and_releases(void) {
    MenuRig r;
    openRevert(r);
    size_t before = r.be.log.size();
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT32(before + 1, r.be.log.size());
    TEST_ASSERT_EQUAL_STRING("hold 0", r.lastLog().c_str());
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
}

void test_revert_times_out_after_15s(void) {
    MenuRig r;
    openRevert(r);
    r.wait(14000);
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);
    r.wait(1100);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply output.preset=0"));
}

void test_revert_on_menu_close(void) {
    MenuRig r;
    openRevert(r);
    r.key(K_MENU);
    TEST_ASSERT_FALSE(r.e.busy());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply output.preset=0"));
    r.be.settle();
    r.wait(300);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "hold 0"));
}

void test_revert_retry_when_queue_full(void) {
    MenuRig r;
    openRevert(r);
    r.be.queueFull = true;
    r.key(K_BACK);
    r.wait(300);
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "apply output.preset=0"));
    r.be.queueFull = false;
    r.wait(300);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply output.preset=0"));
}

void test_second_revert_waits_for_settle(void) {
    MenuRig r;
    openRevert(r);
    r.key(K_BACK);                     // undo queued, not settled yet
    r.select("Frame lock");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "apply output.frame_lock=1"));
    TEST_ASSERT_EQUAL_STRING("Please wait", line0(r).c_str());
}

// Edit mode on an enum row (slider rows open the quick slider instead)
void test_encoder_edit_mode(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("OSD settings"); r.select("Theme");
    r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_STRING("apply osd.theme=1", r.lastLog().c_str());   // GBSC PRO (default) -> Classic
    r.key(K_LONG);
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    TEST_ASSERT_EQUAL_STRING("OSD settings", r.title().c_str());
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_STRING("System", r.title().c_str());
}

void test_encoder_in_confirm_and_revert(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.select("Restart");
    r.key(K_CLICK); r.key(K_ROT_L); r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("run system.restart", r.lastLog().c_str());
    MenuRig q;
    openRevert(q);
    q.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("hold 0", q.lastLog().c_str());
}

// Knob edit of a row with the auto-undo countdown: an undo (long press or timeout) ends the edit too, else the next
// turn changed the value again and reopened the countdown
static void knobRevert(MenuRig& r) {
    r.setInput(5);
    r.key(K_MENU); r.enter("Input"); r.select("Video standard");
    r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);
}

void test_knob_revert_undo_ends_the_edit(void) {
    MenuRig r;
    knobRevert(r);
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
}

void test_knob_revert_timeout_ends_the_edit(void) {
    MenuRig r;
    knobRevert(r);
    r.wait(14000);                     // a tick inside the countdown keeps the menu open (as on the device)
    r.wait(1100);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
}

// OTA results reach the menu as a toast
void test_check_result_toast(void) {
    MenuRig r;
    r.key(K_MENU);
    r.e.message("3.1.0 available", r.now);
    TEST_ASSERT_EQUAL_STRING("3.1.0 available", line0(r).c_str());
}

// Installing a build of another channel names the switch in the confirm line
void test_install_confirm_names_the_switch(void) {
    MenuRig r;
    r.be.sig.updateAvailable = true;
    r.be.texts[LT_UPDATE_VERSION] = "3.1.0";
    r.be.texts[LT_UPDATE_CONFIRM] = "Move to the Stable channel?";
    r.key(K_MENU); r.enter("System"); r.enter("Update");
    TEST_ASSERT_TRUE(r.select("Install"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Move to the Stable channel?", line0(r).c_str());
    r.key(K_BACK);
    r.be.texts[LT_UPDATE_CONFIRM] = "";
    TEST_ASSERT_TRUE(r.select("Install"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Install the update now?", line0(r).c_str());
}

// Other channels: a row checks that channel through the backend, with the Checking toast first
void test_other_channel_row_checks(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("Update"); r.enter("Other channels");
    TEST_ASSERT_TRUE(r.select("Beta"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Checking...", line0(r).c_str());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "fn " + std::to_string(FN_CHECK_CHANNEL_BETA)));
}

// The Other channels Install asks first, naming an older build
void test_manual_install_confirms(void) {
    MenuRig r;
    r.be.sig.updateManual = true;
    r.be.texts[LT_MANUAL_VERSION] = "3.0.0";
    r.be.texts[LT_MANUAL_CONFIRM] = "Install an older version?";
    r.key(K_MENU); r.enter("System"); r.enter("Update"); r.enter("Other channels");
    TEST_ASSERT_TRUE(r.select("Install"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Install an older version?", line0(r).c_str());
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "fn " + std::to_string(FN_INSTALL_MANUAL)));
    r.key(K_BACK);
    r.be.texts[LT_MANUAL_CONFIRM] = "";                  // same channel, newer: the node's own question
    TEST_ASSERT_TRUE(r.select("Install"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Install this version?", line0(r).c_str());
}

void test_check_update_shows_toast_before_running(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("Update");
    TEST_ASSERT_TRUE(r.select("Check now"));             // the first row is the build channel
    r.now += 100;
    r.e.key(K_OK, false, r.now);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Checking...", line0(r).c_str());
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    r.e.tick(r.now + 1);
    TEST_ASSERT_EQUAL_STRING("run system.check_update", r.lastLog().c_str());
}

void test_restart_note_toast(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Output"); r.select("External clock");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("apply system.ext_clock=0", r.lastLog().c_str());
    TEST_ASSERT_EQUAL_STRING("Applies after restart", line0(r).c_str());
}

// ---- Overlays, shortcuts, profile lists ----
// Profiles > Save as new > the detected-source row (queues POP_SAVEAS_GENERATED and pops back to Profiles)
static void saveSourceName(MenuRig& r) {
    TEST_ASSERT_TRUE(r.enter("New profile"));
    r.key(K_OK);                                                       // Signal name (first row)
}

static void addProfile(MenuRig& r, uint8_t index, const char* name, bool fp) {
    ProfileEntry& e = r.be.list.at(index);
    e.used = true;
    strncpy(e.name, name, sizeof(e.name) - 1);
    e.hasFp = fp;
}

void test_mute_toggles_and_times_out(void) {
    MenuRig r;
    r.key(K_MUTE);
    TEST_ASSERT_EQUAL_STRING("apply audio.mute=1", r.lastLog().c_str());
    TEST_ASSERT_EQUAL_UINT8(OV_MUTE, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(1, r.v().value);
    TEST_ASSERT_EQUAL_STRING("Mute on", line0(r).c_str());
    TEST_ASSERT_FALSE(r.v().overMenu);
    r.wait(3100);
    TEST_ASSERT_FALSE(r.e.busy());
}

void test_volume_unmutes_and_clamps(void) {
    MenuRig r;
    r.be.apply(SID_audio_mute, 1); r.be.settle(); r.be.log.clear();
    r.key(K_VOLUP);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply audio.mute=0"));
    TEST_ASSERT_EQUAL_STRING("apply audio.volume=82", r.lastLog().c_str());
    TEST_ASSERT_EQUAL_UINT8(OV_VOLUME, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(82, r.v().value);
    r.be.apply(SID_audio_volume, 99); r.be.settle(); r.be.log.clear();
    r.key(K_VOLUP, true);
    TEST_ASSERT_EQUAL_STRING("apply audio.volume=100", r.lastLog().c_str());   // clamped at 100 %
    r.be.settle(); r.be.log.clear();
    r.key(K_VOLUP, true);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    TEST_ASSERT_EQUAL_INT(100, r.v().value);
}

// Rates not measured yet (a new source, the first ~1.5 s): dashes, not "0.0Hz"
void test_info_unstable_lines_show_dashes(void) {
    MenuRig r;
    r.be.sig.srcLines = 0; r.be.sig.srcHzX10 = 497; r.be.sig.presetId = 0x02; r.be.sig.outHzX10 = 497;
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_STRING("IN   RGB SCART --  49.7Hz", r.v().lines[0]);
}

void test_info_rates_not_measured_show_dashes(void) {
    MenuRig r;
    r.be.sig.srcLines = 312; r.be.sig.srcHzX10 = 0; r.be.sig.presetId = 0x02; r.be.sig.outHzX10 = 0;
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("IN   RGB SCART 288p  --.-Hz", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("OUT  1280x1024  --.-Hz", r.v().lines[1]);
}

void test_info_lines_for_rgb_scart_240p(void) {
    MenuRig r;
    r.be.sig.srcLines = 262; r.be.sig.srcHzX10 = 599; r.be.sig.presetId = 0x01; r.be.sig.outHzX10 = 600;
    r.be.sig.frameLocked = true; r.be.sig.syncType = 0; r.be.sig.sogLevel = 5;
    r.be.texts[LT_PROFILE_ACTIVE] = "Mega Drive"; r.be.texts[LT_WIFI_SSID] = "home_net";
    r.be.texts[LT_WIFI_IP] = "192.168.0.99";
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    // no SYNC/STRP rows (not useful), the IP instead
    TEST_ASSERT_EQUAL_STRING("IN   RGB SCART 240p  59.9Hz", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("OUT  1280x960  60.0Hz LOCK", r.v().lines[1]);
    TEST_ASSERT_EQUAL_STRING("PROF Mega Drive", r.v().lines[2]);
    TEST_ASSERT_EQUAL_STRING("NET  home net", r.v().lines[3]);
    TEST_ASSERT_EQUAL_STRING("IP   192.168.0.99", r.v().lines[4]);
    TEST_ASSERT_EQUAL_UINT8(5, r.v().lineCount);
}

void test_no_signal_menu_opens_info_only(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(VIEW_OVERLAY, r.v().mode);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    TEST_ASSERT_FALSE(r.v().overMenu);
    TEST_ASSERT_EQUAL_STRING("IN   No signal", line0(r).c_str());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal enter"));
}

void test_no_signal_exit_on_every_close_path(void) {
    const uint8_t closers[2] = {K_MENU, K_BACK};
    for (uint8_t c = 0; c < 3; c++) {
        MenuRig r;
        r.be.sig.noSignal = true;
        r.key(K_MENU);
        if (c < 2) r.key(closers[c]);
        else r.wait(16100);
        TEST_ASSERT_FALSE(r.e.busy());
        TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal exit 1"));
        r.wait(20000);
        TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal exit 1"));
    }
}

void test_save_key_opens_profiles(void) {
    MenuRig r;
    r.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
}

void test_update_overlay_locks_keys(void) {
    MenuRig r;
    r.key(K_MENU);
    r.be.sig.updatePct = 40;
    r.wait(300);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(40, r.v().value);
    r.key(K_MENU); r.key(K_BACK);
    r.wait(60000);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, r.v().overlay);
    r.be.sig.updatePct = -1;
    r.wait(300);
    TEST_ASSERT_FALSE(r.e.busy());
}

void test_adjust_overlay_runs_mapped_actions_and_pauses_timeout(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Screen"); r.select("Size");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_ADJUST, r.v().overlay);
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("run screen.scale_left", r.lastLog().c_str());
    r.key(K_UP, true);
    TEST_ASSERT_EQUAL_STRING("run screen.scale_down", r.lastLog().c_str());
    r.wait(20000);
    TEST_ASSERT_EQUAL_UINT8(OV_ADJUST, r.v().overlay);
    r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_STRING("run screen.scale_left", r.lastLog().c_str());
    r.key(K_CLICK); r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_STRING("run screen.scale_up", r.lastLog().c_str());   // vertical axis: down key
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Screen", r.title().c_str());
}

void test_profile_load_list_and_result_toast(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", false);
    addProfile(r, 1, "Mario", false);
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.enter("Load"));
    TEST_ASSERT_EQUAL_STRING("Mario", r.label(1).c_str());             // after Defaults
    TEST_ASSERT_EQUAL_STRING("Zelda", r.label(2).c_str());
    r.key(K_DOWN); r.key(K_DOWN); r.key(K_OK);
    char expect[24]; snprintf(expect, sizeof(expect), "profile %u 0", POP_LOAD);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
    r.be.resultSeq++;
    r.wait(300);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Done", line0(r).c_str());
}

// Rows read "<name without its region word> <region>"; the value is the Active/Unsaved/Match tag
void test_profile_rows_show_the_region_once(void) {
    MenuRig r;
    addProfile(r, 0, "Genesis/Mega Drive NTSC", false);          // an old name: the region word is stripped
    r.be.list.at(0).region = PROFILE_REGION_NTSC_U;
    addProfile(r, 1, "Saturn NTSC-J", false);
    r.be.list.at(1).region = PROFILE_REGION_NTSC_J;
    addProfile(r, 2, "Arcade", false);                            // no region
    r.be.active = 1;
    r.key(K_MENU); r.enter("Profiles"); r.enter("Load");
    TEST_ASSERT_EQUAL_STRING("Arcade", r.label(1).c_str());
    TEST_ASSERT_EQUAL_STRING("", r.value(1).c_str());
    TEST_ASSERT_EQUAL_STRING("Genesis/Mega Driv NTSC-U", r.label(2).c_str());   // 24: the name gives way
    TEST_ASSERT_EQUAL_STRING("", r.value(2).c_str());
    TEST_ASSERT_EQUAL_STRING("Saturn NTSC-J", r.label(3).c_str());
    TEST_ASSERT_EQUAL_STRING("Active", r.value(3).c_str());
}

void test_idle_profile_drops_the_region_word(void) {
    MenuRig r;
    addProfile(r, 0, "Genesis/Mega Drive NTSC", false);           // a name saved with the old suffix
    r.be.list.at(0).region = PROFILE_REGION_NTSC_U;
    r.be.active = 0;
    MenuIdle i; r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("Genesis/Mega Drive", i.profile);
    TEST_ASSERT_EQUAL_STRING("NTSC-U", i.region);
}

// S-Video/Composite: without a profile region the label comes from the decoder's detected standard (PAL60
// cannot be told from NTSC by the rate)
void test_idle_region_from_the_decoder_standard(void) {
    MenuRig r;
    r.be.apply(SID_input_source, 6); r.be.settle();               // Composite
    r.be.sig.srcHzX10 = 599;
    r.be.sig.advStandard = 3;                                     // AD_RESULT: PAL 60
    MenuIdle i; r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("PAL60", i.region);
    r.be.sig.advStandard = 0;
    r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("NTSC", i.region);
    r.be.sig.advStandard = -1;                                    // no lock: from the rate as before
    r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("NTSC", i.region);
}

void test_profile_result_toast_only_for_menu_ops(void) {
    MenuRig r;
    r.key(K_MENU);
    r.be.resultSeq++;
    r.wait(300);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
}

void test_profile_failure_toast(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles");
    saveSourceName(r);
    char expect[24]; snprintf(expect, sizeof(expect), "profile %u -1", POP_SAVEAS_GENERATED);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
    r.be.result = "storage_full";
    r.be.resultSeq++;
    r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Failed: storage full", line0(r).c_str());
}

void test_profiles_save_active(void) {
    MenuRig r;
    addProfile(r, 3, "Saturn", false);
    r.be.active = 3;
    r.be.texts[LT_PROFILE_ACTIVE] = "Saturn";
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Save changes"));
    TEST_ASSERT_EQUAL_STRING("Saturn", r.selectedValue().c_str());
    r.key(K_OK); r.key(K_LEFT); r.key(K_OK);                          // confirm Yes
    char expect[24]; snprintf(expect, sizeof(expect), "profile %u 3", POP_SAVE);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
}

void test_profile_empty_list_row(void) {   // Manage keeps the empty row; Load always has Defaults
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    TEST_ASSERT_EQUAL_UINT8(1, r.v().rowCount);
    TEST_ASSERT_EQUAL_STRING("No profiles", r.label(0).c_str());
    TEST_ASSERT_TRUE(r.v().rows[0].flags & ROWF_DISABLED);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
}

void test_manage_delete_needs_confirm(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", false);
    addProfile(r, 1, "Mario", true);
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    r.key(K_OK);                                  // Mario (first sorted)
    TEST_ASSERT_EQUAL_STRING("Mario", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Rename", r.label(0).c_str());
    TEST_ASSERT_EQUAL_STRING("Forget signal", r.label(2).c_str());
    TEST_ASSERT_EQUAL_STRING("Delete", r.label(3).c_str());
    TEST_ASSERT_TRUE(r.select("Forget signal")); r.key(K_OK);
    char expect[24]; snprintf(expect, sizeof(expect), "profile %u 1", POP_CLEAR_FP);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
    TEST_ASSERT_TRUE(r.select("Delete")); r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    r.key(K_LEFT); r.key(K_OK);
    snprintf(expect, sizeof(expect), "profile %u 1", POP_DELETE);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
}

void test_manage_forget_signal_disabled_without_fp(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", false);
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    r.key(K_OK); r.key(K_DOWN); r.key(K_DOWN);
    TEST_ASSERT_EQUAL_STRING("Forget signal", r.selected().c_str());
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_DISABLED);
    TEST_ASSERT_EQUAL_STRING("No signal stored", r.v().help);
}

void test_deleting_shown_profile_pops(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", false);
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Zelda", r.title().c_str());
    r.be.list.at(0).used = false;
    r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Manage", r.title().c_str());
}

void test_overlay_priority_toast_waits_under_confirm(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles");
    saveSourceName(r);                            // menu-issued operation
    r.key(K_BACK); r.select("System"); r.key(K_OK); r.select("Restart"); r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    r.be.resultSeq++;
    r.wait(300);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Done", line0(r).c_str());
}

void test_mute_ignored_during_confirm(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.select("Restart"); r.key(K_OK);
    r.key(K_MUTE); r.key(K_VOLUP);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
}

void test_info_over_menu_returns_to_menu(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Output");
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    TEST_ASSERT_TRUE(r.v().overMenu);
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Output", r.title().c_str());
}

void test_every_overlay_closes_on_back_and_timeout(void) {
    struct Case { uint8_t key; uint8_t ov; uint32_t timeout; };
    const Case cases[3] = {{K_MUTE, OV_MUTE, 3000}, {K_VOLUP, OV_VOLUME, 3000}, {K_INFO, OV_INFO, 16000}};
    for (const Case& c : cases) {
        MenuRig a; a.key(c.key); TEST_ASSERT_EQUAL_UINT8(c.ov, a.v().overlay); a.key(K_BACK); TEST_ASSERT_FALSE(a.e.busy());
        MenuRig b; b.key(c.key); b.wait(c.timeout + 200); TEST_ASSERT_FALSE(b.e.busy());
    }
    MenuRig q;                                     // confirmation: BACK, and the menu timeout closes it
    q.key(K_MENU); q.enter("System"); q.select("Restart"); q.key(K_OK);
    q.wait(16200);
    TEST_ASSERT_FALSE(q.e.busy());
}

// osd.timeout: 5 s, 10 s, 15 s (default), 30 s, 60 s, 10 min for the menu and the info panel
void test_osd_timeout_setting(void) {
    MenuRig a;
    a.be.apply(SID_osd_timeout, 0); a.be.settle();              // 5 s
    a.key(K_MENU); a.wait(4500);
    TEST_ASSERT_TRUE(a.e.busy());
    a.wait(700);
    TEST_ASSERT_FALSE(a.e.busy());
    MenuRig d;                                                   // default 15 s
    d.key(K_MENU); d.wait(14500);
    TEST_ASSERT_TRUE(d.e.busy());
    d.wait(700);
    TEST_ASSERT_FALSE(d.e.busy());
    MenuRig n;
    n.be.apply(SID_osd_timeout, 5); n.be.settle();              // 10 min
    n.key(K_MENU); n.wait(120000);
    TEST_ASSERT_TRUE(n.e.busy());
    n.wait(480500);
    TEST_ASSERT_FALSE(n.e.busy());                               // not never: the OLED saver and auto-load resume
    n.key(K_MENU);
    n.key(K_BACK); n.key(K_INFO); n.wait(120000);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, n.v().overlay);             // the info panel follows it
    n.key(K_MUTE); n.wait(3200);
    TEST_ASSERT_NOT_EQUAL(OV_MUTE, n.v().overlay);               // pills keep their 3 s
}

// A short osd.timeout must not undo a change while its 15 s revert countdown is up (a confirmation still
// closes with the timeout: nothing runs)
void test_short_timeout_waits_for_revert_and_confirm(void) {
    MenuRig r;
    r.be.apply(SID_osd_timeout, 0); r.be.settle();              // 5 s
    r.key(K_MENU); r.enter("Output"); r.select("Resolution"); r.key(K_RIGHT); r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);
    r.wait(8000);
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);           // still counting down, nothing undone
}

// Brightness/Contrast/Saturation are 0..100 sliders of the picture settings
void test_picture_levels_are_setting_sliders(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Brightness"));
    const MenuViewRow& row = r.v().rows[r.v().cursorRow];
    TEST_ASSERT_EQUAL_UINT8(ROWK_SLIDER, row.kind);
    TEST_ASSERT_EQUAL_INT8(10, row.slider);                        // 50 of 100: half way
    TEST_ASSERT_EQUAL_STRING("50", row.value);
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT16(51, r.be.value(SID_pic_brightness));
}

// Reset picture: 12 writes in one batch, after a confirmation
void test_reset_picture_writes_every_setting(void) {
    MenuRig r;
    r.be.apply(SID_pic_brightness, 70); r.be.apply(SID_pic_red, 10); r.be.settle();
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Reset picture"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    r.be.log.clear();
    r.key(K_LEFT); r.key(K_OK);                                    // Yes
    size_t applies = 0;
    for (auto& l : r.be.log) if (l.rfind("apply ", 0) == 0) applies++;
    TEST_ASSERT_EQUAL_UINT32(12, applies);
    TEST_ASSERT_EQUAL_UINT16(50, r.be.value(SID_pic_brightness));
    TEST_ASSERT_EQUAL_UINT16(128, r.be.value(SID_pic_red));
    r.be.log.clear();
    r.be.queueFull = true;                                         // a full queue: the busy toast, nothing written
    r.key(K_OK); r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_TRUE(r.be.log.empty() || r.be.log.back().rfind("apply ", 0) != 0);
}

// Every confirm is a real question of its own (28 cells, Yes/No below)
void test_confirm_line_for_a_long_label(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("System"); r.enter("WiFi"); TEST_ASSERT_TRUE(r.select("Reset to hotspot"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Forget WiFi and use hotspot?", line0(r).c_str());
    r.key(K_BACK); r.key(K_MENU); r.key(K_MENU); r.enter("System"); r.select("Factory reset"); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Erase settings and profiles?", line0(r).c_str());
}

// Reset picture is all or nothing: with room for only 8 pairs nothing is written (no half-reset page)
void test_reset_picture_is_atomic(void) {
    MenuRig r;
    r.be.apply(SID_pic_brightness, 70); r.be.apply(SID_pic_red, 10); r.be.settle();
    r.be.queueRoom = 8;
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Reset picture"));
    r.key(K_OK); r.be.log.clear();
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "apply picture.brightness=50"));
    TEST_ASSERT_EQUAL_UINT16(70, r.be.value(SID_pic_brightness));
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);                 // busy
}

// An Easy page of 7 rows plus the encoder's Back row: every row is reachable
void test_easy_page_with_encoder_back_row(void) {
    MenuRig r;
    r.be.apply(SID_sys_menu_mode, 0); r.be.settle();
    r.e.setEncoderBackRow(true);
    r.openFull();
    TEST_ASSERT_TRUE(r.select("Picture")); r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());
    r.key(K_ROT_L);
    std::string seen = r.selected();
    for (int i = 0; i < 7; i++) { r.key(K_ROT_R); seen += ", " + r.selected(); }
    TEST_ASSERT_EQUAL_STRING("Back, Filter, Scanlines, Brightness, Contrast, Saturation, Deinterlace, Reset picture",
                             seen.c_str());
    TEST_ASSERT_EQUAL_UINT8(MENU_VIEW_ROWS, r.v().rowCount);
}

void test_busy_reflects_views(void) {
    MenuRig r;
    TEST_ASSERT_FALSE(r.e.busy());
    r.key(K_INFO);
    TEST_ASSERT_TRUE(r.e.busy());
    r.key(K_BACK);
    TEST_ASSERT_FALSE(r.e.busy());
}

// ---- Pending edits, revert settle, info panel ----
void test_save_during_pending_edit_does_not_leak(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Input"); r.select("Source");
    r.key(K_RIGHT);                                // pending value, not applied
    r.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    r.key(K_OK);
    for (auto& l : r.be.log) TEST_ASSERT_TRUE_MESSAGE(l.compare(0, 6, "apply ") != 0, l.c_str());
}

void test_revert_settle_releases_when_value_changes_elsewhere(void) {
    MenuRig r;
    openRevert(r);
    r.key(K_BACK);                                 // undo queued
    r.be.apply(SID_output_preset, 5);              // something else changes it (web, profile load)
    r.be.settle();
    r.wait(300);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "hold 0"));
}

void test_revert_settle_gives_up_after_5s(void) {
    MenuRig r;
    openRevert(r);
    r.key(K_BACK);
    r.wait(4000);
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "hold 0"));
    r.wait(1200);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "hold 0"));
}

void test_open_info_after_idle_stays_open(void) {
    MenuRig r;
    r.wait(20000);
    r.e.openInfo(r.now);
    r.wait(300);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
}

// ---- Row kinds, position, depth ----
static uint8_t kindOf(const MenuRig& r, const std::string& label) {
    for (uint8_t i = 0; i < r.v().rowCount; i++) if (label == r.v().rows[i].label) return r.v().rows[i].kind;
    return 0xFF;
}

void test_row_kinds(void) {
    MenuRig r;
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(ROWK_SUB, r.v().rows[0].kind);
    TEST_ASSERT_FALSE(r.v().deep);
    r.enter("Picture");
    TEST_ASSERT_TRUE(r.v().deep);
    TEST_ASSERT_EQUAL_UINT8(ROWK_SLIDER, kindOf(r, "Brightness"));   // picture.brightness 0..100
    r.select("Red");
    TEST_ASSERT_EQUAL_UINT8(ROWK_HEADER, kindOf(r, "Color"));
    TEST_ASSERT_EQUAL_UINT8(ROWK_SLIDER, kindOf(r, "Red"));
    r.select("Reset colors");
    TEST_ASSERT_EQUAL_UINT8(ROWK_ACTION, kindOf(r, "Reset colors"));
    r.select("Sharpness");
    TEST_ASSERT_EQUAL_UINT8(ROWK_SLIDER, kindOf(r, "Sharpness"));   // picture.sharpness 0..31
    r.select("Deinterlace");
    TEST_ASSERT_EQUAL_UINT8(ROWK_CYCLE, kindOf(r, "Deinterlace"));
}

void test_position_counter(void) {
    MenuRig r;
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(1, r.v().posIndex);
    TEST_ASSERT_EQUAL_UINT8(7, r.v().posCount);
    r.enter("Picture");
    TEST_ASSERT_EQUAL_UINT8(1, r.v().posIndex);
    TEST_ASSERT_EQUAL_UINT8(15, r.v().posCount);
    r.key(K_UP);
    TEST_ASSERT_EQUAL_UINT8(15, r.v().posIndex);
}

// ---- Encoder sessions ----
void test_click_opens_menu_when_closed(void) {
    MenuRig r;
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
}

void test_rotation_and_long_do_nothing_when_closed(void) {
    MenuRig r;
    r.key(K_ROT_R); r.key(K_LONG);
    TEST_ASSERT_EQUAL_UINT8(VIEW_CLOSED, r.v().mode);
}



static void clickEnter(MenuRig& r, const char* l) { TEST_ASSERT_TRUE_MESSAGE(r.select(l), l); r.key(K_CLICK); }

// v2 OLED parity: with no signal the encoder still reaches Input (the OSD no-signal path is not entered)
// Boards without the OSD chip have no free-run output to protect: the encoder reaches the full menu as with a signal
void test_click_without_signal_opens_the_menu(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER);
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.be.sig.noSignal = true;
    r.openFull();
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    for (auto& l : r.be.log) TEST_ASSERT_TRUE_MESSAGE(l != "nosignal enter", "no-signal path entered");
    clickEnter(r, "Input");
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
    platformSetCaps(CAPS_DEFAULT);
}

void test_back_row_only_in_click_sessions(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.key(K_MENU);
    r.enter("Input");
    TEST_ASSERT_FALSE(r.v().rows[0].flags & ROWF_BACK);
    r.key(K_MENU);                                        // close
    r.openFull();                                       // encoder session
    TEST_ASSERT_FALSE(r.v().rows[0].flags & ROWF_BACK);   // the root has no Back row
    clickEnter(r, "Input");
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());
    TEST_ASSERT_TRUE(r.v().rows[0].flags & ROWF_BACK);
    TEST_ASSERT_EQUAL_UINT8(1, r.v().cursorRow);         // cursor on the first real row
}

void test_back_row_pops_one_level(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.openFull();
    clickEnter(r, "Input");
    r.key(K_ROT_L);                                       // onto Back
    TEST_ASSERT_EQUAL_STRING("Back", r.selected().c_str());
    TEST_ASSERT_EQUAL_UINT8(0, r.v().posIndex);           // Back has no position
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Input", r.selected().c_str());
    TEST_ASSERT_FALSE(r.v().deep);
}

void test_back_row_off_without_option(void) {
    MenuRig r;
    r.openFull();
    clickEnter(r, "Input");
    TEST_ASSERT_FALSE(r.v().rows[0].flags & ROWF_BACK);
}

void test_back_row_enters_the_right_child_at_depth_two(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.openFull();
    clickEnter(r, "Input");
    clickEnter(r, "Sampling");
    TEST_ASSERT_EQUAL_STRING("Sampling", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());
    r.key(K_LONG); r.key(K_LONG); r.key(K_LONG);
    r.setInput(5);                                        // S-Video: Input > Decoder (last child) visible
    r.openFull();
    clickEnter(r, "Input");
    clickEnter(r, "Decoder");
    TEST_ASSERT_EQUAL_STRING("Decoder", r.title().c_str());
}

void test_back_row_cursor_skips_a_leading_header(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.setInput(5);
    r.openFull();
    clickEnter(r, "Input");
    clickEnter(r, "Decoder");                             // starts with the Levels header
    TEST_ASSERT_EQUAL_STRING("Brightness", r.selected().c_str());
}

void test_back_row_on_empty_profile_list(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.openFull();
    clickEnter(r, "Profiles");
    clickEnter(r, "Manage");                              // the fake has no profiles
    TEST_ASSERT_EQUAL_STRING("Manage", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Back", r.selected().c_str());
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Manage", r.selected().c_str());
}

void test_back_row_cleared_when_reopened_by_menu(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.key(K_CLICK);
    r.key(K_LONG);                                        // a long press closes the quick menu
    r.key(K_MENU);
    r.enter("Input");
    TEST_ASSERT_FALSE(r.v().rows[0].flags & ROWF_BACK);
}

// OLED home screen: source and output resolution in large type, rates and lock
// under them, profile with its region (PAL/NTSC), network (AP marked), volume, Wi-Fi strength
static void setRgbs240p(MenuRig& r) {
    r.be.sig.srcLines = 262; r.be.sig.srcHzX10 = 599; r.be.sig.presetId = 0x01; r.be.sig.outHzX10 = 600;
    r.be.sig.frameLocked = true; r.be.sig.syncType = 1;
    r.be.texts[LT_WIFI_IP] = "192.168.0.99"; r.be.texts[LT_WIFI_MODE] = "Client"; r.be.lives[LV_WIFI_RSSI] = -54;
}

void test_idle_block(void) {
    MenuRig r;
    setRgbs240p(r);
    MenuIdle i;
    r.e.idle(i);
    TEST_ASSERT_FALSE(i.noSignal);
    TEST_ASSERT_EQUAL_STRING("240p", i.inRes);
    TEST_ASSERT_EQUAL_STRING("RGB 59.9Hz", i.inLine);
    TEST_ASSERT_EQUAL_STRING("960p", i.outRes);
    TEST_ASSERT_EQUAL_STRING("60.0Hz", i.outHz);
    TEST_ASSERT_TRUE(i.locked);
    TEST_ASSERT_EQUAL_STRING("", i.profile);
    TEST_ASSERT_EQUAL_STRING("NTSC", i.region);                 // no profile: from the 59.9 Hz source
    TEST_ASSERT_EQUAL_STRING("192.168.0.99", i.net);
    TEST_ASSERT_EQUAL_INT(80, i.volume);
    TEST_ASSERT_FALSE(i.muted);
    TEST_ASSERT_EQUAL_INT(-54, i.rssi);
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_STRING("IN   RGB SCART 240p  59.9Hz", r.v().lines[0]);   // the info panel keeps its text
}

// Issue #35: the 1200p preset ids
void test_idle_and_info_name_1200p(void) {
    MenuRig r;
    setRgbs240p(r);
    r.be.sig.presetId = 0x07;
    MenuIdle i;
    r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("1200p", i.outRes);
    r.be.sig.presetId = 0x17;
    r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("1200p", i.outRes);
    r.key(K_INFO);
    TEST_ASSERT_TRUE(std::string(r.v().lines[1]).find("1920x1200") != std::string::npos);
}

void test_idle_block_profile_region_ap_and_no_signal(void) {
    MenuRig r;
    setRgbs240p(r);
    r.be.list.at(0).used = true;
    strcpy(r.be.list.at(0).name, "Mega Drive");
    r.be.list.at(0).region = PROFILE_REGION_PAL;                 // PAL: the profile's region wins
    r.be.active = 0;
    r.be.texts[LT_WIFI_MODE] = "AP"; r.be.texts[LT_WIFI_IP] = "192.168.4.1";
    r.be.apply(SID_audio_mute, 1); r.be.settle();
    MenuIdle i;
    r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("Mega Drive", i.profile);
    TEST_ASSERT_EQUAL_STRING("PAL", i.region);
    TEST_ASSERT_EQUAL_STRING("AP 192.168.4.1", i.net);
    TEST_ASSERT_TRUE(i.muted);
    r.be.sig.noSignal = true;
    r.be.sig.presetId = 0x05;
    r.e.idle(i);
    TEST_ASSERT_TRUE(i.noSignal);
    TEST_ASSERT_EQUAL_STRING("Input RGB SCART", i.inLine);
    TEST_ASSERT_EQUAL_STRING("PAL", i.region);                   // the profile still says PAL
}

// While Auto searches the OLED home says so instead of naming the path being probed
void test_idle_searching_names_no_path(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.be.sig.searching = true;
    r.be.activeIn = 3;                                            // the scan is probing VGA
    MenuIdle i;
    r.e.idle(i);
    TEST_ASSERT_TRUE(i.noSignal);
    TEST_ASSERT_TRUE(i.searching);
    TEST_ASSERT_EQUAL_STRING("Source: Auto", i.inLine);
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_STRING("IN   No signal", r.v().lines[0]);   // the info panel is unchanged (it pauses the scan)
}

// With no source the decoder's free-run colour bars are a valid signal for the scaler; the ADV Controller report
// (free-run, no lock) makes the OLED and the info panel say "No signal", the OSD stays a normal overlay
void test_idle_adv_free_run_says_no_signal(void) {
    MenuRig r;
    setRgbs240p(r);
    r.be.apply(SID_input_source, 6); r.be.settle();               // Composite
    r.be.sig.advFreeRun = true; r.be.sig.srcLines = 312; r.be.sig.srcHzX10 = 500;
    MenuIdle i;
    r.e.idle(i);
    TEST_ASSERT_TRUE(i.noSignal);
    TEST_ASSERT_EQUAL_STRING("Input Composite", i.inLine);
    TEST_ASSERT_EQUAL_STRING("", i.region);
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_STRING("IN   No signal", r.v().lines[0]);
    for (const std::string& l : r.be.log) TEST_ASSERT_TRUE(l != "nosignal enter");   // no frozen 720x480 output
}

// The firmware version sits in the top bar of the main screens only (root menu, OLED idle block)
void test_version_in_the_main_title(void) {
    MenuRig r;
    MenuIdle i;
    r.e.idle(i);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", i.title);
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
    TEST_ASSERT_TRUE(r.enter("Input"));
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
    MenuRig n;
    n.be.texts[LT_FW_GBS] = "";
    n.key(K_MENU);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO", n.title().c_str());
}

void test_adjust_overlay_exposes_the_axis(void) {
    MenuRig r;
    r.openFull();
    clickEnter(r, "Screen");
    clickEnter(r, "Position");
    TEST_ASSERT_EQUAL_UINT8(OV_ADJUST, r.v().overlay);
    TEST_ASSERT_EQUAL_UINT8(0, r.v().choice);
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_UINT8(1, r.v().choice);
}

// ---- Back row index-shift pins ----
void test_back_row_survives_a_signal_change(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.setInput(5);
    r.openFull();
    clickEnter(r, "Input");
    clickEnter(r, "Decoder");
    TEST_ASSERT_EQUAL_STRING("Decoder", r.title().c_str());
    r.setInput(1);                                        // Decoder disappears
    r.e.onSignalChange(); r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_BACK);
}

void test_save_inside_an_encoder_session(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.openFull();
    r.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_BACK);
}

void test_back_row_in_profile_lists(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", false);
    addProfile(r, 1, "Mario", false);
    r.e.setEncoderBackRow(true);
    r.openFull();
    clickEnter(r, "Profiles");
    clickEnter(r, "Load");
    TEST_ASSERT_EQUAL_STRING("Defaults", r.selected().c_str());   // first real row: Defaults, then by name
    r.key(K_LONG);
    clickEnter(r, "Manage");
    clickEnter(r, "Mario");
    TEST_ASSERT_EQUAL_STRING("Mario", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());
    TEST_ASSERT_EQUAL_STRING("Rename", r.selected().c_str());
    r.key(K_ROT_L); r.key(K_CLICK);                         // Back
    TEST_ASSERT_EQUAL_STRING("Manage", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Mario", r.selected().c_str());
}

// ---- Keyboard ----
static void openWifiRow(MenuRig& r, const char* row) {   // Extra hosts moved to System > WiFi (Advanced)
    r.be.apply(SID_sys_menu_mode, 1);
    r.be.settle();
    r.key(K_MENU);
    r.enter("System");
    r.enter("WiFi");
    TEST_ASSERT_TRUE_MESSAGE(r.select(row), row);
    r.key(K_OK);
}
static void openDevRow(MenuRig& r, const char* row) {
    r.be.dev = true;
    r.key(K_MENU);
    r.enter("Developer");
    TEST_ASSERT_TRUE_MESSAGE(r.select(row), row);
    r.key(K_OK);
}

static void kbType(MenuRig& r, uint8_t row, uint8_t col) {
    for (int i = 0; i < 8 && r.v().value / 16 != row; i++) r.key(r.v().value / 16 < row ? K_DOWN : K_UP);
    for (int i = 0; i < 16 && r.v().value % 16 != col; i++) r.key(K_RIGHT);
    r.key(K_OK);
}

void test_ota_password_keyboard_writes_on_done(void) {
    MenuRig r;
    openDevRow(r, "OTA password");
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("OTA password", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("", r.v().lines[1]);
    kbType(r, 3, 0);                                      // lower case
    kbType(r, 0, 0); kbType(r, 0, 1);                     // "ab"
    TEST_ASSERT_EQUAL_STRING("ab", r.v().lines[1]);
    TEST_ASSERT_EQUAL_UINT8(2, r.v().caret);
    r.key(K_LONG);                                        // Done
    TEST_ASSERT_EQUAL_STRING("ab", r.be.texts[LT_OTA_PASSWORD].c_str());
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Developer", r.title().c_str());
}

// The keyboard opens empty (the password is secret): Done on an empty field keeps the stored password
void test_ota_password_empty_done_keeps_the_password(void) {
    MenuRig r;
    r.be.texts[LT_OTA_PASSWORD] = "secret";
    openDevRow(r, "OTA password");
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_STRING("secret", r.be.texts[LT_OTA_PASSWORD].c_str());
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
}

// MENU while the volume or mute pill is up opens the menu (it used to only close the pill)
void test_menu_key_over_a_pill_opens_the_menu(void) {
    MenuRig r;
    r.key(K_VOLUP);
    TEST_ASSERT_EQUAL_UINT8(OV_VOLUME, r.v().overlay);
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
}

// Any other key closes the volume or mute pill and acts at once, not after the pill's 3 s
void test_other_keys_over_a_pill_act_at_once(void) {
    MenuRig r;
    r.key(K_VOLUP);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());           // OK opens the Quick menu
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    MenuRig s;
    s.key(K_MUTE);
    s.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Profiles", s.title().c_str());
    MenuRig m;
    m.key(K_MENU);
    uint8_t at = m.v().posIndex;
    m.key(K_VOLDOWN);                                                    // the pill over the open menu
    TEST_ASSERT_EQUAL_UINT8(OV_VOLUME, m.v().overlay);
    m.key(K_DOWN);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, m.v().overlay);
    TEST_ASSERT_EQUAL_UINT8(at + 1, m.v().posIndex);                     // the cursor moved
}

void test_back_and_arrows_over_the_volume_pill(void) {
    MenuRig r;
    r.key(K_MENU);
    r.key(K_VOLUP);
    r.key(K_BACK);                                                       // EXIT only closes the pill
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);                      // the menu stays
    r.be.log.clear();
    r.key(K_VOLUP);
    r.key(K_LEFT);                                                       // Left/Right still step the volume
    TEST_ASSERT_EQUAL_UINT8(OV_VOLUME, r.v().overlay);
    int steps = 0;
    for (auto& l : r.be.log) if (l.rfind("apply audio.volume=", 0) == 0) steps++;
    TEST_ASSERT_EQUAL_INT(2, steps);
}

void test_keyboard_back_cancels(void) {
    MenuRig r;
    openDevRow(r, "OTA password");
    kbType(r, 0, 0);
    size_t logBefore = r.be.log.size();
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Developer", r.title().c_str());
    for (size_t i = logBefore; i < r.be.log.size(); i++) TEST_ASSERT_TRUE(r.be.log[i].find("text") != 0);
}

void test_keyboard_menu_key_closes_everything(void) {
    MenuRig r;
    openDevRow(r, "OTA password");
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(VIEW_CLOSED, r.v().mode);
}

void test_keyboard_pauses_menu_timeout(void) {
    MenuRig r;
    openDevRow(r, "OTA password");
    r.wait(20000);
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
}

void test_hosts_keyboard_prefilled(void) {
    MenuRig r;
    r.be.texts[LT_EXTRA_HOSTS] = "a.local";
    openWifiRow(r, "Extra hosts");
    TEST_ASSERT_EQUAL_STRING("Extra hosts", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("a.local", r.v().lines[1]);
}

void test_invalid_done_keeps_keyboard(void) {
    MenuRig r;
    r.be.texts[LT_EXTRA_HOSTS] = "a b";
    openWifiRow(r, "Extra hosts");
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Invalid", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("a b", r.v().lines[1]);
    r.wait(2100);
    TEST_ASSERT_EQUAL_STRING("Extra hosts", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("a b", r.be.texts[LT_EXTRA_HOSTS].c_str());   // nothing written
}

void test_keyboard_busy_keeps_keyboard(void) {
    MenuRig r;
    r.be.textBusy = true;
    openDevRow(r, "OTA password");
    kbType(r, 0, 0);                                      // an empty field would just close (keeps the password)
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Busy - try again", r.v().lines[0]);
}

void test_keyboard_ignores_shortcuts(void) {
    MenuRig r;
    openDevRow(r, "OTA password");
    r.key(K_MUTE); r.key(K_VOLUP); r.key(K_INFO); r.key(K_SAVE);
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
}

// ---- Quick names, catalog, Rename ----
#include "menu-catalog.h"
#include "menu-charset.h"
#include "profile-names.h"

static std::string profLog(uint8_t kind, int index, const char* name, uint8_t region) {
    char b[64]; snprintf(b, sizeof(b), "profile %u %d %s %u", kind, index, name, region);
    return b;
}

void test_catalog_sorted_and_fits(void) {
    TEST_ASSERT_EQUAL_UINT8(28, menuCatalogCount());
    char prev[32] = "", cur[32];
    for (uint8_t i = 0; i < menuCatalogCount(); i++) {
        menuCatalogName(i, cur, sizeof(cur));
        TEST_ASSERT_TRUE_MESSAGE(strlen(cur) + 5 <= 24, cur);
        for (const char* c = cur; *c; c++) TEST_ASSERT_TRUE_MESSAGE(menuCharOk(*c), cur);
        if (i) TEST_ASSERT_TRUE_MESSAGE(strcasecmp(prev, cur) < 0, cur);
        strcpy(prev, cur);
    }
}

void test_new_profile_catalog_rows(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    TEST_ASSERT_EQUAL_STRING("3DO", r.label(3).c_str());
    TEST_ASSERT_EQUAL_STRING("NTSC-U", r.value(3).c_str());
}

// The catalog region follows the source (PAL under Always 60Hz too), PAL60 from the decoder
void test_console_region_default_follows_signal(void) {
    MenuRig r;
    r.be.sig.palSource = true;
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    TEST_ASSERT_EQUAL_STRING("PAL", r.value(3).c_str());
    r.key(K_BACK);
    r.be.sig.palSource = false; r.be.sig.advStandard = 3;
    r.enter("New profile");
    TEST_ASSERT_EQUAL_STRING("PAL60", r.value(3).c_str());
}

void test_console_region_cycle_and_save(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    TEST_ASSERT_TRUE(r.select("Saturn"));
    r.key(K_RIGHT); r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("PAL", r.selectedValue().c_str());
    r.key(K_LEFT);
    TEST_ASSERT_EQUAL_STRING("NTSC-J", r.selectedValue().c_str());
    r.key(K_RIGHT); r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("PAL60", r.selectedValue().c_str());
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("None", r.selectedValue().c_str());
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING("NTSC-U", r.selectedValue().c_str());   // wraps
    r.key(K_RIGHT);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(profLog(POP_SAVEAS, -1, "Saturn", PROFILE_REGION_NTSC_J).c_str(), r.lastLog().c_str());
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());        // popped at once
}

void test_console_region_none_saves_plain_name(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    r.select("Amiga");
    r.key(K_LEFT);                                                   // NTSC-U -> None
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(profLog(POP_SAVEAS, -1, "Amiga", PROFILE_REGION_NONE).c_str(), r.lastLog().c_str());
}

void test_console_encoder_edit(void) {
    MenuRig r;
    r.openFull();
    r.select("Profiles"); r.key(K_CLICK);
    r.select("New profile"); r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.select("Saturn"));
    r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_STRING("NTSC-J", r.selectedValue().c_str());
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING(profLog(POP_SAVEAS, -1, "Saturn", PROFILE_REGION_NTSC_J).c_str(), r.lastLog().c_str());
}

void test_console_encoder_long_cancels_edit(void) {
    MenuRig r;
    r.openFull();
    r.select("Profiles"); r.key(K_CLICK);
    r.select("New profile"); r.key(K_CLICK);
    r.select("Saturn"); r.key(K_CLICK); r.key(K_ROT_R);
    r.key(K_LONG);
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_EDITING);
    TEST_ASSERT_EQUAL_STRING("New profile", r.title().c_str());
    TEST_ASSERT_TRUE(r.lastLog().find("profile") != 0);
}

void test_source_name_row_saves_generated(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles");
    saveSourceName(r);
    char expect[24]; snprintf(expect, sizeof(expect), "profile %u -1", POP_SAVEAS_GENERATED);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
}

void test_edit_name_keyboard_saves(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    r.key(K_DOWN); r.key(K_OK);                                      // Edit name...
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Name", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("RGBs 240p", r.v().lines[1]);
    for (int i = 0; i < 3; i++) kbType(r, 3, MenuKeyboard::SP_DEL);   // Del x3 -> "RGBs 2"
    kbType(r, 1, 10);                                                // X
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_STRING(profLog(POP_SAVEAS, -1, "RGBs 2X", PROFILE_REGION_NONE).c_str(), r.lastLog().c_str());
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
}

void test_rename_flow(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", false);
    addProfile(r, 1, "Mario", false);
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    r.key(K_OK);                                                     // Mario
    TEST_ASSERT_EQUAL_STRING("Rename", r.selected().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_KEYBOARD, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Mario", r.v().lines[1]);
    kbType(r, 2, 2);                                                 // '2'
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_STRING(profLog(POP_RENAME, 1, "Mario2", PROFILE_REGION_NONE).c_str(), r.lastLog().c_str());
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());        // back to Profiles once queued
}

void test_rename_of_a_changed_profile_fails(void) {
    MenuRig r;
    addProfile(r, 1, "Mario", false);
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    r.key(K_OK); r.key(K_OK);                                        // Rename
    strcpy(r.be.list.at(1).name, "Luigi");                           // renamed from the web meanwhile
    r.key(K_LONG);
    TEST_ASSERT_TRUE(r.lastLog().find("profile") != 0);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Failed: changed", r.v().lines[0]);
}

void test_names_list_with_back_row(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.openFull();
    r.select("Profiles"); r.key(K_CLICK);
    r.select("New profile"); r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());
    TEST_ASSERT_EQUAL_STRING("Signal name", r.selected().c_str());
    TEST_ASSERT_TRUE(r.select("3DO"));
    r.key(K_CLICK); r.key(K_CLICK);                                  // edit, save
    TEST_ASSERT_EQUAL_STRING(profLog(POP_SAVEAS, -1, "3DO", PROFILE_REGION_NTSC_U).c_str(), r.lastLog().c_str());
}

void test_signal_change_in_names_list(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    r.be.texts[LT_SOURCE_NAME] = "RGsB 240p";
    r.setInput(2);
    r.e.onSignalChange(); r.wait(300);
    TEST_ASSERT_EQUAL_STRING("New profile", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("RGsB 240p", r.value(0).c_str());
}

// ---- Adjust undo/keep, quick slider ----
static void openAdjust(MenuRig& r, const char* item) {
    r.key(K_MENU); r.enter("Screen"); r.select(item); r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_ADJUST, r.v().overlay);
}

static bool logHas(MenuRig& r, const std::string& line) {
    for (auto& l : r.be.log) if (l == line) return true;
    return false;
}

// Issue #45/#52: no per-standard geometry settings any more; Back undoes by reloading the
// preset of the running mode, OK keeps (the device saves the active profile's registers of this mode)
static std::string fnLine(uint8_t fn) { char b[16]; snprintf(b, sizeof(b), "fn %u", fn); return b; }

void test_adjust_exit_after_a_step_undoes(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_RIGHT);                                       // step action
    r.be.log.clear();
    r.key(K_BACK);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_UNDO)));
    for (auto& l : r.be.log) TEST_ASSERT_TRUE(l.find("apply screen.") != 0);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
}

void test_adjust_exit_without_steps_writes_nothing(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.be.log.clear();
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
}

void test_adjust_ok_keeps(void) {
    MenuRig r;
    openAdjust(r, "Size");
    r.key(K_RIGHT);
    r.be.log.clear();
    r.key(K_OK);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_KEEP)));
    TEST_ASSERT_FALSE(logHas(r, fnLine(FN_GEOMETRY_UNDO)));
}

void test_adjust_long_press_keeps(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_ROT_R);
    r.be.log.clear();
    r.key(K_LONG);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_KEEP)));
}

void test_adjust_menu_key_undoes(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_RIGHT);
    r.be.log.clear();
    r.key(K_MENU);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_UNDO)));
    TEST_ASSERT_EQUAL_UINT8(VIEW_CLOSED, r.v().mode);
}

void test_adjust_timeout_undoes(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_RIGHT);
    r.be.sig.updatePct = 10;                              // the upload overlay takes over
    r.be.log.clear();
    r.wait(10);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_UNDO)));
}

void test_borders_legend_back_keeps_nothing_ok_saves(void) {
    MenuRig r;
    openAdjust(r, "Edge mask");
    TEST_ASSERT_EQUAL_STRING("Arrows adjust  OK done", r.v().lines[2]);
    r.key(K_RIGHT);
    r.be.log.clear();
    r.key(K_BACK);                                        // borders have no undo
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    TEST_ASSERT_TRUE(r.select("Edge mask")); r.key(K_OK);   // the Screen list is still open
    TEST_ASSERT_EQUAL_UINT8(OV_ADJUST, r.v().overlay);
    r.key(K_RIGHT);
    r.be.log.clear();
    r.key(K_OK);                                          // saved with the profile like position and size
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_KEEP)));
}

// Issue #27: Screen > Border position moves the masked window; like Borders: no undo, OK saves
void test_border_position_moves_the_mask(void) {
    MenuRig r;
    openAdjust(r, "Border position");
    TEST_ASSERT_EQUAL_STRING("Arrows adjust  OK done", r.v().lines[2]);
    r.key(K_LEFT);
    TEST_ASSERT_EQUAL_STRING("run screen.border_move_left", r.lastLog().c_str());
    r.key(K_UP);
    TEST_ASSERT_EQUAL_STRING("run screen.border_move_up", r.lastLog().c_str());
    r.be.log.clear();
    r.key(K_OK);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_KEEP)));
}

void test_adjust_legend_for_position(void) {
    MenuRig r;
    openAdjust(r, "Position");
    TEST_ASSERT_EQUAL_STRING("Arrows  OK keep  EXIT undo", r.v().lines[2]);
}

void test_quick_slider_opens_on_ok(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Picture"); r.select("Red");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_SLIDER, r.v().overlay);
    TEST_ASSERT_FALSE(r.v().overMenu && r.v().mode == VIEW_LIST);
    TEST_ASSERT_EQUAL_STRING("Red", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("128", r.v().lines[1]);
    r.key(K_RIGHT);
    TEST_ASSERT_TRUE(r.lastLog().find("=129") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("129", r.v().lines[1]);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Red", r.selected().c_str());
}

void test_quick_slider_opens_on_click(void) {
    MenuRig r;
    r.openFull(); r.select("Picture"); r.key(K_CLICK); r.select("Red");
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_UINT8(OV_SLIDER, r.v().overlay);
    r.key(K_ROT_L);
    TEST_ASSERT_TRUE(r.lastLog().find("=127") != std::string::npos);
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    TEST_ASSERT_EQUAL_STRING("Picture", r.title().c_str());
}

void test_quick_slider_timeout(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Picture"); r.select("Red"); r.key(K_OK);
    r.wait(5100);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
}

void test_quick_slider_back_and_menu(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Picture"); r.select("Red"); r.key(K_OK);
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
    r.key(K_OK);
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(VIEW_CLOSED, r.v().mode);
}

// ---- Auto-load toasts, result waits, toast queue ----
void test_autoload_toast_after_result(void) {
    MenuRig r;
    r.be.apply(SID_osd_status_popup, 0); r.be.settle();   // pop-up Off: the toast names the profile
    r.be.opsPending = 1;                                  // the POP_LOAD was just queued
    r.e.autoloadStarted("Zelda", r.now);
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    r.be.finishOp("");
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Loaded Zelda", r.v().lines[0]);
}

void test_autoload_failure_toast(void) {
    MenuRig r;
    r.be.opsPending = 1;
    r.e.autoloadStarted("Zelda", r.now);
    r.be.finishOp("io");
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Failed: io", r.v().lines[0]);
}

// An auto-load over unsaved changes discards them and says whose (pop-up Off: a second toast)
void test_autoload_discarded_toast(void) {
    MenuRig r;
    r.be.apply(SID_osd_status_popup, 0); r.be.settle();
    r.be.opsPending = 1;
    r.e.autoloadStarted("Mario", r.now, "Zelda");
    r.be.finishOp("");
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Loaded Mario", r.v().lines[0]);
    r.wait(2100);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discarded: Zelda", r.v().lines[0]);
}

void test_autoload_failed_discards_nothing(void) {
    MenuRig r;
    r.be.apply(SID_osd_status_popup, 0); r.be.settle();
    r.be.opsPending = 1;
    r.e.autoloadStarted("Mario", r.now, "Zelda");
    r.be.finishOp("io");                                  // the load failed: the changes are still there
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Failed: io", r.v().lines[0]);
    r.wait(2100);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
}

void test_autoload_does_not_eat_user_result(void) {
    MenuRig r;
    r.be.apply(SID_osd_status_popup, 0); r.be.settle();
    r.key(K_MENU); r.enter("Profiles");
    saveSourceName(r);                                    // user op: result number 1
    r.key(K_MENU);                                        // menu closed
    r.be.opsPending = 2;                                  // user op + the auto-load just queued
    r.e.autoloadStarted("Mario", r.now);                  // result number 2
    r.be.finishOp(""); r.be.finishOp("");                 // both ran in one drain
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Done", r.v().lines[0]);
    r.wait(2100);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Loaded Mario", r.v().lines[0]);
}

void test_web_op_queued_first_does_not_answer_the_menu(void) {
    MenuRig r;
    r.be.opsPending = 1;                                  // a web op is already waiting in the queue
    r.key(K_MENU); r.enter("Profiles");
    saveSourceName(r);                                    // menu op is number 2
    r.be.finishOp("io");                                  // the web op's result
    r.wait(10);
    TEST_ASSERT_TRUE(r.v().overlay != OV_TOAST);
    r.be.finishOp("");
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Done", r.v().lines[0]);
}

void test_wait_times_out(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles");
    saveSourceName(r);
    r.wait(5100);
    TEST_ASSERT_EQUAL_STRING("Failed: timeout", r.v().lines[0]);
}

void test_fresh_toast_is_not_replaced(void) {
    MenuRig r;
    r.e.message("Mario", r.now);
    r.wait(10);
    r.e.message("Zelda", r.now);                          // within 1 s: queued
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Mario", r.v().lines[0]);
    r.wait(2100);
    TEST_ASSERT_EQUAL_STRING("Zelda", r.v().lines[0]);
}

// ---- Adjust steps vs the slow legacy command queue ----
void test_adjust_step_dropped_while_one_is_in_flight(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_RIGHT);
    r.be.actionsBusy = true;                              // the first step has not run yet
    r.be.log.clear();
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
}

void test_adjust_undo_waits_for_steps_in_flight(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_RIGHT);
    r.be.actionsBusy = true;
    r.be.log.clear();
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);       // closes at once
    TEST_ASSERT_FALSE(logHas(r, fnLine(FN_GEOMETRY_UNDO)));
    r.be.actionsBusy = false;
    r.wait(10);
    TEST_ASSERT_TRUE(logHas(r, fnLine(FN_GEOMETRY_UNDO)));
}

void test_adjust_keep_waits_for_steps_in_flight(void) {
    MenuRig r;
    openAdjust(r, "Position");
    r.key(K_RIGHT);
    r.be.actionsBusy = true;
    r.be.log.clear();
    r.key(K_OK);
    char fn[16]; snprintf(fn, sizeof(fn), "fn %u", FN_GEOMETRY_KEEP);
    TEST_ASSERT_FALSE(logHas(r, fn));
    r.be.actionsBusy = false;
    r.wait(10);
    TEST_ASSERT_TRUE(logHas(r, fn));
}

void test_device_notice_toast(void) {
    MenuRig r;
    r.e.notice(S_T_ADV_CONTROLLER_OLD, r.now);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Flash ADV Controller via USB", r.v().lines[0]);
}

// Console log: every toast reaches the hook, shown or queued behind a fresh one
static std::vector<std::string> gToastLog;
static void toastHook(const char* text) { gToastLog.push_back(text); }

void test_toast_hook_sees_every_toast(void) {
    MenuRig r;
    gToastLog.clear();
    r.e.setToastHook(toastHook);
    r.e.notice(S_T_2X_UNSUPPORTED, r.now);
    r.e.notice(S_T_ADV_CONTROLLER_OLD, r.now);          // the first is still fresh: this one waits
    TEST_ASSERT_EQUAL_size_t(2, gToastLog.size());
    TEST_ASSERT_EQUAL_STRING("2X off: not for this source", gToastLog[0].c_str());
    TEST_ASSERT_EQUAL_STRING("Flash ADV Controller via USB", gToastLog[1].c_str());
    r.e.setToastHook(nullptr);
}

// ---- ADV Controller update prompt ----
static bool fnLogged(MenuRig& r, uint8_t fn) {
    char b[16]; snprintf(b, sizeof(b), "fn %u", fn);
    for (auto& l : r.be.log) if (l == b) return true;
    return false;
}

void test_adv_controller_prompt_ok_on_default_is_not_now(void) {
    MenuRig r;
    r.e.promptAdvController(r.now);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Update the ADV Controller?", r.v().lines[0]);  // the offer says what it does
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_FALSE(fnLogged(r, FN_ADV_CONTROLLER_SKIP));
    TEST_ASSERT_TRUE(r.lastLog().find("run") != 0);
}

void test_adv_controller_prompt_moved_to_no_skips(void) {
    MenuRig r;
    r.e.promptAdvController(r.now);
    r.key(K_LEFT); r.key(K_RIGHT); r.key(K_OK);
    TEST_ASSERT_TRUE(fnLogged(r, FN_ADV_CONTROLLER_SKIP));
}

void test_adv_controller_prompt_yes_starts(void) {
    MenuRig r;
    r.e.promptAdvController(r.now);
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("run system.update_adv_controller", r.lastLog().c_str());
}

void test_adv_controller_prompt_timeout_and_back_do_not_skip(void) {
    MenuRig r;
    r.e.promptAdvController(r.now);
    r.wait(17000);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_FALSE(fnLogged(r, FN_ADV_CONTROLLER_SKIP));
    r.e.promptAdvController(r.now);
    r.key(K_LEFT); r.key(K_RIGHT); r.key(K_BACK);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_FALSE(fnLogged(r, FN_ADV_CONTROLLER_SKIP));
}

// A key on the prompt restarts its 16 s: it closed under the user's finger while choosing
void test_adv_controller_prompt_key_restarts_the_timeout(void) {
    MenuRig r;
    r.e.promptAdvController(r.now);
    r.wait(MenuEngine::kPromptMs - 2000);
    r.key(K_RIGHT);
    r.wait(MenuEngine::kPromptMs - 1000);                           // past the first 16 s, within 16 s of the key
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    r.wait(2000);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);                // untouched again: it closes
    TEST_ASSERT_FALSE(fnLogged(r, FN_ADV_CONTROLLER_SKIP));
}

void test_adv_controller_prompt_not_over_an_open_menu(void) {
    MenuRig r;
    r.key(K_MENU);
    r.e.promptAdvController(r.now);
    TEST_ASSERT_EQUAL_UINT8(VIEW_LIST, r.v().mode);
}

void test_adv_controller_menu_row_follows_availability(void) {
    MenuRig r;
    r.be.texts[LT_FW_ADV] = "3.2.0";
    r.key(K_MENU); r.enter("System"); r.enter("Update");
    TEST_ASSERT_TRUE(r.select("ADV Controller"));                   // the installed version
    TEST_ASSERT_EQUAL_UINT8(ROWK_LIVE, r.v().rows[r.v().cursorRow].kind);
    TEST_ASSERT_EQUAL_STRING("3.2.0", r.selectedValue().c_str());
    TEST_ASSERT_FALSE(r.select("ADV update"));                      // no bundled build to install
    r.key(K_MENU);
    r.be.sig.advControllerUpdateAvailable = true;
    r.key(K_MENU); r.enter("System"); r.enter("Update");
    TEST_ASSERT_TRUE(r.select("ADV Controller"));                   // the installed version stays
    TEST_ASSERT_EQUAL_STRING("3.2.0", r.selectedValue().c_str());
    TEST_ASSERT_TRUE(r.select("ADV update"));                       // its own row installs the bundled build
    TEST_ASSERT_EQUAL_UINT8(ROWK_ACTION, r.v().rows[r.v().cursorRow].kind);
    TEST_ASSERT_EQUAL_STRING("3.0.1", r.selectedValue().c_str());
}

// The update screen names the firmware being written: GBS-Control (ESP8266) or ADV Controller
void test_update_overlay_title_for_adv_controller(void) {
    MenuRig r;
    r.be.sig.updatePct = 10; r.be.sig.updateAdvController = true; r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Updating ADV Controller", r.v().lines[0]);
}

// The update screen stopped at 97-98 % and the device restarted with no outcome shown. The title keeps
// "Updating ..."; the outcome replaces the bar, centred
void test_update_overlay_shows_verify_and_done(void) {
    MenuRig r;
    r.be.sig.updatePct = 100; r.be.sig.updateStage = UPD_VERIFYING; r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Updating GBS-Control", r.v().lines[0]);
    TEST_ASSERT_EQUAL_UINT8(UPV_VERIFYING, r.v().choice);
    TEST_ASSERT_EQUAL_STRING("Verifying", r.v().lines[1]);
    r.be.sig.updateStage = UPD_DONE; r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Updating GBS-Control", r.v().lines[0]);
    TEST_ASSERT_EQUAL_UINT8(UPV_DONE, r.v().choice);
    TEST_ASSERT_EQUAL_STRING("Update successful", r.v().lines[1]);
}

void test_update_overlay_progress_has_no_status_line(void) {
    MenuRig r;
    r.be.sig.updatePct = 40; r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(UPV_PROGRESS, r.v().choice);
    TEST_ASSERT_EQUAL_INT(40, r.v().value);
}

void test_update_overlay_adv_controller_outcome_then_closes(void) {
    MenuRig r;
    r.be.sig.updatePct = 90; r.be.sig.updateAdvController = true; r.wait(10);
    r.be.sig.updatePct = -1; r.be.sig.updateAdvController = false;     // the updater finished
    r.e.updateResult(true, true, r.now);
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Updating ADV Controller", r.v().lines[0]);
    TEST_ASSERT_EQUAL_UINT8(UPV_DONE, r.v().choice);
    TEST_ASSERT_EQUAL_STRING("Update successful", r.v().lines[1]);
    r.wait(MenuEngine::kUpdateResultMs);
    TEST_ASSERT_FALSE(r.e.busy());
}

void test_update_overlay_shows_failure(void) {
    MenuRig r;
    r.be.sig.updatePct = 30; r.wait(10);
    r.be.sig.updatePct = -1;                                              // the ESP upload failed
    r.e.updateResult(false, false, r.now);
    r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Updating GBS-Control", r.v().lines[0]);
    TEST_ASSERT_EQUAL_UINT8(UPV_FAILED, r.v().choice);
    TEST_ASSERT_EQUAL_STRING("Update failed", r.v().lines[1]);
}

void test_update_result_without_overlay_is_a_toast(void) {
    MenuRig r;
    r.e.updateResult(false, true, r.now);                                 // refused before it started
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
}

void test_update_overlay_title_for_the_esp(void) {
    MenuRig r;
    r.be.sig.updatePct = 10; r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Updating GBS-Control", r.v().lines[0]);
}

// The root title is the product name
void test_root_title_is_product_name(void) {
    MenuRig r;
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
}

// Headers with nothing visible under them are not rows; a mode change keeps the stack valid
static bool hasRow(MenuRig& r, const char* label) {
    for (uint8_t i = 0; i < r.v().rowCount; i++) if (r.label(i) == label) return true;
    return false;
}

void test_header_hidden_when_group_empty(void) {
    MenuRig r;
    r.be.apply(SID_sys_menu_mode, 0); r.be.settle();
    r.key(K_MENU);
    r.enter("Output");
    for (int i = 0; i < 10; i++) r.key(K_DOWN);
    TEST_ASSERT_FALSE(hasRow(r, "External clock"));
    uint8_t easyCount = r.v().posCount;
    r.be.apply(SID_sys_menu_mode, 1); r.be.settle(); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(easyCount + 3, r.v().posCount);   // Lock method, Analog out, External clock
}

void test_mode_switch_closes_vanished_page(void) {
    MenuRig r;
    r.setInput(5);
    r.be.apply(SID_sys_menu_mode, 1); r.be.settle();
    r.key(K_MENU);
    r.enter("Input");
    TEST_ASSERT_TRUE(r.enter("Decoder"));
    r.be.apply(SID_sys_menu_mode, 0); r.be.settle(); r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Input", r.title().c_str());              // the Advanced page closed back to its parent
    TEST_ASSERT_FALSE(hasRow(r, "Decoder"));
}

void test_leaving_advanced_toasts_and_keeps_values(void) {
    MenuRig r;
    r.be.apply(SID_sys_menu_mode, 1); r.be.settle();
    r.be.apply(SID_sys_ext_clock, 0); r.be.settle();
    r.key(K_MENU);
    r.be.apply(SID_sys_menu_mode, 0); r.be.settle(); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Advanced changes stay active", r.v().lines[0]);
    TEST_ASSERT_EQUAL_UINT16(0, r.be.value(SID_sys_ext_clock));           // the hidden value still applies
}

// Variable choices (Filter, Scanlines, Resolution) with Custom, revert and commit
static std::string valueOf(MenuRig& r, const char* label) {
    for (uint8_t i = 0; i < r.v().rowCount; i++) if (r.label(i) == label) return r.value(i);
    return "?";
}

void test_choice_custom_label(void) {
    MenuRig r;
    r.be.apply(SID_pic_scanlines, 1); r.be.apply(SID_pic_scan_strength, 0x40); r.be.settle();
    r.key(K_MENU); r.enter("Picture"); r.select("Scanlines");
    TEST_ASSERT_EQUAL_STRING("Custom", r.selectedValue().c_str());
    r.be.apply(SID_pic_scan_strength, 0x30); r.be.settle(); r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Medium", r.selectedValue().c_str());
}

void test_choice_off_matches_on_first_setting(void) {
    MenuRig r;
    r.be.apply(SID_pic_scanlines, 0); r.be.apply(SID_pic_scan_strength, 0x40); r.be.settle();
    r.key(K_MENU); r.enter("Picture"); r.select("Scanlines");
    TEST_ASSERT_EQUAL_STRING("Off", r.selectedValue().c_str());
}

void test_choice_step_from_custom(void) {
    MenuRig r;
    r.be.apply(SID_pic_sharpness, 10); r.be.settle();                 // no Filter entry has sharpness 10
    r.key(K_MENU); r.enter("Picture"); r.select("Filter");
    TEST_ASSERT_EQUAL_STRING("Custom", r.selectedValue().c_str());
    r.key(K_RIGHT); r.be.settle(); r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Normal", r.selectedValue().c_str());    // Right from Custom: the first entry
    TEST_ASSERT_EQUAL_UINT16(22, r.be.value(SID_pic_sharpness));
    r.be.apply(SID_pic_sharpness, 10); r.be.settle(); r.wait(300);
    r.key(K_LEFT); r.be.settle(); r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Smooth", r.selectedValue().c_str());    // Left from Custom: the last entry
    TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_pic_line_filter));
}

static void stepResolution(MenuRig& r, int presses, uint8_t key) {
    for (int i = 0; i < presses; i++) r.key(key);
    r.key(K_OK);                                                       // NF_COMMIT: OK applies the pending value
    r.be.settle(); r.wait(50);
}

void test_choice_hides_advanced_and_capped_options(void) {
    MenuRig r;
    r.be.apply(SID_sys_menu_mode, 0); r.be.apply(SID_output_preset, 1); r.be.settle();
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    const char* seen[12]; int n = 0;
    for (int i = 0; i < 9; i++) { r.key(K_RIGHT); seen[n++] = strdup(r.selectedValue().c_str()); }
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE_MESSAGE(strcmp(seen[i], "960p fixed") && strcmp(seen[i], "1024p fixed"), seen[i]);
        TEST_ASSERT_TRUE_MESSAGE(strcmp(seen[i], "HD bypass") && strcmp(seen[i], "Downscale"), seen[i]);
    }
}

void test_resolution_commit_and_revert_restore_both(void) {
    MenuRig r;
    r.be.apply(SID_output_preset, 0); r.be.apply(SID_output_match, 1); r.be.settle();
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    TEST_ASSERT_EQUAL_STRING("960p/1024p", r.selectedValue().c_str());
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT16(0, r.be.value(SID_output_preset));       // pending only: nothing applied yet
    TEST_ASSERT_EQUAL_STRING("1024p fixed", r.selectedValue().c_str());
    r.key(K_OK); r.be.settle();
    TEST_ASSERT_EQUAL_UINT16(4, r.be.value(SID_output_preset));
    TEST_ASSERT_EQUAL_UINT16(0, r.be.value(SID_output_match));
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);
    r.wait(16000); r.be.settle(); r.wait(50);                         // the countdown expires: both settings back
    TEST_ASSERT_EQUAL_UINT16(0, r.be.value(SID_output_preset));
    TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_output_match));
}

void test_resolution_alias_and_base_preset(void) {
    MenuRig r;
    r.be.apply(SID_output_preset, 4); r.be.apply(SID_output_match, 1); r.be.settle();
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    TEST_ASSERT_EQUAL_STRING("960p/1024p", r.selectedValue().c_str());  // 1024p with matching behaves as 960p/1024p
    r.be.apply(SID_output_preset, 2); r.be.settle(); r.be.base = 5; r.wait(300);
    TEST_ASSERT_EQUAL_STRING("1080p", r.selectedValue().c_str());       // a profile (Custom) shows its base preset
}

// A choice writes only the settings whose value changes: an unchanged setting must not run its apply group
// again (line filter and scanlines are APPLY_PRESET: a rewrite reloads the preset and blanks the screen)
void test_choice_writes_only_changed_settings(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Filter"));
    r.be.log.clear();
    r.key(K_RIGHT);                                                    // Normal -> Sharp
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply picture.sharpness=31"));
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "apply picture.line_filter=0"));
    r.be.apply(SID_pic_scanlines, 1); r.be.apply(SID_pic_scan_strength, 0x50); r.be.settle();
    TEST_ASSERT_TRUE(r.select("Scanlines"));
    r.be.log.clear();
    r.key(K_RIGHT);                                                    // Light -> Medium
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply picture.scanline_strength=48"));
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "apply picture.scanlines=1"));
}

// Strong looked lighter. MADPT_Y/UV_MI_OFFSET is a mixing offset (0xff = no scanlines, upstream steps it down for
// stronger lines): a stronger choice writes a lower value
void test_scanlines_stronger_writes_a_lower_offset(void) {
    MenuRig r;
    r.be.apply(SID_pic_scanlines, 0); r.be.settle();
    r.key(K_MENU); r.enter("Picture"); TEST_ASSERT_TRUE(r.select("Scanlines"));
    uint16_t s[3];
    const char* names[3] = {"Light", "Medium", "Strong"};
    for (int i = 0; i < 3; i++) {
        r.key(K_RIGHT); r.be.settle(); r.wait(300);
        TEST_ASSERT_EQUAL_STRING(names[i], r.selectedValue().c_str());
        s[i] = r.be.value(SID_pic_scan_strength);
    }
    TEST_ASSERT_EQUAL_UINT16(0x30, s[1]);                 // Medium stays the upstream default
    TEST_ASSERT_TRUE(s[0] > s[1]);
    TEST_ASSERT_TRUE(s[1] > s[2]);
}

// Steps inside the revert overlay may touch a setting the first change did not: the undo restores it too
void test_resolution_revert_after_several_steps(void) {
    MenuRig r;
    r.be.apply(SID_output_preset, 1); r.be.apply(SID_output_match, 0); r.be.settle();
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    r.key(K_RIGHT); r.key(K_OK); r.be.settle();                        // 720p: only the preset is written
    TEST_ASSERT_EQUAL_UINT16(3, r.be.value(SID_output_preset));
    TEST_ASSERT_EQUAL_UINT8(OV_REVERT, r.v().overlay);
    r.key(K_RIGHT); r.be.settle();                                     // 960p fixed
    r.key(K_RIGHT); r.be.settle();                                     // 960p/1024p: match on
    TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_output_match));
    r.wait(16000); r.be.settle(); r.wait(50);
    TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_output_preset));
    TEST_ASSERT_EQUAL_UINT16(0, r.be.value(SID_output_match));
}

// Original board (no decoder, no OSD): Downscale first and HD bypass last, no S-Video/Composite rows; with a
// profile loaded the row shows the base preset and Left/Right never land on Custom
void test_resolution_on_the_original_board(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER);
    MenuRig r;
    r.be.apply(SID_output_preset, 6); r.be.settle();
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    TEST_ASSERT_EQUAL_STRING("Downscale", r.selectedValue().c_str());
    r.key(K_LEFT);
    TEST_ASSERT_EQUAL_STRING("HD bypass", r.selectedValue().c_str());   // wraps to the last entry
    r.key(K_MENU);
    r.be.apply(SID_output_preset, 2); r.be.settle(); r.be.base = 3; r.be.active = 0;
    r.key(K_MENU); r.enter("Output"); r.select("Resolution");
    TEST_ASSERT_EQUAL_STRING("720p", r.selectedValue().c_str());
    for (int i = 0; i < 12; i++) {
        r.key(K_RIGHT);
        TEST_ASSERT_TRUE_MESSAGE(r.selectedValue() != "Custom", "Custom offered");
    }
    r.key(K_MENU);
    r.key(K_MENU); r.enter("Input");
    TEST_ASSERT_FALSE(r.select("Video standard"));
    TEST_ASSERT_FALSE(r.select("2X"));
    TEST_ASSERT_FALSE(r.select("Decoder"));
    platformSetCaps(CAPS_DEFAULT);
}

// ---- Profiles page ----
static std::string popLog(uint8_t kind, int index) {
    char b[24]; snprintf(b, sizeof(b), "profile %u %d", kind, index); return b;
}

void test_load_list_defaults_first_and_tags(void) {
    MenuRig r;
    addProfile(r, 1, "Mario", false);
    addProfile(r, 2, "Zelda", false);
    r.be.active = 1;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.enter("Load"));
    TEST_ASSERT_EQUAL_STRING("Defaults", r.label(0).c_str());
    TEST_ASSERT_EQUAL_STRING("", r.value(0).c_str());
    TEST_ASSERT_EQUAL_STRING("Mario", r.label(1).c_str());
    TEST_ASSERT_EQUAL_STRING("Active", r.value(1).c_str());
    TEST_ASSERT_EQUAL_STRING("", r.value(2).c_str());
    r.be.modified = true; r.be.matches = {2}; r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Unsaved", r.value(1).c_str());
    TEST_ASSERT_EQUAL_STRING("Match", r.value(2).c_str());
    r.be.active = -1; r.be.modified = false; r.wait(300);
    TEST_ASSERT_EQUAL_STRING("Active", r.value(0).c_str());           // no profile: the defaults are in use
}

void test_load_while_modified_confirms(void) {
    MenuRig r;
    addProfile(r, 1, "Mario", false);
    addProfile(r, 2, "Zelda", false);
    r.be.active = 1; r.be.modified = true;
    r.key(K_MENU); r.enter("Profiles"); r.enter("Load");
    TEST_ASSERT_TRUE(r.select("Zelda"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discard unsaved changes?", line0(r).c_str());
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 2).c_str(), r.lastLog().c_str());
    r.be.modified = false; r.be.log.clear();
    r.key(K_OK);                                                       // unmodified: at once
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 2).c_str(), r.lastLog().c_str());
}

void test_defaults_row_queues_defaults(void) {
    MenuRig r;
    addProfile(r, 1, "Mario", false);
    r.be.active = 1;
    r.key(K_MENU); r.enter("Profiles"); r.enter("Load");
    TEST_ASSERT_EQUAL_STRING("Defaults", r.selected().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_DEFAULTS, -1).c_str(), r.lastLog().c_str());
}

void test_save_changes_confirms_with_name(void) {
    MenuRig r;
    addProfile(r, 3, "Super Mario World 2", false);
    r.be.active = 3;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Save changes"));
    TEST_ASSERT_EQUAL_STRING("Super Mario World 2", r.selectedValue().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Save to Super Mario World 2", line0(r).c_str());   // the question has a row of its own
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_SAVE, 3).c_str(), r.lastLog().c_str());
}

void test_save_changes_confirm_clips_a_long_name_to_the_row(void) {
    MenuRig r;
    addProfile(r, 3, "Sonic the Hedgehog 2 JP", false);
    r.be.active = 3;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Save changes"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Save to Sonic the Hedgehog 2", line0(r).c_str());   // 28 cells, trailing space trimmed
}

void test_save_changes_without_profile_opens_new(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Save changes"));
    TEST_ASSERT_EQUAL_STRING("", r.selectedValue().c_str());           // nothing to save into (not "Defaults")
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("New profile", r.title().c_str());
}

// Discard changes: the profile in use loaded again as it was saved, after the same question as a load over changes
void test_discard_changes_confirms_and_loads_the_profile_again(void) {
    MenuRig r;
    addProfile(r, 3, "Super Mario World 2", false);
    r.be.active = 3;
    r.be.modified = true;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Discard changes"));
    TEST_ASSERT_EQUAL_STRING("Super Mario World 2", r.selectedValue().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discard unsaved changes?", line0(r).c_str());
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 3).c_str(), r.lastLog().c_str());
}

void test_discard_changes_without_changes_only_says_so(void) {
    MenuRig r;
    addProfile(r, 3, "Super Mario World 2", false);
    r.be.active = 3;
    std::string before = r.lastLog();
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Discard changes"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("No unsaved changes", line0(r).c_str());
    TEST_ASSERT_EQUAL_STRING(before.c_str(), r.lastLog().c_str());      // nothing queued
}

void test_discard_changes_without_profile_only_says_so(void) {   // Defaults in use: nothing saved to go back to
    MenuRig r;
    r.be.modified = true;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.select("Discard changes"));
    TEST_ASSERT_EQUAL_STRING("", r.selectedValue().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
}

void test_manage_rows_and_replace_text(void) {
    MenuRig r;
    addProfile(r, 0, "Zelda", true);
    r.key(K_MENU); r.enter("Profiles"); r.enter("Manage");
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Rename", r.label(0).c_str());
    TEST_ASSERT_EQUAL_STRING("Replace with current", r.label(1).c_str());
    TEST_ASSERT_EQUAL_STRING("Forget signal", r.label(2).c_str());
    TEST_ASSERT_EQUAL_STRING("Delete", r.label(3).c_str());
    TEST_ASSERT_TRUE(r.select("Replace with current"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Overwrite for this signal?", line0(r).c_str());   // not this profile's signal
    r.key(K_BACK);
    r.be.matches = {0}; r.wait(300);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Overwrite this profile?", line0(r).c_str());
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_REPLACE, 0).c_str(), r.lastLog().c_str());
}

void test_save_key_cursor(void) {
    MenuRig r;
    r.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("New profile", r.selected().c_str());
    r.key(K_MENU);
    addProfile(r, 3, "Saturn", false);
    r.be.active = 3;
    r.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Save changes", r.selected().c_str());
}

void test_profiles_row_value(void) {
    MenuRig r;
    r.key(K_MENU); TEST_ASSERT_TRUE(r.select("Profiles"));
    TEST_ASSERT_EQUAL_STRING("Defaults", r.selectedValue().c_str());
    addProfile(r, 0, "SNES NTSC-J", false);
    r.be.list.at(0).region = PROFILE_REGION_NTSC_J;
    r.be.active = 0; r.wait(300);
    TEST_ASSERT_EQUAL_STRING("SNES NTSC-J", r.selectedValue().c_str());   // the region word once
}

// ---- New profile and the same-signal prompt ----
void test_new_profile_rows(void) {
    MenuRig r;
    r.key(K_MENU); r.enter("Profiles"); TEST_ASSERT_TRUE(r.enter("New profile"));
    TEST_ASSERT_EQUAL_STRING("Signal name", r.label(0).c_str());
    TEST_ASSERT_EQUAL_STRING("RGBs 240p", r.value(0).c_str());         // the generated name (fake text)
    TEST_ASSERT_EQUAL_STRING("Edit name...", r.label(1).c_str());
    TEST_ASSERT_EQUAL_STRING("Consoles", r.label(2).c_str());
    TEST_ASSERT_TRUE(r.v().rows[2].flags & ROWF_HEADER);
    TEST_ASSERT_EQUAL_STRING("Signal name", r.selected().c_str());
    TEST_ASSERT_EQUAL_STRING("Saves the current settings as a new profile. Pick a name from the list or type your own.", r.v().help);
}

static void openSameSignal(MenuRig& r) {
    addProfile(r, 0, "SNES", true);
    r.be.list.at(0).region = PROFILE_REGION_NTSC_J;
    r.be.matches = {0};
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    r.key(K_OK);                                                       // Signal name
    TEST_ASSERT_EQUAL_UINT8(OV_SAME_SIGNAL, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Same signal as", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("SNES NTSC-J", r.v().lines[1]);
    TEST_ASSERT_EQUAL_UINT8(0, r.v().choice);                          // Keep both
}

void test_same_signal_prompt_keep_both(void) {
    MenuRig r;
    openSameSignal(r);
    r.be.log.clear();
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_SAVEAS_GENERATED, -1).c_str(), r.lastLog().c_str());
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
}

void test_same_signal_prompt_replace(void) {
    MenuRig r;
    openSameSignal(r);
    r.be.log.clear();
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(1, r.v().choice);
    r.key(K_OK);                                                       // the generated name keeps the old name
    TEST_ASSERT_EQUAL_STRING(popLog(POP_REPLACE, 0).c_str(), r.lastLog().c_str());
}

void test_same_signal_prompt_cancel(void) {
    MenuRig r;
    openSameSignal(r);
    r.be.log.clear();
    r.key(K_LEFT);                                                     // wraps to Cancel
    TEST_ASSERT_EQUAL_UINT8(2, r.v().choice);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());
    TEST_ASSERT_EQUAL_STRING("New profile", r.title().c_str());
}

void test_same_signal_replace_with_a_console(void) {
    MenuRig r;
    addProfile(r, 0, "SNES", true);
    r.be.matches = {0};
    r.key(K_MENU); r.enter("Profiles"); r.enter("New profile");
    TEST_ASSERT_TRUE(r.select("3DO"));
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT8(OV_SAME_SIGNAL, r.v().overlay);
    r.key(K_RIGHT); r.key(K_OK);                                       // Replace it: name and region with it
    char expect[48]; snprintf(expect, sizeof(expect), "profile %u 0 3DO %u", POP_REPLACE, PROFILE_REGION_NTSC_U);
    TEST_ASSERT_EQUAL_STRING(expect, r.lastLog().c_str());
}

// ---- Status pop-up ----
static void statusScene(MenuRig& r) {
    setRgbs240p(r);
    addProfile(r, 0, "SNES", true);
    r.be.list.at(0).region = PROFILE_REGION_NTSC_J;
    r.be.active = 0;
}

void test_status_popup_overlay_lines(void) {
    MenuRig r;
    statusScene(r);
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_STATUS, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("RGB SCART 240p NTSC", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("1280x960 60.0Hz SNES NTSC-J", r.v().lines[1]);
    TEST_ASSERT_EQUAL_UINT8(2, r.v().lineCount);
    r.wait(4100);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);                     // 4 s
}

void test_status_popup_not_due_shows_nothing(void) {
    MenuRig r;
    statusScene(r);
    r.e.statusPopup(r.now, false); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    r.be.apply(SID_osd_status_popup, 0); r.be.settle();
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);                     // Off
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER);
    r.be.apply(SID_osd_status_popup, 2); r.be.settle();
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);                     // no OSD chip: no pop-up
    platformSetCaps(CAPS_DEFAULT);
}

void test_load_match_cycles_and_confirms(void) {
    MenuRig r;
    statusScene(r);
    addProfile(r, 5, "SNES", true); r.be.list.at(5).region = PROFILE_REGION_NTSC_U;
    addProfile(r, 7, "SFC", true);
    r.be.matches = {7, 0, 5};                                           // ranked; cycling is by index
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Profile 1 of 3: Left/Right", r.v().lines[2]);
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 5).c_str(), r.lastLog().c_str());
    TEST_ASSERT_EQUAL_UINT8(OV_STATUS, r.v().overlay);                   // stays up
    TEST_ASSERT_EQUAL_STRING("Profile 2 of 3: Left/Right", r.v().lines[2]);
    r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 7).c_str(), r.lastLog().c_str());
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 0).c_str(), r.lastLog().c_str());   // wraps
    r.be.opsPending = 1; r.be.log.clear();
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT32(0, r.be.log.size());                        // a load still queued: ignored
    r.be.opsPending = 0; r.be.modified = true;
    r.key(K_LEFT);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discard unsaved changes?", line0(r).c_str());
    r.key(K_LEFT); r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 7).c_str(), r.lastLog().c_str());
}

// After an auto-load the TV re-syncs first, so the choice line was gone before it could be read: a pop-up that
// offers another profile stays 6 s (from the last Left/Right), a plain one 4 s
void test_status_popup_with_a_choice_stays_6_s(void) {
    MenuRig r;
    statusScene(r);
    addProfile(r, 5, "SNES", true);
    r.be.matches = {0, 5};
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_STRING("Profile 1 of 2: Left/Right", r.v().lines[2]);   // was "1/2 Left Right: other"
    r.wait(4100);
    TEST_ASSERT_EQUAL_UINT8(OV_STATUS, r.v().overlay);                   // still up after 4 s
    r.key(K_RIGHT);                                                      // loads the other one: 6 s again
    r.wait(5000);
    TEST_ASSERT_EQUAL_UINT8(OV_STATUS, r.v().overlay);
    r.wait(1100);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    r.be.active = -1;                                                    // Defaults: none of them is active
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_STRING("2 profiles: Left/Right", r.v().lines[2]);
}

// The auto-load notice ("Kept changes - N64 PAL" then) stayed on the pop-up for good. The device calls statusPopup()
// every pass while statusNoticePending(); the notice stayed pending after the pop-up showed it, so it reopened forever
static void discardedLoad(MenuRig& r, const char* name, const char* old) {
    r.be.opsPending = 1;
    r.e.autoloadStarted(name, r.now, old);
    r.be.finishOp("");
    r.wait(10);
}

void test_discarded_notice_pop_up_closes(void) {
    MenuRig r;
    statusScene(r);
    discardedLoad(r, "MD NTSC-U", "N64 PAL");
    TEST_ASSERT_TRUE(r.e.statusNoticePending());
    for (int i = 0; i < 60; i++) {                                       // 6 s of device passes
        if (r.e.statusNoticePending()) r.e.statusPopup(r.now, false);
        if (i == 1) TEST_ASSERT_EQUAL_STRING("Discarded: N64 PAL", r.v().lines[2]);
        r.wait(100);
    }
    TEST_ASSERT_FALSE(r.e.statusNoticePending());
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);                     // closed after 4 s
}

void test_status_popup_not_blocking(void) {
    MenuRig r;
    statusScene(r);
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_FALSE(r.e.blocking());                                   // auto-load still runs
    r.e.notice(S_T_DONE, r.now);
    TEST_ASSERT_EQUAL_UINT8(OV_STATUS, r.v().overlay);                   // a toast waits behind it
    r.key(K_MENU);                                                       // other keys close it and act
    TEST_ASSERT_TRUE(r.v().overMenu);                                    // the menu opened (the queued toast over it)
    TEST_ASSERT_TRUE(r.e.blocking());
}

// A notice the pop-up never showed (signal gone before the gate) must not name the old profile
// next to a later auto-load that discarded nothing
void test_discarded_notice_does_not_outlive_its_load(void) {
    MenuRig r;
    statusScene(r);
    discardedLoad(r, "MD NTSC-U", "N64 PAL");                            // the pop-up gate never opened
    r.be.opsPending = 1;
    r.e.autoloadStarted("Saturn", r.now);                                // next console, nothing unsaved
    r.be.finishOp("");
    r.wait(10);
    TEST_ASSERT_FALSE(r.e.statusNoticePending());
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_TRUE(r.v().lineCount < 3 || strncmp(r.v().lines[2], "Discarded", 9) != 0);
}

void test_status_popup_discarded_changes(void) {
    MenuRig r;
    statusScene(r);
    discardedLoad(r, "Mario", "Zelda");
    TEST_ASSERT_NOT_EQUAL(OV_TOAST, r.v().overlay);                      // the pop-up carries it
    r.e.statusPopup(r.now, false); r.wait(10);                          // even when not otherwise due
    TEST_ASSERT_EQUAL_UINT8(OV_STATUS, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discarded: Zelda", r.v().lines[2]);
}

void test_status_popup_drops_the_loaded_toast(void) {
    MenuRig r;
    statusScene(r);
    r.be.opsPending = 1;
    r.e.autoloadStarted("Zelda", r.now);
    r.be.finishOp("");
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);                     // the pop-up names the profile instead
}

// A menu opened between the load and the pop-up gate turned the pop-up into a toast that left
// out the loaded profile ("Discarded: X" alone, or nothing at all)
void test_status_popup_busy_names_the_loaded_profile(void) {
    MenuRig r;
    statusScene(r);
    discardedLoad(r, "Mario", "Zelda");
    r.key(K_MENU);                                                       // a menu opens before the gate
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_TRUE(r.v().overMenu);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Loaded Mario", r.v().lines[0]);
    r.wait(2100);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discarded: Zelda", r.v().lines[0]);
}

void test_status_popup_busy_names_a_plain_load(void) {
    MenuRig r;
    statusScene(r);
    r.be.opsPending = 1;
    r.e.autoloadStarted("Mario", r.now);                                 // nothing unsaved
    r.be.finishOp("");
    r.wait(10);
    r.key(K_MENU);
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_TOAST, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Loaded Mario", r.v().lines[0]);
}

void test_status_popup_busy_forgets_an_old_load(void) {
    MenuRig r;
    statusScene(r);
    r.be.opsPending = 1;
    r.e.autoloadStarted("Mario", r.now);
    r.be.finishOp("");
    r.wait(10);
    r.wait(15000);                                                       // the gate opened only much later
    r.key(K_MENU);
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_NOT_EQUAL(OV_TOAST, r.v().overlay);
}

// The discarded notice ages like the loaded name: a pop-up gate that opens much later (the signal went before it)
// does not name an old load next to another signal
void test_discarded_notice_goes_stale(void) {
    MenuRig r;
    statusScene(r);
    discardedLoad(r, "Mario", "Zelda");
    r.wait(15000);
    r.e.statusPopup(r.now, true); r.wait(10);
    TEST_ASSERT_TRUE(r.v().lineCount < 3 || strncmp(r.v().lines[2], "Discarded", 9) != 0);
}

// ---- Quick menu ----
void test_quick_page_opens_on_ok(void) {
    MenuRig r;
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_EQUAL_UINT8(5, r.v().rowCount);
    const char* expect[5] = {"Profile", "Source", "Resolution", "Filter", "Scanlines"};
    for (uint8_t i = 0; i < 5; i++) TEST_ASSERT_EQUAL_STRING(expect[i], r.label(i).c_str());
    TEST_ASSERT_EQUAL_STRING("Defaults", r.value(0).c_str());
    r.key(K_BACK);
    TEST_ASSERT_FALSE(r.e.busy());
    r.setInput(5);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("2X", r.label(5).c_str());                  // S-Video / Composite
    TEST_ASSERT_TRUE(r.e.blocking());                                    // auto-load waits for it
}

void test_quick_menu_to_full_and_back(void) {
    MenuRig r;
    r.key(K_OK);
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
    r.key(K_MENU);
    TEST_ASSERT_FALSE(r.e.busy());
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    r.key(K_LONG);                                                       // a long press closes
    TEST_ASSERT_FALSE(r.e.busy());
}

// The encoder reaches the full menu from an "All settings" row at the bottom
void test_quick_all_settings_row_opens_the_full_menu(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Profile", r.selected().c_str());           // no row above Profile
    TEST_ASSERT_FALSE(r.v().rows[0].flags & ROWF_BACK);
    TEST_ASSERT_EQUAL_UINT8(6, r.v().posCount);
    r.key(K_ROT_L);                                                      // the list wraps: one notch away
    TEST_ASSERT_EQUAL_STRING("All settings", r.selected().c_str());
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_SUB);
    TEST_ASSERT_EQUAL_UINT8(ROWK_SUB, r.v().rows[r.v().cursorRow].kind);   // ">" on both displays
    TEST_ASSERT_EQUAL_STRING("Leaves the Quick menu and opens the full menu with every page.", r.v().help);
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
    TEST_ASSERT_FALSE(r.v().rows[0].flags & ROWF_BACK);                  // the root has no Back row
    clickEnter(r, "Input");
    TEST_ASSERT_EQUAL_STRING("Back", r.label(0).c_str());                // still an encoder session
    r.key(K_LONG); r.key(K_LONG);
    TEST_ASSERT_FALSE(r.e.busy());
}

void test_quick_all_settings_row_only_in_click_sessions(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.key(K_OK);                                                         // the remote has MENU
    TEST_ASSERT_EQUAL_UINT8(5, r.v().posCount);
    TEST_ASSERT_FALSE(r.select("All settings"));
    r.key(K_LONG);                                                       // the long press closes here too
    TEST_ASSERT_FALSE(r.e.busy());
    MenuRig n;                                                           // board without the encoder option
    n.key(K_CLICK);
    TEST_ASSERT_EQUAL_UINT8(5, n.v().posCount);
}

// TV OSD (standard board): the TV5725 icons from the Quick menu
static const uint8_t kStandard = CAP_ORIGINAL | CAP_OLED | CAP_ENCODER;

void test_quick_tv_osd_row_visibility(void) {
    MenuRig pro;
    pro.e.setEncoderBackRow(true);
    pro.key(K_CLICK);
    TEST_ASSERT_FALSE(pro.select("TV OSD"));                             // the Pro has the STV9426 menu
    platformSetCaps(kStandard);
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.select("TV OSD"));
    r.key(K_DOWN);
    TEST_ASSERT_EQUAL_STRING("All settings", r.selected().c_str());     // just above All settings
    r.key(K_LONG);
    r.be.sig.presetId = 0x21;                                            // HD bypass: the scaler draws nothing
    r.key(K_CLICK);
    TEST_ASSERT_FALSE(r.select("TV OSD"));
    platformSetCaps(CAPS_DEFAULT);
}

static void openTvOsd(MenuRig& r) {
    r.e.setEncoderBackRow(true);
    r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.select("TV OSD"));
    r.key(K_CLICK);
}

void test_tv_osd_mode_keys(void) {
    platformSetCaps(kStandard);
    MenuRig r;
    openTvOsd(r);
    TEST_ASSERT_EQUAL_UINT8(OV_TVOSD, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("tvosd 0", r.lastLog().c_str());           // open
    r.key(K_ROT_R);
    TEST_ASSERT_EQUAL_STRING("tvosd 2", r.lastLog().c_str());           // turn: change
    r.key(K_ROT_L);
    TEST_ASSERT_EQUAL_STRING("tvosd 1", r.lastLog().c_str());
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("tvosd 3", r.lastLog().c_str());           // click: next icon
    r.key(K_LONG);
    TEST_ASSERT_EQUAL_STRING("tvosd 4", r.lastLog().c_str());           // hold: leave
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    platformSetCaps(CAPS_DEFAULT);
}

void test_tv_osd_mode_times_out(void) {
    platformSetCaps(kStandard);
    MenuRig r;
    openTvOsd(r);
    r.wait(MenuEngine::kTvOsdMs - 200);
    TEST_ASSERT_EQUAL_UINT8(OV_TVOSD, r.v().overlay);                    // the menu timeout pauses meanwhile
    r.wait(300);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "tvosd 4"));
    TEST_ASSERT_TRUE(r.e.busy());                                        // back on the Quick menu, not closed at once
    platformSetCaps(CAPS_DEFAULT);
}

void test_tv_osd_mode_closes_with_the_menu(void) {
    platformSetCaps(kStandard);
    MenuRig r;
    openTvOsd(r);
    r.be.sig.noSignal = true;                                            // the source went away
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_NONE, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "tvosd 4"));
    r.be.sig.noSignal = false;
    r.key(K_CLICK);                                                      // the Quick menu again
    TEST_ASSERT_TRUE(r.select("TV OSD"));
    r.key(K_CLICK);
    r.be.sig.updatePct = 10;                                             // an upload replaces every overlay
    r.wait(10);
    TEST_ASSERT_EQUAL_UINT8(OV_UPDATE, r.v().overlay);
    TEST_ASSERT_EQUAL_INT(2, countLog(r, "tvosd 4"));
    platformSetCaps(CAPS_DEFAULT);
}

void test_tv_osd_mode_forwards_only_while_open(void) {
    platformSetCaps(kStandard);
    MenuRig r;
    openTvOsd(r);
    r.key(K_LONG);
    size_t n = r.be.log.size();
    r.key(K_ROT_R);                                                      // the Quick menu cursor, not the TV OSD
    for (size_t i = n; i < r.be.log.size(); i++) TEST_ASSERT_TRUE(r.be.log[i].rfind("tvosd", 0) != 0);
    platformSetCaps(CAPS_DEFAULT);
}

void test_quick_all_settings_row_hidden_without_signal(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.be.sig.noSignal = true;
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_FALSE(r.select("All settings"));                         // nothing else is usable on the free-run
}

void test_quick_all_settings_row_goes_when_info_starts_the_free_run(void) {
    MenuRig r;
    r.e.setEncoderBackRow(true);
    r.key(K_CLICK);
    r.key(K_ROT_L);
    TEST_ASSERT_EQUAL_STRING("All settings", r.selected().c_str());
    r.be.sig.noSignal = true;
    r.key(K_INFO); r.key(K_INFO);                                        // the info panel enters the free-run
    TEST_ASSERT_EQUAL_UINT8(5, r.v().posCount);
    TEST_ASSERT_EQUAL_STRING("Scanlines", r.selected().c_str());         // cursor clamped to a real row
}

void test_quick_save_opens_profiles(void) {
    MenuRig r;
    r.key(K_OK);
    r.key(K_SAVE);
    TEST_ASSERT_EQUAL_STRING("Profiles", r.title().c_str());
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());   // inside the full menu
}

void test_quick_profile_cycles_matches(void) {
    MenuRig r;
    addProfile(r, 0, "SNES", true);
    addProfile(r, 4, "SFC", true);
    r.be.active = 0; r.be.matches = {4, 0};
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Profile", r.selected().c_str());
    TEST_ASSERT_EQUAL_STRING("SNES", r.selectedValue().c_str());
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_STRING(popLog(POP_LOAD, 4).c_str(), r.lastLog().c_str());
    r.be.modified = true;
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(OV_CONFIRM, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("Discard unsaved changes?", line0(r).c_str());
}

void test_quick_profile_ok_opens_load(void) {
    MenuRig r;
    r.key(K_OK);
    r.key(K_OK);                                                         // on Profile
    TEST_ASSERT_EQUAL_STRING("Load", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Defaults", r.selected().c_str());
    r.key(K_BACK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
}

void test_quick_source_commits(void) {
    MenuRig r;
    r.key(K_OK); TEST_ASSERT_TRUE(r.select("Source"));
    TEST_ASSERT_EQUAL_STRING("Auto: RGB SCART", r.selectedValue().c_str());
    r.key(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT16(0, r.be.value(SID_input_source));          // pending only
    TEST_ASSERT_EQUAL_STRING("RGB SCART", r.selectedValue().c_str());
    r.key(K_OK);
    TEST_ASSERT_EQUAL_UINT16(1, r.be.value(SID_input_source));
}

// ---- No signal ----
static int countPrefix(const MenuRig& r, const std::string& p) {
    int n = 0;
    for (auto& l : r.be.log) if (l.rfind(p, 0) == 0) n++;
    return n;
}

void test_no_signal_quick_menu_starts_on_source(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_EQUAL_STRING("Source", r.selected().c_str());         // the only row that works without a signal
    r.key(K_BACK);
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("Source", r.selected().c_str());
    r.key(K_LONG);
    r.key(K_MENU);                                                     // the info panel, then OK picks the input
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Source", r.selected().c_str());
}

void test_quick_menu_with_signal_starts_on_profile(void) {
    MenuRig r;
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Profile", r.selected().c_str());
}

void test_no_signal_ok_opens_quick_with_free_run(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal enter"));
    r.key(K_INFO);                                                       // the info panel inside the same session
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    r.key(K_INFO);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal enter"));
    TEST_ASSERT_EQUAL_INT(0, countPrefix(r, "nosignal exit"));
    r.key(K_BACK);
    TEST_ASSERT_FALSE(r.e.busy());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal exit 1"));            // closed: the blanking comes back
}

void test_no_signal_rows_are_na_except_source(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_OK); TEST_ASSERT_TRUE(r.select("Resolution"));
    TEST_ASSERT_TRUE(r.v().rows[r.v().cursorRow].flags & ROWF_DISABLED);
    TEST_ASSERT_EQUAL_STRING("No signal", r.v().help);
    r.be.log.clear();
    r.key(K_RIGHT); r.key(K_OK);
    TEST_ASSERT_EQUAL_INT(0, countPrefix(r, "apply"));                   // no preset reload over the free-run
}

void test_no_signal_source_change_skips_restore(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_OK); TEST_ASSERT_TRUE(r.select("Source"));
    r.key(K_RIGHT); r.key(K_OK);
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "apply input.source=1"));       // from Auto: RGB SCART
    TEST_ASSERT_FALSE(r.e.busy());                                       // closed: detection resumes
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal exit 0"));
}

// A Source write the queue refuses keeps the page and the free-run (no skipped restore)
void test_no_signal_source_refused_keeps_the_page(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_OK); TEST_ASSERT_TRUE(r.select("Source"));
    r.be.queueFull = true;
    r.key(K_RIGHT); r.key(K_OK);
    TEST_ASSERT_TRUE(r.e.busy());
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_EQUAL_INT(0, countPrefix(r, "nosignal exit"));
    r.be.queueFull = false;
    r.key(K_BACK); r.key(K_BACK);                                        // toast, then the page
    TEST_ASSERT_FALSE(r.e.busy());
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal exit 1"));            // nothing changed: the blanking comes back
}

void test_no_signal_long_press_closes_and_menu_opens_info(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_CLICK); r.key(K_LONG);
    TEST_ASSERT_FALSE(r.e.busy());
    TEST_ASSERT_EQUAL_INT(1, countPrefix(r, "nosignal exit"));
    r.key(K_OK); r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    TEST_ASSERT_FALSE(r.v().overMenu);
    TEST_ASSERT_EQUAL_INT(2, countLog(r, "nosignal enter"));
}

void test_no_signal_without_osd_behaves_as_with_signal(void) {
    platformSetCaps(CAP_ORIGINAL | CAP_OLED | CAP_ENCODER);
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_INT(0, countLog(r, "nosignal enter"));
    TEST_ASSERT_TRUE(r.select("Resolution"));
    TEST_ASSERT_FALSE(r.v().rows[r.v().cursorRow].flags & ROWF_DISABLED);
    r.key(K_BACK);
    r.e.setEncoderBackRow(true);
    r.key(K_CLICK);
    TEST_ASSERT_TRUE(r.select("All settings"));                          // no free-run: the full menu is reachable
    r.key(K_CLICK);
    TEST_ASSERT_EQUAL_STRING("GBSC PRO v3.0.0", r.title().c_str());
    platformSetCaps(CAPS_DEFAULT);
}

// With no signal the info panel says how to pick the input, and OK does it
void test_no_signal_info_hints_ok_for_the_input(void) {
    MenuRig r;
    r.be.sig.noSignal = true;
    r.key(K_MENU);
    TEST_ASSERT_EQUAL_UINT8(OV_INFO, r.v().overlay);
    TEST_ASSERT_EQUAL_STRING("IN   No signal", r.v().lines[0]);
    TEST_ASSERT_EQUAL_STRING("OK   Choose the input", r.v().lines[1]);
    r.key(K_OK);
    TEST_ASSERT_EQUAL_STRING("Quick menu", r.title().c_str());
    TEST_ASSERT_TRUE(r.select("Source"));
    TEST_ASSERT_EQUAL_INT(1, countLog(r, "nosignal enter"));             // the same free-run session
    TEST_ASSERT_EQUAL_INT(0, countPrefix(r, "nosignal exit"));
}

void test_signal_info_has_no_input_hint(void) {
    MenuRig r;
    r.key(K_INFO);
    for (uint8_t i = 0; i < r.v().lineCount; i++) TEST_ASSERT_TRUE(strncmp(r.v().lines[i], "OK ", 3) != 0);
    r.key(K_OK);
    TEST_ASSERT_FALSE(r.e.busy());                                       // with a signal OK just closes it
}

// With no active profile the panel says "Defaults", as the main menu's Profiles row
void test_signal_info_profile_defaults_when_none(void) {
    MenuRig r;
    r.be.texts[LT_PROFILE_ACTIVE] = "";
    r.key(K_INFO);
    bool found = false;
    for (uint8_t i = 0; i < r.v().lineCount; i++) found = found || !strcmp(r.v().lines[i], "PROF Defaults");
    TEST_ASSERT_TRUE(found);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_signal_info_profile_defaults_when_none);
    RUN_TEST(test_no_signal_info_hints_ok_for_the_input);
    RUN_TEST(test_signal_info_has_no_input_hint);
    RUN_TEST(test_no_signal_ok_opens_quick_with_free_run);
    RUN_TEST(test_no_signal_quick_menu_starts_on_source);
    RUN_TEST(test_quick_menu_with_signal_starts_on_profile);
    RUN_TEST(test_no_signal_rows_are_na_except_source);
    RUN_TEST(test_no_signal_source_change_skips_restore);
    RUN_TEST(test_no_signal_source_refused_keeps_the_page);
    RUN_TEST(test_no_signal_long_press_closes_and_menu_opens_info);
    RUN_TEST(test_no_signal_without_osd_behaves_as_with_signal);
    RUN_TEST(test_quick_page_opens_on_ok);
    RUN_TEST(test_quick_menu_to_full_and_back);
    RUN_TEST(test_quick_all_settings_row_opens_the_full_menu);
    RUN_TEST(test_quick_all_settings_row_only_in_click_sessions);
    RUN_TEST(test_quick_tv_osd_row_visibility);
    RUN_TEST(test_tv_osd_mode_keys);
    RUN_TEST(test_tv_osd_mode_times_out);
    RUN_TEST(test_tv_osd_mode_closes_with_the_menu);
    RUN_TEST(test_tv_osd_mode_forwards_only_while_open);
    RUN_TEST(test_quick_all_settings_row_hidden_without_signal);
    RUN_TEST(test_quick_all_settings_row_goes_when_info_starts_the_free_run);
    RUN_TEST(test_quick_save_opens_profiles);
    RUN_TEST(test_quick_profile_cycles_matches);
    RUN_TEST(test_quick_profile_ok_opens_load);
    RUN_TEST(test_quick_source_commits);
    RUN_TEST(test_status_popup_overlay_lines);
    RUN_TEST(test_status_popup_not_due_shows_nothing);
    RUN_TEST(test_load_match_cycles_and_confirms);
    RUN_TEST(test_status_popup_with_a_choice_stays_6_s);
    RUN_TEST(test_discarded_notice_pop_up_closes);
    RUN_TEST(test_status_popup_not_blocking);
    RUN_TEST(test_status_popup_discarded_changes);
    RUN_TEST(test_discarded_notice_does_not_outlive_its_load);
    RUN_TEST(test_status_popup_drops_the_loaded_toast);
    RUN_TEST(test_status_popup_busy_names_the_loaded_profile);
    RUN_TEST(test_status_popup_busy_names_a_plain_load);
    RUN_TEST(test_status_popup_busy_forgets_an_old_load);
    RUN_TEST(test_discarded_notice_goes_stale);
    RUN_TEST(test_new_profile_rows);
    RUN_TEST(test_same_signal_prompt_keep_both);
    RUN_TEST(test_same_signal_prompt_replace);
    RUN_TEST(test_same_signal_prompt_cancel);
    RUN_TEST(test_same_signal_replace_with_a_console);
    RUN_TEST(test_load_list_defaults_first_and_tags);
    RUN_TEST(test_load_while_modified_confirms);
    RUN_TEST(test_defaults_row_queues_defaults);
    RUN_TEST(test_save_changes_confirms_with_name);
    RUN_TEST(test_save_changes_confirm_clips_a_long_name_to_the_row);
    RUN_TEST(test_save_changes_without_profile_opens_new);
    RUN_TEST(test_discard_changes_confirms_and_loads_the_profile_again);
    RUN_TEST(test_discard_changes_without_changes_only_says_so);
    RUN_TEST(test_discard_changes_without_profile_only_says_so);
    RUN_TEST(test_manage_rows_and_replace_text);
    RUN_TEST(test_save_key_cursor);
    RUN_TEST(test_profiles_row_value);
    RUN_TEST(test_resolution_on_the_original_board);
    RUN_TEST(test_choice_writes_only_changed_settings);
    RUN_TEST(test_scanlines_stronger_writes_a_lower_offset);
    RUN_TEST(test_resolution_revert_after_several_steps);
    RUN_TEST(test_menu_opens_at_root_with_titles);
    RUN_TEST(test_cursor_wraps_and_skips_headers);
    RUN_TEST(test_window_scrolls_with_cursor);
    RUN_TEST(test_back_pops_and_closes_at_root);
    RUN_TEST(test_menu_key_closes_everything);
    RUN_TEST(test_sync_cleanup_only_on_rgb_scart_and_sog);
    RUN_TEST(test_disabled_row_shows_reason_in_help);
    RUN_TEST(test_hidden_submenu_pops_to_visible_parent);
    RUN_TEST(test_cursor_keeps_child_when_rows_change);
    RUN_TEST(test_window_size_five);
    RUN_TEST(test_menu_times_out_after_15s);
    RUN_TEST(test_bool_setting_toggles_through_apply);
    RUN_TEST(test_enum_left_right_wrap);
    RUN_TEST(test_numeric_repeat_allowed_toggle_repeat_ignored);
    RUN_TEST(test_volume_row_ok_opens_the_volume_pill);
    RUN_TEST(test_filter_default_is_normal_with_the_line_filter);
    RUN_TEST(test_choice_applies_both_pairs);
    RUN_TEST(test_choice_is_atomic);
    RUN_TEST(test_stepper_runs_actions);
    RUN_TEST(test_confirm_default_no);
    RUN_TEST(test_queue_full_shows_busy_toast);
    RUN_TEST(test_commit_on_ok_for_resolution);
    RUN_TEST(test_revert_keeps_first_old_value);
    RUN_TEST(test_revert_releases_hold_only_after_settle);
    RUN_TEST(test_revert_ok_keeps_and_releases);
    RUN_TEST(test_revert_times_out_after_15s);
    RUN_TEST(test_revert_on_menu_close);
    RUN_TEST(test_revert_retry_when_queue_full);
    RUN_TEST(test_second_revert_waits_for_settle);
    RUN_TEST(test_encoder_edit_mode);
    RUN_TEST(test_encoder_in_confirm_and_revert);
    RUN_TEST(test_knob_revert_undo_ends_the_edit);
    RUN_TEST(test_knob_revert_timeout_ends_the_edit);
    RUN_TEST(test_check_update_shows_toast_before_running);
    RUN_TEST(test_check_result_toast);
    RUN_TEST(test_install_confirm_names_the_switch);
    RUN_TEST(test_other_channel_row_checks);
    RUN_TEST(test_manual_install_confirms);
    RUN_TEST(test_restart_note_toast);
    RUN_TEST(test_mute_toggles_and_times_out);
    RUN_TEST(test_volume_unmutes_and_clamps);
    RUN_TEST(test_info_lines_for_rgb_scart_240p);
    RUN_TEST(test_info_rates_not_measured_show_dashes);
    RUN_TEST(test_info_unstable_lines_show_dashes);
    RUN_TEST(test_no_signal_menu_opens_info_only);
    RUN_TEST(test_no_signal_exit_on_every_close_path);
    RUN_TEST(test_save_key_opens_profiles);
    RUN_TEST(test_update_overlay_locks_keys);
    RUN_TEST(test_adjust_overlay_runs_mapped_actions_and_pauses_timeout);
    RUN_TEST(test_profile_load_list_and_result_toast);
    RUN_TEST(test_profile_rows_show_the_region_once);
    RUN_TEST(test_idle_profile_drops_the_region_word);
    RUN_TEST(test_idle_region_from_the_decoder_standard);
    RUN_TEST(test_profile_result_toast_only_for_menu_ops);
    RUN_TEST(test_profile_failure_toast);
    RUN_TEST(test_profiles_save_active);
    RUN_TEST(test_profile_empty_list_row);
    RUN_TEST(test_manage_delete_needs_confirm);
    RUN_TEST(test_manage_forget_signal_disabled_without_fp);
    RUN_TEST(test_deleting_shown_profile_pops);
    RUN_TEST(test_overlay_priority_toast_waits_under_confirm);
    RUN_TEST(test_mute_ignored_during_confirm);
    RUN_TEST(test_info_over_menu_returns_to_menu);
    RUN_TEST(test_every_overlay_closes_on_back_and_timeout);
    RUN_TEST(test_osd_timeout_setting);
    RUN_TEST(test_picture_levels_are_setting_sliders);
    RUN_TEST(test_reset_picture_writes_every_setting);
    RUN_TEST(test_reset_picture_is_atomic);
    RUN_TEST(test_easy_page_with_encoder_back_row);
    RUN_TEST(test_confirm_line_for_a_long_label);
    RUN_TEST(test_short_timeout_waits_for_revert_and_confirm);
    RUN_TEST(test_busy_reflects_views);
    RUN_TEST(test_save_during_pending_edit_does_not_leak);
    RUN_TEST(test_revert_settle_releases_when_value_changes_elsewhere);
    RUN_TEST(test_revert_settle_gives_up_after_5s);
    RUN_TEST(test_open_info_after_idle_stays_open);
    RUN_TEST(test_row_kinds);
    RUN_TEST(test_position_counter);
    RUN_TEST(test_click_opens_menu_when_closed);
    RUN_TEST(test_rotation_and_long_do_nothing_when_closed);
    RUN_TEST(test_click_without_signal_opens_the_menu);
    RUN_TEST(test_back_row_only_in_click_sessions);
    RUN_TEST(test_back_row_pops_one_level);
    RUN_TEST(test_back_row_off_without_option);
    RUN_TEST(test_back_row_enters_the_right_child_at_depth_two);
    RUN_TEST(test_back_row_cursor_skips_a_leading_header);
    RUN_TEST(test_back_row_on_empty_profile_list);
    RUN_TEST(test_back_row_cleared_when_reopened_by_menu);
    RUN_TEST(test_idle_block);
    RUN_TEST(test_idle_block_profile_region_ap_and_no_signal);
    RUN_TEST(test_idle_and_info_name_1200p);
    RUN_TEST(test_idle_searching_names_no_path);
    RUN_TEST(test_idle_adv_free_run_says_no_signal);
    RUN_TEST(test_version_in_the_main_title);
    RUN_TEST(test_adjust_overlay_exposes_the_axis);
    RUN_TEST(test_back_row_survives_a_signal_change);
    RUN_TEST(test_save_inside_an_encoder_session);
    RUN_TEST(test_back_row_in_profile_lists);
    RUN_TEST(test_ota_password_keyboard_writes_on_done);
    RUN_TEST(test_keyboard_back_cancels);
    RUN_TEST(test_ota_password_empty_done_keeps_the_password);
    RUN_TEST(test_menu_key_over_a_pill_opens_the_menu);
    RUN_TEST(test_other_keys_over_a_pill_act_at_once);
    RUN_TEST(test_back_and_arrows_over_the_volume_pill);
    RUN_TEST(test_keyboard_menu_key_closes_everything);
    RUN_TEST(test_keyboard_pauses_menu_timeout);
    RUN_TEST(test_hosts_keyboard_prefilled);
    RUN_TEST(test_invalid_done_keeps_keyboard);
    RUN_TEST(test_keyboard_busy_keeps_keyboard);
    RUN_TEST(test_keyboard_ignores_shortcuts);
    RUN_TEST(test_catalog_sorted_and_fits);
    RUN_TEST(test_new_profile_catalog_rows);
    RUN_TEST(test_console_region_default_follows_signal);
    RUN_TEST(test_console_region_cycle_and_save);
    RUN_TEST(test_console_region_none_saves_plain_name);
    RUN_TEST(test_console_encoder_edit);
    RUN_TEST(test_console_encoder_long_cancels_edit);
    RUN_TEST(test_source_name_row_saves_generated);
    RUN_TEST(test_edit_name_keyboard_saves);
    RUN_TEST(test_rename_flow);
    RUN_TEST(test_rename_of_a_changed_profile_fails);
    RUN_TEST(test_names_list_with_back_row);
    RUN_TEST(test_signal_change_in_names_list);
    RUN_TEST(test_adjust_exit_after_a_step_undoes);
    RUN_TEST(test_adjust_exit_without_steps_writes_nothing);
    RUN_TEST(test_adjust_ok_keeps);
    RUN_TEST(test_adjust_long_press_keeps);
    RUN_TEST(test_adjust_menu_key_undoes);
    RUN_TEST(test_adjust_timeout_undoes);
    RUN_TEST(test_borders_legend_back_keeps_nothing_ok_saves);
    RUN_TEST(test_border_position_moves_the_mask);
    RUN_TEST(test_adjust_legend_for_position);
    RUN_TEST(test_quick_slider_opens_on_ok);
    RUN_TEST(test_quick_slider_opens_on_click);
    RUN_TEST(test_quick_slider_timeout);
    RUN_TEST(test_quick_slider_back_and_menu);
    RUN_TEST(test_autoload_toast_after_result);
    RUN_TEST(test_autoload_failure_toast);
    RUN_TEST(test_autoload_discarded_toast);
    RUN_TEST(test_autoload_failed_discards_nothing);
    RUN_TEST(test_autoload_does_not_eat_user_result);
    RUN_TEST(test_web_op_queued_first_does_not_answer_the_menu);
    RUN_TEST(test_wait_times_out);
    RUN_TEST(test_fresh_toast_is_not_replaced);
    RUN_TEST(test_adjust_step_dropped_while_one_is_in_flight);
    RUN_TEST(test_adjust_undo_waits_for_steps_in_flight);
    RUN_TEST(test_adjust_keep_waits_for_steps_in_flight);
    RUN_TEST(test_device_notice_toast);
    RUN_TEST(test_toast_hook_sees_every_toast);
    RUN_TEST(test_adv_controller_prompt_ok_on_default_is_not_now);
    RUN_TEST(test_adv_controller_prompt_moved_to_no_skips);
    RUN_TEST(test_adv_controller_prompt_yes_starts);
    RUN_TEST(test_adv_controller_prompt_timeout_and_back_do_not_skip);
    RUN_TEST(test_adv_controller_prompt_key_restarts_the_timeout);
    RUN_TEST(test_adv_controller_prompt_not_over_an_open_menu);
    RUN_TEST(test_adv_controller_menu_row_follows_availability);
    RUN_TEST(test_update_overlay_title_for_adv_controller);
    RUN_TEST(test_update_overlay_title_for_the_esp);
    RUN_TEST(test_update_overlay_shows_verify_and_done);
    RUN_TEST(test_update_overlay_progress_has_no_status_line);
    RUN_TEST(test_update_overlay_adv_controller_outcome_then_closes);
    RUN_TEST(test_update_overlay_shows_failure);
    RUN_TEST(test_update_result_without_overlay_is_a_toast);
    RUN_TEST(test_root_title_is_product_name);
    RUN_TEST(test_header_hidden_when_group_empty);
    RUN_TEST(test_mode_switch_closes_vanished_page);
    RUN_TEST(test_leaving_advanced_toasts_and_keeps_values);
    RUN_TEST(test_choice_custom_label);
    RUN_TEST(test_choice_off_matches_on_first_setting);
    RUN_TEST(test_choice_step_from_custom);
    RUN_TEST(test_choice_hides_advanced_and_capped_options);
    RUN_TEST(test_resolution_commit_and_revert_restore_both);
    RUN_TEST(test_resolution_alias_and_base_preset);
    return UNITY_END();
}
