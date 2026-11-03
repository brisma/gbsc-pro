// Menu model types: node layout, predicates, engine functions, readbacks, keys, signal state
#pragma once
#include <stdint.h>

enum MenuNodeType : uint8_t { N_SUB, N_SETTING, N_CHOICE, N_STEPPER, N_ACTION, N_TOGGLE_FN, N_LIVE, N_ADJUST2D, N_HEADER };

enum MenuNodeFlags : uint8_t {
    NF_REVERT = 1,         // revert countdown after a change
    NF_RESTART_NOTE = 2,   // "Applies after restart" toast after a change
    NF_CONFIRM = 4,        // confirmation before running
    NF_FN = 8,             // ref is a MenuFn (engine function), not an ActionId
    NF_COMMIT = 16,        // Left/Right edit a pending value, OK applies it
    NF_PERCENT = 32,       // the value is shown with a '%' (volume)
    NF_ADVANCED = 64,      // hidden in Easy menu mode
};

enum MenuPred : uint8_t {
    P_NONE, P_DEVMODE, P_SVAV, P_SV, P_AV, P_SV_WY, P_AV_NTSC, P_AV_PAL, P_RGB_SYNC, P_RGBS, P_ACE_ON,
    P_SCANLINES_OK, P_PROFILE_ACTIVE, P_UPDATE_AVAILABLE, P_ADV_CONTROLLER_UPDATE,
    P_VGA,                    // VGA input only
    P_I2P_ON,                 // S-Video/Composite; n/a while 2X is off
    P_DEINT_OK,               // n/a on S-Video/Composite with 2X on
    P_FTL_OK,                 // n/a when Always 60Hz forces a 50 Hz source to 60
    P_FTL_ON,                 // n/a while Smooth scrolling is off
    P_WIFI_AP,                // hotspot mode only
    P_SOG,                    // sync on green or luma: RGB SoG and Component
    P_TV_OSD,                 // standard board with a source the scaler draws (tvOsdAllowed)
    P_UPDATE_MANUAL,          // Other channels: a checked build can be installed
    P_COUNT };

enum MenuFn : uint8_t {
    FN_NONE, FN_OPEN_INFO, FN_DECODER_RESET, FN_GEOMETRY_RESET, FN_WIFI_RESET, FN_CHECK_UPDATE,
    FN_PROFILE_SAVE_ACTIVE, FN_PROFILE_LOAD_LIST, FN_PROFILE_MANAGE_LIST, FN_TERMINATION,
    FN_EDIT_OTA, FN_EDIT_HOSTS, FN_PROFILE_NAME_LIST, FN_GEOMETRY_KEEP, FN_ADV_CONTROLLER_SKIP,
    FN_GEOMETRY_UNDO,     // adjust Back: reload the running mode's preset (no per-standard geometry settings)
    FN_PICTURE_RESET,     // every Picture page setting back to its default
    FN_QUICK_PROFILE,     // Quick menu Profile row: Left/Right cycle this signal's profiles, OK opens Load
    FN_TV_OSD,            // Quick menu TV OSD row (standard board): the TV5725 icons
    FN_CHECK_CHANNEL_STABLE, FN_CHECK_CHANNEL_BETA,   // Other channels: read that channel
    FN_INSTALL_MANUAL,    // Other channels: install the checked build
    FN_PROFILE_DISCARD,   // Discard changes: the profile in use loaded again as it was saved
    FN_COUNT };

enum MenuLive : uint8_t {
    LV_NONE, LV_ADC_GAIN, LV_POSITION, LV_SIZE, LV_BORDERS, LV_HTOTAL,
    LV_PLL_DIV, LV_SDRAM, LV_OSR, LV_ADC_FILTER, LV_DEBUG_VIEW, LV_FREEZE, LV_IF_OFFSET, LV_SOG, LV_OUT_RATE,
    LV_SYNCWATCHER, LV_OSD_ALWAYS_ON, LV_WIFI_RSSI, LV_OSD_TEST, LV_PRINT_INFO, LV_IR_LOG, LV_SYNC_POLARITY, LV_OTA,
    LV_BORDER_POS, LV_COUNT };

enum MenuText : uint8_t {
    LT_NONE, LT_WIFI_MODE, LT_WIFI_SSID, LT_WIFI_IP, LT_UPDATE_VERSION, LT_UPDATE_RESULT, LT_FW_GBS, LT_FW_ADV,
    LT_OTA_PASSWORD, LT_EXTRA_HOSTS, LT_PROFILE_ACTIVE, LT_WIFI_PASS, LT_WIFI_HOST, LT_SOURCE_NAME, LT_ADV_CONTROLLER_NEW,
    LT_PROFILE_LABEL,     // active profile "<name> <region>" or "Defaults": formatted by menu-model (backends: "")
    LT_PROFILE_SAVE,      // Save changes: the active profile's label, empty when none (menu-model)
    LT_FW_CHANNEL,        // the installed build's channel: "Stable" | "Beta" | "Dev"
    LT_UPDATE_OFFER, LT_UPDATE_CONFIRM,                        // recommended: "Stable switch", "Switch to Stable"
    LT_MANUAL_VERSION, LT_MANUAL_OFFER, LT_MANUAL_CONFIRM,     // Other channels: the checked build
    LT_COUNT };

enum MenuKey : uint8_t { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_BACK, K_MENU, K_INFO, K_SAVE, K_MUTE, K_VOLUP,
                         K_VOLDOWN, K_ROT_L, K_ROT_R, K_CLICK, K_LONG };

// One node of the PROGMEM tree. Field use by type:
//   SETTING  ref = SettingId, opts = option list index (0: registry options) or 100 + numeric step
//   ACTION   ref = ActionId (MenuFn with NF_FN), ref2 = MenuLive readback, opts = MenuText row value (0: none)
//   STEPPER  ref = decrement ActionId, opts = increment ActionId, ref2 = MenuLive readback
//   TOGGLE_FN ref = MenuFn;  LIVE ref = MenuText;  CHOICE ref = choice index;  ADJUST2D ref = adjust index, ref2 = live
//   SUB      children/count, or ref = MenuFn of a dynamic list (NF_FN)
struct MenuNode {
    uint8_t type;
    uint8_t flags;
    uint16_t label;               // MenuStr
    uint16_t help;                // MenuStr (S_NONE: none)
    uint8_t ref;
    uint8_t ref2;
    uint8_t pred;                 // MenuPred
    uint8_t opts;
    uint8_t count;
    const MenuNode* children;     // PROGMEM array (SUB only)
};

enum : uint8_t { UPD_RECEIVING, UPD_VERIFYING, UPD_DONE };

struct MenuSignal {
    bool noSignal;
    bool searching;               // Source = Auto is scanning the inputs (or judging RGB SCART): no input found yet
    bool advFreeRun;              // S-Video/Composite with no source: the decoder outputs free-run colour bars, a valid
                                  // signal for the scaler (ADV Controller report); texts say "No signal", the OSD stays normal
    bool advNtsc;                 // decoder reports an NTSC standard (comb filter keys)
    bool scanlinesAllowed;        // areScanLinesAllowed()
    uint8_t presetId;             // rto->presetID
    uint16_t outHzX10, srcHzX10;
    uint16_t srcLines;
    bool interlaced;
    uint8_t syncType;             // 0 HV, 1 CSync, 2 SOG
    bool frameLocked;
    bool palGroup;                // videoStandardInput 2 or 4 (PAL regions, per-standard screen keys)
    bool palSource;               // a 50 Hz source, also under Always 60Hz (register mode 2 or 4): region default
    uint8_t sogLevel;
    bool updateAvailable;
    bool updateManual;            // Other channels: a checked build can be installed
    int8_t updatePct;             // -1: no upload in progress (ESP image or ADV Controller update)
    bool updateAdvController;              // the running update is the ADV Controller one (overlay title)
    uint8_t updateStage;          // UPD_*: receiving, verifying the image, done (restarting)
    bool advControllerUpdateAvailable;     // the embedded ADV Controller build differs from the running one
    int8_t advStandard;           // S-Video/Composite: ADV7280 AD_RESULT 0..7 while locked (ADV Controller INFO), else -1
};
// AD_RESULT names for the region label: NTSC, NTSC443, PAL-M, PAL60, PAL, SECAM, PAL-N, SECAM (-1/out: "")
const char* menuAdvStandardName(int8_t s);
