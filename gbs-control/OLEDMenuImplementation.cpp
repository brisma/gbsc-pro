#define OSD_TIMEOUT 8000

#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "pro/options-pro.h"  // GBSC-PRO extensions
#include "pro/menu/menu-presets.h"  // Virtual preset menu
#include "pro/gbs-control-pro.h" // Core functions, global variables
#include "OLEDMenuImplementation.h"
#include "options.h"
#include "tv5725.h"
#include "slot.h"
#include "src/WebSockets.h"
#include "src/WebSocketsServer.h"
#include "fonts.h"
#include "OSDManager.h"

typedef TV5725<GBS_ADDR> GBS;
extern void applyPresets(uint8_t videoMode);
extern bool loadSlotSettings();
extern void setOutModeHdBypass(bool bypass);
extern void saveUserPrefs();
extern float getOutputFrameRate();
extern void loadDefaultUserOptions();
extern uint8_t getVideoMode();
extern runTimeOptions *rto;
extern userOptions *uopt;
extern const char *ap_ssid;
extern const char *ap_password;
extern const char *device_hostname_full;
extern WebSocketsServer webSocket;
extern OLEDMenuManager oledMenu;
extern OSDManager osdManager;
unsigned long oledMenuFreezeStartTime;
unsigned long oledMenuFreezeTimeoutInMS;

bool resolutionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    oledMenuFreezeTimeoutInMS = 1000; // freeze for 1s
    oledMenuFreezeStartTime = millis();
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    uint8_t videoMode = getVideoMode();
    PresetPreference preset = PresetPreference::Output1080P;
    switch (item->tag) {
        case MT_1280x960:
            preset = PresetPreference::Output960P;
            break;
        case MT1280x1024:
            preset = PresetPreference::Output1024P;
            break;
        case MT1280x720:
            preset = PresetPreference::Output720P;
            break;
        case MT1920x1080:
            preset = PresetPreference::Output1080P;
            break;
        case MT1920x1200:
            preset = PresetPreference::Output1200P;
            break;
        case MT_480s576:
            preset = PresetPreference::Output480P;
            break;
        case MT_DOWNSCALE:
            preset = PresetPreference::OutputDownscale;
            break;
        case MT_BYPASS:
            preset = PresetPreference::OutputCustomized;
            break;
        default:
            break;
    }
    if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read()) {
        videoMode = rto->videoStandardInput;
    }
    if (item->tag != MT_BYPASS) {
        uopt->presetPreference = preset;
        rto->useHdmiSyncFix = 1;
        if (rto->videoStandardInput == 14) {
            rto->videoStandardInput = 15;
        } else {
            applyPresets(videoMode);
        }
    } else {
        setOutModeHdBypass(false);
        uopt->presetPreference = preset;
        if (rto->videoStandardInput != 15) {
            rto->autoBestHtotalEnabled = 0;
            if (rto->applyPresetDoneStage == 11) {
                rto->applyPresetDoneStage = 1;
            } else {
                rto->applyPresetDoneStage = 10;
            }
        } else {
            rto->applyPresetDoneStage = 1;
        }
    }
    saveUserPrefs();
    manager->freeze();
    return false;
}
bool presetSelectionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    // Convert tag to slot character: 0-25 → A-Z, 26-35 → 0-9
    if (item->tag < 26) {
        uopt->presetSlot = 'A' + item->tag;
    } else {
        uopt->presetSlot = '0' + (item->tag - 26);
    }
    uopt->presetPreference = PresetPreference::OutputCustomized;

    // Load slot settings and apply preset (doPostPresetLoadSteps applies ADV/GBS colors)
    loadSlotSettings();
    applyPresets(rto->videoStandardInput);
    saveUserPrefs();
    manager->freeze();
    oledMenuFreezeTimeoutInMS = 2000;
    oledMenuFreezeStartTime = millis();

    return false;
}
bool presetsCreationMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    manager->clearSubItems(item);
    File slotsBinaryFileRead = LittleFS.open(SLOTS_FILE, "r");
    int curNumSlot = 0;
    if (slotsBinaryFileRead) {
        SlotMeta slot;  // Read one slot at a time (47 bytes vs 940+ for full array)
        for (int i = 0; i < SLOTS_TOTAL; ++i) {
            slotsBinaryFileRead.seek(i * sizeof(SlotMeta));
            slotsBinaryFileRead.read((byte *)&slot, sizeof(SlotMeta));
            if (strcmp(EMPTY_SLOT_NAME, slot.name) == 0 || !strlen(slot.name)) {
                continue;
            }
            curNumSlot++;
            if (curNumSlot > OLED_MENU_MAX_SUBITEMS_NUM) {
                break;
            }
            manager->registerItem(item, slot.slot, slot.name, presetSelectionMenuHandler);
        }
        slotsBinaryFileRead.close();
    }

    if (curNumSlot > OLED_MENU_MAX_SUBITEMS_NUM) {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_TOO_MANY_PRESETS));
    }

    if (!item->numSubItem) {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_NO_PRESETS));
    }
    return true;
}
bool resetMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        // not precise
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            ESP.reset();
            return false;
        }
        return false;
    }

    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    switch (item->tag) {
        case MT_RESET_GBS:
            display->drawXbm(CENTER_IMAGE(TEXT_RESETTING_GBS));
            break;
        case MT_RESTORE_FACTORY:
            display->drawXbm(CENTER_IMAGE(TEXT_RESTORING));
            break;
        case MT_RESET_WIFI:
            display->drawXbm(CENTER_IMAGE(TEXT_RESETTING_WIFI));
            break;
    }
    display->display();
    // Drop all websocket clients but keep the server running
    webSocket.disconnect();
    delay(50);
    switch (item->tag) {
        case MT_RESET_WIFI:
            WiFi.persistent(true);
            WiFi.disconnect();
            WiFi.persistent(false);
            break;
        case MT_RESTORE_FACTORY:
            loadDefaultUserOptions();
            saveUserPrefs();
            break;
    }
    manager->freeze();
    oledMenuFreezeStartTime = millis();
    oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds
    return false;
}
bool currentSettingHandler(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long lastUpdateTime = 0;
    if (isFirstTime) {
        lastUpdateTime = 0;
        oledMenuFreezeStartTime = millis();
        oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds if no input
        manager->freeze();
    } else if (nav != OLEDMenuNav::IDLE) {
        manager->unfreeze();
        return false;
    }
    if (millis() - lastUpdateTime <= 200) {
        return false;
    }
    OLEDDisplay &display = *manager->getDisplay();
    display.clear();
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    display.setFont(ArialMT_Plain_16);
    if (rto->sourceDisconnected || !rto->boardHasPower) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            return false;
        }
        display.setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display.drawXbm(CENTER_IMAGE(TEXT_NO_INPUT));
    } else {
        // TODO translations
        boolean vsyncActive = 0;
        boolean hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        rto->presetID = GBS::GBS_PRESET_ID::read();

        display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        if (rto->presetID == 0x01 || rto->presetID == 0x11) {
            display.drawString(0, 0, "1280x960");
        } else if (rto->presetID == 0x02 || rto->presetID == 0x12) {
            display.drawString(0, 0, "1280x1024");
        } else if (rto->presetID == 0x03 || rto->presetID == 0x13) {
            display.drawString(0, 0, "1280x720");
        } else if (rto->presetID == 0x05 || rto->presetID == 0x15) {
            display.drawString(0, 0, "1920x1080");
        } else if (rto->presetID == 0x07 || rto->presetID == 0x17) {
            display.drawString(0, 0, "1920x1200");
        } else if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            display.drawString(0, 0, "Downscale");
        } else if (rto->presetID == 0x04) {
            display.drawString(0, 0, "720x480");
        } else if (rto->presetID == 0x14) {
            display.drawString(0, 0, "768x576");
        } else {
            display.drawString(0, 0, "bypass");
        }

        display.drawString(0, 20, String(ofr, 5) + "Hz");

        if (currentInput == 1) {
            display.drawString(0, 41, "RGB");
        } else {
            display.drawString(0, 41, "YpBpR");
        }

        if (currentInput == 1) {
            vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
            if (vsyncActive) {
                display.drawString(70, 41, "V");
                hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                if (hsyncActive) {
                    display.drawString(53, 41, "H");
                }
            }
        }
    }
    display.display();
    lastUpdateTime = millis();

    return false;
}
bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    static char ssid[64];
    static char ip[25];
    static char domain[25];
    WiFiMode_t wifiMode = WiFi.getMode();
    manager->clearSubItems(item);
    if (wifiMode == WIFI_STA) {
        sprintf(ssid, "SSID: %s", WiFi.SSID().c_str());
        manager->registerItem(item, 0, ssid);
        if (WiFi.isConnected()) {
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_CONNECTED));
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_URL));
            sprintf(ip, "http://%s", WiFi.localIP().toString().c_str());
            manager->registerItem(item, 0, ip);
            sprintf(domain, "http://%s", device_hostname_full);
            manager->registerItem(item, 0, domain);
        } else {
            // shouldn't happen?
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_DISCONNECTED));
        }
    } else if (wifiMode == WIFI_AP) {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_CONNECT_TO));
        sprintf(ssid, "SSID: %s (%s)", ap_ssid, ap_password);
        manager->registerItem(item, 0, ssid);
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_URL));
        manager->registerItem(item, 0, "http://192.168.4.1");
        sprintf(domain, "http://%s", device_hostname_full);
        manager->registerItem(item, 0, domain);
    } else {
        // shouldn't happen?
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_DISCONNECTED));
    }
    return true;
}
bool osdMenuHanlder(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long start;
    static long left;
    char buf[30];
    auto display = manager->getDisplay();

    if (isFirstTime) {
        left = OSD_TIMEOUT;
        start = millis();
        manager->freeze();
        osdManager.tick(OSDNav::ENTER);
    } else {
        display->clear();
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display->drawStringf(OLED_MENU_WIDTH / 2, 16, buf, "OSD (%ds)", left / 1000 + 1);
        display->display();
        if (REVERSE_ROTARY_ENCODER_FOR_OLED_MENU){
            // reverse nav back to normal
            if(nav == OLEDMenuNav::DOWN) {
                nav = OLEDMenuNav::UP;
            } else if(nav == OLEDMenuNav::UP) {
                nav = OLEDMenuNav::DOWN;
            }
        }
        switch (nav) {
            case OLEDMenuNav::ENTER:
                osdManager.tick(OSDNav::ENTER);
                start = millis();
                break;
            case OLEDMenuNav::DOWN:
                if(REVERSE_ROTARY_ENCODER_FOR_OSD) {
                    osdManager.tick(OSDNav::RIGHT);
                } else {
                    osdManager.tick(OSDNav::LEFT);
                }
                start = millis();
                break;
            case OLEDMenuNav::UP:
                if(REVERSE_ROTARY_ENCODER_FOR_OSD) {
                    osdManager.tick(OSDNav::LEFT);
                } else {
                    osdManager.tick(OSDNav::RIGHT);
                }
                start = millis();
                break;
            default:
                break;
        }
        left = OSD_TIMEOUT - (millis() - start);
        if (left <= 0) {
            manager->unfreeze();
            osdManager.menuOff();
        }
    }
    return true;
}

// Firmware version display handler
bool firmwareVersionHandler(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    if (isFirstTime) {
        oledMenuFreezeStartTime = millis();
        oledMenuFreezeTimeoutInMS = 8000; // auto-exit after 8 seconds
        manager->freeze();
    } else if (nav != OLEDMenuNav::IDLE) {
        // Any input exits back to menu
        manager->unfreeze();
        return false;
    }

    // Check timeout
    if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
        manager->unfreeze();
        return false;
    }

    OLEDDisplay &display = *manager->getDisplay();
    display.clear();
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    display.setTextAlignment(TEXT_ALIGN_CENTER);

    // Title (medium font, centered)
    display.setFont(URW_Gothic_L_Book_14);
    display.drawString(64, 0, "Firmware version");

    // Version info (smaller font, centered, tighter spacing)
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 20, "GBS Control: " GBS_FW_VERSION);
    display.drawString(64, 32, "ADV Control: " ADV_FW_VERSION);

    // Hint to exit
    display.drawString(64, 52, "Press to exit");

    display.display();
    return true;
}

void initOLEDMenu()
{
    OLEDMenuItem *root = oledMenu.rootItem;
    OLED_initInputMenu(root);

    // OSD Menu
    // oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_OSD), osdMenuHanlder);

    // Resolutions
    OLEDMenuItem *resMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESOLUTION));
    const char *resolutions[6] = {"1280x960", "1280x1024", "1280x720", "1920x1080", "1920x1200","480/576"};
    uint8_t tags[6] = {MT_1280x960, MT1280x1024, MT1280x720, MT1920x1080, MT1920x1200, MT_480s576};
    for (int i = 0; i < 6; ++i) {
        oledMenu.registerItem(resMenu, tags[i], resolutions[i], resolutionMenuHandler);
    }
    // downscale and passthrough
    // oledMenu.registerItem(resMenu, MT_DOWNSCALE, IMAGE_ITEM(OM_DOWNSCALE), resolutionMenuHandler);
    // oledMenu.registerItem(resMenu, MT_BYPASS, IMAGE_ITEM(OM_PASSTHROUGH), resolutionMenuHandler);
    OLED_initSettingsMenu(root);

    // Presets (virtual menu)
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_PRESET), presetsVirtualMenuHandler);

    // WiFi
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_WIFI), wifiMenuHandler);

    // Current Settings
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_CURRENT), currentSettingHandler);

    // Reset (Misc.)
    OLEDMenuItem *resetMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESET_RESTORE));
    oledMenu.registerItem(resetMenu, MT_RESET_GBS, IMAGE_ITEM(OM_RESET_GBS), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESTORE_FACTORY, IMAGE_ITEM(OM_RESTORE_FACTORY), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESET_WIFI, IMAGE_ITEM(OM_RESET_WIFI), resetMenuHandler);

    // Firmware version
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_FIRMWARE_VERSION), firmwareVersionHandler);
}
