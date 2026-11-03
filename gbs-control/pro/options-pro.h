// Pro fields of struct userOptions (options.h). Ranges, defaults and keys live in settings/settings-table.h: the
// comments here say what a field is, not its values.
#ifndef OPTIONS_PRO_H_
#define OPTIONS_PRO_H_

#include <stdint.h>

#define USER_OPTIONS_PRO_FIELDS \
    uint8_t volume;                 /* audio.volume in percent, 100 = 0 dB (pt2257-map.h) */ \
    uint8_t audioMuted;             /* audio.mute */ \
    uint8_t activeInputType;        /* the input the hardware is on (InputType 1..6); not a setting */ \
    uint8_t inputChoice;            /* input.source: 0 Auto, 1..6 a temporary manual input (Auto at every start) */ \
    uint8_t inputLast;              /* input.last: the last path where a source was found (scan hint, boot path) */ \
    uint8_t scanVga, scanRgbs, scanRgsb, scanYuv, scanSv, scanAv;   /* input.scan_*: paths Auto looks at */ \
    uint8_t inputLostWait;          /* input.lost_wait: seconds without a source before Auto looks elsewhere */ \
    uint8_t wifiOptimize;           /* system.wifi_optimize: the GBSC picks the quietest memory clock */ \
    uint8_t wifiWatch;              /* dev.wifi_watch: pings the router every minute, logs when the link goes bad */ \
    uint8_t svVideoFormat;          /* adv.format_sv: ADV7280 standard index (advFormatRegister) */ \
    uint8_t avVideoFormat;          /* adv.format_av */ \
    uint8_t advSyncStripper;        /* input.sync_stripper: LM1881 in the RGB SCART / RGB SoG sync path */ \
    uint8_t adaptiveSync;           /* input.adaptive_sync: the sync processor follows the source's sync pulse (sync-pulse.h) */ \
    uint8_t osdTheme;               /* osd.theme */ \
    uint8_t osdPosition;            /* osd.position: menu, info, keyboard and update placement (9 positions) */ \
    uint8_t osdPopupPosition;       /* osd.popup_position: mute/volume pill and status pop-up */ \
    uint8_t osdTimeout;             /* osd.timeout: menu and info auto-close (index into the options) */ \
    uint8_t gbsColorR;              /* picture.red: R offset through the VDS YUV matrix, 128 = neutral */ \
    uint8_t gbsColorG;              /* picture.green */ \
    uint8_t gbsColorB;              /* picture.blue */ \
    uint8_t picBrightness;          /* picture.brightness, 50 = neutral */ \
    uint8_t picContrast;            /* picture.contrast, 50 = neutral */ \
    uint8_t picSaturation;          /* picture.saturation, 50 = neutral */ \
    uint8_t picSharpness;           /* picture.sharpness: VDS peaking gain, 0 = off */ \
    uint8_t advI2P;                 /* adv.i2p: decoder line doubling (2X) */ \
    uint8_t advSmooth;              /* adv.smooth: needs 2X */ \
    uint8_t advACE;                 /* adv.ace: adaptive contrast enhancement */ \
    uint8_t advBrightness;          /* adv.brightness, 50 = neutral (adv-state.cpp maps it to the register) */ \
    uint8_t advContrast;            /* adv.contrast */ \
    uint8_t advSaturation;          /* adv.saturation */ \
    uint8_t advHue;                 /* adv.hue */ \
    uint8_t advACELumaGain;         /* adv.ace_luma_gain */ \
    uint8_t advACEChromaGain;       /* adv.ace_chroma_gain */ \
    uint8_t advACEChromaMax;        /* adv.ace_chroma_max */ \
    uint8_t advACEGammaGain;        /* adv.ace_gamma_gain */ \
    uint8_t advACEResponseSpeed;    /* adv.ace_response_speed */ \
    uint8_t advFilterYShaping;      /* adv.filter_y: CVBS luma shaping filter */ \
    uint8_t advFilterCShaping;      /* adv.filter_c: CVBS chroma shaping filter */ \
    uint8_t advFilterWYShaping;     /* adv.filter_wy: S-Video luma shaping filter */ \
    uint8_t advFilterWYOverride;    /* adv.filter_wy_override: 0 automatic, 1 the filter above */ \
    uint8_t advFilterCombNTSC;      /* adv.comb_ntsc: NTSC comb bandwidth */ \
    uint8_t advFilterCombPAL;       /* adv.comb_pal: PAL comb bandwidth */ \
    uint8_t advCombLumaModeNTSC;    /* adv.comb_luma_ntsc */ \
    uint8_t advCombChromaModeNTSC;  /* adv.comb_chroma_ntsc */ \
    uint8_t advCombChromaTapsNTSC;  /* adv.ct_ntsc: chroma comb taps */ \
    uint8_t advCombLumaModePAL;     /* adv.comb_luma_pal */ \
    uint8_t advCombChromaModePAL;   /* adv.comb_chroma_pal */ \
    uint8_t advCombChromaTapsPAL;   /* adv.ct_pal */ \
    uint8_t advDnr;                 /* adv.dnr: digital noise reduction */ \
    uint8_t advCti;                 /* adv.cti: chroma transient improvement */ \
    uint8_t hdmiLimitedRange;       /* output.hdmi_range */ \
    uint8_t  boardOverride;         /* system.board: BOARD_AUTO / BOARD_PRO / BOARD_ORIGINAL (Standard) */ \
    uint8_t  encoderReverse;        /* system.encoder_direction: 0 the board's own direction, 1 reversed */ \
    uint8_t  encoderSteps;          /* system.encoder_steps: 1, 2 or 4 menu steps per detent */ \
    uint8_t  devMode;               /* system.devmode: developer entries in menus and API */ \
    uint8_t  menuMode;              /* system.menu_mode: 0 Easy, 1 Advanced */ \
    uint8_t  profilesAutoload;      /* profiles.autoload: load the profile that matches the signal */ \
    uint8_t  profilesSwitchInput;   /* profiles.switch_input: a manual load switches to the profile's input */ \
    uint8_t  profilesStartup;       /* profiles.startup: 0 last used working state, 1 defaults */ \
    uint8_t  osdStatusPopup;        /* osd.status_popup: 0 off, 1 input or profile change, 2 every change */ \
    uint8_t  tvChanges;             /* osd.tv_changes: standard board, TV5725 icons while the user adjusts */ \
    char     profileActive[4];      /* profile.active: id of the loaded profile, "" for none */ \
    char     profileHash[9];        /* profile.hash: CRC32 (hex) of the Profile scope at load/save */ \
    uint8_t  autoUpdateCheck;       /* system.auto_update_check */ \
    char     otaPassword[33];       /* system.ota_password: ArduinoOTA password (write-only over HTTP) */ \
    char     extraHosts[64];        /* system.extra_hosts: extra Host names, comma-separated (guardExtraHostsValid) */ \
    char     advControllerSkip[9];  /* system.adv_controller_skip: ADV Controller build id the boot prompt skips */ \
    uint8_t  syncwatcherMode;       /* dev.syncwatcher: per-profile override, 0 inherit, 1 on, 2 off */

#endif // OPTIONS_PRO_H_
