// The menu v3 tree (NF_ADVANCED marks the Advanced-only nodes)
#include <string.h>
#include "../settings/settings-pgm.h"
#include "../settings/settings-registry.h"
#include "../api/api-actions.h"
#include "menu-strings.h"
#include "menu-tree.h"

// ---- Option lists (labels for settings that have none in the registry) ----
static const char kOlDeint[] SET_PROGMEM = "Adaptive=0|Bob=1";
static const char kOlFilterY[] SET_PROGMEM =
    "AutoWide=0|AutoNarrow=1|SVHS-1=2|SVHS-2=3|SVHS-3=4|SVHS-4=5|SVHS-5=6|SVHS-6=7|SVHS-7=8|SVHS-8=9|SVHS-9=10|SVHS-10=11|SVHS-11=12|SVHS-12=13|SVHS-13=14|SVHS-14=15|SVHS-15=16|SVHS-16=17|SVHS-17=18|SVHS-18=19|PAL-NN1=20|PAL-NN2=21|PAL-NN3=22|PAL-WN1=23|PAL-WN2=24|"
    "NTSC-NN1=25|NTSC-NN2=26|NTSC-NN3=27|NTSC-WN1=28|NTSC-WN2=29|NTSC-WN3=30";
static const char kOlFilterWy[] SET_PROGMEM = "SVHS-1=2|SVHS-2=3|SVHS-3=4|SVHS-4=5|SVHS-5=6|SVHS-6=7|SVHS-7=8|SVHS-8=9|SVHS-9=10|SVHS-10=11|SVHS-11=12|SVHS-12=13|SVHS-13=14|SVHS-14=15|SVHS-15=16|SVHS-16=17|SVHS-17=18|SVHS-18=19";
static const char kOlWyMode[] SET_PROGMEM = "Auto=0|Manual=1";
static const char kOlFilterC[] SET_PROGMEM = "Auto 1.5MHz=0|Auto 2.2MHz=1|Shape 1=2|Shape 2=3|Shape 3=4|Shape 4=5|Shape 5=6|Wideband=7";
static const char kOlCombLuma[] SET_PROGMEM = "Adaptive=0|Notch=4|2-line upper=5|3-line=6|2-line lower=7";
static const char kOlCombChroma[] SET_PROGMEM = "Adaptive=0|Off=4|2-line upper=5|3-line=6|2-line lower=7";
static const char kOlSyncwatcher[] SET_PROGMEM = "Inherit=0|On=1|Off=2";

static const char* const kOptLists[OL_COUNT] SET_PROGMEM = {
    nullptr, kOlDeint, kOlFilterY, kOlFilterWy, kOlWyMode, kOlFilterC, kOlCombLuma, kOlCombChroma, kOlSyncwatcher};

const char* menuOptList(uint8_t index) {
    if (index == OL_NONE || index >= OL_COUNT) return nullptr;
    const char* p;
    memcpy_P(&p, &kOptLists[index], sizeof(p));
    return p;
}

// ---- Choices and adjust mappings ----
static const uint8_t kNone = 0xFF;
static const MenuChoiceOpt kChFilter[] SET_PROGMEM = {   // sharpness; the line filter stays on
    {S_NORMAL, SID_pic_sharpness, 22, SID_pic_line_filter, 1, 0},
    {S_SHARP, SID_pic_sharpness, 31, SID_pic_line_filter, 1, 0},
    {S_SMOOTH, SID_pic_sharpness, 0, SID_pic_line_filter, 1, 0},
};
static const MenuChoiceOpt kChScanlines[] SET_PROGMEM = {   // mixing offset: lower = darker lines (0xff = none)
    {S_OFF, SID_pic_scanlines, 0, SID_pic_scan_strength, 0, CHO_SID1_ONLY},
    {S_LIGHT, SID_pic_scanlines, 1, SID_pic_scan_strength, 0x50, 0},
    {S_MEDIUM, SID_pic_scanlines, 1, SID_pic_scan_strength, 0x30, 0},
    {S_STRONG, SID_pic_scanlines, 1, SID_pic_scan_strength, 0x10, 0},
};
static const MenuChoiceOpt kChResolution[] SET_PROGMEM = {   // ascending, 960p/1024p as one choice
    {S_RES_DOWNSCALE, SID_output_preset, 6, kNone, 0, CHO_ORIGINAL},
    {S_RES_480, SID_output_preset, 1, kNone, 0, 0},
    {S_RES_720, SID_output_preset, 3, kNone, 0, 0},
    {S_RES_960_FIXED, SID_output_preset, 0, SID_output_match, 0, CHO_ADVANCED},
    {S_RES_960_1024, SID_output_preset, 0, SID_output_match, 1, 0},
    {S_RES_960_1024, SID_output_preset, 4, SID_output_match, 1, CHO_ALIAS},   // 1024p with matching: same output
    {S_RES_1024_FIXED, SID_output_preset, 4, SID_output_match, 0, CHO_ADVANCED},
    {S_RES_1080, SID_output_preset, 5, kNone, 0, 0},
    {S_RES_1200, SID_output_preset, 7, kNone, 0, 0},
    {S_RES_BYPASS, SID_output_preset, 10, kNone, 0, CHO_ORIGINAL},
};
#define CHOICE(arr) {(uint8_t)(sizeof(arr) / sizeof(arr[0])), arr}
static const MenuChoice kChoices[] SET_PROGMEM = {
    CHOICE(kChFilter), CHOICE(kChScanlines), CHOICE(kChResolution),
};
void menuChoiceRead(uint8_t index, MenuChoice& out) { memcpy_P(&out, &kChoices[index], sizeof(out)); }
void menuChoiceOptRead(const MenuChoice& c, uint8_t i, MenuChoiceOpt& out) { memcpy_P(&out, &c.opts[i], sizeof(out)); }

// IR: the Size keys send the oppositely named scale actions (remote legend)
static const MenuAdjust kAdjusts[] SET_PROGMEM = {
    {AID_scr_move_left, AID_scr_move_right, AID_scr_move_up, AID_scr_move_down},
    {AID_scr_scale_right, AID_scr_scale_left, AID_scr_scale_down, AID_scr_scale_up},
    {AID_scr_border_left, AID_scr_border_right, AID_scr_border_up, AID_scr_border_down},
    {AID_scr_bmove_left, AID_scr_bmove_right, AID_scr_bmove_up, AID_scr_bmove_down},
};
void menuAdjustRead(uint8_t index, MenuAdjust& out) { memcpy_P(&out, &kAdjusts[index], sizeof(out)); }

void menuNodeRead(const MenuNode* pgm, MenuNode& out) { memcpy_P(&out, pgm, sizeof(out)); }

// ---- Node macros ----
#define NODE(type, flags, label, help, ref, ref2, pred, opts) {type, flags, label, help, ref, ref2, pred, opts, 0, nullptr}
#define SUB(label, help, pred, kids) {N_SUB, 0, label, help, FN_NONE, 0, pred, 0, (uint8_t)(sizeof(kids) / sizeof(kids[0])), kids}
#define SUBV(label, help, pred, kids, text) {N_SUB, 0, label, help, FN_NONE, 0, pred, text, (uint8_t)(sizeof(kids) / sizeof(kids[0])), kids}
#define SUBF(label, help, pred, kids, flags) {N_SUB, flags, label, help, FN_NONE, 0, pred, 0, (uint8_t)(sizeof(kids) / sizeof(kids[0])), kids}
#define DYN(label, help, fnId) {N_SUB, NF_FN, label, help, fnId, 0, P_NONE, 0, 0, nullptr}
#define SET(label, help, sid, pred, opts, flags) NODE(N_SETTING, flags, label, help, SID_##sid, 0, pred, opts)
#define ACT(label, help, aid, live, pred, flags) NODE(N_ACTION, flags, label, help, AID_##aid, live, pred, 0)
#define ACTV(label, help, aid, pred, text) NODE(N_ACTION, 0, label, help, AID_##aid, LV_NONE, pred, text)
#define FNA(label, help, fnId, pred, flags) NODE(N_ACTION, (flags) | NF_FN, label, help, fnId, LV_NONE, pred, 0)
#define FNAT(label, help, fnId, pred, text) NODE(N_ACTION, NF_FN, label, help, fnId, LV_NONE, pred, text)
#define STEP(label, help, dec, inc, live, pred) NODE(N_STEPPER, 0, label, help, AID_##dec, live, pred, AID_##inc)
#define LIVE(label, help, lt, pred, flags) NODE(N_LIVE, flags, label, help, lt, 0, pred, 0)
#define HEAD(label) NODE(N_HEADER, 0, label, S_NONE, 0, 0, P_NONE, 0)
#define ADJ(label, help, index, live, flags) NODE(N_ADJUST2D, flags, label, help, index, live, P_NONE, 0)
#define CHO(label, help, index, pred, flags) NODE(N_CHOICE, flags, label, help, index, 0, pred, 0)
#define TOG(label, help, fnId, pred, flags) NODE(N_TOGGLE_FN, (flags) | NF_FN, label, help, fnId, 0, pred, 0)

// ---- Input ----
static const MenuNode kAdc[] SET_PROGMEM = {
    SET(S_AUTO_GAIN, H_AUTO_GAIN, input_auto_gain, P_NONE, 0, 0),
    STEP(S_GAIN, H_GAIN, in_gain_down, in_gain_up, LV_ADC_GAIN, P_NONE),
    STEP(S_SOG_LEVEL, H_SOG_LEVEL, in_sog_down, in_sog_up, LV_SOG, P_SOG),
    SET(S_CALIBRATION, H_CALIBRATION, sys_adc_cal, P_NONE, 0, 0),
};
static const MenuNode kAce[] SET_PROGMEM = {
    SET(S_ENABLED, H_ACE_ENABLE, adv_ace, P_NONE, 0, 0),
    SET(S_LUMA_GAIN, H_LUMA_GAIN, adv_ace_luma, P_ACE_ON, 0, 0),
    SET(S_CHROMA_GAIN, H_CHROMA_GAIN, adv_ace_chroma, P_ACE_ON, 0, 0),
    SET(S_CHROMA_MAX, H_CHROMA_MAX, adv_ace_cmax, P_ACE_ON, 0, 0),
    SET(S_GAMMA_GAIN, H_GAMMA_GAIN, adv_ace_gamma, P_ACE_ON, 0, 0),
    SET(S_RESPONSE_SPEED, H_RESPONSE_SPEED, adv_ace_speed, P_ACE_ON, 0, 0),
    ACT(S_RESET_ADAPTIVE, H_ACE_DEFAULTS, adv_ace_defaults, LV_NONE, P_NONE, NF_CONFIRM),
};
static const MenuNode kDecoder[] SET_PROGMEM = {
    HEAD(S_LEVELS),
    SET(S_BRIGHTNESS, H_BRIGHTNESS, adv_brightness, P_NONE, 0, 0),
    SET(S_CONTRAST, H_CONTRAST, adv_contrast, P_NONE, 0, 0),
    SET(S_SATURATION, H_SATURATION, adv_saturation, P_NONE, 0, 0),
    SET(S_HUE, H_HUE, adv_hue, P_NONE, 0, 0),
    HEAD(S_COMB),                                   // UG-637: the comb settings act on CVBS (Composite) only
    SET(S_LUMA_MODE, H_LUMA_MODE, adv_cl_ntsc, P_AV_NTSC, OL_COMB_LUMA, 0),
    SET(S_LUMA_MODE, H_LUMA_MODE, adv_cl_pal, P_AV_PAL, OL_COMB_LUMA, 0),
    SET(S_CHROMA_MODE, H_CHROMA_MODE, adv_cc_ntsc, P_AV_NTSC, OL_COMB_CHROMA, 0),
    SET(S_CHROMA_MODE, H_CHROMA_MODE, adv_cc_pal, P_AV_PAL, OL_COMB_CHROMA, 0),
    SET(S_BANDWIDTH, H_BANDWIDTH, adv_comb_ntsc, P_AV_NTSC, 0, 0),
    SET(S_BANDWIDTH, H_BANDWIDTH, adv_comb_pal, P_AV_PAL, 0, 0),
    SET(S_CHROMA_TAPS, H_CHROMA_TAPS, adv_ct_ntsc, P_AV_NTSC, 0, 0),
    SET(S_CHROMA_TAPS, H_CHROMA_TAPS, adv_ct_pal, P_AV_PAL, 0, 0),
    HEAD(S_FILTERS),
    SET(S_Y_FILTER, H_Y_FILTER, adv_filter_y, P_AV, OL_FILTER_Y, 0),
    SET(S_Y_FILTER_MODE, H_Y_FILTER_MODE, adv_filter_wy_ovr, P_SV, OL_WY_MODE, 0),
    SET(S_Y_FILTER, H_Y_FILTER, adv_filter_wy, P_SV_WY, OL_FILTER_WY, 0),
    SET(S_C_FILTER, H_C_FILTER, adv_filter_c, P_AV, OL_FILTER_C, 0),
    HEAD(S_ENHANCE),
    SET(S_DNR, H_DNR, adv_dnr, P_NONE, 0, 0),
    SET(S_CTI, H_CTI, adv_cti, P_NONE, 0, 0),
    SUB(S_ACE, H_ACE, P_NONE, kAce),
    ACT(S_RESET_FILTERS, H_FILTER_DEFAULTS, adv_filt_defaults, LV_NONE, P_NONE, NF_CONFIRM),
    FNA(S_RESET_DECODER, H_RESET_DECODER, FN_DECODER_RESET, P_NONE, NF_CONFIRM),
};
static const MenuNode kAutoSearch[] SET_PROGMEM = {   // VGA, SoG, S-Video, Composite: Pro only (CAP_ADV)
    SET(S_IN_VGA, H_SCAN, input_scan_vga, P_NONE, 0, 0),
    SET(S_IN_RGBS, H_SCAN, input_scan_rgbs, P_NONE, 0, 0),
    SET(S_IN_RGSB, H_SCAN, input_scan_rgsb, P_NONE, 0, 0),
    SET(S_IN_YUV, H_SCAN, input_scan_yuv, P_NONE, 0, 0),
    SET(S_IN_SV, H_SCAN, input_scan_sv, P_NONE, 0, 0),
    SET(S_IN_AV, H_SCAN, input_scan_av, P_NONE, 0, 0),
};
static const MenuNode kInput[] SET_PROGMEM = {
    SET(S_SOURCE, H_SOURCE, input_source, P_NONE, 0, NF_COMMIT),
    SUBF(S_AUTO_SEARCH, H_AUTO_SEARCH, P_NONE, kAutoSearch, 0),
    SET(S_LOST_WAIT, H_LOST_WAIT, input_lost_wait, P_NONE, 0, NF_ADVANCED),
    SET(S_VIDEO_FORMAT, H_VIDEO_FORMAT, adv_format_sv, P_SV, 0, NF_REVERT),
    SET(S_VIDEO_FORMAT, H_VIDEO_FORMAT, adv_format_av, P_AV, 0, NF_REVERT),
    SET(S_2X, H_2X, adv_i2p, P_SVAV, 0, 0),
    SET(S_2X_SMOOTH, H_2X_SMOOTH, adv_smooth, P_I2P_ON, 0, NF_ADVANCED),
    SET(S_ADAPTIVE_SYNC, H_ADAPTIVE_SYNC, input_adaptive, P_NONE, 0, 0),
    SET(S_SYNC_STRIPPER, H_SYNC_STRIPPER, input_sync_strip, P_RGB_SYNC, 0, NF_REVERT),
    SET(S_RGBHV_SCALING, H_RGBHV_SCALING, pic_rgbhv_scale, P_VGA, 0, 0),
    TOG(S_TERMINATION, H_TERMINATION, FN_TERMINATION, P_RGBS, NF_ADVANCED),
    SUBF(S_ADC, H_ADC, P_NONE, kAdc, NF_ADVANCED),
    SUBF(S_DECODER, H_DECODER, P_SVAV, kDecoder, NF_ADVANCED),
};

// ---- Output ----
static const MenuNode kOutput[] SET_PROGMEM = {
    CHO(S_RESOLUTION, H_RESOLUTION, CH_RESOLUTION, P_NONE, NF_REVERT | NF_COMMIT),
    SET(S_FORCE60, H_FORCE60, output_force60, P_NONE, 0, 0),
    SET(S_FRAME_LOCK, H_FTL, output_ftl, P_FTL_OK, 0, NF_REVERT),
    SET(S_LOCK_METHOD, H_LOCK_METHOD, output_ftl_method, P_FTL_ON, 0, NF_ADVANCED),
    SET(S_HDMI_RANGE, H_HDMI_RANGE, output_hdmi, P_NONE, 0, 0),
    SET(S_COMPONENT, H_ANALOG_OUT, output_component, P_NONE, 0, NF_REVERT | NF_ADVANCED),
    SET(S_EXT_CLOCK, H_EXT_CLOCK, sys_ext_clock, P_NONE, 0, NF_RESTART_NOTE | NF_ADVANCED),
};

// ---- Picture ----
static const MenuNode kPicture[] SET_PROGMEM = {
    CHO(S_FILTER, H_FILTER, CH_FILTER, P_NONE, 0),
    CHO(S_SCANLINES, H_SCANLINES, CH_SCANLINES, P_SCANLINES_OK, 0),
    SET(S_BRIGHTNESS, H_PIC_BRIGHTNESS, pic_brightness, P_NONE, 0, 0),
    SET(S_CONTRAST, H_PIC_CONTRAST, pic_contrast, P_NONE, 0, 0),
    SET(S_SATURATION, H_PIC_SATURATION, pic_saturation, P_NONE, 0, 0),
    SET(S_DEINTERLACE, H_DEINTERLACE, pic_deint, P_DEINT_OK, OL_DEINT, 0),
    FNA(S_RESET_PICTURE, H_RESET_PICTURE, FN_PICTURE_RESET, P_NONE, NF_CONFIRM),
    HEAD(S_DETAIL),
    SET(S_SHARPNESS, H_SHARPNESS, pic_sharpness, P_NONE, 0, NF_ADVANCED),
    SET(S_LINE_FILTER, H_LINE_FILTER, pic_line_filter, P_NONE, 0, NF_ADVANCED),
    SET(S_STEP_RESPONSE, H_STEP_RESPONSE, pic_step_resp, P_NONE, 0, NF_ADVANCED),
    SET(S_SCAN_STRENGTH, H_SCAN_STRENGTH, pic_scan_strength, P_SCANLINES_OK, OPTS_STEP16, NF_ADVANCED),
    HEAD(S_COLOR),
    SET(S_RED, H_RED, pic_red, P_NONE, 0, NF_ADVANCED),
    SET(S_GREEN, H_GREEN, pic_green, P_NONE, 0, NF_ADVANCED),
    SET(S_BLUE, H_BLUE, pic_blue, P_NONE, 0, NF_ADVANCED),
    ACT(S_RESET_COLORS, H_RESET_COLORS, pic_reset_colors, LV_NONE, P_NONE, NF_CONFIRM | NF_ADVANCED),
};

// ---- Screen ----
static const MenuNode kScreen[] SET_PROGMEM = {
    ADJ(S_POSITION, H_POSITION, AJ_MOVE, LV_POSITION, 0),
    ADJ(S_SIZE, H_SIZE, AJ_SIZE, LV_SIZE, 0),
    ADJ(S_BORDERS, H_BORDERS, AJ_BORDERS, LV_BORDERS, 0),
    FNA(S_RESET_GEOMETRY, H_RESET_GEOMETRY, FN_GEOMETRY_RESET, P_NONE, NF_CONFIRM),
    ADJ(S_BORDER_POS, H_BORDER_POS, AJ_BORDER_MOVE, LV_BORDER_POS, NF_ADVANCED),
    SET(S_FULL_HEIGHT, H_FULL_HEIGHT, screen_full_h, P_NONE, 0, NF_ADVANCED),
};

// ---- Profiles ----
static const MenuNode kProfiles[] SET_PROGMEM = {
    DYN(S_LOAD, H_LOAD, FN_PROFILE_LOAD_LIST),
    DYN(S_NEW_PROFILE, H_NEW_PROFILE, FN_PROFILE_NAME_LIST),
    NODE(N_ACTION, NF_FN, S_SAVE_CHANGES, H_SAVE_CHANGES, FN_PROFILE_SAVE_ACTIVE, LV_NONE, P_NONE, LT_PROFILE_SAVE),
    NODE(N_ACTION, NF_FN, S_DISCARD_CHANGES, H_DISCARD_CHANGES, FN_PROFILE_DISCARD, LV_NONE, P_NONE, LT_PROFILE_SAVE),
    DYN(S_MANAGE, H_MANAGE, FN_PROFILE_MANAGE_LIST),
    SET(S_AUTOLOAD, H_AUTOLOAD, profiles_autoload, P_NONE, 0, 0),
    SET(S_SWITCH_INPUT, H_SWITCH_INPUT, profiles_switch_input, P_NONE, 0, NF_ADVANCED),
    SET(S_STARTUP, H_STARTUP, profiles_startup, P_NONE, 0, NF_ADVANCED),
};

// ---- System ----
static const MenuNode kSound[] SET_PROGMEM = {
    SET(S_VOLUME, H_VOLUME, audio_volume, P_NONE, 102, NF_PERCENT),   // percent, 2 per step
    SET(S_MUTE, H_MUTE, audio_mute, P_NONE, 0, 0),
};
static const MenuNode kWifi[] SET_PROGMEM = {
    LIVE(S_MODE, H_MODE, LT_WIFI_MODE, P_NONE, 0),
    LIVE(S_NETWORK, H_NETWORK, LT_WIFI_SSID, P_NONE, 0),
    LIVE(S_ADDRESS, H_ADDRESS, LT_WIFI_IP, P_NONE, 0),
    LIVE(S_HOSTNAME, H_HOSTNAME, LT_WIFI_HOST, P_NONE, 0),
    LIVE(S_PASSWORD, H_PASSWORD, LT_WIFI_PASS, P_WIFI_AP, 0),
    SET(S_WIFI_OPTIMIZE, H_WIFI_OPTIMIZE, sys_wifi_optimize, P_NONE, 0, NF_ADVANCED),
    NODE(N_ACTION, NF_FN | NF_ADVANCED, S_EXTRA_HOSTS, H_EXTRA_HOSTS, FN_EDIT_HOSTS, LV_NONE, P_NONE, LT_EXTRA_HOSTS),
    FNA(S_RESET_AP, H_RESET_AP, FN_WIFI_RESET, P_NONE, NF_CONFIRM),
};
// OTA over plain HTTP: Other channels is the only way onto Beta or an older build
static const MenuNode kUpdateChannels[] SET_PROGMEM = {
    FNA(S_CH_STABLE, H_CH_CHECK, FN_CHECK_CHANNEL_STABLE, P_NONE, 0),
    FNA(S_CH_BETA, H_CH_CHECK, FN_CHECK_CHANNEL_BETA, P_NONE, 0),
    NODE(N_ACTION, NF_FN | NF_CONFIRM, S_INSTALL, H_INSTALL_MANUAL, FN_INSTALL_MANUAL, LV_NONE, P_UPDATE_MANUAL,
         LT_MANUAL_VERSION),
    LIVE(S_OFFER, H_OFFER, LT_MANUAL_OFFER, P_UPDATE_MANUAL, 0),
};
static const MenuNode kUpdate[] SET_PROGMEM = {   // both firmware versions stay visible
    LIVE(S_CHANNEL, H_CHANNEL, LT_FW_CHANNEL, P_NONE, 0),
    FNA(S_CHECK_NOW, H_CHECK_NOW, FN_CHECK_UPDATE, P_NONE, 0),
    ACTV(S_INSTALL, H_INSTALL, sys_install_update, P_UPDATE_AVAILABLE, LT_UPDATE_VERSION),
    LIVE(S_OFFER, H_OFFER, LT_UPDATE_OFFER, P_UPDATE_AVAILABLE, 0),
    SUB(S_OTHER_CHANNELS, H_OTHER_CHANNELS, P_NONE, kUpdateChannels),
    LIVE(S_FW_GBS, H_FW_GBS, LT_FW_GBS, P_NONE, 0),
    LIVE(S_UPDATE_ADV_CONTROLLER, H_FW_GBS, LT_FW_ADV, P_NONE, 0),
    ACTV(S_ADV_UPDATE, H_UPDATE_ADV_CONTROLLER, sys_update_adv_controller, P_ADV_CONTROLLER_UPDATE, LT_ADV_CONTROLLER_NEW),
    SET(S_DAILY_CHECK, H_DAILY_CHECK, sys_auto_update, P_NONE, 0, NF_ADVANCED),
    LIVE(S_LAST_RESULT, H_LAST_RESULT, LT_UPDATE_RESULT, P_NONE, NF_ADVANCED),
};
static const MenuNode kOsd[] SET_PROGMEM = {
    SET(S_MENU_POSITION, H_POSITION_OSD, osd_position, P_NONE, 0, 0),
    SET(S_OSD_THEME, H_OSD_THEME, osd_theme, P_NONE, 0, 0),
    SET(S_OSD_POPUP_POS, H_OSD_POPUP_POS, osd_popup_pos, P_NONE, 0, 0),
    SET(S_OSD_TIMEOUT, H_OSD_TIMEOUT, osd_timeout, P_NONE, 0, 0),
};
static const MenuNode kSystem[] SET_PROGMEM = {
    FNA(S_SIGNAL_INFO, H_SIGNAL_INFO, FN_OPEN_INFO, P_NONE, 0),
    SUB(S_AUDIO, H_AUDIO, P_NONE, kSound),
    SUB(S_WIFI, H_WIFI, P_NONE, kWifi),
    SUB(S_UPDATE, H_UPDATE, P_NONE, kUpdate),
    SET(S_STATUS_POPUP, H_STATUS_POPUP, osd_status_popup, P_NONE, 0, 0),
    SET(S_TV_CHANGES, H_TV_CHANGES, osd_tv_changes, P_NONE, 0, 0),   // standard board: TV5725 icons while adjusting
    SET(S_ENCODER_DIR, H_ENCODER_DIR, sys_encoder_dir, P_NONE, 0, 0),   // Easy
    ACT(S_RESTART, H_RESTART, sys_restart, LV_NONE, P_NONE, 0),
    ACT(S_FACTORY_RESET, H_FACTORY_RESET, sys_factory_reset, LV_NONE, P_NONE, 0),
    SUBF(S_OSD, H_OSD, P_NONE, kOsd, NF_ADVANCED),
    SET(S_BOARD, H_BOARD, sys_board, P_NONE, 0, NF_ADVANCED | NF_RESTART_NOTE),   // Auto / Standard / Pro
    SET(S_ENCODER_STEPS, H_ENCODER_STEPS, sys_encoder_steps, P_NONE, 0, NF_ADVANCED),
    SET(S_DEV_MODE, H_DEV_MODE, sys_devmode, P_NONE, 0, NF_ADVANCED),
};

// ---- Developer ----   (Advanced + System > Developer menu)
static const MenuNode kDevSync[] SET_PROGMEM = {
    SET(S_WATCHER_MODE, H_WATCHER_MODE, dev_syncwatcher, P_NONE, OL_SYNCWATCHER, 0),
    ACT(S_SYNC_WATCHER, H_SYNC_WATCHER, dev_syncwatcher, LV_SYNCWATCHER, P_NONE, 0),
    ACT(S_INVERT_SYNC, H_INVERT_SYNC, dev_invert_sync, LV_SYNC_POLARITY, P_NONE, 0),
    STEP(S_HSYNC_POS, H_HSYNC_POS, dev_hs_left, dev_hs_right, LV_NONE, P_NONE),
    ACT(S_RESET_SYNC, H_RESET_SYNC, dev_sync_reset, LV_NONE, P_NONE, 0),
    ACT(S_IF_OFFSET, H_IF_OFFSET, dev_if_offset, LV_IF_OFFSET, P_NONE, 0),
};
static const MenuNode kDevClock[] SET_PROGMEM = {
    STEP(S_HTOTAL, H_HTOTAL, dev_htotal_down, dev_htotal_up, LV_HTOTAL, P_NONE),
    ACT(S_RESYNC_HTOTAL, H_RESYNC_HTOTAL, dev_htotal_resync, LV_NONE, P_NONE, 0),
    STEP(S_PLL_DIV, H_PLL_DIV, dev_pll_down, dev_pll_up, LV_PLL_DIV, P_NONE),
    ACT(S_SDRAM, H_SDRAM, dev_sdram_cycle, LV_SDRAM, P_NONE, 0),
    ACT(S_OVERSAMPLING, H_OVERSAMPLING, dev_osr_cycle, LV_OSR, P_NONE, 0),
    ACT(S_ADC_FILTER, H_ADC_FILTER, dev_adc_filter, LV_ADC_FILTER, P_NONE, 0),
    ACT(S_SNAP, H_SNAP, dev_snap_rate, LV_OUT_RATE, P_NONE, 0),
};
static const MenuNode kDevDebug[] SET_PROGMEM = {
    ACT(S_DEBUG_VIEW, H_DEBUG_VIEW, dev_debug_view, LV_DEBUG_VIEW, P_NONE, 0),
    ACT(S_FREEZE, H_FREEZE, dev_freeze, LV_FREEZE, P_NONE, 0),
    ACT(S_RESET_CHIP, H_RESET_CHIP, dev_reset_chip, LV_NONE, P_NONE, NF_CONFIRM),
    ACT(S_OSD_TEST, H_OSD_TEST, dev_osd_test, LV_OSD_TEST, P_NONE, 0),
    ACT(S_OSD_ALWAYS_ON, H_OSD_ALWAYS_ON, dev_osd_always_on, LV_OSD_ALWAYS_ON, P_NONE, 0),
    ACT(S_OSD_ROM_DUMP, H_OSD_ROM_DUMP, dev_osd_rom_dump, LV_NONE, P_NONE, 0),
};
static const MenuNode kDevSerial[] SET_PROGMEM = {
    ACT(S_PRINT_INFO, H_PRINT_INFO, dev_print_info, LV_PRINT_INFO, P_NONE, 0),
    ACT(S_VIDEO_TIMINGS, H_VIDEO_TIMINGS, dev_timings, LV_NONE, P_NONE, 0),
    ACT(S_LIST_FILES, H_LIST_FILES, dev_list_files, LV_NONE, P_NONE, 0),
    ACT(S_IR_LOG, H_IR_LOG, dev_ir_codes, LV_IR_LOG, P_NONE, 0),
    SET(S_WIFI_WATCH, H_WIFI_WATCH, dev_wifi_watch, P_NONE, 0, 0),
};
static const MenuNode kDeveloper[] SET_PROGMEM = {
    SUB(S_SYNC, H_SYNC_DEV, P_NONE, kDevSync),
    SUB(S_CLOCK_ADC, H_CLOCK_ADC, P_NONE, kDevClock),
    STEP(S_MEM_SHIFT, H_MEM_SHIFT, dev_mem_left, dev_mem_right, LV_NONE, P_NONE),
    SUB(S_DEBUG, H_DEBUG, P_NONE, kDevDebug),
    SUB(S_SERIAL_LOG, H_SERIAL_LOG, P_NONE, kDevSerial),
    ACT(S_ENABLE_OTA, H_ENABLE_OTA, dev_enable_ota, LV_OTA, P_NONE, 0),
    FNAT(S_OTA_PASSWORD, H_OTA_PASSWORD, FN_EDIT_OTA, P_NONE, LT_OTA_PASSWORD),
};

// ---- Root ----
static const MenuNode kRootKids[] SET_PROGMEM = {
    SUB(S_INPUT, H_INPUT, P_NONE, kInput),
    SUB(S_OUTPUT, H_OUTPUT, P_NONE, kOutput),
    SUB(S_PICTURE, H_PICTURE, P_NONE, kPicture),
    SUB(S_SCREEN, H_SCREEN, P_NONE, kScreen),
    SUBV(S_PROFILES, H_PROFILES, P_NONE, kProfiles, LT_PROFILE_LABEL),   // value: the active profile
    SUB(S_SYSTEM, H_SYSTEM, P_NONE, kSystem),
    SUB(S_DEVELOPER, H_DEVELOPER, P_DEVMODE, kDeveloper),
    SET(S_MENU_MODE, H_MENU_MODE, sys_menu_mode, P_NONE, 0, 0),
};
static const MenuNode kRoot SET_PROGMEM = SUB(S_MENU, S_NONE, P_NONE, kRootKids);

const MenuNode* menuRoot() { return &kRoot; }

// ---- Quick menu (OK with the menu closed) ----
static const MenuNode kQuickKids[] SET_PROGMEM = {
    NODE(N_ACTION, NF_FN, S_PROFILE, H_QUICK_PROFILE, FN_QUICK_PROFILE, LV_NONE, P_NONE, LT_PROFILE_LABEL),
    SET(S_SOURCE, H_SOURCE, input_source, P_NONE, 0, NF_COMMIT),
    CHO(S_RESOLUTION, H_RESOLUTION, CH_RESOLUTION, P_NONE, NF_REVERT | NF_COMMIT),
    CHO(S_FILTER, H_FILTER, CH_FILTER, P_NONE, 0),
    CHO(S_SCANLINES, H_SCANLINES, CH_SCANLINES, P_SCANLINES_OK, 0),
    SET(S_2X, H_2X, adv_i2p, P_SVAV, 0, 0),
    FNA(S_TV_OSD, H_TV_OSD, FN_TV_OSD, P_TV_OSD, 0),   // last: just above the encoder's All settings row
};
static const MenuNode kQuick SET_PROGMEM = SUB(S_QUICK, S_NONE, P_NONE, kQuickKids);

const MenuNode* menuQuickRoot() { return &kQuick; }

