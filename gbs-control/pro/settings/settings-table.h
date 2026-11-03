// Single source of truth for persisted settings.
// SETTING(id, key, type, uopt field, default, min, max, scope, apply groups, options, flags, caps)
#pragma once

// Enum options, "Name|Name=value|..." (a "@cap" suffix needs that capability, "#a" is Advanced only). The registry
// turns each into a flash array named after it (settings-registry.cpp).
#define SETTINGS_OPTIONS(OPT) \
    OPT(OPT_INPUTS,    "Auto=0|RGB SCART=1|RGB SoG=2|VGA=3|Component=4|S-Video=5@adv|Composite=6@adv") \
    OPT(OPT_LOST_WAIT, "1s=1|2s=2|5s=5|10s=10")   /* seconds */ \
    OPT(OPT_THEMES,    "GBSC PRO|Classic|Light|Retro") \
    OPT(OPT_OSD_POS,   "Top left|Top|Top right|Center left|Center|Center right|Bottom left|Bottom|Bottom right") \
    OPT(OPT_OSD_TIME,  "5s|10s|15s|30s|60s|10min") \
    OPT(OPT_PRESETS,   "960p=0|480p=1|Custom=2|720p=3|1024p=4|1080p=5|1200p=7|Downscale=6@original|Bypass=10@original") \
    OPT(OPT_BOARD,     "Auto=0|Standard=2|Pro=1")   /* Standard = the plain gbs-control board */ \
    OPT(OPT_AUTOUPD,   "Off|Daily") \
    OPT(OPT_ENC_DIR,   "Normal|Reversed") \
    OPT(OPT_ENC_STEP,  "1|2|4")   /* menu steps per detent */ \
    OPT(OPT_FTL,       "Method 1|Method 2")   /* 0 Vtotal+VSST, 1 Vtotal */ \
    OPT(OPT_HDMI,      "Auto=1|Full=0|Limited=3|Limited SD=2#a")   /* values: 0 off, 1 HD modes, 2 SD/PC modes, 3 all */ \
    OPT(OPT_MENU_MODE, "Easy|Advanced") \
    OPT(OPT_STARTUP,   "Last used|Defaults") \
    OPT(OPT_POPUP,     "Off|Input only|Always") \
    OPT(OPT_COMB_BW_N, "Narrow=0|Medium=1|Wide=3")   /* UG-637 Table 64: 01 = 10 = Medium */ \
    OPT(OPT_COMB_BW_P, "Narrow=0|Medium=1|Wide=2|Widest=3")   /* Table 68 */ \
    OPT(OPT_TAPS_N,    "3-2 lines=1|5-3 lines=2|5-4 lines=3")   /* 00: do not use */ \
    OPT(OPT_TAPS_P,    "5-2 lines=1|5-3 lines=2|5-4 lines=3") \
    OPT(OPT_FORMATS,   "Auto=0|Auto 7.5 IRE=12|PAL=1|NTSC 7.5 IRE=2|PAL-60=3|NTSC 4.43=4#a|NTSC-J=5|PAL-N 7.5 IRE=6#a|PAL-M 0 IRE=7#a|PAL-M 7.5 IRE=8#a|PAL-Nc=9#a|PAL-Nc 7.5IRE=10#a|SECAM=11#a")   /* index: advFormatRegister() */

#define SETTINGS_TABLE(SETTING) \
    /* ---- Device ---- */ \
    SETTING(input_source,      "input.source",          Enum, inputChoice,                   0,      0, 10,     Device,  APPLY_INPUT,       OPT_INPUTS,  0, CAP_NONE) \
    SETTING(input_last,        "input.last",            U8,   inputLast,                     1,      1, 6,      Device,  APPLY_NONE,        nullptr,     SETTING_FLAG_DEVICE_ONLY, CAP_NONE) \
    SETTING(input_scan_vga,    "input.scan_vga",        Bool, scanVga,                       1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_ADV) \
    SETTING(input_scan_rgbs,   "input.scan_rgbs",       Bool, scanRgbs,                      1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(input_scan_rgsb,   "input.scan_rgsb",       Bool, scanRgsb,                      1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_ADV) \
    SETTING(input_scan_yuv,    "input.scan_yuv",        Bool, scanYuv,                       1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(input_scan_sv,     "input.scan_sv",         Bool, scanSv,                        1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_ADV) \
    SETTING(input_scan_av,     "input.scan_av",         Bool, scanAv,                        1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_ADV) \
    SETTING(input_lost_wait,   "input.lost_wait",       Enum, inputLostWait,                 2,      1, 10,     Device,  APPLY_NONE,        OPT_LOST_WAIT, 0, CAP_NONE) \
    SETTING(osd_theme,         "osd.theme",             Enum, osdTheme,                      0,      0, 3,      Device,  APPLY_OSD,         OPT_THEMES,  0, CAP_OSD) \
    SETTING(osd_position,      "osd.position",          Enum, osdPosition,                   4,      0, 8,      Device,  APPLY_OSD,         OPT_OSD_POS, 0, CAP_OSD) \
    SETTING(osd_popup_pos,     "osd.popup_position",    Enum, osdPopupPosition,              7,      0, 8,      Device,  APPLY_OSD,         OPT_OSD_POS, 0, CAP_OSD) \
    SETTING(osd_timeout,       "osd.timeout",           Enum, osdTimeout,                    2,      0, 5,      Device,  APPLY_OSD,         OPT_OSD_TIME, 0, CAP_NONE) \
    SETTING(osd_status_popup,  "osd.status_popup",      Enum, osdStatusPopup,                1,      0, 2,      Device,  APPLY_NONE,        OPT_POPUP,   0, CAP_OSD) \
    SETTING(osd_tv_changes,    "osd.tv_changes",        Bool, tvChanges,                     1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_ORIGINAL) \
    SETTING(audio_volume,      "audio.volume",          U8,   volume,                        80,     0, 100,    Profile, APPLY_AUDIO,       nullptr,     0, CAP_AUDIO) \
    SETTING(audio_mute,        "audio.mute",            Bool, audioMuted,                    0,      0, 1,      Device,  APPLY_AUDIO,       nullptr,     0, CAP_AUDIO) \
    SETTING(output_component,  "output.component",      Bool, wantOutputComponent,           0,      0, 1,      Device,  APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(sys_adc_cal,       "system.adc_calibration",Bool, enableCalibrationADC,          1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(sys_ext_clock,     "system.ext_clock",      Bool, disableExternalClockGenerator, 1,      0, 1,      Device,  APPLY_NONE,        nullptr,     SETTING_FLAG_INVERT, CAP_NONE) \
    SETTING(sys_board,         "system.board",          Enum, boardOverride,                 0,      0, 2,      Device,  APPLY_NONE,        OPT_BOARD,   0, CAP_NONE) \
    SETTING(sys_encoder_dir,   "system.encoder_direction", Enum, encoderReverse,             0,      0, 1,      Device,  APPLY_NONE,        OPT_ENC_DIR, 0, CAP_ENCODER) \
    SETTING(sys_encoder_steps, "system.encoder_steps",  Enum, encoderSteps,                  0,      0, 2,      Device,  APPLY_NONE,        OPT_ENC_STEP, 0, CAP_ENCODER) \
    SETTING(sys_auto_update,   "system.auto_update_check", Enum, autoUpdateCheck,            1,      0, 1,      Device,  APPLY_NONE,        OPT_AUTOUPD, 0, CAP_NONE) \
    SETTING(sys_wifi_optimize, "system.wifi_optimize",  Bool, wifiOptimize,                  1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(profiles_autoload, "profiles.autoload",     Bool, profilesAutoload,              1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(profiles_switch_input, "profiles.switch_input", Bool, profilesSwitchInput,     1,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(profiles_startup,  "profiles.startup",      Enum, profilesStartup,               0,      0, 1,      Device,  APPLY_NONE,        OPT_STARTUP, 0, CAP_NONE) \
    SETTING(sys_devmode,       "system.devmode",        Bool, devMode,                       0,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(sys_menu_mode,     "system.menu_mode",      Enum, menuMode,                      0,      0, 1,      Device,  APPLY_NONE,        OPT_MENU_MODE, 0, CAP_NONE) \
    /* ---- Profile: output ---- */ \
    SETTING(output_preset,     "output.preset",         Enum, presetPreference,              0,      0, 10,     Profile, APPLY_PRESET,      OPT_PRESETS, 0, CAP_NONE) \
    SETTING(output_ftl,        "output.frame_lock",     Bool, enableFrameTimeLock,           0,      0, 1,      Profile, APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(output_ftl_method, "output.frame_lock_method", Enum, frameTimeLockMethod,        0,      0, 1,      Profile, APPLY_NONE,        OPT_FTL,     0, CAP_NONE) \
    SETTING(output_force60,    "output.force_60hz",     Bool, PalForce60,                    0,      0, 1,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(output_match,      "output.match_preset",   Bool, matchPresetSource,             1,      0, 1,      Profile, APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(output_hdmi,       "output.hdmi_range",     Enum, hdmiLimitedRange,              1,      0, 3,      Profile, APPLY_COLOR,       OPT_HDMI,    0, CAP_NONE) \
    /* ---- Profile: input ---- */ \
    SETTING(input_auto_gain,   "input.auto_gain",       Bool, enableAutoGain,                0,      0, 1,      Profile, APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(input_sync_strip,  "input.sync_stripper",   Bool, advSyncStripper,               1,      0, 1,      Profile, APPLY_SYNC,        nullptr,     0, CAP_ADV) \
    SETTING(input_adaptive,    "input.adaptive_sync",   Bool, adaptiveSync,                  1,      0, 1,      Profile, APPLY_NONE,        nullptr,     0, CAP_NONE) \
    /* ---- Profile: picture ---- */ \
    SETTING(pic_scanlines,     "picture.scanlines",     Bool, wantScanlines,                 0,      0, 1,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(pic_scan_strength, "picture.scanline_strength", U8, scanlineStrength,            0x30,   0, 0x60,   Profile, APPLY_PICTURE,     nullptr,     0, CAP_NONE) \
    SETTING(pic_deint,         "picture.deinterlace",   U8,   deintMode,                     0,      0, 2,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(pic_line_filter,   "picture.line_filter",   Bool, wantVdsLineFilter,             1,      0, 1,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(pic_sharpness,     "picture.sharpness",     U8,   picSharpness,                  22,     0, 31,     Profile, APPLY_PICTURE,     nullptr,     0, CAP_NONE) \
    SETTING(pic_rgbhv_scale,   "picture.rgbhv_scaling", Bool, preferScalingRgbhv,            1,      0, 1,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(pic_step_resp,     "picture.step_response", Bool, wantStepResponse,              1,      0, 1,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    SETTING(pic_red,           "picture.red",           U8,   gbsColorR,                     128,    0, 255,    Profile, APPLY_COLOR,       nullptr,     0, CAP_NONE) \
    SETTING(pic_green,         "picture.green",         U8,   gbsColorG,                     128,    0, 255,    Profile, APPLY_COLOR,       nullptr,     0, CAP_NONE) \
    SETTING(pic_blue,          "picture.blue",          U8,   gbsColorB,                     128,    0, 255,    Profile, APPLY_COLOR,       nullptr,     0, CAP_NONE) \
    SETTING(pic_brightness,    "picture.brightness",    U8,   picBrightness,                 50,     0, 100,    Profile, APPLY_COLOR,       nullptr,     0, CAP_NONE) \
    SETTING(pic_contrast,      "picture.contrast",      U8,   picContrast,                   50,     0, 100,    Profile, APPLY_COLOR,       nullptr,     0, CAP_NONE) \
    SETTING(pic_saturation,    "picture.saturation",    U8,   picSaturation,                 50,     0, 100,    Profile, APPLY_COLOR,       nullptr,     0, CAP_NONE) \
    SETTING(screen_full_h,     "screen.full_height",    Bool, wantFullHeight,                1,      0, 1,      Profile, APPLY_PRESET,      nullptr,     0, CAP_NONE) \
    /* ---- Profile: ADV7280 decoder ---- */ \
    SETTING(adv_format_sv,     "adv.format_sv",         Enum, svVideoFormat,                 0,      0, 12,     Profile, APPLY_ADV_FORMAT,  OPT_FORMATS, 0, CAP_ADV) \
    SETTING(adv_format_av,     "adv.format_av",         Enum, avVideoFormat,                 0,      0, 12,     Profile, APPLY_ADV_FORMAT,  OPT_FORMATS, 0, CAP_ADV) \
    SETTING(adv_i2p,           "adv.i2p",               Bool, advI2P,                        0,      0, 1,      Profile, APPLY_ADV_I2P,     nullptr,     0, CAP_ADV) \
    SETTING(adv_smooth,        "adv.smooth",            Bool, advSmooth,                     0,      0, 1,      Profile, APPLY_ADV_I2P,     nullptr,     0, CAP_ADV) \
    SETTING(adv_ace,           "adv.ace",               Bool, advACE,                        0,      0, 1,      Profile, APPLY_ADV_ACE,     nullptr,     0, CAP_ADV) \
    SETTING(adv_brightness,    "adv.brightness",        U8,   advBrightness,                 50,     0, 100,    Profile, APPLY_ADV_BCSH,    nullptr,     0, CAP_ADV) \
    SETTING(adv_contrast,      "adv.contrast",          U8,   advContrast,                   50,     0, 100,    Profile, APPLY_ADV_BCSH,    nullptr,     0, CAP_ADV) \
    SETTING(adv_saturation,    "adv.saturation",        U8,   advSaturation,                 50,     0, 100,    Profile, APPLY_ADV_BCSH,    nullptr,     0, CAP_ADV) \
    SETTING(adv_hue,           "adv.hue",               U8,   advHue,                        50,     0, 100,    Profile, APPLY_ADV_BCSH,    nullptr,     0, CAP_ADV) \
    SETTING(adv_ace_luma,      "adv.ace_luma_gain",     U8,   advACELumaGain,                13,     0, 31,     Profile, APPLY_ADV_ACE,     nullptr,     0, CAP_ADV) \
    SETTING(adv_ace_chroma,    "adv.ace_chroma_gain",   U8,   advACEChromaGain,              8,      0, 15,     Profile, APPLY_ADV_ACE,     nullptr,     0, CAP_ADV) \
    SETTING(adv_ace_cmax,      "adv.ace_chroma_max",    U8,   advACEChromaMax,               8,      0, 15,     Profile, APPLY_ADV_ACE,     nullptr,     0, CAP_ADV) \
    SETTING(adv_ace_gamma,     "adv.ace_gamma_gain",    U8,   advACEGammaGain,               8,      0, 15,     Profile, APPLY_ADV_ACE,     nullptr,     0, CAP_ADV) \
    SETTING(adv_ace_speed,     "adv.ace_response_speed",U8,   advACEResponseSpeed,           15,     0, 15,     Profile, APPLY_ADV_ACE,     nullptr,     0, CAP_ADV) \
    SETTING(adv_filter_y,      "adv.filter_y",          U8,   advFilterYShaping,             1,      0, 30,     Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_filter_c,      "adv.filter_c",          U8,   advFilterCShaping,             0,      0, 7,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_filter_wy,     "adv.filter_wy",         U8,   advFilterWYShaping,            19,     2, 19,     Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_filter_wy_ovr, "adv.filter_wy_override",Bool, advFilterWYOverride,           1,      0, 1,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_comb_ntsc,     "adv.comb_ntsc",         Enum, advFilterCombNTSC,             0,      0, 3,      Profile, APPLY_ADV_FILTERS, OPT_COMB_BW_N, 0, CAP_ADV) \
    SETTING(adv_comb_pal,      "adv.comb_pal",          Enum, advFilterCombPAL,              1,      0, 3,      Profile, APPLY_ADV_FILTERS, OPT_COMB_BW_P, 0, CAP_ADV) \
    SETTING(adv_cl_ntsc,       "adv.comb_luma_ntsc",    U8,   advCombLumaModeNTSC,           0,      0, 7,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_cc_ntsc,       "adv.comb_chroma_ntsc",  U8,   advCombChromaModeNTSC,         0,      0, 7,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_ct_ntsc,       "adv.comb_taps_ntsc",    Enum, advCombChromaTapsNTSC,         2,      1, 3,      Profile, APPLY_ADV_FILTERS, OPT_TAPS_N,  0, CAP_ADV) \
    SETTING(adv_cl_pal,        "adv.comb_luma_pal",     U8,   advCombLumaModePAL,            0,      0, 7,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_cc_pal,        "adv.comb_chroma_pal",   U8,   advCombChromaModePAL,          0,      0, 7,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_ct_pal,        "adv.comb_taps_pal",     Enum, advCombChromaTapsPAL,          3,      1, 3,      Profile, APPLY_ADV_FILTERS, OPT_TAPS_P,  0, CAP_ADV) \
    SETTING(adv_dnr,           "adv.dnr",               Bool, advDnr,                        0,      0, 1,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(adv_cti,           "adv.cti",               Bool, advCti,                        0,      0, 1,      Profile, APPLY_ADV_FILTERS, nullptr,     0, CAP_ADV) \
    SETTING(dev_syncwatcher,   "dev.syncwatcher",       U8,   syncwatcherMode,           0,      0, 2,      Profile, APPLY_NONE,        nullptr,     0, CAP_NONE) \
    SETTING(dev_wifi_watch,    "dev.wifi_watch",        Bool, wifiWatch,                     0,      0, 1,      Device,  APPLY_NONE,        nullptr,     0, CAP_NONE)

// String settings: SETTING_STR(id, key, uopt char[] field, scope, flags); max length = sizeof(field) - 1
#define SETTINGS_STR_TABLE(SETTING_STR) \
    SETTING_STR(sys_extra_hosts, "system.extra_hosts", extraHosts, Device, 0) \
    SETTING_STR(sys_ota_password, "system.ota_password", otaPassword, Device, SETTING_FLAG_SECRET) \
    SETTING_STR(profile_active, "profile.active", profileActive, Device, SETTING_FLAG_DEVICE_ONLY | SETTING_FLAG_BACKUP) \
    SETTING_STR(profile_hash, "profile.hash", profileHash, Device, SETTING_FLAG_DEVICE_ONLY | SETTING_FLAG_BACKUP) \
    SETTING_STR(sys_adv_controller_skip, "system.adv_controller_skip", advControllerSkip, Device, SETTING_FLAG_DEVICE_ONLY)
