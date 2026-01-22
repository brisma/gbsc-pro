// ====================================================================================
// menu-helpers.cpp
// Menu Helper Functions for OLED Display and IR Remote
//
// This file contains utility functions used by menu handlers:
// - OLED display helpers (showMenu*, oledPrepare, exitMenu)
// - IR receiver helpers (irDecode, irResume)
// - OLED menu feedback helpers
// - Profile navigation helpers
// - IR key validation and resolution helpers
// ====================================================================================

#include "../menu-core.h"
#include "../../../OLEDMenuManager.h"
#include "../../../OLEDMenuImplementation.h"

#include <SSD1306Wire.h>

#include "../../drivers/ir_remote.h"

// ====================================================================================
// External References
// ====================================================================================

extern SSD1306Wire display;
extern OLEDMenuManager oledMenu;
extern unsigned long oledMenuFreezeStartTime;
extern unsigned long oledMenuFreezeTimeoutInMS;

// ====================================================================================
// OLED Display Helpers
// ====================================================================================

void oledPrepare() {
    if (oledClearFlag) display.clear();
    oledClearFlag = ~0;
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
}

void showMenu(const char* title, const char* label) {
    oledPrepare();
    display.drawString(1, 0, title);
    display.drawString(1, 28, label);
    display.display();
}

void showMenuCentered(const char* text, uint8_t x, uint8_t y) {
    oledPrepare();
    display.drawString(x, y, text);
    display.display();
}

void showMenuToggle(const char* title, const char* label, bool isOn) {
    oledPrepare();
    display.drawString(1, 0, title);
    display.drawString(1, 22, label);
    display.drawString(1, 44, isOn ? "ON" : "OFF");
    display.display();
}

void showMenuValue(const char* title, const char* label, const char* value) {
    oledPrepare();
    display.drawString(1, 0, title);
    display.drawString(1, 22, label);
    display.drawString(1, 44, value);
    display.display();
}

void exitMenu() {
    oled_menuItem = OLED_None;
    OSD_displayOff();   // Turn off OSD before clearing (avoids glitch)
    OSD_clearAll();
    OSD_init();
}

// ====================================================================================
// IR Receiver Helpers
// ====================================================================================

bool irDecode() {
    if (irrecv.decode(&results)) {
        irDecodedFlag = 1;
        return true;
    }
    return false;
}

void irResume() {
    irrecv.resume();
}

// ====================================================================================
// OLED Menu Feedback Helpers
// ====================================================================================

void showSelectionFeedback(OLEDMenuManager *manager, const char* label) {
    oledMenuFreezeTimeoutInMS = 1000;
    oledMenuFreezeStartTime = millis();
    OLEDDisplay *oledDisplay = manager->getDisplay();
    oledDisplay->clear();
    oledDisplay->setColor(OLEDDISPLAY_COLOR::WHITE);
    oledDisplay->setFont(ArialMT_Plain_16);
    oledDisplay->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    oledDisplay->drawString(OLED_MENU_WIDTH / 2, 16, label);
    oledDisplay->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    oledDisplay->display();
    manager->freeze();
}

bool checkFreezeTimeout(OLEDMenuManager *manager) {
    if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
        manager->unfreeze();
        return false;
    }
    return true;
}

// ====================================================================================
// Profile Navigation Helpers
// ====================================================================================

// Get slot index (0-35) from menu state, or -1 if not a valid slot
static int getSlotIndex(int state, OLED_MenuState base) {
    int idx = state - base;
    return (idx >= 0 && idx < 36) ? idx : -1;
}

int getLoadSlotIndex(int state) {
    return getSlotIndex(state, OLED_Profile_Load1);
}

int getSaveSlotIndex(int state) {
    return getSlotIndex(state, OLED_Profile_Save1);
}

// Navigate to slot with offset (wraps around 0-35)
static OLED_MenuState getSlotWithOffset(OLED_MenuState base, int idx, int offset) {
    return (OLED_MenuState)(base + ((idx + offset + 36) % 36));
}

OLED_MenuState getNextSlot(OLED_MenuState base, int idx) {
    return getSlotWithOffset(base, idx, 1);
}

OLED_MenuState getPrevSlot(OLED_MenuState base, int idx) {
    return getSlotWithOffset(base, idx, -1);
}

char getSlotChar(int idx) {
    extern String slotIndexMap;
    return slotIndexMap[idx];
}

bool handleProfileRow(bool isLoadRow) {
    extern struct userOptions *uopt;
    extern struct runTimeOptions *rto;
    extern char userCommand;
    extern void saveUserPrefs();
    extern void applyPresets(uint8_t videoMode);
    extern bool loadSlotSettings();
    extern bool saveSlotSettingsAt(int slotIndex, const char* name);

    int idx = isLoadRow ? getLoadSlotIndex(oled_menuItem) : getSaveSlotIndex(oled_menuItem);
    if (idx < 0) return false;

    OLED_MenuState base = isLoadRow ? OLED_Profile_Load1 : OLED_Profile_Save1;

    // Special case: first Load slot shows menu header
    if (isLoadRow && idx == 0) {
        showMenu("Menu-", "Profile");
    }

    // Visual feedback for up/down navigation
    if (results.value == IR_KEY_UP || results.value == IR_KEY_DOWN) {
        OSD_writeCharAtRow(1, 0, arrow_right_icon, isLoadRow ? OSD_CURSOR_ACTIVE : OSD_BACKGROUND);
        OSD_writeCharAtRow(2, 0, arrow_right_icon, isLoadRow ? OSD_BACKGROUND : OSD_CURSOR_ACTIVE);
        OSD_writeCharAtRow(3, 0, arrow_right_icon, OSD_BACKGROUND);
        if (isLoadRow) OSD_handleCommand(OSD_CMD_PROFILE_SAVELOAD);
    }

    OSD_handleCommand(OSD_CMD_PROFILE_SLOTDISPLAY);

    if (irDecode()) {
        switch (results.value) {
            case IR_KEY_MENU:
                exitMenu();
                break;
            case IR_KEY_UP:
                if (!isLoadRow) {
                    oled_menuItem = (OLED_MenuState)(OLED_Profile_Load1 + getLoadSlotIdx());
                    OSD_handleCommand(OSD_CMD_PAGE_CHANGE_ROW1);
                    OSD_handleCommand(OSD_CMD_PROFILE_SAVELOAD);
                }
                break;
            case IR_KEY_DOWN:
                if (isLoadRow) {
                    oled_menuItem = (OLED_MenuState)(OLED_Profile_Save1 + getSaveSlotIdx());
                    OSD_handleCommand(OSD_CMD_PAGE_CHANGE_ROW2);
                    OSD_handleCommand(OSD_CMD_PROFILE_SAVELOAD);
                }
                break;
            case IR_KEY_RIGHT:
                oled_menuItem = getNextSlot(base, idx);
                // Update slot name for the new index
                if (isLoadRow) updateLoadSlotName((idx + 1) % 36);
                else updateSaveSlotName((idx + 1) % 36);
                break;
            case IR_KEY_LEFT:
                oled_menuItem = getPrevSlot(base, idx);
                // Update slot name for the new index
                if (isLoadRow) updateLoadSlotName((idx + 35) % 36);
                else updateSaveSlotName((idx + 35) % 36);
                break;
            case IR_KEY_OK:
                uopt->presetSlot = getSlotChar(idx);
                uopt->presetPreference = OutputCustomized;
                if (isLoadRow) {
                    loadSlotSettings();
                    applyPresets(rto->videoStandardInput);
                    saveUserPrefs();
                    OSD_showOkFeedback(ROW_1);
                } else {
                    saveUserPrefs();
                    userCommand = '4';  // Save GBS preset
                    saveSlotSettingsAt(idx, NULL); // Save ADV settings to slots.bin
                    OSD_showOkFeedback(ROW_2);
                }
                break;
            case IR_KEY_EXIT:
                exitMenu();
                break;
        }
        irResume();
    }
    return true;
}

// ====================================================================================
// IR Key Validation Helpers
// ====================================================================================

// Check if the IR key is a valid menu navigation key
bool IR_isValidMenuKey(uint32_t key)
{
    switch (key) {
        case IR_KEY_MENU:
        case IR_KEY_SAVE:
        case IR_KEY_INFO:
        case IR_KEY_RIGHT:
        case IR_KEY_LEFT:
        case IR_KEY_UP:
        case IR_KEY_DOWN:
        case IR_KEY_OK:
        case IR_KEY_EXIT:
        case IR_KEY_MUTE:
        case IR_KEY_VOL_UP:
        case IR_KEY_VOL_DN:
            return true;
        default:
            return false;
    }
}

// Get the user command for a given resolution
char IR_getResolutionCommand(uint8_t resolution)
{
    switch (resolution) {
        case Output960P:  return 'f';  // 1280x960
        case Output720P:  return 'g';  // 1280x720
        case Output480P:  return 'h';  // 480p/576p
        case Output1024P: return 'p';  // 1280x1024
        case Output1080P: return 's';  // 1920x1080
        case Output1200P: return 'L';  // 1920x1200
        default:          return 'g';  // Default to 720p
    }
}

// ====================================================================================
// IR Key Repeat Helper
// ====================================================================================

// Shared global state for IR key repeat (only one menu active at a time)
static uint32_t irRepeatLastKey = 0;
static unsigned long irRepeatLastTime = 0;

// Clear repeat state (call on menu navigation or exit)
void IR_clearRepeatKey(void) {
    irRepeatLastKey = 0;
}

// Process IR input with key repeat support using shared global state
uint32_t IR_getKeyRepeat(void) {
    uint32_t key = results.value;
    bool isRepeat = results.repeat || key == 0xFFFFFFFF;

    if (isRepeat) {
        if (irRepeatLastKey == 0) {
            return 0;  // No previous key stored, ignore
        }
        // Throttle repeat rate
        unsigned long now = millis();
        if (now - irRepeatLastTime < IR_REPEAT_INTERVAL_MS) {
            return 0;  // Too soon, skip this repeat
        }
        irRepeatLastTime = now;
        return irRepeatLastKey;
    }

    // Normal key press - store it and return
    irRepeatLastKey = key;
    irRepeatLastTime = millis();
    return key;
}
