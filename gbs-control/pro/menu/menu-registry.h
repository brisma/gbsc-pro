// ====================================================================================
// menu-registry.h
// Menu System Registry - Single Source of Truth
//
// This file contains:
// - OLED_MenuState enum: All OLED menu state identifiers
// - MenuItemMapping struct: OLED state to OSD page/row mapping
// - MENU_ITEMS_*: X-macros for generating oledToOsdMap[]
// - IR handler function declarations
// - OLED handler function declarations
//
// To add a new menu item:
// 1. Add enum value to OLED_MenuState
// 2. Add IR handler declaration
// 3. Add entry to appropriate MENU_ITEMS_* X-macro (if OSD mapping needed)
// 4. Implement IR handler in appropriate menu-*.cpp file
// ====================================================================================

#pragma once

#include <Arduino.h>
#include "../osd/osd-registry.h"  // For OsdCommand enum

// ====================================================================================
// Constants - Mode Options
// ====================================================================================

#define MODEOPTION_MAX 12
#define MODEOPTION_MIN 0
#define STEP 1

// ====================================================================================
// Constants - Video Format
// ====================================================================================

#define RGB1 0x01
#define YUV0 0x00

// ====================================================================================
// Constants - Sync Selection
// ====================================================================================

#define HV_Enable  0x00
#define HV_Disable 0x01

// ====================================================================================
// Video Format Names
// ====================================================================================

// Video format names lookup table (for SV/AV mode display)
static const char* const videoFormatNames[] = {
    "Auto", "PAL", "NTSC-M", "PAL-60", "NTSC443", "NTSC-J",
    "PAL-N w/ p", "PAL-M w/o p", "PAL-M", "PAL Cmb -N",
    "PAL Cmb -N w/ p", "SECAM"
};

// Get video format name by index (0-11), returns "Unknown" if out of range
inline const char* getVideoFormatName(uint8_t index) {
    if (index < MODEOPTION_MAX) return videoFormatNames[index];
    return "Unknown";
}

// ====================================================================================
// OLED_MenuState Enum - All OLED Menu State Identifiers
// ====================================================================================

typedef enum {
    OLED_None,

    // Main menu (top level)
    OLED_Input,
    OLED_OutputResolution,
    OLED_ScreenSettings,
    OLED_ColorSettings,
    OLED_SystemSettings,
    OLED_FactoryReset,

    // Input submenu
    OLED_Input_RGBs,
    OLED_Input_RGsB,
    OLED_Input_VGA,
    OLED_Input_YPBPR,
    OLED_Input_SV,
    OLED_Input_AV,

    // Output Resolution submenu
    OLED_OutputResolution_1080,
    OLED_OutputResolution_1200,
    OLED_OutputResolution_1024,
    OLED_OutputResolution_960,
    OLED_OutputResolution_720,
    OLED_OutputResolution_480,

    // Screen Settings submenu
    OLED_ScreenSettings_Move,
    OLED_ScreenSettings_Scale,
    OLED_ScreenSettings_Borders,
    OLED_ScreenSettings_MoveAdjust,
    OLED_ScreenSettings_ScaleAdjust,
    OLED_ScreenSettings_BordersAdjust,

    // Color Settings submenu
    OLED_ColorSettings_ADCGain,
    OLED_ColorSettings_Scanlines,
    OLED_ColorSettings_LineFilter,
    OLED_ColorSettings_Sharpness,
    OLED_ColorSettings_Peaking,
    OLED_ColorSettings_StepResponse,
    OLED_ColorSettings_RGB_R,
    OLED_ColorSettings_RGB_G,
    OLED_ColorSettings_RGB_B,
    OLED_ColorSettings_Y_Gain,
    OLED_ColorSettings_Color,
    OLED_ColorSettings_DefaultColor,

    // System Settings submenu
    OLED_SystemSettings_MatchedPresets,
    OLED_ScreenSettings_FullHeight,
    OLED_SystemSettings_Force5060Hz,
    OLED_SystemSettings_ClockGenerator,
    OLED_SystemSettings_ADCCalibration,
    OLED_SystemSettings_FrameTimeLock,
    OLED_SystemSettings_HdmiLimitedRange,
    OLED_SystemSettings_LockMethod,
    OLED_SystemSettings_Deinterlace,
    OLED_SystemSettings_SyncStripper,

    // SV/AV Input Settings submenu
    OLED_SystemSettings_SVAVInputSettings,
    OLED_SystemSettings_SVAVInput_I2P,
    OLED_SystemSettings_SVAVInput_Smooth,
    OLED_SystemSettings_SVAVInput_ACESettings,
    OLED_SystemSettings_SVAVInput_Bright,
    OLED_SystemSettings_SVAVInput_Contrast,
    OLED_SystemSettings_SVAVInput_Saturation,
    OLED_SystemSettings_SVAVInput_Hue,
    OLED_SystemSettings_SVAVInput_Default,

    // ACE Settings submenu (inside SV/AV Settings)
    OLED_ACESettings_Enable,
    OLED_ACESettings_LumaGain,
    OLED_ACESettings_ChromaGain,
    OLED_ACESettings_ChromaMax,
    OLED_ACESettings_GammaGain,
    OLED_ACESettings_ResponseSpeed,
    OLED_ACESettings_Default,

    // Video Filters Settings submenu (inside SV/AV Settings)
    // Page 1: Y Filter, C Filter/Override, Bandwidth
    // Page 2: Luma Mode, Chroma Mode, Chroma Taps
    // Page 3: Default
    OLED_SystemSettings_SVAVInput_FiltersSettings,  // Link to Filters submenu
    OLED_VideoFiltersSettings_YFilter,      // Page 1 Row 1: Y Filter (AV: YSFM, SV: WYSFM)
    OLED_VideoFiltersSettings_CFilter,      // Page 1 Row 2 (AV only): C Filter
    OLED_VideoFiltersSettings_SVOverride,   // Page 1 Row 2 (SV only): Override (Auto/Manual)
    OLED_VideoFiltersSettings_Bandwidth,    // Page 1 Row 3: Bandwidth
    OLED_VideoFiltersSettings_LumaMode,     // Page 2 Row 1: Luma Mode (NTSC/PAL)
    OLED_VideoFiltersSettings_ChromaMode,   // Page 2 Row 2: Chroma Mode (NTSC/PAL)
    OLED_VideoFiltersSettings_ChromaTaps,   // Page 2 Row 3: Chroma Taps (NTSC/PAL)
    OLED_VideoFiltersSettings_Default,      // Page 3: Default

    // I2P Settings submenu (inside SV/AV Settings)
    OLED_SystemSettings_SVAVInput_I2PSettings,  // Link to I2P Settings submenu (Page 1, row 3)
    OLED_I2PSettings_Enable,                // Row 1: Enable I2P/2X
    OLED_I2PSettings_Smooth,                // Row 2: Smooth

    // Preferences menu (Page 2 row 3 of main menu)
    OLED_Preferences,
    OLED_Preferences_Theme,
    OLED_Preferences_Volume,
    OLED_Preferences_Mute,

    // Firmware Version menu (Page 3 row 1 of main menu)
    OLED_FirmwareVersion,
    OLED_FirmwareVersion_Info,  // Info screen (read-only)

    // Factory Reset confirmation screen
    OLED_FactoryReset_Confirm,

    // Special screens
    OLED_Mute_Display,
    OLED_Volume_Adjust,
    OLED_Info_Display,
    OLED_EnableOTA,
    OLED_Restart,
    OLED_ResetDefaults,

    // Developer submenu
    OLED_Developer,
    OLED_Developer_MemoryAdjust,
    OLED_Developer_HSyncAdjust,
    OLED_Developer_HTotalAdjust,
    OLED_Developer_DebugView,
    OLED_Developer_ADCFilter,
    OLED_Developer_FreezeCapture,
    OLED_Developer_ResyncHTotal,
    OLED_Developer_CycleSDRAM,
    OLED_Developer_PLLDivider,
    OLED_Developer_InvertSync,
    OLED_Developer_SyncWatcher,
    OLED_Developer_SyncProcessor,
    OLED_Developer_Oversampling,
    OLED_Developer_SnapFrameRate,
    OLED_Developer_IFAutoOffset,
    OLED_Developer_SOGLevel,
    OLED_Developer_ResetChip,

    // Profile/Slot Management
    // Row 1: Load profile slots (1-36 = A-Z, 0-9)
    OLED_Profile_Load1,
    OLED_Profile_Load2,
    OLED_Profile_Load3,
    OLED_Profile_Load4,
    OLED_Profile_Load5,
    OLED_Profile_Load6,
    OLED_Profile_Load7,
    OLED_Profile_Load8,
    OLED_Profile_Load9,
    OLED_Profile_Load10,
    OLED_Profile_Load11,
    OLED_Profile_Load12,
    OLED_Profile_Load13,
    OLED_Profile_Load14,
    OLED_Profile_Load15,
    OLED_Profile_Load16,
    OLED_Profile_Load17,
    OLED_Profile_Load18,
    OLED_Profile_Load19,
    OLED_Profile_Load20,
    OLED_Profile_Load21,
    OLED_Profile_Load22,
    OLED_Profile_Load23,
    OLED_Profile_Load24,
    OLED_Profile_Load25,
    OLED_Profile_Load26,
    OLED_Profile_Load27,
    OLED_Profile_Load28,
    OLED_Profile_Load29,
    OLED_Profile_Load30,
    OLED_Profile_Load31,
    OLED_Profile_Load32,
    OLED_Profile_Load33,
    OLED_Profile_Load34,
    OLED_Profile_Load35,
    OLED_Profile_Load36,
    // Row 2: Save profile slots (1-36 = A-Z, 0-9)
    OLED_Profile_Save1,
    OLED_Profile_Save2,
    OLED_Profile_Save3,
    OLED_Profile_Save4,
    OLED_Profile_Save5,
    OLED_Profile_Save6,
    OLED_Profile_Save7,
    OLED_Profile_Save8,
    OLED_Profile_Save9,
    OLED_Profile_Save10,
    OLED_Profile_Save11,
    OLED_Profile_Save12,
    OLED_Profile_Save13,
    OLED_Profile_Save14,
    OLED_Profile_Save15,
    OLED_Profile_Save16,
    OLED_Profile_Save17,
    OLED_Profile_Save18,
    OLED_Profile_Save19,
    OLED_Profile_Save20,
    OLED_Profile_Save21,
    OLED_Profile_Save22,
    OLED_Profile_Save23,
    OLED_Profile_Save24,
    OLED_Profile_Save25,
    OLED_Profile_Save26,
    OLED_Profile_Save27,
    OLED_Profile_Save28,
    OLED_Profile_Save29,
    OLED_Profile_Save30,
    OLED_Profile_Save31,
    OLED_Profile_Save32,
    OLED_Profile_Save33,
    OLED_Profile_Save34,
    OLED_Profile_Save35,
    OLED_Profile_Save36,
} OLED_MenuState;

// ====================================================================================
// MenuItemMapping Struct - OLED State to OSD Page/Row Mapping
// ====================================================================================

// Mapping from OLED menu item to OSD page/row (for Menu_navigateTo)
typedef struct {
    uint16_t item;            // OLED_MenuState value
    OsdCommand pageCmd;       // OSD page command to render
    uint8_t row;              // Row 1-3 within that page
} MenuItemMapping;

// ====================================================================================
// Menu Item Mappings X-Macros
// Format: MENU_ITEM(OLED_State, OSD_Command, Row)
// ====================================================================================

// Main Menu Mappings
#define MENU_ITEMS_MAIN \
    MENU_ITEM(OLED_Input,                          OSD_CMD_MAIN_PAGE1, 1) \
    MENU_ITEM(OLED_SystemSettings_SVAVInputSettings, OSD_CMD_MAIN_PAGE1, 2) \
    MENU_ITEM(OLED_OutputResolution,               OSD_CMD_MAIN_PAGE1, 3) \
    MENU_ITEM(OLED_ScreenSettings,                 OSD_CMD_MAIN_PAGE2, 1) \
    MENU_ITEM(OLED_ColorSettings,                  OSD_CMD_MAIN_PAGE2, 2) \
    MENU_ITEM(OLED_SystemSettings,                 OSD_CMD_MAIN_PAGE2, 3) \
    MENU_ITEM(OLED_Preferences,                    OSD_CMD_MAIN_PAGE3, 1) \
    MENU_ITEM(OLED_Developer,                      OSD_CMD_MAIN_PAGE3, 2) \
    MENU_ITEM(OLED_FirmwareVersion,                OSD_CMD_MAIN_PAGE3, 3) \
    MENU_ITEM(OLED_FactoryReset,                   OSD_CMD_MAIN_PAGE4, 1) \
    MENU_ITEM(OLED_Restart,                        OSD_CMD_MAIN_PAGE4, 2)

// Preferences Menu Mappings
#define MENU_ITEMS_PREFERENCES \
    MENU_ITEM(OLED_Preferences_Theme,  OSD_CMD_PREFERENCES_PAGE1, 1) \
    MENU_ITEM(OLED_Preferences_Volume, OSD_CMD_PREFERENCES_PAGE1, 2) \
    MENU_ITEM(OLED_Preferences_Mute,   OSD_CMD_PREFERENCES_PAGE1, 3)

// Input Menu Mappings
#define MENU_ITEMS_INPUT \
    MENU_ITEM(OLED_Input_RGBs,  OSD_CMD_INPUT_PAGE1, 1) \
    MENU_ITEM(OLED_Input_RGsB,  OSD_CMD_INPUT_PAGE1, 2) \
    MENU_ITEM(OLED_Input_VGA,   OSD_CMD_INPUT_PAGE1, 3) \
    MENU_ITEM(OLED_Input_YPBPR, OSD_CMD_INPUT_PAGE2, 1) \
    MENU_ITEM(OLED_Input_SV,    OSD_CMD_INPUT_PAGE2, 2) \
    MENU_ITEM(OLED_Input_AV,    OSD_CMD_INPUT_PAGE2, 3)

// Output Resolution Menu Mappings
#define MENU_ITEMS_OUTPUT \
    MENU_ITEM(OLED_OutputResolution_1080, OSD_CMD_OUTPUT_1080_1024_960, 1) \
    MENU_ITEM(OLED_OutputResolution_1024, OSD_CMD_OUTPUT_1080_1024_960, 2) \
    MENU_ITEM(OLED_OutputResolution_960,  OSD_CMD_OUTPUT_1080_1024_960, 3) \
    MENU_ITEM(OLED_OutputResolution_720,  OSD_CMD_OUTPUT_720_480, 1) \
    MENU_ITEM(OLED_OutputResolution_480,  OSD_CMD_OUTPUT_720_480, 2)

// Screen Settings Menu Mappings
#define MENU_ITEMS_SCREEN \
    MENU_ITEM(OLED_ScreenSettings_Move,       OSD_CMD_SCREEN_PAGE1, 1) \
    MENU_ITEM(OLED_ScreenSettings_Scale,      OSD_CMD_SCREEN_PAGE1, 2) \
    MENU_ITEM(OLED_ScreenSettings_Borders,    OSD_CMD_SCREEN_PAGE1, 3) \
    MENU_ITEM(OLED_ScreenSettings_FullHeight, OSD_CMD_SCREEN_PAGE2, 1)

// Color Settings Menu Mappings
#define MENU_ITEMS_COLOR \
    MENU_ITEM(OLED_ColorSettings_RGB_R,        OSD_CMD_COLOR_PAGE1, 1) \
    MENU_ITEM(OLED_ColorSettings_RGB_G,        OSD_CMD_COLOR_PAGE1, 2) \
    MENU_ITEM(OLED_ColorSettings_RGB_B,        OSD_CMD_COLOR_PAGE1, 3) \
    MENU_ITEM(OLED_ColorSettings_ADCGain,      OSD_CMD_COLOR_PAGE2, 1) \
    MENU_ITEM(OLED_ColorSettings_Scanlines,   OSD_CMD_COLOR_PAGE2, 2) \
    MENU_ITEM(OLED_ColorSettings_LineFilter,  OSD_CMD_COLOR_PAGE2, 3) \
    MENU_ITEM(OLED_ColorSettings_Sharpness,   OSD_CMD_COLOR_PAGE3, 1) \
    MENU_ITEM(OLED_ColorSettings_Peaking,     OSD_CMD_COLOR_PAGE3, 2) \
    MENU_ITEM(OLED_ColorSettings_StepResponse, OSD_CMD_COLOR_PAGE3, 3) \
    MENU_ITEM(OLED_ColorSettings_Y_Gain,      OSD_CMD_COLOR_PAGE4, 1) \
    MENU_ITEM(OLED_ColorSettings_Color,       OSD_CMD_COLOR_PAGE4, 2) \
    MENU_ITEM(OLED_ColorSettings_DefaultColor, OSD_CMD_COLOR_PAGE4, 3)

// System Settings Menu Mappings
#define MENU_ITEMS_SYSTEM \
    MENU_ITEM(OLED_SystemSettings_SyncStripper,      OSD_CMD_SYS_PAGE1, 1) \
    MENU_ITEM(OLED_SystemSettings_MatchedPresets,    OSD_CMD_SYS_PAGE1, 2) \
    MENU_ITEM(OLED_SystemSettings_Deinterlace,       OSD_CMD_SYS_PAGE1, 3) \
    MENU_ITEM(OLED_SystemSettings_Force5060Hz,       OSD_CMD_SYS_PAGE2, 1) \
    MENU_ITEM(OLED_SystemSettings_LockMethod,        OSD_CMD_SYS_PAGE2, 2) \
    MENU_ITEM(OLED_SystemSettings_ADCCalibration,    OSD_CMD_SYS_PAGE2, 3) \
    MENU_ITEM(OLED_SystemSettings_FrameTimeLock,     OSD_CMD_SYS_PAGE4, 1) \
    MENU_ITEM(OLED_SystemSettings_ClockGenerator,   OSD_CMD_SYS_PAGE4, 2) \
    MENU_ITEM(OLED_SystemSettings_HdmiLimitedRange,    OSD_CMD_SYS_PAGE4, 3)

// SV/AV Input Settings Menu Mappings
// Page 1: I2P Settings link, Video Filters link, ACE Settings link
// Page 2: Brightness, Contrast, Saturation
// Page 3: Hue, Default
#define MENU_ITEMS_SVAVINPUT \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_I2PSettings,      OSD_CMD_SVAVINPUT_PAGE1, 1) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_FiltersSettings,  OSD_CMD_SVAVINPUT_PAGE1, 2) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_ACESettings,      OSD_CMD_SVAVINPUT_PAGE1, 3) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_Bright,           OSD_CMD_SVAVINPUT_PAGE2, 1) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_Contrast,         OSD_CMD_SVAVINPUT_PAGE2, 2) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_Saturation,       OSD_CMD_SVAVINPUT_PAGE2, 3) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_Hue,              OSD_CMD_SVAVINPUT_PAGE3, 1) \
    MENU_ITEM(OLED_SystemSettings_SVAVInput_Default,          OSD_CMD_SVAVINPUT_PAGE3, 2)

// ACE Settings Menu Mappings (submenu inside SV/AV Settings)
#define MENU_ITEMS_ACE \
    MENU_ITEM(OLED_ACESettings_Enable,        OSD_CMD_ACE_PAGE1, 1) \
    MENU_ITEM(OLED_ACESettings_LumaGain,      OSD_CMD_ACE_PAGE1, 2) \
    MENU_ITEM(OLED_ACESettings_ChromaGain,    OSD_CMD_ACE_PAGE1, 3) \
    MENU_ITEM(OLED_ACESettings_ChromaMax,     OSD_CMD_ACE_PAGE2, 1) \
    MENU_ITEM(OLED_ACESettings_GammaGain,     OSD_CMD_ACE_PAGE2, 2) \
    MENU_ITEM(OLED_ACESettings_ResponseSpeed, OSD_CMD_ACE_PAGE2, 3) \
    MENU_ITEM(OLED_ACESettings_Default,       OSD_CMD_ACE_PAGE3, 1)

// Video Filters Settings Menu Mappings (submenu inside SV/AV Settings)
// Page 1: Y Filter, C Filter/Override, Bandwidth
// Page 2: Luma Mode, Chroma Mode, Chroma Taps
// Page 3: Default
#define MENU_ITEMS_VIDEOFILTERS \
    MENU_ITEM(OLED_VideoFiltersSettings_YFilter,      OSD_CMD_VIDEOFILTERS_PAGE1, 1) \
    MENU_ITEM(OLED_VideoFiltersSettings_CFilter,      OSD_CMD_VIDEOFILTERS_PAGE1, 2) \
    MENU_ITEM(OLED_VideoFiltersSettings_SVOverride,   OSD_CMD_VIDEOFILTERS_PAGE1, 2) \
    MENU_ITEM(OLED_VideoFiltersSettings_Bandwidth,    OSD_CMD_VIDEOFILTERS_PAGE1, 3) \
    MENU_ITEM(OLED_VideoFiltersSettings_LumaMode,     OSD_CMD_VIDEOFILTERS_PAGE2, 1) \
    MENU_ITEM(OLED_VideoFiltersSettings_ChromaMode,   OSD_CMD_VIDEOFILTERS_PAGE2, 2) \
    MENU_ITEM(OLED_VideoFiltersSettings_ChromaTaps,   OSD_CMD_VIDEOFILTERS_PAGE2, 3) \
    MENU_ITEM(OLED_VideoFiltersSettings_Default,      OSD_CMD_VIDEOFILTERS_PAGE3, 1)

// I2P Settings Menu Mappings (submenu inside SV/AV Settings)
#define MENU_ITEMS_I2P \
    MENU_ITEM(OLED_I2PSettings_Enable,  OSD_CMD_I2P_PAGE1, 1) \
    MENU_ITEM(OLED_I2PSettings_Smooth,  OSD_CMD_I2P_PAGE1, 2)

// Developer Menu Mappings (6 pages, 3 items each = 17 items total)
#define MENU_ITEMS_DEVELOPER \
    MENU_ITEM(OLED_Developer_MemoryAdjust,  OSD_CMD_DEV_PAGE1, 1) \
    MENU_ITEM(OLED_Developer_HSyncAdjust,   OSD_CMD_DEV_PAGE1, 2) \
    MENU_ITEM(OLED_Developer_HTotalAdjust,  OSD_CMD_DEV_PAGE1, 3) \
    MENU_ITEM(OLED_Developer_DebugView,     OSD_CMD_DEV_PAGE2, 1) \
    MENU_ITEM(OLED_Developer_ADCFilter,     OSD_CMD_DEV_PAGE2, 2) \
    MENU_ITEM(OLED_Developer_FreezeCapture, OSD_CMD_DEV_PAGE2, 3) \
    MENU_ITEM(OLED_Developer_ResyncHTotal,  OSD_CMD_DEV_PAGE3, 1) \
    MENU_ITEM(OLED_Developer_CycleSDRAM,    OSD_CMD_DEV_PAGE3, 2) \
    MENU_ITEM(OLED_Developer_PLLDivider,    OSD_CMD_DEV_PAGE3, 3) \
    MENU_ITEM(OLED_Developer_InvertSync,    OSD_CMD_DEV_PAGE4, 1) \
    MENU_ITEM(OLED_Developer_SyncWatcher,   OSD_CMD_DEV_PAGE4, 2) \
    MENU_ITEM(OLED_Developer_SyncProcessor, OSD_CMD_DEV_PAGE4, 3) \
    MENU_ITEM(OLED_Developer_Oversampling,  OSD_CMD_DEV_PAGE5, 1) \
    MENU_ITEM(OLED_Developer_SnapFrameRate, OSD_CMD_DEV_PAGE5, 2) \
    MENU_ITEM(OLED_Developer_IFAutoOffset,  OSD_CMD_DEV_PAGE5, 3) \
    MENU_ITEM(OLED_Developer_SOGLevel,      OSD_CMD_DEV_PAGE6, 1) \
    MENU_ITEM(OLED_Developer_ResetChip,     OSD_CMD_DEV_PAGE6, 2)

// All Mapped Menu Items (for generating oledToOsdMap[])
#define ALL_MAPPED_MENU_ITEMS \
    MENU_ITEMS_MAIN \
    MENU_ITEMS_INPUT \
    MENU_ITEMS_OUTPUT \
    MENU_ITEMS_SCREEN \
    MENU_ITEMS_COLOR \
    MENU_ITEMS_SYSTEM \
    MENU_ITEMS_SVAVINPUT \
    MENU_ITEMS_ACE \
    MENU_ITEMS_VIDEOFILTERS \
    MENU_ITEMS_I2P \
    MENU_ITEMS_PREFERENCES \
    MENU_ITEMS_DEVELOPER

// ====================================================================================
// IR Menu Handler Function Declarations
// ====================================================================================

bool IR_handleMainMenu();
bool IR_handleInputSelection();
bool IR_handleOutputResolution();
bool IR_handleScreenSettings();
bool IR_handleColorSettings();
bool IR_handleSystemSettings();
bool IR_handleADVSettings();
bool IR_handleACESettings();
bool IR_handleVideoFiltersSettings();
bool IR_handleI2PSettings();
bool IR_handlePreferencesMenu();
bool IR_handleProfileManagement();
bool IR_handleMuteDisplay();
bool IR_handleMiscSettings();
bool IR_handleInfoDisplay();
bool IR_handleDeveloperMenu();

// Volume repeat support (called from IR_handleInput when volume menu opens)
void Volume_setInitialKey(uint32_t key);

// ====================================================================================
// IR Remote Main Functions
// ====================================================================================

void IR_handleMenuSelection(void);
void IR_handleInput(void);

// ====================================================================================
// OLED Menu Initialization Functions (for OLEDMenuImplementation)
// ====================================================================================

// Forward declarations
class OLEDMenuManager;
struct OLEDMenuItem;
enum class OLEDMenuNav;

void OLED_initInputMenu(OLEDMenuItem *root);
void OLED_initSettingsMenu(OLEDMenuItem *root);

// ====================================================================================
// OLED Menu Handler Functions (for OLEDMenuImplementation)
// ====================================================================================

bool OLED_handleInputSelection(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool OLED_handleSettingSelection(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool OLED_handleTvModeSelection(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);

// ====================================================================================
// Menu Navigation Functions
// ====================================================================================

// Navigate to menu item (handles OSD page/row automatically)
void Menu_navigateTo(OLED_MenuState newItem);

// ====================================================================================
// OLED to OSD Mapping Table (generated from X-macro in menu-core.cpp)
// ====================================================================================

extern const MenuItemMapping oledToOsdMap[];
extern const size_t oledToOsdMapSize;

