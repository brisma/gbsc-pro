#define OSD_TIMEOUT 8000
#include <ESP8266WiFi.h>
#include "OLEDMenuImplementation.h"
#include "options.h"
#include "tv5725.h"
#include "slot.h"
#include "src/WebSockets.h"
#include "src/WebSocketsServer.h"
#include "fonts.h"
#include "OSDManager.h"
#include <stdio.h>

#include "ntsc_1920x1080.h"

// #define ACE

#define RGB1 0x01
#define YUV0 0x00
extern uint8_t RGB_Com;

#define HV_Enable 0x00
#define HV_Disable 0x01

#define VGA_Sync 1
#define RGBs_Sync 2
#define Ypbpr_Sync 3

typedef TV5725<GBS_ADDR> GBS;
extern void applyPresets(uint8_t videoMode);
extern void setOutModeHdBypass(bool bypass);
extern void saveUserPrefs();
extern float getOutputFrameRate();
extern void loadDefaultUserOptions();
extern uint8_t getVideoMode();
extern void ChangeAvModeOption(uint8_t num);
extern void ChangeSvModeOption(uint8_t num);

extern void doPostPresetLoadSteps();
extern void writeProgramArrayNew(const uint8_t *programArray, boolean skipMDSection);
extern runTimeOptions *rto;
extern userOptions *uopt;
extern const char *ap_ssid;
extern const char *final_ssid;
extern const char *ap_password;
extern const char *device_hostname_full;
extern WebSocketsServer webSocket;
extern OLEDMenuManager oledMenu;
extern OSDManager osdManager;
unsigned long oledMenuFreezeStartTime;
unsigned long oledMenuFreezeTimeoutInMS;

const unsigned char RGBs[7] = {0x41, 0x44, 'S', 0x40};
const unsigned char RGsB[7] = {0x41, 0x44, 'S', 0x50};
const unsigned char VGA[7] = {0x41, 0x44, 'S', 0x60};
const unsigned char Ypbpr[7] = {0x41, 0x44, 'S', 0x70};
const unsigned char Adv_7391_SV[7] = {0x41, 0x44, 'S', 0x10};
const unsigned char Adv_7391_AV[7] = {0x41, 0x44, 'S', 0x20};

const unsigned char INFO[7] = {0x41, 0x44, 'I', 0x80};

unsigned char TvMode[7] = {0x41, 0x44, 'T'};

unsigned char Adv_2X[7] = {0x41, 0x44, 'S', 0x30};
unsigned char Adv_1X[7] = {0x41, 0x44, 'S', 0x31};
unsigned char Adv_SM_ON[7] = {0x41, 0x44, 'S', 0x90};
unsigned char Adv_SM_OFF[7] = {0x41, 0x44, 'S', 0x91};

unsigned char Adv_COMPATIBILITY_ON[7] = {0x41, 0x44, 'S', 0xA0};
unsigned char Adv_COMPATIBILITY_OFF[7] = {0x41, 0x44, 'S', 0xA1};

unsigned char Adv_BCSH[7] = {0x41, 0x44, 'N'};
// unsigned char Adv_SIGNALIZED[7] = {0x41, 0x44, 'N', 0x11};



extern uint8_t syncFound;
//1：RGBs\
  2：VGA\
  3：Ypbpr\
  检测该信号的同步源
// extern uint8_t InCurrent;
// 通过OLED菜单更改了输入信号
extern uint8_t InputChanged;
//1：RGBs\
  2：VGA\
  3：Ypbpr\
  选择输入的信号
extern uint8_t SeleInputSource;

// 上电选择亮度
extern uint8_t BriorCon;

// 屏显
extern uint8_t Info;
// 解析菜单处理程序
bool resolutionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime)
    {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
            manager->unfreeze();
        }
        return false;
    }
    oledMenuFreezeTimeoutInMS = 1000;   // freeze for 1s    //（led菜单冻结超时）。
    oledMenuFreezeStartTime = millis(); // （led菜单冻结开始时间
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
    switch (item->tag)
    {
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
    case MT_480s576:
        preset = PresetPreference::Output480P;
        break;
    case MT_DOWNSCALE:
        preset = PresetPreference::OutputDownscale;
        break;
    case MT_BYPASS:
        preset = PresetPreference::OutputBypass;//  OutputBypass   OutputCustomized
        break;
    default:
        break;
    }
    if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
    {
        videoMode = rto->videoStandardInput;
    }

    if (item->tag != MT_BYPASS)
    {
        uopt->presetPreference = preset;
        rto->useHdmiSyncFix = 1;           // 使用 HDMI 同步修复
        if (rto->videoStandardInput == 14) // 视频标准输入
        {
            rto->videoStandardInput = 15;
        }
        else
        {
            applyPresets(videoMode);
        }
    }
    else
    {
        // setOutModeHdBypass(false);
        // uopt->presetPreference = preset;
        // if (rto->videoStandardInput != 15)
        // {
        //     rto->autoBestHtotalEnabled = 0;
        //     if (rto->applyPresetDoneStage == 11)
        //     {
        //         rto->applyPresetDoneStage = 1;
        //     }
        //     else
        //     {
        //         rto->applyPresetDoneStage = 10;
        //     }
        // }
        // else
        // {
        //     rto->applyPresetDoneStage = 1;
        // }
      // if(Info == InfoVGA)
      // {
      //     uopt->preferScalingRgbhv = false;
      // }
      // else 
      // {
      //   uopt->preferScalingRgbhv = true;
      //   setOutModeHdBypass(false);
      //   uopt->presetPreference = OutputBypass;
      // }
    }
    saveUserPrefs();
    manager->freeze();
    return false;
}
// 预设选择菜单处理程序
bool presetSelectionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime)
    {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
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
    uopt->presetSlot = 'A' + item->tag; // ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,
    uopt->presetPreference = PresetPreference::OutputCustomized;
    saveUserPrefs();
    if (rto->videoStandardInput == 14)
    {
        // VGA 升频路径：让同步器处理
        rto->videoStandardInput = 15;
    }
    else
    {
        // 正常途径
        applyPresets(rto->videoStandardInput);
    }
    saveUserPrefs();
    manager->freeze();
    oledMenuFreezeTimeoutInMS = 2000;
    oledMenuFreezeStartTime = millis();

    return false;
}
// 预设创建菜单处理程序
bool presetsCreationMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    SlotMetaArray slotsObject;
    File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");
    manager->clearSubItems(item);
    int curNumSlot = 0;
    if (slotsBinaryFileRead)
    {
        slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileRead.close();
        for (int i; i < SLOTS_TOTAL; ++i)
        {
            const SlotMeta &slot = slotsObject.slot[i];
            if (strcmp(EMPTY_SLOT_NAME, slot.name) == 0 || !strlen(slot.name))
            {
                continue;
            }
            curNumSlot++;
            if (curNumSlot > OLED_MENU_MAX_SUBITEMS_NUM)   //上限数量
            {
                break;
            }
            manager->registerItem(item, slot.slot, slot.name, presetSelectionMenuHandler);
        }
    }

    if (curNumSlot > OLED_MENU_MAX_SUBITEMS_NUM)
    {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_TOO_MANY_PRESETS));
    }

    if (!item->numSubItem)
    {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_NO_PRESETS));
    }
    return true;
}
// 重置菜单处理程序
bool resetMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime)
    {
        // not precise
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
            manager->unfreeze();
            ESP.reset();
            return false;
        }
        return false;
    }

    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    switch (item->tag)
    {
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
    webSocket.close();
    delay(50);
    switch (item->tag)
    {
    case MT_RESET_WIFI:
        WiFi.disconnect();
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
                                                                                          // 是首次
bool currentSettingHandler(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long lastUpdateTime = 0;
    if (isFirstTime)
    {
        lastUpdateTime = 0;
        oledMenuFreezeStartTime = millis();
        oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds if no input
        manager->freeze();
    }
    else if (nav != OLEDMenuNav::IDLE)
    {
        manager->unfreeze();
        return false;
    }
    if (millis() - lastUpdateTime <= 200)
    {
        return false;
    }
    OLEDDisplay &display = *manager->getDisplay();
    display.clear();
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    display.setFont(ArialMT_Plain_16);
    if (rto->sourceDisconnected || !rto->boardHasPower)
    {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
            manager->unfreeze();
            return false;
        }
        display.setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display.drawXbm(CENTER_IMAGE(TEXT_NO_INPUT));
    }
    else
    {
        // TODO translations
        boolean vsyncActive = 0;
        boolean hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        rto->presetID = GBS::GBS_PRESET_ID::read();

        display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        if (rto->presetID == 0x01 || rto->presetID == 0x11)
        {
            display.drawString(0, 0, "1280x960");
        }
        else if (rto->presetID == 0x02 || rto->presetID == 0x12)
        {
            display.drawString(0, 0, "1280x1024");
        }
        else if (rto->presetID == 0x03 || rto->presetID == 0x13)
        {
            display.drawString(0, 0, "1280x720");
        }
        else if (rto->presetID == 0x05 || rto->presetID == 0x15)
        {
            display.drawString(0, 0, "1920x1080");
        }
        else if (rto->presetID == 0x06 || rto->presetID == 0x16)
        {
            display.drawString(0, 0, "Downscale");
        }
        else if (rto->presetID == 0x04)
        {
            display.drawString(0, 0, "720x480");
        }
        else if (rto->presetID == 0x14)
        {
            display.drawString(0, 0, "768x576");
        }
        else
        {
            display.drawString(0, 0, "bypass");
        }

        display.drawString(0, 20, String(ofr, 5) + "Hz");

        if (currentInput == 1)
        {
            display.drawString(0, 41, "RGB");
        }
        else
        {
            display.drawString(0, 41, "YpBpR");
        }

        if (currentInput == 1)
        {
            vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
            if (vsyncActive)
            {
                display.drawString(70, 41, "V");
                hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                if (hsyncActive)
                {
                    display.drawString(53, 41, "H");
                }
            }
        }
    }
    display.display();
    lastUpdateTime = millis();

    return false;
}

class ChecksumSender {
public:
  // 构造函数：注入串口依赖（默认使用Serial）
  explicit ChecksumSender(HardwareSerial& serial = Serial)
    : m_serial(serial) {
    initRandomSeed();
  }

  // 发送版本1：自动生成随机数
  void send(const unsigned char* buff) {
    // send(buff, static_cast<unsigned char>(random(254)));
    unsigned char buff_lin[7];
    buff_lin[0] = buff[0];
    buff_lin[1] = buff[1];
    buff_lin[2] = buff[2];
    buff_lin[3] = buff[3];
    buff_lin[4] = random(254);
    buff_lin[5] = 0xfe;
    buff_lin[6] = buff_lin[0] + buff_lin[1] + buff_lin[2] + buff_lin[3] + buff_lin[4] + buff_lin[5];
    m_serial.write(buff_lin, sizeof(buff_lin));
  }

  // 发送版本2：手动指定reg、num值
  void send(const unsigned char* buff, unsigned char reg, unsigned char val) {
    unsigned char buff_lin[7];
    copyBaseData(buff, buff_lin);
    buff_lin[3] = reg;
    buff_lin[4] = val;
    buff_lin[5] = 0xFE;
    buff_lin[6] = calculateChecksum(buff_lin, 6); // 计算前6字节校验和
    
    m_serial.write(buff_lin, sizeof(buff_lin));
  }

private:
  HardwareSerial& m_serial;  // 串口引用

  // 初始化随机数种子
  void initRandomSeed() {
    randomSeed(analogRead(A0));  // 使用悬空引脚噪声生成种子
  }

  // 复制基础数据（前4字节）
  void copyBaseData(const unsigned char* src, unsigned char* dest) {
    for(int i=0; i<4; ++i) {
      dest[i] = src[i];
    }
  }

  // 计算校验和（可复用方法）
  unsigned char calculateChecksum(const unsigned char* data, size_t length) {
    unsigned char sum = 0;
    for(size_t i=0; i<length; ++i) {
      sum += data[i];
    }
    return sum;
  }
};

ChecksumSender sender;
// void Checksum_Send(const unsigned char *buff)
// {
//     unsigned char buff_lin[7];
//     buff_lin[0] = buff[0];
//     buff_lin[1] = buff[1];
//     buff_lin[2] = buff[2];
//     buff_lin[3] = buff[3];
//     buff_lin[4] = random(254);
//     buff_lin[5] = 0xfe;
//     buff_lin[6] = buff_lin[0] + buff_lin[1] + buff_lin[2] + buff_lin[3] + buff_lin[4] + buff_lin[5];
//     Serial.write(buff_lin, 7);
// }

void Checksum_Sendmode(const unsigned char *buff, uint8_t mode)
{     
    unsigned char buff_lin[7];
    buff_lin[0] = buff[0];
    buff_lin[1] = buff[1];
    buff_lin[2] = buff[2];
    buff_lin[3] = buff[3] | (mode & 0x0f);
    buff_lin[4] = random(254);
    buff_lin[5] = 0xfe;
    buff_lin[6] = buff_lin[0] + buff_lin[1] + buff_lin[2] + buff_lin[3] + buff_lin[4] + buff_lin[5];
    Serial.write(buff_lin, 7);
}

static void LoadDefault()
{
    loadDefaultUserOptions();

    rto->autoBestHtotalEnabled = true; // 已启用自动最佳总计
    rto->syncLockFailIgnore = 16;      //
    rto->forceRetime = false;          //
    rto->syncWatcherEnabled = true;    //
    rto->phaseADC = 16;                //
    rto->phaseSP = 16;                 //
    rto->failRetryAttempts = 0;        //
    rto->presetID = 0;                 //
    rto->isCustomPreset = false;       //
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false; // 运动自适应隔行扫描
    rto->deinterlaceAutoEnabled = true;           // 去隔行扫描自动启用
    rto->scanlinesEnabled = false;                // 扫描线启用
    rto->boardHasPower = true;                    // 板有电源
    rto->presetIsPalForce60 = false;              // 预设为 PalForce60
    rto->syncTypeCsync = false;                   // 同步类型
    rto->isValidForScalingRGBHV = false;          // 有效缩放
    rto->medResLineCount = 0x33;                  //
    rto->osr = 0;                                 //
    rto->useHdmiSyncFix = 0;                      //
    rto->notRecognizedCounter = 0;                //

    rto->videoStandardInput = 0;    // 视频标准输入
    rto->outModeHdBypass = false;   //
    rto->videoIsFrozen = true;      //
    rto->sourceDisconnected = true; //
    // rto->isInLowPowerMode = false; //
    rto->applyPresetDoneStage = 0; //
    // rto->presetVlineShift = 0;     //
    rto->clampPositionIsSet = 0;     //
    rto->coastPositionIsSet = 0;     //
    rto->continousStableCounter = 0; //
    rto->currentLevelSOG = 5;        //
    rto->thisSourceMaxLevelSOG = 31; //
}

static void resetSyncProcessor_yuv()
{
    GBS::SFTRST_SYNC_RSTZ::write(0);
    GBS::SFTRST_MODE_RSTZ::write(0);
    // GBS::SFTRST_HDBYPS_RSTZ::write(0);
    // delayMicroseconds(10);
    delay(10);
    GBS::SFTRST_SYNC_RSTZ::write  (1);
    GBS::SFTRST_MODE_RSTZ::write  (1);
    // GBS::SFTRST_HDBYPS_RSTZ::write(1);
    // rto->clampPositionIsSet = false;  // resetSyncProcessor 是 autosog 的一部分
    // rto->coastPositionIsSet = false;

    LoadDefault();
}

static void resetSyncProcessor()
{
    GBS::SFTRST_SYNC_RSTZ::write(0);
    delay(10);
    GBS::SFTRST_SYNC_RSTZ::write(1);
    LoadDefault();
}
static void resetModeDetect()
{
    GBS::SFTRST_MODE_RSTZ::write(0);
    delay(1); // needed
    GBS::SFTRST_MODE_RSTZ::write(1);
    // rto->clampPositionIsSet = false;
    // rto->coastPositionIsSet = false;
}

void SetReg(unsigned char reg, unsigned char val)
{
  sender.send(Adv_BCSH,reg,val);
}

// void Signalized(void)
// {
//   sender.send(Adv_SIGNALIZED);
// }

void InputVGA_mode(uint8_t mode)
{

    Checksum_Sendmode(VGA, !mode);
    // InCurrent = S_VGA;
    SeleInputSource = S_VGA;
    Info = InfoVGA;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(RGB1);
    GBS::SP_EXT_SYNC_SEL::write(HV_Enable);
    GBS::ADC_INPUT_SEL::write(RGB1);
    BriorCon = 0;
    // loadDefaultUserOptions();
    rto->sourceDisconnected = true;
    saveUserPrefs();
}
void InputRGsB_mode(uint8_t mode)
{

    Checksum_Sendmode(RGsB, !mode);
    // InCurrent = S_RGBs;
    SeleInputSource = S_RGBs;
    Info = InfoRGsB;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(RGB1);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(RGB1);
    BriorCon = 0;
    // loadDefaultUserOptions();
    rto->sourceDisconnected = true;
    saveUserPrefs();
}
void InputRGBs_mode(uint8_t mode)
{
    Checksum_Sendmode(RGBs, !mode);
    // InCurrent = S_RGBs;
    SeleInputSource = S_RGBs;
    Info = InfoRGBs;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(RGB1);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(RGB1);
    BriorCon = 0;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    saveUserPrefs();
}

void InputRGBs(void)
{
    sender.send(RGBs);
    // InCurrent = S_RGBs;
    SeleInputSource = S_RGBs;
    Info = InfoRGBs;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(RGB1);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(RGB1);
    BriorCon = 0;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    saveUserPrefs();
}
void InputYUV(void)
{
    sender.send(Ypbpr);
    // InCurrent = S_YUV;
    SeleInputSource = S_YUV;
    Info = InfoYUV;
    resetSyncProcessor();
    // GBS::ADC_SOGEN::write(YUV0);
    // GBS:: SP_EXT_SYNC_SEL::write(HV_Disable);
    // GBS:: ADC_INPUT_SEL::write(YUV0);
    BriorCon = 1;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    saveUserPrefs();
}

void InputNULL(void)
{
    sender.send(Ypbpr);
    SeleInputSource = S_YUV;
    // Info = InfoYUV;
    resetSyncProcessor();
    rto->sourceDisconnected = true;
}
void InputRGsB(void)
{
    sender.send(RGsB);
    // InCurrent = S_RGBs;
    SeleInputSource = S_RGBs;
    Info = InfoRGsB;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(RGB1);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(RGB1);
    BriorCon = 0;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    saveUserPrefs();
}
void InputVGA(void)
{
    // sender.send(VGA);
    Checksum_Sendmode(VGA, 1);
    // InCurrent = S_VGA;
    SeleInputSource = S_VGA;
    Info = InfoVGA;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(RGB1);
    GBS::SP_EXT_SYNC_SEL::write(HV_Enable);
    GBS::ADC_INPUT_SEL::write(RGB1);
    BriorCon = 0;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    saveUserPrefs();
}
void InputINFO(void)
{
    sender.send(INFO);
    SeleInputSource = S_YUV;
    // Info = InfoSV;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(YUV0);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(YUV0);
    BriorCon = 2;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    saveUserPrefs();
}
void InputSV(void)
{
    sender.send(Adv_7391_SV);
    // InCurrent = S_YUV;
    SeleInputSource = S_YUV;
    Info = InfoSV;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(YUV0);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(YUV0);
    BriorCon = 2;
    rto->sourceDisconnected = true;
    // doPostPresetLoadSteps();
    // loadDefaultUserOptions();
    rto->isInLowPowerMode = false;
    saveUserPrefs();
}

void InputSV_mode(uint8_t mode)
{
    Checksum_Sendmode(Adv_7391_SV, mode);
    // InCurrent = S_YUV;
    SeleInputSource = S_YUV;
    Info = InfoSV;
    resetSyncProcessor();
    // rto->inputIsYpBpR = 1;
    GBS::ADC_SOGEN::write(YUV0);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(YUV0);
    BriorCon = 2;
    rto->sourceDisconnected = true;
    // doPostPresetLoadSteps()
    // loadDefaultUserOptions();
    // writeProgramArrayNew(ntsc_1920x1080, true);
    // doPostPresetLoadSteps();
    rto->isInLowPowerMode = false;
    saveUserPrefs();
}
void InputAV(void)
{
    sender.send(Adv_7391_AV);
    // InCurrent = S_YUV;
    SeleInputSource = S_YUV;
    Info = InfoAV;
    // resetSyncProcessor_yuv();
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(YUV0);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(YUV0);
    BriorCon = 2;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    rto->isInLowPowerMode = false;
    saveUserPrefs();
}

void InputAV_mode(uint8_t mode)
{
    Checksum_Sendmode(Adv_7391_AV, mode);
    // InCurrent = S_YUV;
    SeleInputSource = S_YUV;
    Info = InfoAV;
    resetSyncProcessor();
    GBS::ADC_SOGEN::write(YUV0);
    GBS::SP_EXT_SYNC_SEL::write(HV_Disable);
    GBS::ADC_INPUT_SEL::write(YUV0);
    BriorCon = 2;
    rto->sourceDisconnected = true;
    // loadDefaultUserOptions();
    rto->isInLowPowerMode = false;
    saveUserPrefs();
}

void Send_TvMode(uint8_t Mode)
{
    TvMode[3] = Mode;
    sender.send(TvMode);
    saveUserPrefs();
}

void Send_Line(bool line)
{
    if (line)
        sender.send(Adv_2X);
    else
        sender.send(Adv_1X);
    saveUserPrefs();
}

void Send_Smooth(bool Smooth)
{
    if (Smooth)
        sender.send(Adv_SM_ON);
    else
        sender.send(Adv_SM_OFF);
    saveUserPrefs();
}

void Send_Compatibility(bool Com)
{
    if (!Com)
        sender.send(Adv_COMPATIBILITY_ON);
    else
        sender.send(Adv_COMPATIBILITY_OFF);
    saveUserPrefs();
}
bool Adv7391TvModeSwHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{

    if (!isFirstTime)
    {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
            manager->unfreeze();
        }
        return false;
    }
    oledMenuFreezeTimeoutInMS = 1000;   // freeze for 1s    //（led菜单冻结超时）。
    oledMenuFreezeStartTime = millis(); // （led菜单冻结开始时间
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    uint8_t videoMode = getVideoMode();

    TVMODE_PresetPreference preset = TVMODE_PresetPreference::MT_MODE_AUTO;

switch (item->tag) {
    case MT_MODE_AUTO:
        preset = TVMODE_PresetPreference::MT_MODE_AUTO;
        break;
    case MT_MODE_PAL:
        preset = TVMODE_PresetPreference::MT_MODE_PAL;
        break;
    case MT_MODE_NTSCM:
        preset = TVMODE_PresetPreference::MT_MODE_NTSCM;
        break;
    case MT_MODE_PAL60:
        preset = TVMODE_PresetPreference::MT_MODE_PAL60;
        break;
    case MT_MODE_NTSC443:
        preset = TVMODE_PresetPreference::MT_MODE_NTSC443;
        break;
    case MT_MODE_NTSCJ:
        preset = TVMODE_PresetPreference::MT_MODE_NTSCJ;
        break;
    case MT_MODE_PALNwp:
        preset = TVMODE_PresetPreference::MT_MODE_PALNwp;
        break;
    case MT_MODE_PALMwop:
        preset = TVMODE_PresetPreference::MT_MODE_PALMwop;
        break;
    case MT_MODE_PALM:
        preset = TVMODE_PresetPreference::MT_MODE_PALM;
        break;
    case MT_MODE_PALCmbN:
        preset = TVMODE_PresetPreference::MT_MODE_PALCmbN;
        break;
    case MT_MODE_PALCmbNwp:
        preset = TVMODE_PresetPreference::MT_MODE_PALCmbNwp;
        break;
    case MT_MODE_SECAM:
        preset = TVMODE_PresetPreference::MT_MODE_SECAM;
        break;
    default:
        // 处理未知模式
        break;
}

    uopt->TVMODE_presetPreference = preset;
    // 更新后的条件处理逻辑
// if (preset == TVMODE_PresetPreference::MT_MODE_AUTO) {  
    if (Info == InfoAV) ChangeAvModeOption(0);
    else if (Info == InfoSV) ChangeSvModeOption(0);
    if (Info == InfoSV || Info == InfoAV) {
        TvMode[3] = modes[preset];  // 0x04
        sender.send(TvMode);
    }
    // saveUserPrefs();
    manager->freeze();
    return false;
}

bool InputSwHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{

    unsigned char Adv_On[7] = {0x41, 0x44, 'S', 0xfd};
    unsigned char Adv_Off[7] = {0x41, 0x44, 'S', 0x01};

    if (!isFirstTime)
    {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
            manager->unfreeze();
        }
        return false;
    }

    oledMenuFreezeTimeoutInMS = 1000;   // freeze for 1s    //（led菜单冻结超时）。
    oledMenuFreezeStartTime = millis(); // （led菜单冻结开始时间
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    uint8_t videoMode = getVideoMode();

    INPUT_PresetPreference preset = INPUT_PresetPreference::MT_RGBs;

    switch (item->tag)
    {
    case MT_RGBs:
        preset = INPUT_PresetPreference::MT_RGBs;
        break;
    case MT_RGsB:
        preset = INPUT_PresetPreference::MT_RGsB;
        break;
    case MT_VGA:
        preset = INPUT_PresetPreference::MT_VGA;
        break;
    case MT_YPBPR:
        preset = INPUT_PresetPreference::MT_YPBPR;
        break;
    case MT_SV:
        preset = INPUT_PresetPreference::MT_SV;
        break;
    case MT_AV:
        preset = INPUT_PresetPreference::MT_AV;
        break;
    default:
        break;
    }

    uopt->INPUT_presetPreference = preset;

    if (preset == INPUT_PresetPreference::MT_RGBs)
    {
        InputRGBs();
    }
    else if (preset == INPUT_PresetPreference::MT_RGsB)
    {
        InputRGsB();
    }

    else if (preset == INPUT_PresetPreference::MT_VGA)
    {
        InputVGA();
    }
    else if (preset == INPUT_PresetPreference::MT_YPBPR)
    {
        InputYUV();
    }

    else if (preset == INPUT_PresetPreference::MT_SV)
    {
        InputSV();
    }
    else if (preset == INPUT_PresetPreference::MT_AV)
    {
        InputAV();
    }
    // saveUserPrefs();
    manager->freeze();
    return false;
}

bool SettingHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    unsigned char Adv_ACE_ON[7] = {0x41, 0x44, 'S', 0x80};
    unsigned char Adv_ACE_OFF[7] = {0x41, 0x44, 'S', 0x81};

    if (!isFirstTime)
    {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS)
        {
            manager->unfreeze();
        }
        return false;
    }

    oledMenuFreezeTimeoutInMS = 1000;   // freeze for 1s    //（led菜单冻结超时）。
    oledMenuFreezeStartTime = millis(); // （led菜单冻结开始时间
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    uint8_t videoMode = getVideoMode();
    SETTING_PresetPreference preset = SETTING_PresetPreference::MT_7391_1X;

    // MT_7391_1X    ,
    // MT_7391_2X    ,
    // MT_SMOOTH_OFF ,
    // MT_SMOOTH_ON  ,
    switch (item->tag)
    {
    case MT_7391_1X:
        preset = SETTING_PresetPreference::MT_7391_1X;
        break;
    case MT_7391_2X:
        preset = SETTING_PresetPreference::MT_7391_2X;
        break;
    case MT_SMOOTH_OFF:
        preset = SETTING_PresetPreference::MT_SMOOTH_OFF;
        break;
    case MT_SMOOTH_ON:
        preset = SETTING_PresetPreference::MT_SMOOTH_ON;
        break;
    case MT_COMPATIBILITY_OFF:
        preset = SETTING_PresetPreference::MT_COMPATIBILITY_OFF;
        break;
    case MT_COMPATIBILITY_ON:
        preset = SETTING_PresetPreference::MT_COMPATIBILITY_ON;
#ifdef ACE  
        case MT_ACE_OFF:
            preset = SETTING_PresetPreference::MT_ACE_OFF;
            break;
        case MT_ACE_ON:
            preset = SETTING_PresetPreference::MT_ACE_ON;
        break;
#endif        
    default:
        break;
    }

    uopt->SETTING_presetPreference = preset;

    if (preset == SETTING_PresetPreference::MT_7391_1X)
    {
        sender.send(Adv_1X);
    }
    else if (preset == SETTING_PresetPreference::MT_7391_2X)
    {   
        sender.send(Adv_2X);
    }
    else if (preset == SETTING_PresetPreference::MT_SMOOTH_OFF)
    {
        sender.send(Adv_SM_OFF);
    }
    else if (preset == SETTING_PresetPreference::MT_SMOOTH_ON)
    {   
        sender.send(Adv_SM_ON);
    }
    else if (preset == SETTING_PresetPreference::MT_COMPATIBILITY_OFF)
    {
        RGB_Com = COMPATIBILITY_OFF;
        Send_Compatibility(RGB_Com);
    }
    else if (preset == SETTING_PresetPreference::MT_COMPATIBILITY_ON)
    {
        RGB_Com = COMPATIBILITY_ON;
        Send_Compatibility(RGB_Com);
    }
#ifdef ACE    
    else if (preset == SETTING_PresetPreference::MT_ACE_OFF)
    {
        sender.send(Adv_ACE_OFF);
    }
    else if (preset == SETTING_PresetPreference::MT_ACE_ON)
    {
        sender.send(Adv_ACE_ON);
    }
#endif
    // saveUserPrefs();
    manager->freeze();
    return false;
}

bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    static char ssid[64];
    static char ip[25];
    static char domain[25];
    WiFiMode_t wifiMode = WiFi.getMode();
    manager->clearSubItems(item);
    if (wifiMode == WIFI_STA)
    {
        sprintf(ssid, "SSID: %s", WiFi.SSID().c_str());
        manager->registerItem(item, 0, ssid);
        if (WiFi.isConnected())
        {
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_CONNECTED));
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_URL));
            sprintf(ip, "http://%s", WiFi.localIP().toString().c_str());
            manager->registerItem(item, 0, ip);
            sprintf(domain, "http://%s", device_hostname_full);
            manager->registerItem(item, 0, domain);
        }
        else
        {
            // shouldn't happen?
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_DISCONNECTED));
        }
    }
    else if (wifiMode == WIFI_AP)
    {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_CONNECT_TO));
        sprintf(ssid, "SSID: %s (%s)", final_ssid, ap_password);
        manager->registerItem(item, 0, ssid);
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_URL));
        manager->registerItem(item, 0, "http://192.168.4.1");
        sprintf(domain, "http://%s", device_hostname_full);
        manager->registerItem(item, 0, domain);
    }
    else
    {
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

    if (isFirstTime)
    {
        left = OSD_TIMEOUT;
        start = millis();
        manager->freeze();
        osdManager.tick(OSDNav::ENTER);
    }
    else
    {
        display->clear();
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display->drawStringf(OLED_MENU_WIDTH / 2, 16, buf, "OSD (%ds)", left / 1000 + 1);
        display->display();
        if (REVERSE_ROTARY_ENCODER_FOR_OLED_MENU)
        {
            // 倒退导航恢复正常
            if (nav == OLEDMenuNav::DOWN)
            {
                nav = OLEDMenuNav::UP;
            }
            else if (nav == OLEDMenuNav::UP)
            {
                nav = OLEDMenuNav::DOWN;
            }
        }
        switch (nav)
        {
        case OLEDMenuNav::ENTER:
            osdManager.tick(OSDNav::ENTER);
            start = millis();
            break;
        case OLEDMenuNav::DOWN:
            if (REVERSE_ROTARY_ENCODER_FOR_OSD)
            {
                osdManager.tick(OSDNav::RIGHT);
            }
            else
            {
                osdManager.tick(OSDNav::LEFT);
            }
            start = millis();
            break;
        case OLEDMenuNav::UP:
            if (REVERSE_ROTARY_ENCODER_FOR_OSD)
            {
                osdManager.tick(OSDNav::LEFT);
            }
            else
            {
                osdManager.tick(OSDNav::RIGHT);
            }
            start = millis();
            break;
        default:
            break;
        }
        left = OSD_TIMEOUT - (millis() - start);
        if (left <= 0)
        {
            manager->unfreeze();
            osdManager.menuOff();
        }
    }
    return true;
}
void initOLEDMenu()
{
    OLEDMenuItem *root = oledMenu.rootItem;
    ////////////////////////////////////////////////////////////////////////////////////////////
    // // ADV7391 SW  OLEDMenuItem
    OLEDMenuItem *advMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_ADVINPUT));

    const char *AdvInPut[6] = {
        "RGBs",
        "RGsB",
        "VGA",
        "YPBPR",
        "SV",
        "AV",
    };
    uint8_t tags_input[] = {
        MT_RGBs,
        MT_RGsB,
        MT_VGA,
        MT_YPBPR,
        MT_SV,
        MT_AV,
    };

    for (int i = 0; i < sizeof(tags_input); ++i)
    {
        oledMenu.registerItem(advMenu, tags_input[i], AdvInPut[i], InputSwHandler);
    }

    // OSD Menu     TEXT_NO_INPUT  OM_OSD  OM_ADV7391
    // oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_OSD), osdMenuHanlder);

    // Resolutions
    OLEDMenuItem *resMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESOLUTION));
    const char *resolutions[4] = {"1280x960", "1280x1024", "1280x720", "1920x1080"};   //, "480/576"
    uint8_t            tags[4] = {MT_1280x960, MT1280x1024, MT1280x720, MT1920x1080};  //, MT_480s576
    for (int i = 0;     i < 4; ++i)
    {
        oledMenu.registerItem(resMenu, tags[i], resolutions[i], resolutionMenuHandler);
    }

    // downscale and passthrough
    // oledMenu.registerItem(resMenu, MT_DOWNSCALE, IMAGE_ITEM(OM_DOWNSCALE), resolutionMenuHandler);
    // oledMenu.registerItem(resMenu, MT_BYPASS, IMAGE_ITEM(OM_PASSTHROUGH), resolutionMenuHandler);

    OLEDMenuItem *SettingMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_SETTING)); // setting

    
#ifdef ACE    
    const char *AdvSetting[6] = {   
#else
    const char *AdvSetting[4] = { 
#endif       
        // "    1X    ",
        // "    2X    ",
        "Smooth_Off",
        "Smooth_On ",
        "Compatibility_Off",
        "Compatibility_On ",
#ifdef ACE  
        "ACE_Off",
        "ACE_On ",
#endif 
    };
    //

    uint8_t tags_setting[] = {
        // MT_7391_1X,
        // MT_7391_2X,
        MT_SMOOTH_OFF,
        MT_SMOOTH_ON,
        MT_COMPATIBILITY_OFF,
        MT_COMPATIBILITY_ON,
#ifdef ACE  
        MT_ACE_OFF,
        MT_ACE_ON,
#endif 
    };

    for (int i = 0; i < sizeof(tags_setting) ; ++i)   
    {
        oledMenu.registerItem(SettingMenu, tags_setting[i], AdvSetting[i], SettingHandler);
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////
    // ADV7391 SW  ->  TV MODE

    OLEDMenuItem *TvModeMenu = oledMenu.registerItem(SettingMenu, MT_NULL, IMAGE_ITEM(OM_TVMODE));
    const char *AdvTvMode[12] = {
        "AUTO",
        "PAL",
        "NTSC-M",
        "PAL-60",
        "NTSC443",
        "NTSC-J",
        "PAL-N w/ p",
        "PAL-M w/o p",
        "PAL-M",
        "PAL Cmb -N",
        "PAL Cmb -N w/ p",
        "SECAM",
    };

    uint8_t tags_AdvTvMode[] = {
        MT_MODE_AUTO,
        MT_MODE_PAL,
        MT_MODE_NTSCM,
        MT_MODE_PAL60,
        MT_MODE_NTSC443,
        MT_MODE_NTSCJ,
        MT_MODE_PALNwp,
        MT_MODE_PALMwop,
        MT_MODE_PALM,
        MT_MODE_PALCmbN,
        MT_MODE_PALCmbNwp,
        MT_MODE_SECAM,
    };
    for (int i = 0; i < sizeof(tags_AdvTvMode); ++i)
    {
        oledMenu.registerItem(TvModeMenu, tags_AdvTvMode[i], AdvTvMode[i], Adv7391TvModeSwHandler);
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////




    // Presets
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_PRESET), presetsCreationMenuHandler);

    // WiFi
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_WIFI), wifiMenuHandler);

    // Current Settings
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_CURRENT), currentSettingHandler);

    // Reset (Misc.)
    OLEDMenuItem *resetMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESET_RESTORE)); // OM_RESET_RESTORE
    oledMenu.registerItem(resetMenu, MT_RESET_GBS, IMAGE_ITEM(OM_RESET_GBS), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESTORE_FACTORY, IMAGE_ITEM(OM_RESTORE_FACTORY), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESET_WIFI, IMAGE_ITEM(OM_RESET_WIFI), resetMenuHandler);
}
