// define menu items and handlers here
#ifndef OLED_MENU_IMPLEMENTATION_H_
#define OLED_MENU_IMPLEMENTATION_H_
#include "OLEDMenuManager.h"


#define RGB1 0x01
#define YUV0 0x00


#define COMPATIBILITY_OFF 0
#define COMPATIBILITY_ON  1

#define Auto_1 0x04
#define Auto_2 0x14
#define Auto_3 0x24
#define Auto_4 0x34

#define Ntsc_J 0x44
#define Ntsc_M 0x54
#define Pal_60 0x64
#define Ntsc_443 0x74
#define Pal 0x84
#define Pal_N 0x94
#define Pal_Mwp 0xa4
#define Pal_M 0xb4
#define Pal_CN 0xc4
#define Pal_CNw 0xd4
#define SECAM 0xe4


    // 模式映射表
const uint8_t modes[12] = {
        0x04,  // 0: Auto
        0x84,  // 1: Pal
        0x54,  // 2: Ntsc_M
        0x64,  // 3: Pal_60
        0x74,  // 4: Ntsc443
        0x44,  // 5: Ntsc_J
        0x94,  // 6: Pal_N_wp
        0xA4,  // 7: Pal_M_wop
        0xB4,  // 8: Pal_M
        0xC4,  // 9: Pal_Cmb_N
        0xD4,  // 10: Pal_Cmb_N_wp
        0xE4   // 11: Secam
    };
// #define Auto          0x04
// #define Pal           0x84
// #define Ntsc_M        0x54
// #define Pal_60        0x64
// #define Ntsc443       0x74
// #define Ntsc_J        0x44
// #define Pal_N_wp      0x94
// #define Pal_M_wop     0xA4
// #define Pal_M         0xB4
// #define Pal_Cmb_N     0xC4
// #define Pal_Cmb_N_wp  0xD4
// #define Secam         0xE4



#define S_RGBs 1
#define S_VGA  2
#define S_YUV  3

#define InfoRGBs 1
#define InfoRGsB 2
#define InfoVGA 3
#define InfoYUV 4
#define InfoSV 5
#define InfoAV 6

enum MenuItemTag : uint16_t
{
    // unique identifiers for sub-items
    MT_NULL, // null tag, used by root menu items, since they can be differentiated by handlers
    MT_1280x960,
    MT1280x1024,
    MT1280x720,
    MT1920x1080,
    MT_480s576,
    MT_DOWNSCALE,
    MT_BYPASS,
    MT_RESET_GBS,
    MT_RESTORE_FACTORY,
    MT_RESET_WIFI,
};

// declarations of resolutionMenuHandler, presetSelectionMenuHandler, presetsCreationMenuHandler, resetMenuHandler, currentSettingHandler, wifiMenuHandler
bool resolutionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool presetSelectionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool presetsCreationMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool resetMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool currentSettingHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool OSDHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
void initOLEDMenu();

bool InputSwHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav);

void Checksum_Send(const unsigned char *buff);
void Checksum_Sendmode(const unsigned char *buff, uint8_t mode);
void InputRGBs(void);
void InputRGsB(void);
void InputVGA(void);
void InputYUV(void);
void InputSV(void);
void InputAV(void);
void Send_TvMode(uint8_t Mode);
void Send_Line(bool line);
void Send_Smooth(bool Smooth);
void Send_Compatibility(bool Com);
void InputINFO(void);

void InputSV_mode(uint8_t mode);
void InputAV_mode(uint8_t mode);

void InputRGBs_mode(uint8_t mode);
void InputRGsB_mode(uint8_t mode);
void InputVGA_mode(uint8_t mode);
void InputNULL(void);


void SetReg(unsigned char reg, unsigned char val);
void Signalized(void);
#endif