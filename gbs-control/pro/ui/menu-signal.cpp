#include <string.h>
#include "menu-signal.h"
#include "../settings/settings-registry.h"

uint16_t menuSourceLines(uint8_t videoStandardInput, uint16_t vperiodIf) {
    return ((videoStandardInput == 1 || videoStandardInput == 2) && vperiodIf > 400) ? (uint16_t)(vperiodIf / 2) : vperiodIf;
}

bool MenuRatesRetry::due(bool noSignal, uint16_t srcHzX10, uint32_t now) {
    if (noSignal || srcHzX10) { have_ = false; return false; }
    if (!have_) { have_ = true; at_ = now; tries_ = 0; return false; }   // just shown: the signal settles first
    if (tries_ >= kMaxTries || now - at_ < kRetryMs) return false;
    at_ = now;
    tries_++;
    return true;
}

static bool within2(uint16_t a, uint16_t b) { return a + 2 >= b && a <= b + 2; }

uint16_t MenuLineFilter::feed(uint16_t lines, uint32_t now) {
    if (lines < kMinLines || lines > kMaxLines) lines = 0;   // no video has it: nothing to show
    if (!lines || !within2(lines, cand_)) {          // moved: start over, hide it until it stays
        cand_ = lines;
        since_ = now;
        shown_ = 0;
        return 0;
    }
    if (shown_ && within2(lines, shown_)) return shown_;
    if (now - since_ >= kStableMs) shown_ = lines;
    return shown_;
}

bool menuAdvIsNtsc(uint8_t format, uint8_t status00) {
    if (format == 0 || format == 12) {   // Auto, Auto 7.5 IRE: the detected standard decides
        if (!(status00 & 0x80)) return true;
        if (status00 & 0x60) return false;
        return true;   // 0x18 (480p, 240p/480i) or no standard bit: NTSC
    }
    return format == 2 || format == 3 || format == 4 || format == 5 || format == 7 || format == 8;
}

uint8_t menuSyncType(bool sogMode, bool csync) { return sogMode ? 2 : csync ? 1 : 0; }

bool menuSignalChanged(const MenuSignalKey& a, const MenuSignalKey& b) {
    if (a.input != b.input || a.standard != b.standard || a.bypass != b.bypass || a.deint != b.deint ||
        a.preset != b.preset)
        return true;
    int d = (int)a.lines - (int)b.lines;
    return d > 2 || d < -2;
}

static uint8_t defaults(const uint8_t* sids, uint8_t n, MenuWrite* out) {
    for (uint8_t i = 0; i < n; i++) { SettingDef d; settingsDefAt(sids[i], d); out[i] = {sids[i], d.def}; }
    return n;
}

uint8_t menuDecoderResetWrites(MenuWrite* out) {
    static const uint8_t kSids[12] = {SID_adv_brightness, SID_adv_contrast, SID_adv_saturation, SID_adv_hue, SID_adv_ace,
                                      SID_adv_i2p, SID_adv_smooth, SID_adv_ace_luma, SID_adv_ace_chroma, SID_adv_ace_cmax,
                                      SID_adv_ace_gamma, SID_adv_ace_speed};
    return defaults(kSids, 12, out);
}

uint8_t menuPictureResetWrites(MenuWrite* out) {
    static const uint8_t kSids[12] = {SID_pic_sharpness, SID_pic_line_filter, SID_pic_scanlines, SID_pic_scan_strength,
                                      SID_pic_brightness, SID_pic_contrast, SID_pic_saturation, SID_pic_deint,
                                      SID_pic_step_resp, SID_pic_red, SID_pic_green, SID_pic_blue};
    return defaults(kSids, 12, out);
}


bool menuStatusKeyChanged(const StatusKey& a, const StatusKey& b) {
    int d = (int)a.lines - (int)b.lines;
    return a.input != b.input || a.mode != b.mode || a.profile != b.profile || a.interlaced != b.interlaced ||
           d > 4 || d < -4;   // 240p and 480i share mode 1 and ~525 lines: only the parity tells them apart
}

bool menuStatusPopupDue(uint8_t setting, const StatusKey& before, const StatusKey& now) {
    if (setting == 1) return before.input != now.input || before.profile != now.profile;
    if (setting == 2) return menuStatusKeyChanged(before, now);
    return false;
}
