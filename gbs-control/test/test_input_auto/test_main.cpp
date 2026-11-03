// Auto input state machine
#include <unity.h>
#include <string>
#include "input-auto.h"

void setUp(void) {}
void tearDown(void) {}

static const InputAutoBoard kPro = {true, true}, kProNoCtl = {true, false}, kStd = {false, false};

static std::string paths(const InputAutoBoard& b, uint8_t hint) {
    uint8_t p[8]; uint8_t n = inputAutoPaths(b, hint, p, 8);
    std::string s;
    for (uint8_t i = 0; i < n; i++) s += (char)('0' + p[i]);
    return s;
}

void test_paths_pro_order(void) {
    TEST_ASSERT_EQUAL_STRING("312456", paths(kPro, 0).c_str());          // VGA, RGB/Component tier, S-Video, Composite
    TEST_ASSERT_EQUAL_STRING("431256", paths(kPro, 4).c_str());          // the last found path first
    TEST_ASSERT_EQUAL_STRING("163245", paths(kPro, 6).c_str());          // 5/6 never ahead of RGB SCART
    TEST_ASSERT_EQUAL_STRING("153246", paths(kPro, 5).c_str());
}

void test_paths_standard_board(void) {
    TEST_ASSERT_EQUAL_STRING("14", paths(kStd, 0).c_str());               // RGB header, YPbPr
    TEST_ASSERT_EQUAL_STRING("14", paths(kStd, 3).c_str());
    TEST_ASSERT_EQUAL_STRING("41", paths(kStd, 4).c_str());
    TEST_ASSERT_EQUAL_STRING("14", paths(kStd, 6).c_str());
}

void test_paths_pro_without_controller(void) {
    TEST_ASSERT_EQUAL_STRING("3124", paths(kProNoCtl, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("3124", paths(kProNoCtl, 6).c_str());
}

void test_boot_is_auto(void) {
    InputAuto a; a.begin(kPro, 4);
    TEST_ASSERT_EQUAL_UINT8(0, a.choice());
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(4, a.want());
}

void test_auto_found_sets_hint(void) {
    InputAuto a; a.begin(kPro, 0);
    a.pathResult(3, false, 100);
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
    a.pathResult(1, true, 200);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.found());
    TEST_ASSERT_TRUE(a.takeHintChanged());
}

void test_hint_changes_only_on_found(void) {
    InputAuto a; a.begin(kPro, 1);
    for (uint8_t p : {1, 3, 2, 4}) { a.pathResult(p, false, 10); TEST_ASSERT_FALSE(a.takeHintChanged()); }
    a.pathResult(5, true, 20);
    TEST_ASSERT_TRUE(a.takeHintChanged());
    TEST_ASSERT_FALSE(a.takeHintChanged());                               // once
    a.tick(true, 30);
    TEST_ASSERT_FALSE(a.takeHintChanged());
}

void test_auto_scan_walks_and_wraps(void) {
    InputAuto a; a.begin(kPro, 0);
    for (uint8_t p : {3, 1, 2, 4, 5, 6}) { TEST_ASSERT_EQUAL_UINT8(p, a.want()); a.pathResult(p, false, 10); }
    TEST_ASSERT_EQUAL_UINT8(3, a.want());
    a.pathResult(1, true, 20);                                            // a stale result for another path is ignored
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
}

void test_scanning_signal_means_found(void) {
    InputAuto a; a.begin(kPro, 4);
    a.tick(true, 10);                                                     // the hinted path has a source at boot
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(4, a.found());
}

void test_auto_short_dropout_keeps_path(void) {
    InputAuto a; a.begin(kPro, 1);
    a.tick(true, 0);
    a.tick(false, 1000);
    a.tick(false, 1000 + InputAuto::kAbsentMs - 1);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
    a.tick(true, 7000);
    a.tick(false, 8000);
    a.tick(false, 8000 + InputAuto::kAbsentMs);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());                                 // the scan starts from the found path
}

void test_manual_survives_short_dropout(void) {
    InputAuto a; a.begin(kPro, 1);
    a.choose(6, 0);
    TEST_ASSERT_EQUAL_UINT8(IA_MANUAL, a.state());
    TEST_ASSERT_EQUAL_UINT8(6, a.want());
    a.tick(true, 100);
    a.tick(false, 200);
    a.tick(false, 200 + InputAuto::kAbsentMs - 1);
    a.tick(true, 5300);
    TEST_ASSERT_EQUAL_UINT8(IA_MANUAL, a.state());
    TEST_ASSERT_EQUAL_UINT8(6, a.choice());
}

void test_manual_reverts_after_absence(void) {
    InputAuto a; a.begin(kPro, 4);
    a.choose(6, 0);
    a.tick(true, 100);
    a.tick(false, 200);
    a.tick(false, 200 + InputAuto::kAbsentMs);
    TEST_ASSERT_EQUAL_UINT8(0, a.choice());
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());                                 // hint 6 never ahead of RGB SCART
}

void test_manual_never_seen_reverts_after_60s(void) {
    InputAuto a; a.begin(kPro, 3);
    a.choose(5, 1000);
    a.tick(false, 1000 + InputAuto::kNeverSeenMs - 1);
    TEST_ASSERT_EQUAL_UINT8(IA_MANUAL, a.state());
    a.tick(false, 1000 + InputAuto::kNeverSeenMs);
    TEST_ASSERT_EQUAL_UINT8(0, a.choice());
    TEST_ASSERT_EQUAL_UINT8(3, a.want());
}

// Choosing Auto on purpose looks for the best source, also while the manual input shows one
void test_choosing_auto_scans_by_priority(void) {
    InputAuto a; a.begin(kPro, 1);
    a.choose(6, 0);                                                       // manual Composite with a picture
    a.tick(true, 10);
    a.choose(0, 20);
    TEST_ASSERT_EQUAL_UINT8(0, a.choice());
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(3, a.want());                                 // VGA first, then RGB SCART ...
    for (uint8_t p : {3, 1, 2, 4, 5}) a.pathResult(p, false, 30);
    a.pathResult(6, true, 40);                                            // only Composite has a source
    a.pathResult(1, false, 50);                                           // (the RGB SCART look of a Composite find)
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(6, a.found());
}

void test_redirect_moves_the_found_path(void) {
    InputAuto a; a.begin(kPro, 1);
    a.tick(true, 0);
    a.takeHintChanged();
    a.redirect(6, 10);                                                    // the RGB SCART judge picked Composite
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(6, a.want());
    TEST_ASSERT_EQUAL_UINT8(6, a.found());
    TEST_ASSERT_TRUE(a.takeHintChanged());
}

// Input search identification: the move to Composite comes while the scan is still on RGB SCART,
// before any preset; a Composite find waiting for its look at RGB SCART (confirm_) is redirected too
// Standard board (no link): the device glue redirects to the detected input on every pass with a source, so a gap
// shorter than the lost wait never starts a "return" scan (the upstream detection alternates the inputs by itself)
void test_standard_board_gap_starts_no_scan(void) {
    InputAuto a; a.begin(kStd, 1);
    a.takeScanStarted();                                                  // the boot scan
    uint32_t t = 0;
    for (int i = 0; i < 20; i++, t += 50) { a.tick(true, t); a.redirect(1, t); }
    for (int i = 0; i < 20; i++, t += 50) a.tick(false, t);               // 1 s without a source
    a.tick(true, t); a.redirect(1, t);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_FALSE(a.takeScanStarted());
}

void test_redirect_from_scanning(void) {
    InputAuto a; a.begin(kPro, 1);                                        // scanning, RGB SCART probed first
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    a.takeHintChanged();
    a.redirect(6, 10);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(6, a.want());
    TEST_ASSERT_EQUAL_UINT8(6, a.found());
    TEST_ASSERT_TRUE(a.takeHintChanged());
    InputAuto b; b.begin(kPro, 6);                                         // a Composite hint: RGB SCART first
    b.tick(true, 0);                                                      // a find that waits for RGB SCART's look
    b.redirect(6, 10);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, b.state());
    TEST_ASSERT_EQUAL_UINT8(6, b.want());
}

void test_composite_find_checks_rgb_scart_first(void) {
    InputAuto a; a.begin(kPro, 0);
    for (uint8_t p : {3, 1, 2, 4, 5}) a.pathResult(p, false, 10);       // the console comes on during the scan
    a.pathResult(6, true, 20);                                            // pin 20 CVBS of a SCART cable seen first
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());                                 // RGB SCART gets one look
    a.tick(true, 30);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.found());                                // the judge decides from there
}

// RGB SoG shares the RGB wires with RGB SCART: a SCART console powering up while the scan probes SoG locked there
// with an unstable sync for 12 s. A SoG find gets one look at RGB SCART first; a real SoG source
// (nothing on RGB SCART) is kept
void test_sog_find_checks_rgb_scart_first(void) {
    InputAuto a; a.begin(kPro, 0);
    a.pathResult(3, false, 10);                                           // VGA
    a.pathResult(1, false, 11);                                           // RGB SCART: the console is still off
    a.tick(true, 20);                                                     // RGB SoG: the console comes on
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
    a.tick(true, 30);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.found());

    InputAuto b; b.begin(kPro, 2);                                        // a real SoG source, hinted
    b.tick(true, 10);
    TEST_ASSERT_EQUAL_UINT8(1, b.want());
    b.pathResult(1, false, 20);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, b.state());
    TEST_ASSERT_EQUAL_UINT8(2, b.found());
    TEST_ASSERT_TRUE(b.foundByFallback());

    InputAutoBoard s = kPro; s.skip = 1;                                  // RGB SCART (bit 0) left out of Auto search
    InputAuto c; c.begin(s, 2);
    c.tick(true, 10);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, c.state());
    TEST_ASSERT_EQUAL_UINT8(2, c.found());
}

void test_composite_find_kept_without_rgb_scart(void) {
    InputAuto a; a.begin(kPro, 6);
    a.pathResult(1, false, 10);
    a.pathResult(6, true, 20);
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
    a.pathResult(1, false, 30);                                           // no sync on RGB SCART: an AV cable
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(6, a.found());
    a.pathResult(1, true, 40);                                            // a late result changes nothing
    TEST_ASSERT_EQUAL_UINT8(6, a.found());
}

void test_svideo_find_needs_no_check(void) {
    InputAuto a; a.begin(kPro, 5);
    a.pathResult(1, false, 10);
    a.pathResult(5, true, 20);                                            // own jack, not on the SCART
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(5, a.found());
}

void test_tv_path_present_drops_after_lost_sync(void) {
    TEST_ASSERT_TRUE(inputTvPathPresent(true, false, 1, 0));
    TEST_ASSERT_TRUE(inputTvPathPresent(true, false, 1, InputAuto::kNoSyncLost - 1));   // a short glitch
    TEST_ASSERT_FALSE(inputTvPathPresent(true, false, 1, InputAuto::kNoSyncLost));      // upstream still recovering
    TEST_ASSERT_FALSE(inputTvPathPresent(true, true, 1, 0));
    TEST_ASSERT_FALSE(inputTvPathPresent(true, false, 0, 0));
    TEST_ASSERT_FALSE(inputTvPathPresent(false, false, 1, 0));
}

// The Pro's VGA wires are the SCART RGB wires: a 15 kHz console shows on the VGA path too, and is not VGA
void test_vga_path_needs_a_vga_line_rate(void) {
    TEST_ASSERT_TRUE(inputVgaLineRate(216));                              // 31 kHz
    TEST_ASSERT_TRUE(inputVgaLineRate(280));                              // ~24 kHz
    TEST_ASSERT_FALSE(inputVgaLineRate(433));                             // 15.6 kHz (Mega Drive over SCART)
    TEST_ASSERT_FALSE(inputVgaLineRate(0));                               // no reading
    TEST_ASSERT_FALSE(inputVgaLineRate(511));                             // counter overflow: no line
}

// A 15 kHz source on the VGA path gets one look at RGB SCART: a SCART console wins there, an Amiga or an arcade
// board on the VGA connector (nothing on RGB SCART) goes back to VGA
void test_vga_15khz_checks_rgb_scart(void) {
    InputAuto a; a.begin(kPro, 0);
    TEST_ASSERT_EQUAL_UINT8(3, a.want());
    a.lowRateVga(10);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
    a.tick(true, 20);                                                     // the Mega Drive over SCART
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.found());

    InputAuto b; b.begin(kPro, 0);
    b.lowRateVga(10);
    b.pathResult(1, false, 30);                                           // nothing on RGB SCART: a real 15 kHz VGA source
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, b.state());
    TEST_ASSERT_EQUAL_UINT8(3, b.found());
}

void test_vga_15khz_only_while_scanning_vga(void) {
    InputAuto a; a.begin(kPro, 4);                                        // scanning Component first
    a.lowRateVga(10);
    TEST_ASSERT_EQUAL_UINT8(4, a.want());
}

// Input > Auto search: paths left out of the scan (bit = 1 << (path - 1))
static uint8_t bit(uint8_t path) { return (uint8_t)(1 << (path - 1)); }

void test_auto_search_skips_paths(void) {
    InputAutoBoard b = kPro; b.skip = bit(3) | bit(2);                    // no VGA, no RGB SoG
    TEST_ASSERT_EQUAL_STRING("1456", paths(b, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("1456", paths(b, 3).c_str());                // a skipped hint is not tried first
    TEST_ASSERT_EQUAL_STRING("4156", paths(b, 4).c_str());
    b.skip = 0x3F;                                                        // everything off: Auto looks at every path
    TEST_ASSERT_EQUAL_STRING("312456", paths(b, 0).c_str());
    InputAutoBoard s = kStd; s.skip = bit(4);
    TEST_ASSERT_EQUAL_STRING("1", paths(s, 4).c_str());
}

void test_auto_search_rgb_scart_look_only_when_scanned(void) {
    InputAutoBoard b = kPro; b.skip = bit(1);                             // RGB SCART left out
    InputAuto a; a.begin(b, 6);
    TEST_ASSERT_EQUAL_UINT8(6, a.want());                                 // "6324 5": Composite first (hint)
    a.tick(true, 10);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());                         // no RGB SCART look
    TEST_ASSERT_EQUAL_UINT8(6, a.found());
    InputAuto v; v.begin(b, 0);
    v.lowRateVga(10);                                                     // 15 kHz on VGA, RGB SCART not scanned
    TEST_ASSERT_EQUAL_UINT8(3, v.want());
}

void test_auto_search_change_restarts_the_scan(void) {
    InputAuto a; a.begin(kPro, 0);
    a.pathResult(3, false, 10);
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
    InputAutoBoard b = kPro; b.skip = bit(1);
    a.setBoard(b, 20);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(3, a.want());                                 // the new list from the top
    InputAuto f; f.begin(kPro, 1); f.tick(true, 10);                      // found: a change does not interrupt it
    f.setBoard(b, 20);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, f.state());
}

// The 15 kHz VGA look returns whether the find waits for RGB SCART: the glue holds "present" back only then (with RGB
// SCART left out of Auto search, a 15 kHz source on VGA is a find, not a scan stuck on VGA)
void test_low_rate_vga_without_rgb_scart_is_a_find(void) {
    InputAutoBoard b = kPro; b.skip = bit(1);
    InputAuto v; v.begin(b, 0);
    TEST_ASSERT_FALSE(v.lowRateVga(10));
    v.tick(true, 20);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, v.state());
    TEST_ASSERT_EQUAL_UINT8(3, v.found());
    InputAuto a; a.begin(kPro, 0);
    TEST_ASSERT_TRUE(a.lowRateVga(10));
    TEST_ASSERT_EQUAL_UINT8(1, a.want());
}

// Log support: why and when a scan started, its order, the first empty round, a fallback find
void test_scan_start_reasons(void) {
    InputAuto a; a.begin(kPro, 0, 100);
    TEST_ASSERT_TRUE(a.takeScanStarted());
    TEST_ASSERT_FALSE(a.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_BOOT, a.scanReason());
    TEST_ASSERT_EQUAL_UINT32(100, a.scanStartedAt());
    a.choose(2, 200);
    a.tick(true, 300);                                                    // the manual input shows a source
    a.tick(false, 400);
    a.tick(false, 400 + InputAuto::kAbsentMs);
    TEST_ASSERT_TRUE(a.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_BACK_TO_AUTO, a.scanReason());
    TEST_ASSERT_EQUAL_UINT32(400 + InputAuto::kAbsentMs, a.scanStartedAt());

    InputAuto n; n.begin(kPro, 0, 0); n.takeScanStarted();
    n.choose(5, 10);                                                      // never sees a source
    n.tick(false, 10 + InputAuto::kNeverSeenMs);
    TEST_ASSERT_TRUE(n.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_BACK_TO_AUTO, n.scanReason());

    InputAuto f; f.begin(kPro, 1, 0); f.takeScanStarted();
    f.tick(true, 10);
    TEST_ASSERT_FALSE(f.takeScanStarted());
    f.tick(false, 20);
    f.tick(false, 20 + InputAuto::kAbsentMs);
    TEST_ASSERT_TRUE(f.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_LOST, f.scanReason());

    InputAutoBoard b = kPro; b.skip = bit(2);
    f.setBoard(b, 9000);
    TEST_ASSERT_TRUE(f.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_BOARD, f.scanReason());

    f.choose(4, 9100);
    f.choose(0, 9200);
    TEST_ASSERT_TRUE(f.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_CHOSEN, f.scanReason());
    TEST_ASSERT_EQUAL_UINT32(9200, f.scanStartedAt());
}

void test_round_ended_once_per_scan(void) {
    InputAuto a; a.begin(kPro, 0, 0);
    uint8_t order[6];
    uint8_t n = a.scanPaths(order, 6);
    for (uint8_t r = 0; r < 2; r++)
        for (uint8_t i = 0; i < n; i++) {
            if (r == 0 && i < n - 1) TEST_ASSERT_FALSE(a.takeRoundEnded());
            a.pathResult(a.want(), false, 10);
        }
    TEST_ASSERT_TRUE(a.takeRoundEnded());
    TEST_ASSERT_FALSE(a.takeRoundEnded());
}

// Console log: the detail lines of a scan come only from its first round
void test_first_round_until_every_path_was_tried(void) {
    InputAuto a; a.begin(kPro, 0, 0);
    uint8_t order[6];
    uint8_t n = a.scanPaths(order, 6);
    for (uint8_t i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(a.firstRound());
        a.pathResult(a.want(), false, 10);
    }
    TEST_ASSERT_FALSE(a.firstRound());
    a.choose(2, 20);
    a.choose(0, 30);                                                      // a new scan
    TEST_ASSERT_TRUE(a.firstRound());
}

void test_scan_paths_order(void) {
    InputAuto a; a.begin(kPro, 0, 0);
    uint8_t got[6], want[6];
    uint8_t n = a.scanPaths(got, 6);
    uint8_t m = inputAutoPaths(kPro, 0, want, 6);
    TEST_ASSERT_EQUAL_UINT8(m, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, got, n);
}

void test_fallback_find_is_flagged(void) {
    InputAuto a; a.begin(kPro, 0, 0);
    a.lowRateVga(10);
    a.pathResult(1, false, 20);                                           // nothing on RGB SCART
    TEST_ASSERT_EQUAL_UINT8(3, a.found());
    TEST_ASSERT_TRUE(a.foundByFallback());
    InputAuto b; b.begin(kPro, 1, 0);
    b.tick(true, 10);
    TEST_ASSERT_FALSE(b.foundByFallback());
}

// The hardware starts on the scan's first path, not on the hint: a Composite hint still scans RGB SCART first, and
// starting on Composite powered the decoder up only to switch away 2 s later (the TV5725 then saw the ADV
// Controller's vertical sync with the console's and took RGB SCART for RGBHV: 576p instead of 288p)
void test_boot_path_is_the_first_scanned(void) {
    TEST_ASSERT_EQUAL_UINT8(InputTypeRGBs, inputAutoBootPath(kPro, InputTypeAV));
    TEST_ASSERT_EQUAL_UINT8(InputTypeRGBs, inputAutoBootPath(kPro, InputTypeSV));
    TEST_ASSERT_EQUAL_UINT8(InputTypeVGA, inputAutoBootPath(kPro, InputTypeVGA));
    TEST_ASSERT_EQUAL_UINT8(InputTypeYUV, inputAutoBootPath(kPro, InputTypeYUV));
    TEST_ASSERT_EQUAL_UINT8(InputTypeVGA, inputAutoBootPath(kPro, 0));           // no hint: priority order
    InputAutoBoard b = kPro; b.skip = bit(1);                                    // RGB SCART left out
    TEST_ASSERT_EQUAL_UINT8(InputTypeAV, inputAutoBootPath(b, InputTypeAV));
    TEST_ASSERT_EQUAL_UINT8(InputTypeYUV, inputAutoBootPath(kStd, InputTypeYUV));
    InputAuto a; a.begin(kPro, InputTypeAV, 0);
    TEST_ASSERT_EQUAL_UINT8(inputAutoBootPath(kPro, InputTypeAV), a.want());    // the scan starts where the hardware is
}

// The detection sweeps the SOG slicer one step per pass (6, 5, 4, 3, 2, 6, ...); a scan round lasted about two
// sweeps, so Component always saw 6, 5, 4 and a PS2 that needs 3 was never found. A path that slices
// sync ends only once every level 2..6 was sampled on it: the first pass after S-Video/Composite also resets the
// level to 5 (low power), so counting passes was not enough.
static uint8_t sogBit(uint8_t level) { return (uint8_t)(1 << level); }
static const uint8_t kAllSog = (uint8_t)((1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));

void test_sog_sweep_steps(void) {
    TEST_ASSERT_EQUAL_UINT8(5, inputSogSweepNext(6));
    TEST_ASSERT_EQUAL_UINT8(3, inputSogSweepNext(4));
    TEST_ASSERT_EQUAL_UINT8(2, inputSogSweepNext(3));
    TEST_ASSERT_EQUAL_UINT8(6, inputSogSweepNext(2));
    TEST_ASSERT_EQUAL_UINT8(6, inputSogSweepNext(0));       // a level set elsewhere (0, 1, 13) rejoins the sweep
    TEST_ASSERT_EQUAL_UINT8(6, inputSogSweepNext(13));
}

void test_sog_paths_end_after_every_slicer_level(void) {
    const uint8_t sogPaths[] = {InputTypeRGBs, InputTypeRGsB, InputTypeYUV};
    for (uint8_t path : sogPaths) {
        // after S-Video: the leftover 2, then the low power reset to 5 and the sweep goes on from there
        uint8_t seen = 0, passes = 0;
        const uint8_t sampled[] = {2, 4, 3, 2, 6};
        for (uint8_t s : sampled) { seen |= sogBit(s); passes++; }
        TEST_ASSERT_FALSE_MESSAGE(inputTvPathDone(path, passes, seen), "SOG 5 not tried yet");
        seen |= sogBit(5); passes++;
        TEST_ASSERT_TRUE(inputTvPathDone(path, passes, seen));
        // whatever level the sweep is on, five passes sample every level
        for (uint8_t start = 2; start <= 6; start++) {
            uint8_t level = start; seen = 0;
            for (passes = 0; passes < 5; passes++) { seen |= sogBit(level); level = inputSogSweepNext(level); }
            TEST_ASSERT_EQUAL_HEX8(kAllSog, seen);
            TEST_ASSERT_TRUE(inputTvPathDone(path, passes, seen));
        }
    }
    TEST_ASSERT_FALSE(inputTvPathDone(InputTypeVGA, 2, kAllSog));   // H/V sync: no slicer, 3 passes (about 1.5 s)
    TEST_ASSERT_TRUE(inputTvPathDone(InputTypeVGA, 3, 0));
    TEST_ASSERT_FALSE(inputTvPathDone(InputTypeSV, 9, kAllSog));    // the decoder paths are timed by the ADV report
    TEST_ASSERT_FALSE(inputTvPathDone(InputTypeAV, 9, kAllSog));
}

// input.lost_wait: how long a source stays absent before Auto looks elsewhere (and a manual choice returns to Auto);
// 2 s by default, 5 s used to be fixed and took about 8 s from "No signal" to "Searching"
void test_lost_wait_is_configurable(void) {
    InputAuto a; a.begin(kPro, 1);
    a.setAbsentMs(2000);
    a.tick(true, 0);
    a.tick(false, 1000);
    a.tick(false, 2999);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    a.tick(false, 3000);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());                                 // from the lost path
    InputAuto m; m.begin(kPro, 4);
    m.setAbsentMs(10000);
    m.choose(4, 0);
    m.tick(true, 100);
    m.tick(false, 200);
    m.tick(false, 10199);
    TEST_ASSERT_EQUAL_UINT8(IA_MANUAL, m.state());
    m.tick(false, 10200);
    TEST_ASSERT_EQUAL_UINT8(0, m.choice());                              // back to Auto after the longer wait
}
// S-Video chosen by hand while the Mega Drive showed on RGB SCART went back to Auto within a second.
// The hardware was still on RGB SCART for a few passes and its source counted as "seen on S-Video" (and became the
// hint); then the S-Video absence ran out input.lost_wait. A source counts only on the input the machine wants.
void test_source_counts_only_on_the_wanted_input(void) {
    TEST_ASSERT_FALSE(inputAutoSourceCounts(IA_MANUAL, 1, 5));           // still on RGB SCART after choosing S-Video
    TEST_ASSERT_TRUE(inputAutoSourceCounts(IA_MANUAL, 5, 5));
    TEST_ASSERT_FALSE(inputAutoSourceCounts(IA_SCANNING, 1, 3));         // a new scan still on the old input
    TEST_ASSERT_TRUE(inputAutoSourceCounts(IA_SCANNING, 3, 3));
    TEST_ASSERT_TRUE(inputAutoSourceCounts(IA_FOUND, 1, 1));
}

void test_manual_pick_without_source_stays_until_the_never_seen_wait(void) {
    InputAuto a; a.begin(kPro, 1);
    a.setAbsentMs(2000);
    a.tick(true, 0);                                                      // Mega Drive found on RGB SCART
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    a.takeHintChanged();
    a.choose(5, 1000);                                                    // S-Video by hand, no source there
    uint8_t active = 1;                                                   // the switch lands a few passes later
    for (uint32_t t = 1000; t < 1000 + InputAuto::kNeverSeenMs; t += 50) {
        if (t >= 1300) active = 5;
        bool rgbSource = active == 1;
        a.tick(rgbSource && inputAutoSourceCounts(a.state(), active, a.want()), t);
        TEST_ASSERT_EQUAL_UINT8(IA_MANUAL, a.state());
    }
    TEST_ASSERT_EQUAL_UINT8(5, a.choice());
    TEST_ASSERT_EQUAL_UINT8(1, a.found());                                // the hint stays RGB SCART
    TEST_ASSERT_FALSE(a.takeHintChanged());
    a.tick(false, 1000 + InputAuto::kNeverSeenMs);
    TEST_ASSERT_EQUAL_UINT8(0, a.choice());                               // then back to Auto
}


// A console swapped on the same SCART inside input.lost_wait (Mega Drive off, N64 on 3 s later) stayed on RGB SCART
// with a black picture. A source back after a gap of about 2 s (the device's "present" already waits ~1.7 s of no
// sync, kNoSyncLost) is a new source: the scan runs again, with the cable identification
void test_source_back_after_a_gap_is_searched_again(void) {
    InputAuto a; a.begin(kPro, 1, 0); a.takeScanStarted();
    a.tick(true, 10);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    a.tick(false, 1000);
    a.tick(true, 1000 + InputAuto::kBackAsNewMs - 1);                    // a short drop: the same source
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_FALSE(a.takeScanStarted());
    a.tick(false, 2000);
    a.tick(true, 2000 + InputAuto::kBackAsNewMs);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_TRUE(a.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_RETURN, a.scanReason());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());                                // from the path it was on
}

// A find that falls back after the RGB SCART look (Composite, RGB SoG, 15 kHz on VGA) leaves the hardware on RGB
// SCART, and the found path takes more than kBackAsNewMs to show the source after the switch back: that absence is
// not a return (it looped: find, "return", the same find...). Only a source the found path showed counts
static void findComposite(InputAuto& a) {
    for (int i = 0; i < 6 && a.want() != 6; i++) a.pathResult(a.want(), false, 10);
    a.pathResult(6, true, 10);
    a.pathResult(1, false, 20);                                          // RGB SCART empty: Composite kept (fallback)
}

void test_absence_right_after_a_fallback_find_is_not_a_return(void) {
    InputAuto a; a.begin(kPro, 6, 0); a.takeScanStarted();
    findComposite(a);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    a.tick(false, 30);                                                   // still switching back to Composite
    a.tick(true, 30 + InputAuto::kBackAsNewDecoderMs + 500);
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    TEST_ASSERT_FALSE(a.takeScanStarted());
}

// On S-Video/Composite "present" is the decoder report with no 1.7 s count (a one-reply unlock drops it): a return
// there needs kBackAsNewDecoderMs of absence (~2 s like the TV5725 paths)
void test_decoder_return_needs_two_seconds(void) {
    InputAuto a; a.begin(kPro, 6, 0); a.takeScanStarted();
    findComposite(a);
    a.tick(true, 100);                                                   // Composite shows the source
    a.tick(false, 200);
    a.tick(true, 200 + InputAuto::kBackAsNewDecoderMs - 1);              // a reset button, a short unlock
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    a.tick(false, 5000);
    a.tick(true, 5000 + InputAuto::kBackAsNewDecoderMs);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_RETURN, a.scanReason());
}

// On Composite the decoder went from a PAL N64 to an NTSC Mega Drive with no gap (the SCART cable carries CVBS on pin
// 20 too) and stayed on Composite; on RGB SCART (sync from pin 20) the other way round took 0.9 s. A 50 <-> 60 Hz
// change of the source is a likely new source: search again (with the identification)
void test_decoder_rate_change_is_a_new_source(void) {
    RateWatch w;
    TEST_ASSERT_FALSE(w.feed(50, 0));                                    // the first rate is the reference
    TEST_ASSERT_FALSE(w.feed(0, 100));                                   // unknown (lock lost): never a change
    TEST_ASSERT_FALSE(w.feed(60, 200));                                  // a new rate must hold kHoldMs ...
    TEST_ASSERT_FALSE(w.feed(50, 300));                                  // ... a flicker back cancels it
    TEST_ASSERT_FALSE(w.feed(60, 400));
    TEST_ASSERT_FALSE(w.feed(60, 400 + RateWatch::kHoldMs - 1));
    TEST_ASSERT_TRUE(w.feed(60, 400 + RateWatch::kHoldMs));             // held: one change
    TEST_ASSERT_FALSE(w.feed(60, 400 + 2 * RateWatch::kHoldMs));         // 60 is the reference now
    TEST_ASSERT_FALSE(w.feed(50, 3000));                                 // a candidate ...
    TEST_ASSERT_FALSE(w.feed(0, 3100));                                  // ... unlocked: forgotten
    TEST_ASSERT_FALSE(w.feed(50, 3000 + RateWatch::kHoldMs));            // starts over: held only from here
    TEST_ASSERT_TRUE(w.feed(50, 3000 + 2 * RateWatch::kHoldMs));
    w.reset();
    TEST_ASSERT_FALSE(w.feed(50, 9000));                                 // after a reset: a reference again
    InputAuto a; a.begin(kPro, 6, 0); a.takeScanStarted();
    for (int i = 0; i < 6 && a.want() != 6; i++) a.pathResult(a.want(), false, 10);   // nothing until Composite
    a.pathResult(6, true, 10);                                           // Composite found (confirm look) ...
    a.pathResult(1, false, 20);                                          // ... RGB SCART empty: Composite kept
    TEST_ASSERT_EQUAL_UINT8(IA_FOUND, a.state());
    a.sourceChanged(30);
    TEST_ASSERT_EQUAL_UINT8(IA_SCANNING, a.state());
    TEST_ASSERT_TRUE(a.takeScanStarted());
    TEST_ASSERT_EQUAL_UINT8(IA_WHY_FORMAT, a.scanReason());
    TEST_ASSERT_EQUAL_UINT8(1, a.want());                                // RGB SCART first: the identification
    InputAuto m; m.begin(kPro, 6, 0); m.choose(6, 10); m.tick(true, 20); m.takeScanStarted();
    m.sourceChanged(30);                                                 // a manual input stays manual
    TEST_ASSERT_EQUAL_UINT8(IA_MANUAL, m.state());
    TEST_ASSERT_FALSE(m.takeScanStarted());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_paths_pro_order);
    RUN_TEST(test_paths_standard_board);
    RUN_TEST(test_paths_pro_without_controller);
    RUN_TEST(test_boot_is_auto);
    RUN_TEST(test_auto_found_sets_hint);
    RUN_TEST(test_hint_changes_only_on_found);
    RUN_TEST(test_auto_scan_walks_and_wraps);
    RUN_TEST(test_scanning_signal_means_found);
    RUN_TEST(test_auto_short_dropout_keeps_path);
    RUN_TEST(test_manual_survives_short_dropout);
    RUN_TEST(test_manual_reverts_after_absence);
    RUN_TEST(test_manual_never_seen_reverts_after_60s);
    RUN_TEST(test_choosing_auto_scans_by_priority);
    RUN_TEST(test_redirect_moves_the_found_path);
    RUN_TEST(test_redirect_from_scanning);
    RUN_TEST(test_standard_board_gap_starts_no_scan);
    RUN_TEST(test_composite_find_checks_rgb_scart_first);
    RUN_TEST(test_composite_find_kept_without_rgb_scart);
    RUN_TEST(test_sog_find_checks_rgb_scart_first);
    RUN_TEST(test_svideo_find_needs_no_check);
    RUN_TEST(test_tv_path_present_drops_after_lost_sync);
    RUN_TEST(test_vga_path_needs_a_vga_line_rate);
    RUN_TEST(test_vga_15khz_checks_rgb_scart);
    RUN_TEST(test_vga_15khz_only_while_scanning_vga);
    RUN_TEST(test_auto_search_skips_paths);
    RUN_TEST(test_auto_search_rgb_scart_look_only_when_scanned);
    RUN_TEST(test_low_rate_vga_without_rgb_scart_is_a_find);
    RUN_TEST(test_scan_start_reasons);
    RUN_TEST(test_round_ended_once_per_scan);
    RUN_TEST(test_first_round_until_every_path_was_tried);
    RUN_TEST(test_scan_paths_order);
    RUN_TEST(test_boot_path_is_the_first_scanned);
    RUN_TEST(test_fallback_find_is_flagged);
    RUN_TEST(test_auto_search_change_restarts_the_scan);
    RUN_TEST(test_sog_sweep_steps);
    RUN_TEST(test_sog_paths_end_after_every_slicer_level);
    RUN_TEST(test_lost_wait_is_configurable);
    RUN_TEST(test_source_counts_only_on_the_wanted_input);
    RUN_TEST(test_manual_pick_without_source_stays_until_the_never_seen_wait);
    RUN_TEST(test_source_back_after_a_gap_is_searched_again);
    RUN_TEST(test_decoder_rate_change_is_a_new_source);
    RUN_TEST(test_absence_right_after_a_fallback_find_is_not_a_return);
    RUN_TEST(test_decoder_return_needs_two_seconds);
    return UNITY_END();
}
