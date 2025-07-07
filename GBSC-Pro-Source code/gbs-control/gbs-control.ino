/*
System
*/
extern unsigned long OledUpdataTime;

// import machine
static uint8_t Info_sate = 0;
static bool decode_flag = 0;
static unsigned long lastVsyncLock = millis();
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)
#define DEBUG_IN_PIN D6 
// LED_BUILTIN    15
// #define LEDON     pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW)
// #define LEDOFF    pinMode(LED_BUILTIN, INPUT);  digitalWrite(LED_BUILTIN, HIGH)
#define HAVE_BUTTONS 0
static inline void writeBytes(uint8_t slaveRegister, uint8_t *values, uint8_t numValues);
const uint8_t *loadPresetFromSPIFFS(byte forVideoMode);

/*
TIM
*/
#include <PolledTimeout.h>
esp8266::polledTimeout::periodicFastUs halfPeriod(1000);

static unsigned long Tim_signal = 0;
static unsigned long Tim_sys = 0;
static unsigned long Tim_web = 0;
static unsigned long Tim_menuItem = 0;
static unsigned long Tim_Resolution = 0, Tim_Resolution_Start = 0;
/*
TV5725
*/
#include "tv5725.h"
#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_720x480.h"
#include "pal_768x576.h"
#include "ntsc_1280x720.h"
#include "ntsc_1280x1024.h"
#include "ntsc_1920x1080.h"
#include "pal_1280x720.h"
#include "pal_1280x1024.h"
#include "pal_1920x1080.h"
#include "ntsc_downscale.h"
#include "pal_downscale.h"

#include "presetMdSection.h"
#include "presetDeinterlacerSection.h"
#include "presetHdBypassSection.h"
#include "ofw_RGBS.h"

#include "options.h"
#include "slot.h"
#include "osd.h"
typedef TV5725<GBS_ADDR> GBS;

struct MenuAttrs
{
    static const int8_t shiftDelta = 4;
    static const int8_t scaleDelta = 4;
    static const int16_t vertShiftRange = 300;
    static const int16_t horizShiftRange = 400;
    static const int16_t vertScaleRange = 100;
    static const int16_t horizScaleRange = 130;
    static const int16_t barLength = 100;
};
typedef MenuManager<GBS, MenuAttrs> Menu;

enum PresetID : uint8_t {
    PresetHdBypass = 0x21,
    PresetBypassRGBHV = 0x22,
};
struct runTimeOptions rtos;
struct runTimeOptions *rto = &rtos;
struct userOptions uopts;
struct userOptions *uopt = &uopts;
struct adcOptions adcopts;
struct adcOptions *adco = &adcopts;

String slotIndexMap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,";

char serialCommand;
char userCommand;
static uint8_t lastSegment = 0xFF;

static uint16_t St;
static uint16_t Sp;

static unsigned char R_VAL = 0;
static unsigned char G_VAL = 0;
static unsigned char B_VAL = 0;
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))


void PR_rgb(void)
{
#if 0 
  printf("\n\n");

  printf("Read VAL:Y %d U %d V %d \n", (signed char)GBS::VDS_Y_OFST::read(), (signed char)GBS::VDS_U_OFST::read(), (signed char)GBS::VDS_V_OFST::read());

  // printf("VAL:Y %d U %d V %d \n",(signed char)(signed char)GBS::VDS_Y_OFST::read(),(signed char)((signed char)GBS::VDS_U_OFST::read()),(signed char)((signed char)GBS::VDS_V_OFST::read()));

  printf("math:R %0.2lf G %0.2lf B %0.2lf \n", ((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.402 * (signed char)((signed char)GBS::VDS_V_OFST::read()))), ((signed char)((signed char)GBS::VDS_Y_OFST::read()) - (float)(0.344136 * (signed char)((signed char)GBS::VDS_U_OFST::read())) - 0.714136 * (signed char)((signed char)GBS::VDS_V_OFST::read())), ((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.772 * (signed char)((signed char)GBS::VDS_U_OFST::read()))));
  printf("VAL:R %d G %d B %d \n", R_VAL, G_VAL, B_VAL);
#endif
}
void Color_Conversion(void)
{
    GBS::VDS_Y_OFST::write((signed char)((float)(0.299 * (signed char)(R_VAL - 128)) + (float)(0.587 * (signed char)(G_VAL - 128)) + (float)(0.114 * (signed char)(B_VAL - 128))));
    GBS::VDS_U_OFST::write((signed char)((float)(-0.169 * (signed char)(R_VAL - 128)) - (float)(0.331 * (signed char)(G_VAL - 128)) + (float)(0.500 * (signed char)(B_VAL - 128)))); //
    GBS::VDS_V_OFST::write((signed char)((float)(0.500 * (signed char)(R_VAL - 128)) - (float)(0.419 * (signed char)(G_VAL - 128)) - (float)(0.081 * (signed char)(B_VAL - 128))));
}

/*
IR
*/
#include <IRremoteESP8266.h>
#include <IRutils.h>
const int kRecvPin = 2; // D4，infrared receiver PIN
IRrecv irrecv(kRecvPin);
decode_results results;

/*
OSD
*/

float version = 1.2 ;
String full_version = String(version,1);
const char *final_version = full_version.c_str();
#define MODEOPTION_MAX 12
#define MODEOPTION_MIN 0

#define STEP 1
#define OSD_CLOSE_TIME 16000// *100            // 16 sec
#define OSD_RESOLUTION_UP_TIME 1000     // 1 sec
#define OSD_RESOLUTION_CLOSE_TIME 20000 // 20 sec
#include "OSD_TV/remote.h"
#include "OSD_TV/OSD_stv9426.h"
#include "OSD_TV/profile_name.h"
#include "OSD_TV/PT2257.h"

#define New_oled_menuItem_Start 153

// OSDSET
#define OSD_CROSS_TOP '7'
#define OSD_CROSS_MID '8'
#define OSD_CROSS_BOTTOM '9'


typedef enum {
    OSD_ScreenSettings = 63,
    OSD_ColorSettings = 64,
    OSD_ResetDefault = 67,

    OSD_Resolution = 62,
    OSD_Resolution_1080 = 68,
    OSD_Resolution_1024 = 69,
    OSD_Resolution_960 = 70,
    OSD_Resolution_720 = 71,
    OSD_Resolution_pass = 74,
    OSD_SystemSettings = 65,

    OSD_Input = New_oled_menuItem_Start + 1,
    OSD_Input_RGBs,
    OSD_Input_RBsB,
    OSD_Input_VGA,
    OSD_Input_YPBPR,
    OSD_Input_SV,
    OSD_Input_AV,
    OSD_SystemSettings_SVAVInput,
    OSD_SystemSettings_SVAVInput_DoubleLine,
    OSD_SystemSettings_SVAVInput_Smooth,
    OSD_SystemSettings_SVAVInput_Bright,
    OSD_SystemSettings_SVAVInput_contrast,
    OSD_SystemSettings_SVAVInput_saturation,
    OSD_SystemSettings_SVAVInput_default,
    OSD_SystemSettings_SVAVInput_Compatibility,
    OSD_Resolution_RetainedSettings,

} OSD_Menu;
char adl = 0;
boolean IR = 0;
int COl_L = 1;
int Volume = 0;
boolean MUTE_R = 0;
static int oled_menuItem = 0;
static int oled_menuItem_last = 0;
static uint8_t OLED_clear_flag = ~0;
boolean NEW_OLED_MENU = true;

static uint8_t SvModeOption, AvModeOption;
static uint8_t SvModeOptionChanged, AvModeOptionChanged;
static bool SmoothOption;
static bool LineOption;
static bool SettingLineOptionChanged, SettingSmoothOptionChanged;
static uint8_t Bright = 128;
static uint8_t Contrast = 128;
static uint8_t Saturation = 128;

uint8_t RGB_Com;

uint8_t Keep_S = 0;
static uint8_t tentative = 0xfe;
// uint8_t RGBs_CompatibilityChanged;
// uint8_t RGsB_CompatibilityChanged;
// uint8_t VGA_CompatibilityChanged;

int A1_yellow;
int A2_main0;
int A3_main0;


void handle_0(void);
void handle_1(void);
void handle_2(void);
void handle_3(void);
void handle_4(void);
void handle_5(void);
void handle_6(void);
void handle_7(void);
void handle_8(void);
void handle_9(void);
void handle_a(void);
void handle_b(void);
void handle_c(void);
void handle_d(void);
void handle_e(void);
void handle_f(void);
void handle_g(void);
void handle_h(void);
void handle_i(void);
void handle_j(void);
void handle_k(void);
void handle_l(void);
void handle_m(void);
void handle_n(void);
void handle_o(void);
void handle_p(void);
void handle_q(void);
void handle_r(void);
void handle_s(void);
void handle_t(void);
void handle_u(void);
void handle_v(void);
void handle_w(void);
void handle_x(void);
void handle_y(void);
void handle_z(void);
void handle_A(void);
void handle_caret(void);
void handle_at(void);
void handle_exclamation(void);
void handle_hash(void);
void handle_dollar(void);
void handle_percent(void);
void handle_ampersand(void);
void handle_asterisk(void);


typedef struct
{
    int key;               
    void (*handler)(void); 
} MenuEntry;

static const MenuEntry menuTable[] = {

    {'!', handle_exclamation}, // ASCII 33
    {'#', handle_hash},        // ASCII 35
    {'$', handle_dollar},      // ASCII 36
    {'%', handle_percent},     // ASCII 37
    {'&', handle_ampersand},   // ASCII 38
    {'*', handle_asterisk},    // ASCII 42
    {'@', handle_at},          // ASCII 64
    {'^', handle_caret},       // ASCII 94

    {'0', handle_0}, // ASCII 48
    {'1', handle_1}, // ASCII 49
    {'2', handle_2}, // ASCII 50
    {'3', handle_3}, // ASCII 51
    {'4', handle_4}, // ASCII 52
    {'5', handle_5}, // ASCII 53
    {'6', handle_6}, // ASCII 54
    {'7', handle_7}, // ASCII 55
    {'8', handle_8}, // ASCII 56
    {'9', handle_9}, // ASCII 57


    {'a', handle_a}, // ASCII 97
    {'b', handle_b}, // ASCII 98
    {'c', handle_c}, // ASCII 99
    {'d', handle_d}, // ASCII 100
    {'e', handle_e}, // ASCII 101
    {'f', handle_f}, // ASCII 102
    {'g', handle_g}, // ASCII 103
    {'h', handle_h}, // ASCII 104
    {'i', handle_i}, // ASCII 105
    {'j', handle_j}, // ASCII 106
    {'k', handle_k}, // ASCII 107
    {'l', handle_l}, // ASCII 108
    {'m', handle_m}, // ASCII 109
    {'n', handle_n}, // ASCII 110
    {'o', handle_o}, // ASCII 111
    {'p', handle_p}, // ASCII 112
    {'q', handle_q}, // ASCII 113
    {'r', handle_r}, // ASCII 114
    {'s', handle_s}, // ASCII 115
    {'t', handle_t}, // ASCII 116
    {'u', handle_u}, // ASCII 117
    {'v', handle_v}, // ASCII 118
    {'w', handle_w}, // ASCII 119
    {'x', handle_x}, // ASCII 120
    {'y', handle_y}, // ASCII 121
    {'z', handle_z}, // ASCII 122


    {'A', handle_A}, // ASCII 122
};



/*
OLED
*/
#include "SSD1306Wire.h"
#include "images.h"
SSD1306Wire display(0x3c, D2, D1); 
/*
Decode
*/
#include <Versatile_RotaryEncoder.h>
Versatile_RotaryEncoder *versatile_encoder;
const int pin_a = 14;   // D5 = GPIO14 (input of one direction for encoder)
const int pin_b = 13;  // D7 = GPIO13	(input of one direction for encoder)
const int pin_switch = 0; // D3 = GPIO0 


void handleRotate(int8_t rotation);
void handlePress();

/*
OLED MENU
*/
#define USE_NEW_OLED_MENU 1
#if USE_NEW_OLED_MENU
#include "OLEDMenuImplementation.h" 
#include "OSDManager.h"             
OLEDMenuManager oledMenu(&display);
OSDManager osdManager;
volatile OLEDMenuNav oledNav = OLEDMenuNav::IDLE;
volatile uint8_t rotaryIsrID = 0;
#endif

uint8_t syncFound = 0;
// uint8_t InCurrent = 0;
uint8_t BriorCon = 0;
uint8_t Info = 0;
// uint8_t InputChanged = 0;
uint8_t SeleInputSource = 0;

/*
audio
*/
#include "src/si5351mcu.h"
Si5351mcu Si;

/*
wifi
*/
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h> 
#include <ArduinoOTA.h>
#include "PersWiFiManager.h"
#include "src/WebSockets.h"
#include "src/WebSocketsServer.h"

#define THIS_DEVICE_MASTER
#ifdef THIS_DEVICE_MASTER
const char *ap_ssid = "gbscontrol";


uint32_t chipId = ESP.getChipId();


String full_ssid = String(ap_ssid) + String(chipId);
const char *final_ssid = full_ssid.c_str();

const char *ap_password = "qqqqqqqq";
const char *device_hostname_full = "gbscontrol.local";
const char *device_hostname_partial = "gbscontrol"; // for MDNS
// static const char ap_info_string[] PROGMEM =
//     "(WiFi): AP mode (SSID: ";
// const char *ap_info_tail = ", pass 'qqqqqqqq'): Access 'gbscontrol.local' in your browser";
// static const char st_info_string[] PROGMEM =
//     "(WiFi): Access 'http://gbscontrol:80' or 'http://gbscontrol.local' (or device IP) in your browser";
#endif

AsyncWebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);
// AsyncWebSocket webSocket("/ws");
PersWiFiManager persWM(server, dnsServer);

// #define HAVE_PINGER_LIBRARY
#ifdef HAVE_PINGER_LIBRARY
#include <Pinger.h>
#include <PingerResponse.h>
unsigned long pingLastTime;
Pinger pinger; 
#endif

void clearFrame()
{
    writeOneByte(0xF0, 0);    
    writeOneByte(0x46, 0x00); 
    writeOneByte(0x47, 0x00); 

    // Clear memory banks
    for (int y = 0; y < 6; y++) {
        writeOneByte(0xF0, (uint8_t)y); // Select the bank
        for (int z = 0; z < 16; z++) {
            uint8_t bank[16] = {0};       // Initialize bank with zeros
            writeBytes(z * 16, bank, 16); // Write zeros to the bank
        }
    }
}

static void LoadDefault()
{
    loadDefaultUserOptions();

    rto->autoBestHtotalEnabled = true; 
    rto->syncLockFailIgnore = 16;      
    rto->forceRetime = false;          
    rto->syncWatcherEnabled = true;    
    rto->phaseADC = 16;                
    rto->phaseSP = 16;                 
    rto->failRetryAttempts = 0;        
    rto->presetID = 0;                 
    rto->isCustomPreset = false;       
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false; 
    rto->deinterlaceAutoEnabled = true;           
    rto->scanlinesEnabled = false;                
    rto->boardHasPower = true;                    
    rto->presetIsPalForce60 = false;              
    rto->syncTypeCsync = false;                   
    rto->isValidForScalingRGBHV = false;          
    rto->medResLineCount = 0x33;                  
    rto->osr = 0;                                 
    rto->useHdmiSyncFix = 0;                      
    rto->notRecognizedCounter = 0;                

    rto->videoStandardInput = 0;    
    rto->outModeHdBypass = false;   
    rto->videoIsFrozen = true;      
    rto->sourceDisconnected = true; 
    // rto->isInLowPowerMode = false;
    rto->applyPresetDoneStage = 0; //
    // rto->presetVlineShift = 0;    
    rto->clampPositionIsSet = 0;     
    rto->coastPositionIsSet = 0;     
    rto->continousStableCounter = 0; 
    rto->currentLevelSOG = 5;        
    rto->thisSourceMaxLevelSOG = 31; 
}

void web_service(uint8_t inputStage, uint8_t segmentCurrent, uint8_t registerCurrent, uint8_t readout, uint8_t inputToogleBit);
void UpDisplay(void);
void printBinary(unsigned char num);
void turnOffWiFi();
void turnOffWiFi()
{
    WiFi.mode(WIFI_OFF); 
    Serial.println("WiFi has been turned off.");
}
void turnOnWiFi();
void turnOnWiFi()
{
    WiFi.mode(WIFI_AP);                  // Setting WiFi Mode to Access Point Mode
    WiFi.softAP(full_ssid, ap_password); 
    Serial.println("WiFi has been turned on.");
    Serial.print("Access Point IP address: ");
    Serial.println(WiFi.softAPIP()); // Print the IP address of the access point
}

void printBinary(unsigned char num)
{

    for (int i = sizeof(num) * 8 - 1; i >= 0; i--) {

        putchar((num & (1U << i)) ? '1' : '0');
    }
    putchar('\n'); 
}
void UpDisplay(void)
{
    uint8_t videoMode = getVideoMode();
    if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read()) {
        videoMode = rto->videoStandardInput;
    }
    if (uopt->presetPreference != 2) 
    {
        rto->useHdmiSyncFix = 1;
        if (rto->videoStandardInput == 14) {
            rto->videoStandardInput = 15;
        } else {
            applyPresets(videoMode);
        }
    } else {
        // printf("out off-1\n");
        setOutModeHdBypass(false); //    false
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
}

void Mode_Option(void);
void Mode_Option(void)
{
    // 带调试信息的参数校验
    static char max_index = sizeof(modes) / sizeof(modes[0]) - 1;
    if (SvModeOptionChanged) {
        SvModeOptionChanged = 0;


        if (SvModeOption >= 0 && SvModeOption <= max_index) {
            Send_TvMode(modes[SvModeOption]);
        } else {
            Send_TvMode(modes[1]); // 强制回退到 Auto 模式
        }
    }
    if (AvModeOptionChanged) {
        AvModeOptionChanged = 0;

        if (AvModeOption >= 0 && AvModeOption <= max_index) {
            Send_TvMode(modes[AvModeOption]);
        } else {
            Send_TvMode(modes[1]);
        }
    }
    if (SettingLineOptionChanged) {
        SettingLineOptionChanged = 0;
        Send_Line(LineOption);
    }
    if (SettingSmoothOptionChanged) {
        SettingSmoothOptionChanged = 0;
        Send_Smooth(SmoothOption);
    }
}

boolean CheckInputFrequency(void);
boolean CheckInputFrequency()
{
    unsigned char freq = 0;
    static unsigned char freq_last;
    freq = getOutputFrameRate();
    if ((abs(freq_last - freq) < 9) || (freq_last == 0)) {
        freq_last = freq;
        return 0;
    }

    Serial.print("freq");
    Serial.printf("%d\n", freq);
    freq_last = freq;
    return 1;
}

void OSD_DISPLAY(const int T, const char C);
void OSD_DISPLAY(const int T, const char C)
{
    __(T, (C * 2) + 1);
}

void ChangeSvModeOption(uint8_t num);
void ChangeSvModeOption(uint8_t num)
{
    SvModeOption = num;
    saveUserPrefs();
}
void ChangeAvModeOption(uint8_t num);
void ChangeAvModeOption(uint8_t num)
{
    AvModeOption = num;
    saveUserPrefs();
}
void Osd_Display(uint8_t start, const char str[]);
void Osd_Display(uint8_t start, const char str[])
{
    static uint8_t start_last = 0;
    if (str == NULL) {
        return;
    }
    if (start == 0XFF)
        start = start_last;
    else
        start_last = start;

    for (uint8_t count = 0; str[count] != '\0'; count++) {
        start_last = count + start + 1;

        if (str[count] == ' ') //
            continue;
        else if (str[count] == '=')
            OSD_DISPLAY(0x3D, count + start);
        else if (str[count] == '.')
            OSD_DISPLAY(0x2E, count + start);
        else if (str[count] == '\'')
            OSD_DISPLAY(0x27, count + start);
        else if (str[count] == '-')
            OSD_DISPLAY(0x3E, count + start);
        else if (str[count] == '/')
            OSD_DISPLAY(0x2F, count + start);
        else if (str[count] == ':')
            OSD_DISPLAY(0x3A, count + start);
        else
            OSD_DISPLAY(str[count], count + start);
    }
}
typedef void (*OSD_cx_Ptr)(volatile int, volatile int, volatile int);
OSD_cx_Ptr osd_cx_ptr;
void OSD_writeString(int startPos, int row, const char *str);
void OSD_writeString(int startPos, int row, const char *str)
{
    int pos = startPos;
    while (*str != '\0') {

        if (row == 1)
            osd_cx_ptr = OSD_c1;
        else if (row == 2)
            osd_cx_ptr = OSD_c2;
        else if (row == 3)
            osd_cx_ptr = OSD_c3;

        if (*str == ' ')
            osd_cx_ptr(*str, 1 + pos * 2, blue_fill);
        else if (*str == '=')
            osd_cx_ptr(0x3D, 1 + pos * 2, main0);
        else if (*str == '.')
            osd_cx_ptr(0x2E, 1 + pos * 2, main0);
        else if (*str == '\'')
            osd_cx_ptr(0x27, 1 + pos * 2, main0);
        else if (*str == '-')
            osd_cx_ptr(0x3E, 1 + pos * 2, main0);
        else if (*str == '/')
            osd_cx_ptr(0x2F, 1 + pos * 2, main0);
        else if (*str == ':')
            osd_cx_ptr(0x3A, 1 + pos * 2, main0);
        else
            osd_cx_ptr(*str, 1 + pos * 2, main0);

        pos++;
        str++;
    }
}



uint8_t getMovingAverage(uint8_t item);
uint8_t getMovingAverage(uint8_t item)
{
    static const uint8_t sz = 16;
    static uint8_t arr[sz] = {0};
    static uint8_t pos = 0;

    arr[pos] = item;
    if (pos < (sz - 1)) {
        pos++;
    } else {
        pos = 0;
    }

    uint16_t sum = 0;
    for (uint8_t i = 0; i < sz; i++) {
        sum += arr[i];
    }

    return sum >> 4;
}

class SerialMirror : public Stream
{
    size_t write(const uint8_t *data, size_t size)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(data, size);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data, size);
        return size;
    }

    size_t write(const char *data, size_t size)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(data, size);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data, size);
        return size;
    }

    size_t write(uint8_t data)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(&data, 1);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data);
        return 1;
    }

    size_t write(char data)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(&data, 1);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data);
        return 1;
    }

    int available()
    {
        return 0;
    }
    int read()
    {
        return -1;
    }
    int peek()
    {
        return -1;
    }
    // void flush()
    // {}
};

SerialMirror SerialM;

#include "framesync.h"

struct FrameSyncAttrs 
{
    static const uint8_t debugInPin = DEBUG_IN_PIN;
    static const uint32_t lockInterval = 100 * 16.70;
    static const int16_t syncCorrection = 2;
    static const int32_t syncTargetPhase = 90;
};
typedef FrameSyncManager<GBS, FrameSyncAttrs> FrameSync;

void externalClockGenResetClock()
{
    if (!rto->extClockGenDetected) {
        return;
    }
    fsDebugPrintf("externalClockGenResetClock()\n");

    uint8_t activeDisplayClock = GBS::PLL648_CONTROL_01::read();

    if (activeDisplayClock == 0x25)
        rto->freqExtClockGen = 40500000;
    else if (activeDisplayClock == 0x45)
        rto->freqExtClockGen = 54000000;
    else if (activeDisplayClock == 0x55)
        rto->freqExtClockGen = 64800000;
    else if (activeDisplayClock == 0x65)
        rto->freqExtClockGen = 81000000;
    else if (activeDisplayClock == 0x85)
        rto->freqExtClockGen = 108000000;
    else if (activeDisplayClock == 0x95)
        rto->freqExtClockGen = 129600000;
    else if (activeDisplayClock == 0xa5)
        rto->freqExtClockGen = 162000000;
    else if (activeDisplayClock == 0x35)
        rto->freqExtClockGen = 81000000;
    else if (activeDisplayClock == 0)
        rto->freqExtClockGen = 81000000;
    // else if (!rto->outModeHdBypass)
    // {
    // }

    if (rto->freqExtClockGen == 108000000) {
        Si.setFreq(0, 87000000);
        delay(1);
    }

    if (rto->freqExtClockGen == 40500000) {
        Si.setFreq(0, 48500000);
        delay(1);
    }

    Si.setFreq(0, rto->freqExtClockGen);
    GBS::PAD_CKIN_ENZ::write(0);
    FrameSync::clearFrequency();
}

void externalClockGenSyncInOutRate()
{
    fsDebugPrintf("externalClockGenSyncInOutRate()\n");

    if (!rto->extClockGenDetected) {
        return;
    }
    if (GBS::PAD_CKIN_ENZ::read() != 0) {
        return;
    }
    if (rto->outModeHdBypass) {
        return;
    }
    if (GBS::PLL648_CONTROL_01::read() != 0x75) {
        return;
    }

    float sfr = getSourceFieldRate(0);
    if (sfr < 47.0f || sfr > 86.0f) {
        return;
    }

    float ofr = getOutputFrameRate();
    if (ofr < 47.0f || ofr > 86.0f) {
        return;
    }

    uint32_t old = rto->freqExtClockGen;
    FrameSync::initFrequency(ofr, old);

    setExternalClockGenFrequencySmooth((sfr / ofr) * rto->freqExtClockGen);

    int32_t diff = rto->freqExtClockGen - old;

    // F("source Hz: ");
    // ;//SerialMprint(F("source Hz: "));
    // ;//SerialMprint(sfr, 5);
    // ;//SerialMprint(F(" new out: "));
    // ;//SerialMprint(getOutputFrameRate(), 5);
    // ;//SerialMprint(F(" clock: "));
    // ;//SerialMprint(rto->freqExtClockGen);
    // ;//SerialMprint(F(" ("));
    // ;//SerialMprint(diff >= 0 ? "+" : "");
    // ;//SerialMprint(diff);
    // ;//SerialMprintln(F(")"));
    delay(1);
}

void externalClockGenDetectAndInitialize()
{
    const uint8_t xtal_cl = 0xD2;

    rto->freqExtClockGen = 81000000;
    rto->extClockGenDetected = 0;

    if (uopt->disableExternalClockGenerator) {
        return;
    }

    uint8_t retVal = 0;
    Wire.beginTransmission(SIADDR);
    retVal = Wire.endTransmission();

    if (retVal != 0) {
        return;
    }

    Wire.beginTransmission(SIADDR);
    Wire.write(0);
    Wire.endTransmission();
    size_t bytes_read = Wire.requestFrom((uint8_t)SIADDR, (size_t)1, false);

    if (bytes_read == 1) {
        retVal = Wire.read();
        if ((retVal & 0x80) == 0) {
            rto->extClockGenDetected = 1;
        } else {
            return;
        }
    } else {
        return;
    }

    Si.init(25000000L);
    Wire.beginTransmission(SIADDR);
    Wire.write(183);
    Wire.write(xtal_cl);
    Wire.endTransmission();
    Si.setPower(0, SIOUT_6mA);
    Si.setFreq(0, rto->freqExtClockGen);
    Si.disable(0);
}

static inline void writeOneByte(uint8_t slaveRegister, uint8_t value)
{
    writeBytes(slaveRegister, &value, 1);
}

static inline void writeBytes(uint8_t slaveRegister, uint8_t *values, uint8_t numValues)
{
    if (slaveRegister == 0xF0 && numValues == 1) {
        lastSegment = *values;
    } else
        GBS::write(lastSegment, slaveRegister, values, numValues);
}

void copyBank(uint8_t *bank, const uint8_t *programArray, uint16_t *index)
{
    for (uint8_t x = 0; x < 16; ++x) {
        bank[x] = pgm_read_byte(programArray + *index);
        (*index)++;
    }
}

boolean videoStandardInputIsPalNtscSd()
{
    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
        return true;
    }

    return false;
}

void zeroAll()
{
    writeOneByte(0xF0, 0);
    writeOneByte(0x46, 0x00);
    writeOneByte(0x47, 0x00);

    for (int y = 0; y < 6; y++) {
        writeOneByte(0xF0, (uint8_t)y);
        for (int z = 0; z < 16; z++) {
            uint8_t bank[16];
            for (int w = 0; w < 16; w++) {
                bank[w] = 0;
            }
            writeBytes(z * 16, bank, 16);
        }
    }
}

void loadHdBypassSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 1);
    for (int j = 3; j <= 5; j++) {
        copyBank(bank, presetHdBypassSection, &index);
        writeBytes(j * 16, bank, 16);
    }
}

void loadPresetDeinterlacerSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 2);
    for (int j = 0; j <= 3; j++) {
        copyBank(bank, presetDeinterlacerSection, &index);
        writeBytes(j * 16, bank, 16);
    }
}

void loadPresetMdSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 1);
    for (int j = 6; j <= 7; j++) {
        copyBank(bank, presetMdSection, &index);
        writeBytes(j * 16, bank, 16);
    }
    bank[0] = pgm_read_byte(presetMdSection + index);
    bank[1] = pgm_read_byte(presetMdSection + index + 1);
    bank[2] = pgm_read_byte(presetMdSection + index + 2);
    bank[3] = pgm_read_byte(presetMdSection + index + 3);
    writeBytes(8 * 16, bank, 4);
}

void writeProgramArrayNew(const uint8_t *programArray, boolean skipMDSection)
{
  uint16_t index = 0;
  uint8_t bank[16];
  uint8_t y = 0;
  uint16_t dump_count = 0;
  FrameSync::cleanup();

  if (rto->videoStandardInput == 15)
  {
    rto->videoStandardInput = 0;
  }

  rto->outModeHdBypass = 0; // Output HD Bypass
  if (GBS::ADC_INPUT_SEL::read() == 0)
  {
    rto->inputIsYpBpR = 1;
  }
  else
  {
    rto->inputIsYpBpR = 0;
  }

  uint8_t reset46 = GBS::RESET_CONTROL_0x46::read();
  uint8_t reset47 = GBS::RESET_CONTROL_0x47::read();

  for (; y < 6; y++)
  {
    writeOneByte(0xF0, (uint8_t)y);
    switch (y)
    {
    case 0:
      for (int j = 0; j <= 1; j++)
      {
        for (int x = 0; x <= 15; x++)
        {
          if (j == 0 && x == 4)
          {
            if (rto->useHdmiSyncFix)
            {
              bank[x] = pgm_read_byte(programArray + index) & ~(1 << 0);
            }
            else
            {
              bank[x] = pgm_read_byte(programArray + index);
            }
          }
          else if (j == 0 && x == 6)
          {
            bank[x] = reset46;
          }
          else if (j == 0 && x == 7)
          {
            bank[x] = reset47;
          }
          else if (j == 0 && x == 9)
          {
            if (rto->useHdmiSyncFix)
            {
              bank[x] = pgm_read_byte(programArray + index) | (1 << 2);
            }
            else
            {
              bank[x] = pgm_read_byte(programArray + index);
            }
          }
          else
          {
            bank[x] = pgm_read_byte(programArray + index);
          }
          index++;
        }
        writeBytes(0x40 + (j * 16), bank, 16);
      }
      copyBank(bank, programArray, &index);
      writeBytes(0x90, bank, 16);
      break;
    case 1:
      for (int j = 0; j <= 2; j++)
      {
        copyBank(bank, programArray, &index);
        if (j == 0)
        {
          bank[0] = bank[0] & ~(1 << 5);
          bank[1] = bank[1] | (1 << 0);
          bank[12] = bank[12] & 0x0f;
          bank[13] = 0;
        }
        writeBytes(j * 16, bank, 16);
      }
      if (!skipMDSection)
      {
        loadPresetMdSection();
        if (rto->syncTypeCsync)
          GBS::MD_SEL_VGA60::write(0);
        else
          GBS::MD_SEL_VGA60::write(1);

        GBS::MD_HD1250P_CNTRL::write(rto->medResLineCount);
      }
      break;
    case 2:
      loadPresetDeinterlacerSection();
      break;
    case 3:
      for (int j = 0; j <= 7; j++)
      {
        copyBank(bank, programArray, &index);

        writeBytes(j * 16, bank, 16);
      }

      for (int x = 0; x <= 15; x++)
      {
        writeOneByte(0x80 + x, 0x00);
      }
      break;
    case 4:
      for (int j = 0; j <= 5; j++)
      {
        copyBank(bank, programArray, &index);
        writeBytes(j * 16, bank, 16);
      }
      break;
    case 5:
      for (int j = 0; j <= 6; j++)
      {
        for (int x = 0; x <= 15; x++)
        {
          bank[x] = pgm_read_byte(programArray + index);
          if (index == 322)
          {
            if (rto->inputIsYpBpR && Info_sate == 0) //&& SeleInputSource == S_YUV)
              bitClear(bank[x], 6);
            else if (rto->inputIsYpBpR == false && Info_sate == 0) //&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs))
              bitSet(bank[x], 6);
          }

          if (index == 323)
          {
            if (rto->inputIsYpBpR && Info_sate == 0) //&& SeleInputSource == S_YUV)
            {
              bitSet(bank[x], 1);
              bitClear(bank[x], 2);
              bitSet(bank[x], 3);
            }
            else if (rto->inputIsYpBpR == false && Info_sate == 0) //&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs))
            {
              bitClear(bank[x], 1);
              bitClear(bank[x], 2);
              bitClear(bank[x], 3);
            }
          }

          if (index == 352)
          {
            bank[x] = 0x02;
          }
          if (index == 375)
          {
            if (videoStandardInputIsPalNtscSd())
            {
              bank[x] = 0x6b;
            }
            else
            {
              bank[x] = 0x02;
            }
          }
          if (index == 382)
          {
            bitSet(bank[x], 5);
          }
          if (index == 407)
          {
            bitSet(bank[x], 0);
          }
          index++;
        }
        writeBytes(j * 16, bank, 16);
      }
      break;
    }
  }

  if (uopt->preferScalingRgbhv && rto->isValidForScalingRGBHV)
  {
    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
    rto->videoStandardInput = 3;
  }
}

void activeFrameTimeLockInitialSteps()
{
    if (rto->extClockGenDetected) {
        return;
    }

    if (rto->presetID != PresetHdBypass && rto->presetID != PresetBypassRGBHV) {
        if (uopt->frameTimeLockMethod == 0) {
        }
        if (uopt->frameTimeLockMethod == 1) {
        }
        if (GBS::VDS_VS_ST::read() == 0) {
            ;
        }
    }
}

void resetInterruptSogSwitchBit()
{
    GBS::INT_CONTROL_RST_SOGSWITCH::write(1);
    GBS::INT_CONTROL_RST_SOGSWITCH::write(0);
}

void resetInterruptSogBadBit()
{
    GBS::INT_CONTROL_RST_SOGBAD::write(1);
    GBS::INT_CONTROL_RST_SOGBAD::write(0);
}

void resetInterruptNoHsyncBadBit()
{
    GBS::INT_CONTROL_RST_NOHSYNC::write(1);
    GBS::INT_CONTROL_RST_NOHSYNC::write(0);
}

void setResetParameters_re() 
{
    rto->videoStandardInput = 0;   
    rto->videoIsFrozen = false;    
    rto->applyPresetDoneStage = 0; 
    rto->presetVlineShift = 0;     
    // rto->sourceDisconnected = true;  
    rto->outModeHdBypass = 0;        
    rto->clampPositionIsSet = 0;     
    rto->coastPositionIsSet = 0;     
    rto->phaseIsSet = 0;             
    rto->continousStableCounter = 0; 
    rto->noSyncCounter = 0;          

    rto->isInLowPowerMode = false;   
    rto->currentLevelSOG = 5;        
    rto->thisSourceMaxLevelSOG = 31; 
    rto->failRetryAttempts = 0;      
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false; 
    rto->scanlinesEnabled = false;                
    rto->syncTypeCsync = false;                   
    rto->isValidForScalingRGBHV = false;          
    rto->medResLineCount = 0x33;
    rto->osr = 0;                  
    rto->useHdmiSyncFix = 0;       
    rto->notRecognizedCounter = 0; 
}

void setResetParameters()
{
    rto->videoStandardInput = 0;
    rto->videoIsFrozen = false; 
    rto->applyPresetDoneStage = 0;
    rto->presetVlineShift = 0;
    rto->sourceDisconnected = true; 
    rto->outModeHdBypass = 0;       
    rto->clampPositionIsSet = 0;    
    rto->coastPositionIsSet = 0;    
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    rto->noSyncCounter = 0;         

    rto->isInLowPowerMode = false;  
    rto->currentLevelSOG = 5;       
    rto->thisSourceMaxLevelSOG = 31;
    rto->failRetryAttempts = 0;     
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false; 
    rto->scanlinesEnabled = false;                
    rto->syncTypeCsync = false;                   
    rto->isValidForScalingRGBHV = false;          
    rto->medResLineCount = 0x33;
    rto->osr = 0;                  
    rto->useHdmiSyncFix = 0;       
    rto->notRecognizedCounter = 0; 

    adco->r_gain = 0;
    adco->g_gain = 0;
    adco->b_gain = 0;

    GBS::ADC_UNUSED_64::write(0);
    GBS::ADC_UNUSED_65::write(0);
    GBS::ADC_UNUSED_66::write(0);
    GBS::ADC_UNUSED_67::write(0);
    GBS::GBS_PRESET_CUSTOM::write(0);
    GBS::GBS_PRESET_ID::write(0);
    GBS::GBS_OPTION_SCALING_RGBHV::write(0);
    GBS::GBS_OPTION_PALFORCED60_ENABLED::write(0);

    GBS::IF_VS_SEL::write(1);
    GBS::IF_VS_FLIP::write(1);
    GBS::IF_HSYNC_RST::write(0x3FF);
    GBS::IF_VB_ST::write(0);
    GBS::IF_VB_SP::write(2);

    FrameSync::cleanup();

    GBS::OUT_SYNC_CNTRL::write(0);
    GBS::DAC_RGBS_PWDNZ::write(0);
    GBS::ADC_TA_05_CTRL::write(0x02); // ADC test enable BIT0    ADC test bus control bit   BIT4:1
    GBS::ADC_TEST_04::write(0x02);    // 1:0 REF test resistance selection 4:2REF test current selection
    GBS::ADC_TEST_0C::write(0x12);
    GBS::ADC_CLK_PA::write(0);
    GBS::ADC_SOGEN::write(1); 
    GBS::SP_SOG_MODE::write(1);
    GBS::ADC_INPUT_SEL::write(1);
    GBS::ADC_POWDZ::write(1);
    setAndUpdateSogLevel(rto->currentLevelSOG);
    GBS::RESET_CONTROL_0x46::write(0x00);
    GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::GPIO_CONTROL_00::write(0x67);
    GBS::GPIO_CONTROL_01::write(0x00);
    GBS::DAC_RGBS_PWDNZ::write(0);
    GBS::PLL648_CONTROL_01::write(0x00);
    GBS::PAD_CKIN_ENZ::write(0);
    GBS::PAD_CKOUT_ENZ::write(1);
    GBS::IF_SEL_ADC_SYNC::write(1);
    GBS::PLLAD_VCORST::write(1);
    GBS::PLL_ADS::write(1);
    GBS::PLL_CKIS::write(0);
    GBS::PLL_MS::write(2);
    GBS::PAD_CONTROL_00_0x48::write(0x2b);
    GBS::PAD_CONTROL_01_0x49::write(0x1f); 
    loadHdBypassSection();
    loadPresetMdSection();
    setAdcParametersGainAndOffset();
    GBS::SP_PRE_COAST::write(9);
    GBS::SP_POST_COAST::write(18);  
    GBS::SP_NO_COAST_REG::write(0); 
    GBS::SP_CS_CLP_ST::write(32);   
    GBS::SP_CS_CLP_SP::write(48);   
    GBS::SP_SOG_SRC_SEL::write(0);  
    GBS::SP_EXT_SYNC_SEL::write(0); 
    GBS::SP_NO_CLAMP_REG::write(1); 
    GBS::PLLAD_ICP::write(0);       
    GBS::PLLAD_FS::write(0);        
    GBS::PLLAD_5_16::write(0x1f);
    GBS::PLLAD_MD::write(0x700);
    resetPLL();
    delay(2);
    resetPLLAD();
    GBS::PLL_VCORST::write(1);
    GBS::PLLAD_CONTROL_00_5x11::write(0x01);
    resetDebugPort();

    GBS::RESET_CONTROL_0x46::write(0x41);
    GBS::RESET_CONTROL_0x47::write(0x17);
    GBS::INTERRUPT_CONTROL_01::write(0xff);
    GBS::INTERRUPT_CONTROL_00::write(0xff);
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    GBS::PAD_SYNC_OUT_ENZ::write(0); 
    rto->clampPositionIsSet = 0;     
    rto->coastPositionIsSet = 0;     
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0; 
    serialCommand = '@';
    userCommand = '@';
}

// void OutputComponentOrVGA()
// {

//   boolean isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
//   if (uopt->wantOutputComponent)
//   {
//     GBS::VDS_SYNC_LEV::write(0x80);
//     GBS::VDS_CONVT_BYPS::write(1);
//     GBS::OUT_SYNC_CNTRL::write(0);
//   }
//   else
//   {
//     GBS::VDS_SYNC_LEV::write(0);
//     GBS::VDS_CONVT_BYPS::write(0);
//     GBS::OUT_SYNC_CNTRL::write(1); 
//   }

//   if (!isCustomPreset)
//   {
//     if (rto->inputIsYpBpR && SeleInputSource == S_YUV && Info_sate == 0 )
//     {
//       applyYuvPatches();
//     }
//     else if (rto->inputIsYpBpR == false && (SeleInputSource == S_VGA || SeleInputSource == S_RGBs) && Info_sate == 0 )
//     {
//       applyRGBPatches();
//     }
//   }
// }

void applyComponentColorMixing()
{
    GBS::VDS_Y_GAIN::write(0x64);
    GBS::VDS_UCOS_GAIN::write(0x19);
    GBS::VDS_VCOS_GAIN::write(0x19);
    GBS::VDS_Y_OFST::write(0xfe);
    GBS::VDS_U_OFST::write(0x01);
}

void toggleIfAutoOffset()
{
    if (GBS::IF_AUTO_OFST_EN::read() == 0) {

        GBS::ADC_ROFCTRL::write(0x40);
        GBS::ADC_GOFCTRL::write(0x42);
        GBS::ADC_BOFCTRL::write(0x40);

        GBS::IF_AUTO_OFST_EN::write(1);
        GBS::IF_AUTO_OFST_PRD::write(0);
    } else {
        if (adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
            GBS::ADC_ROFCTRL::write(adco->r_off);
            GBS::ADC_GOFCTRL::write(adco->g_off);
            GBS::ADC_BOFCTRL::write(adco->b_off);
        }

        GBS::IF_AUTO_OFST_EN::write(0);
        GBS::IF_AUTO_OFST_PRD::write(0);
    }
}

void applyYuvPatches()
{
    GBS::ADC_RYSEL_R::write(1);
    GBS::ADC_RYSEL_B::write(1);
    GBS::ADC_RYSEL_G::write(0);
    GBS::DEC_MATRIX_BYPS::write(1); 
    GBS::IF_MATRIX_BYPS::write(1);

    if (GBS::GBS_PRESET_CUSTOM::read() == 0) {

        GBS::VDS_Y_GAIN::write(0x80);
        GBS::VDS_UCOS_GAIN::write(0x1c);
        GBS::VDS_VCOS_GAIN::write(0x29);
        if (BriorCon == 1) {
            GBS::VDS_Y_OFST::write(0x0E);

            GBS::ADC_RGCTRL::write(0x33);
            GBS::ADC_GGCTRL::write(0x33);
            GBS::ADC_BGCTRL::write(0x33);
        } else if (BriorCon == 2) {
            // GBS::VDS_Y_OFST::write(-0x22);
            // GBS::ADC_RGCTRL::write(0x56);
            // GBS::ADC_GGCTRL::write(0x56);
            // GBS::ADC_BGCTRL::write(0x56);

            GBS::VDS_Y_OFST::write(0x0E);
            GBS::ADC_RGCTRL::write(0x33);
            GBS::ADC_GGCTRL::write(0x33);
            GBS::ADC_BGCTRL::write(0x33);
        }
        GBS::VDS_U_OFST::write(0x03);
        GBS::VDS_V_OFST::write(0x03);
        if (rto->videoStandardInput >= 5 && rto->videoStandardInput <= 7) {
        }
    }
    if (uopt->wantOutputComponent) {
        applyComponentColorMixing();
    }

    R_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.402 * (signed char)((signed char)GBS::VDS_V_OFST::read())))) + 128;
    G_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) - (float)(0.344136 * (signed char)((signed char)GBS::VDS_U_OFST::read())) - 0.714136 * (signed char)((signed char)GBS::VDS_V_OFST::read()))) + 128;
    B_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.772 * (signed char)((signed char)GBS::VDS_U_OFST::read())))) + 128;
}

void applyRGBPatches()
{
    GBS::ADC_RYSEL_R::write(0);
    GBS::ADC_RYSEL_B::write(0);
    GBS::ADC_RYSEL_G::write(0);
    GBS::DEC_MATRIX_BYPS::write(0);
    GBS::IF_MATRIX_BYPS::write(1);

    if (GBS::GBS_PRESET_CUSTOM::read() == 0) {
        GBS::VDS_Y_GAIN::write(0x80);
        GBS::VDS_UCOS_GAIN::write(0x1c);
        GBS::VDS_VCOS_GAIN::write(0x29);
        GBS::VDS_Y_OFST::write(0x00);
        GBS::VDS_U_OFST::write(0x00);
        GBS::VDS_V_OFST::write(0x00);
    }

    if (uopt->wantOutputComponent) {
        applyComponentColorMixing();
    }

    R_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.402 * (signed char)((signed char)GBS::VDS_V_OFST::read())))) + 128;
    G_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) - (float)(0.344136 * (signed char)((signed char)GBS::VDS_U_OFST::read())) - 0.714136 * (signed char)((signed char)GBS::VDS_V_OFST::read()))) + 128;
    B_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.772 * (signed char)((signed char)GBS::VDS_U_OFST::read())))) + 128;
}

void setAdcGain(uint8_t gain)
{
    GBS::ADC_RGCTRL::write(gain);
    GBS::ADC_GGCTRL::write(gain);
    GBS::ADC_BGCTRL::write(gain);
    adco->r_gain = gain;
    adco->g_gain = gain;
    adco->b_gain = gain;
}

void setAdcParametersGainAndOffset()
{
    GBS::ADC_ROFCTRL::write(0x40);
    GBS::ADC_GOFCTRL::write(0x40);
    GBS::ADC_BOFCTRL::write(0x40);
    GBS::ADC_RGCTRL::write(0x7B);
    GBS::ADC_GGCTRL::write(0x7B);
    GBS::ADC_BGCTRL::write(0x7B);
}

void updateHVSyncEdge()
{
    static uint8_t printHS = 0, printVS = 0;
    uint16_t temp = 0;

    if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
        resetInterruptSogBadBit();
        return;
    }

    uint8_t syncStatus = GBS::STATUS_16::read();
    if (rto->syncTypeCsync) {
        if ((syncStatus & 0x02) != 0x02)
            return;
    } else {
        if ((syncStatus & 0x0a) != 0x0a)
            return;
    }

    if ((syncStatus & 0x02) != 0x02) {
    } else {
        if ((syncStatus & 0x01) == 0x00) {

            printHS = 1;

            temp = GBS::HD_HS_SP::read();
            if (GBS::HD_HS_ST::read() < temp) {
                GBS::HD_HS_SP::write(GBS::HD_HS_ST::read());
                GBS::HD_HS_ST::write(temp);
                GBS::SP_HS2PLL_INV_REG::write(1);
            }
        } else {
            printHS = 2;

            temp = GBS::HD_HS_SP::read();
            if (GBS::HD_HS_ST::read() > temp) {
                GBS::HD_HS_SP::write(GBS::HD_HS_ST::read());
                GBS::HD_HS_ST::write(temp);
                GBS::SP_HS2PLL_INV_REG::write(0);
            }
        }

        if (rto->syncTypeCsync == false) {
            if ((syncStatus & 0x08) != 0x08) {
                Serial.println(F("VS can't detect sync edge"));
            } else {
                if ((syncStatus & 0x04) == 0x00) {
                    printVS = 1;

                    temp = GBS::HD_VS_SP::read();
                    if (GBS::HD_VS_ST::read() < temp) {
                        GBS::HD_VS_SP::write(GBS::HD_VS_ST::read());
                        GBS::HD_VS_ST::write(temp);
                    }
                } else {
                    printVS = 2;

                    temp = GBS::HD_VS_SP::read();
                    if (GBS::HD_VS_ST::read() > temp) {
                        GBS::HD_VS_SP::write(GBS::HD_VS_ST::read());
                        GBS::HD_VS_ST::write(temp);
                    }
                }
            }
        }
    }
}

void prepareSyncProcessor() 
{
    writeOneByte(0xF0, 5);
    GBS::SP_SOG_P_ATO::write(0);   
    GBS::SP_JITTER_SYNC::write(0); // Use falling and rising edge to sync input Hsync

    writeOneByte(0x21, 0x18);
    writeOneByte(0x22, 0x0F);
    writeOneByte(0x23, 0x00);
    writeOneByte(0x24, 0x40);
    writeOneByte(0x25, 0x00);
    writeOneByte(0x26, 0x04);
    writeOneByte(0x27, 0x00);
    writeOneByte(0x2a, 0x0F);

    writeOneByte(0x2d, 0x03);
    writeOneByte(0x2e, 0x00);
    writeOneByte(0x2f, 0x02);
    writeOneByte(0x31, 0x2f);

    writeOneByte(0x33, 0x3a);
    writeOneByte(0x34, 0x06);

    if (rto->videoStandardInput == 0)
        GBS::SP_DLT_REG::write(0x70);
    else if (rto->videoStandardInput <= 4)
        GBS::SP_DLT_REG::write(0xC0);
    else if (rto->videoStandardInput <= 6)
        GBS::SP_DLT_REG::write(0xA0);
    else if (rto->videoStandardInput == 7)
        GBS::SP_DLT_REG::write(0x70);
    else
        GBS::SP_DLT_REG::write(0x70);

    if (videoStandardInputIsPalNtscSd()) {
        GBS::SP_H_PULSE_IGNOR::write(0x6b);
    } else {
        GBS::SP_H_PULSE_IGNOR::write(0x02);
    }

    GBS::SP_H_TOTAL_EQ_THD::write(3);

    GBS::SP_SDCS_VSST_REG_H::write(0);
    GBS::SP_SDCS_VSSP_REG_H::write(0);
    GBS::SP_SDCS_VSST_REG_L::write(4);
    GBS::SP_SDCS_VSSP_REG_L::write(1);

    GBS::SP_CS_HS_ST::write(0x10);
    GBS::SP_CS_HS_SP::write(0x00);

    writeOneByte(0x49, 0x00);
    writeOneByte(0x4a, 0x00); //
    writeOneByte(0x4b, 0x44);
    writeOneByte(0x4c, 0x00); //

    writeOneByte(0x51, 0x02);
    writeOneByte(0x52, 0x00);
    writeOneByte(0x53, 0x00);
    writeOneByte(0x54, 0x00);

    if (rto->videoStandardInput != 15 && (GBS::GBS_OPTION_SCALING_RGBHV::read() != 1)) {
        GBS::SP_CLAMP_MANUAL::write(0);
        GBS::SP_CLP_SRC_SEL::write(0);
        GBS::SP_NO_CLAMP_REG::write(1);
        GBS::SP_SOG_MODE::write(1);
        GBS::SP_H_CST_ST::write(0x10);
        GBS::SP_H_CST_SP::write(0x100);
        GBS::SP_DIS_SUB_COAST::write(0);
        GBS::SP_H_PROTECT::write(1);
        GBS::SP_HCST_AUTO_EN::write(0);
        GBS::SP_NO_COAST_REG::write(0);
    }

    GBS::SP_HS_REG::write(1);
    GBS::SP_HS_PROC_INV_REG::write(0); 
    GBS::SP_VS_PROC_INV_REG::write(0); 

    writeOneByte(0x58, 0x05);
    writeOneByte(0x59, 0x00);
    writeOneByte(0x5a, 0x01);
    writeOneByte(0x5b, 0x00);
    writeOneByte(0x5c, 0x03);
    writeOneByte(0x5d, 0x02);
}

void setAndUpdateSogLevel(uint8_t level)
{
    rto->currentLevelSOG = level & 0x1f;
    GBS::ADC_SOGCTRL::write(level);
    setAndLatchPhaseSP();
    setAndLatchPhaseADC();
    latchPLLAD();
    GBS::INTERRUPT_CONTROL_00::write(0xff);
    GBS::INTERRUPT_CONTROL_00::write(0x00);
}
void goLowPowerWithInputDetection_re() 
{
    // GBS::OUT_SYNC_CNTRL::write(0);
    // GBS::DAC_RGBS_PWDNZ::write(0);
    setResetParameters_re();
    prepareSyncProcessor(); 
    delay(100);
    // rto->isInLowPowerMode = true;
}
void goLowPowerWithInputDetection() 
{
    GBS::OUT_SYNC_CNTRL::write(0);
    GBS::DAC_RGBS_PWDNZ::write(0);

    setResetParameters();
    prepareSyncProcessor(); 
    delay(100);
    rto->isInLowPowerMode = true;
}

boolean optimizePhaseSP() 
{
    uint16_t pixelClock = GBS::PLLAD_MD::read();
    uint8_t badHt = 0, prevBadHt = 0, worstBadHt = 0, worstPhaseSP = 0, prevPrevBadHt = 0, goodHt = 0;
    boolean runTest = 1;

    if (GBS::STATUS_SYNC_PROC_HTOTAL::read() < (pixelClock - 8)) {
        return 0;
    }
    if (GBS::STATUS_SYNC_PROC_HTOTAL::read() > (pixelClock + 8)) {
        return 0;
    }

    if (rto->currentLevelSOG <= 2) {

        rto->phaseSP = 16;
        rto->phaseADC = 16;
        if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
            if (rto->osr == 4) {
                rto->phaseADC += 16;
                rto->phaseADC &= 0x1f;
            }
        }
        delay(8);
        runTest = 0;
    }

    if (runTest) {

        for (uint8_t u = 0; u < 34; u++) {
            rto->phaseSP++;
            rto->phaseSP &= 0x1f;
            setAndLatchPhaseSP();
            badHt = 0;
            ESP.wdtFeed();
            delayMicroseconds(256);
            ESP.wdtFeed();
            for (uint8_t i = 0; i < 20; i++) {
                if (GBS::STATUS_SYNC_PROC_HTOTAL::read() != pixelClock) {
                    badHt++;
                    ESP.wdtFeed();
                    delayMicroseconds(384);
                }
            }

            if ((badHt + prevBadHt + prevPrevBadHt) > worstBadHt) {
                worstBadHt = (badHt + prevBadHt + prevPrevBadHt);
                worstPhaseSP = (rto->phaseSP - 1) & 0x1f;
            }

            if (badHt == 0) {

                goodHt++;
            }

            prevPrevBadHt = prevBadHt;
            prevBadHt = badHt;
        }

        if (goodHt < 17) {

            return 0;
        }

        if (worstBadHt != 0) {
            rto->phaseSP = (worstPhaseSP + 16) & 0x1f;

            rto->phaseADC = 16;

            if (rto->videoStandardInput >= 5 && rto->videoStandardInput <= 7) {
                if (rto->osr == 2) {

                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            } else if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                if (rto->osr == 4) {

                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            }
        } else {

            rto->phaseSP = 16;
            rto->phaseADC = 16;
            if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                if (rto->osr == 4) {
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            }
        }
    }

    setAndLatchPhaseSP();
    delay(1);
    setAndLatchPhaseADC();

    return 1;
}

void optimizeSogLevel() // Optimize SOG levels
{
    if (rto->boardHasPower == false) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        return;
    }
    if (rto->videoStandardInput == 15 || GBS::SP_SOG_MODE::read() != 1 || rto->syncTypeCsync == false) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        return;
    }

    if (rto->inputIsYpBpR && Info_sate == 0) //&& SeleInputSource == S_YUV )
    {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 14;
    } else if (rto->inputIsYpBpR == false && Info_sate == 0) //&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs) )

    {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
    }
    setAndUpdateSogLevel(rto->currentLevelSOG);

    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
        delay(1);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(0x0f);
        delay(1);
    }

    GBS::TEST_BUS_EN::write(1);

    delay(100);
    while (1) {
        uint16_t syncGoodCounter = 0;
        unsigned long timeout = millis();
        while ((millis() - timeout) < 60) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                syncGoodCounter++;
                if (syncGoodCounter >= 60) {
                    break;
                }
            } else if (syncGoodCounter >= 4) {
                syncGoodCounter -= 3;
            }
        }

        if (syncGoodCounter >= 60) {
            syncGoodCounter = 0;

            if (GBS::TEST_BUS_2F::read() > 0) {
                delay(20);
                for (int a = 0; a < 50; a++) {
                    syncGoodCounter++;
                    if (GBS::STATUS_SYNC_PROC_HSACT::read() == 0 || GBS::TEST_BUS_2F::read() == 0) {
                        syncGoodCounter = 0;
                        break;
                    }
                }
                if (syncGoodCounter >= 49) {
                    break;
                }
            }
        }

        if (rto->currentLevelSOG >= 2) {
            rto->currentLevelSOG -= 1;
            setAndUpdateSogLevel(rto->currentLevelSOG);
            delay(8);
        } else {
            rto->currentLevelSOG = 13;
            setAndUpdateSogLevel(rto->currentLevelSOG);
            delay(8);
            break;
        }
    }

    rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
    if (rto->thisSourceMaxLevelSOG == 0) {
        rto->thisSourceMaxLevelSOG = 1;
    }

    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
}

uint8_t detectAndSwitchToActiveInput() 
{                                      // if any
    uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
    // printf("currentInput = %d \n",currentInput);
    unsigned long timeout = millis();
    while (millis() - timeout < 450) {
        delay(10);
        handleWiFi(0);

        boolean stable = getStatus16SpHsStable();
        // printf("stable = %d \n",stable);
        if (stable) {
            currentInput = GBS::ADC_INPUT_SEL::read();

            if ((currentInput == 1 && Info_sate == 0) && (SeleInputSource == S_VGA || SeleInputSource == S_RGBs)) // 20240919
            {                                                                                                     // RGBS or RGBHV
                boolean vsyncActive = 0;
                rto->inputIsYpBpR = false; // declare for MD
                rto->currentLevelSOG = 13; //
                setAndUpdateSogLevel(rto->currentLevelSOG);

                unsigned long timeOutStart = millis();
                // vsync test
                while (!vsyncActive && ((millis() - timeOutStart) < 360)) {
                    vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
                    handleWiFi(0); // wifi stack
                    delay(1);
                }


                if ((vsyncActive && Info_sate == 0) && (SeleInputSource == S_VGA || SeleInputSource == S_RGBs)) // 20240919
                {
                    // SerialMprintln(F("VSync: present"));
                    GBS::MD_SEL_VGA60::write(1); 
                    boolean hsyncActive = 0;

                    timeOutStart = millis();
                    while (!hsyncActive && millis() - timeOutStart < 400) {
                        hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                        handleWiFi(0); // wifi stack
                        delay(1);
                    }

                    if (hsyncActive) {
                        ; // SerialMprint(F("HSync: present"));
                        GBS::SP_H_PROTECT::write(1);
                        delay(120);

                        short decodeSuccess = 0;
                        for (int i = 0; i < 3; i++) {
                            
                            rto->syncTypeCsync = 1; // temporary for test
                            float sfr = getSourceFieldRate(1);
                            rto->syncTypeCsync = 0; // undo
                            if (sfr > 40.0f)
                                decodeSuccess++; 
                        }

                        if (decodeSuccess >= 2) {
                            // SerialMprintln(F(" (with CSync)"));
                            GBS::SP_PRE_COAST::write(0x10); 
                            delay(40);
                            rto->syncTypeCsync = true;
                        } else {
                            // SerialMprintln();
                            rto->syncTypeCsync = false; 
                        }

                        for (uint8_t i = 0; i < 16; i++) {

                            uint8_t innerVideoMode = getVideoMode();
                            if (innerVideoMode == 8) {
                                setAndUpdateSogLevel(rto->currentLevelSOG);
                                rto->medResLineCount = GBS::MD_HD1250P_CNTRL::read();
                                // SerialMprintln(F("med res"));

                                return 1;
                            }

                            GBS::MD_HD1250P_CNTRL::write(GBS::MD_HD1250P_CNTRL::read() + 1);

                            delay(30);
                        }

                        rto->videoStandardInput = 15;

                        applyPresets(rto->videoStandardInput);
                        delay(100);

                        return 3;
                    } else {
                        // 
                    }
                }

                if ((!vsyncActive && Info_sate == 0) && SeleInputSource == S_RGBs) // 20240919
                {
                    
                    rto->syncTypeCsync = true;
                    GBS::MD_SEL_VGA60::write(0); 
                    uint16_t testCycle = 0;
                    timeOutStart = millis();
                    while ((millis() - timeOutStart) < 6000) {
                        delay(2);
                        if (getVideoMode() > 0) {
                            if (getVideoMode() != 8) { 
                                return 1;
                            }
                        }
                        testCycle++;
                        
                        if ((testCycle % 150) == 0) {
                            if (rto->currentLevelSOG == 1) {
                                rto->currentLevelSOG = 2;
                            } else {
                                rto->currentLevelSOG += 2;
                            }
                            if (rto->currentLevelSOG >= 15) {
                                rto->currentLevelSOG = 1;
                            }
                            setAndUpdateSogLevel(rto->currentLevelSOG);
                        }

                        
                        if (getVideoMode() == 8) {
                            rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 13;
                            setAndUpdateSogLevel(rto->currentLevelSOG);
                            rto->medResLineCount = GBS::MD_HD1250P_CNTRL::read();
                            ; // SerialMprintln(F("med res"));
                            return 1;
                        }

                        uint8_t currentMedResLineCount = GBS::MD_HD1250P_CNTRL::read();
                        if (currentMedResLineCount < 0x3c) {
                            GBS::MD_HD1250P_CNTRL::write(currentMedResLineCount + 1);
                        } else {
                            GBS::MD_HD1250P_CNTRL::write(0x33);
                        }
                    }
                    return 1;
                }

                GBS::SP_SOG_MODE::write(1);
                resetSyncProcessor();
                resetModeDetect();
                delay(40);
            } else if (currentInput == 0 && Info_sate == 0) //&& SeleInputSource == S_YUV ) // 20240919
            {
                // printf("this 0 \n");
                uint16_t testCycle = 0;
                rto->inputIsYpBpR = true;
                GBS::MD_SEL_VGA60::write(0);

                unsigned long timeOutStart = millis();
                while ((millis() - timeOutStart) < 6000) {
                    delay(2);
                    if (getVideoMode() > 0) {
                        return 2;
                    }

                    testCycle++;
                    if ((testCycle % 180) == 0) {
                        if (rto->currentLevelSOG == 1) {
                            rto->currentLevelSOG = 2;
                        } else {
                            rto->currentLevelSOG += 2;
                        }
                        if (rto->currentLevelSOG >= 16) {
                            rto->currentLevelSOG = 1;
                        }
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
                    }
                }

                rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 14;
                setAndUpdateSogLevel(rto->currentLevelSOG);

                return 2;
            }

            ; // SerialMprintln(" lost..");
            rto->currentLevelSOG = 2;
            setAndUpdateSogLevel(rto->currentLevelSOG);
        }

        GBS::ADC_INPUT_SEL::write(!currentInput);
        delay(200);

        return 0;
    }

    return 0;
}

uint8_t inputAndSyncDetect() 
{
    uint8_t syncFound = detectAndSwitchToActiveInput();
    // printf(" syncFound = %d \n",syncFound);
    if (syncFound == 0) {
        if (!getSyncPresent()) 
        {
            if (rto->isInLowPowerMode == false) {
                rto->sourceDisconnected = true; 
                rto->videoStandardInput = 0;
                GBS::SP_SOG_MODE::write(1);
                goLowPowerWithInputDetection();
                rto->isInLowPowerMode = true;
            }
        }
        return 0;
    } else if (syncFound == 1 && Info_sate == 0) //&& SeleInputSource == S_RGBs)
    {
        rto->inputIsYpBpR = false;
        rto->sourceDisconnected = false;
        rto->isInLowPowerMode = false; 
        resetDebugPort();
        applyRGBPatches();
        if (Info == InfoRGBs || Info == InfoRGsB) {
            // printf("\n RGBS HdmiHoldDetection :0x%02x \n",rto->HdmiHoldDetection);
            rto->HdmiHoldDetection = false;
        }

        return 1;
    } else if (syncFound == 2 && Info_sate == 0) //&& SeleInputSource == S_YUV)
    {
        rto->isInLowPowerMode = false; 
        rto->inputIsYpBpR = true;
        rto->sourceDisconnected = false;
        resetDebugPort();
        applyYuvPatches();
        // GBS::VDS_CONVT_BYPS::write(0);
        // GBS::PIP_CONVT_BYPS::write(0);
        if (Info == InfoYUV || Info == InfoSV || Info == InfoAV) {
            // printf("\n YUV HdmiHoldDetection :0x%02x \n",rto->HdmiHoldDetection);
            rto->HdmiHoldDetection = false;
        }

        return 2;
    } else if (syncFound == 3 && Info_sate == 0) //&& SeleInputSource == S_VGA)
    {
        rto->isInLowPowerMode = false; 
        rto->inputIsYpBpR = false;
        rto->sourceDisconnected = false;
        rto->videoStandardInput = 15;
        resetDebugPort();

        if (Info == InfoVGA && rto->HdmiHoldDetection) {
            // printf("\n VGA HdmiHoldDetection :0x%02x \n",rto->HdmiHoldDetection);
            rto->HdmiHoldDetection = false;
        }
        return 3;
    }

    return 0;
}


uint8_t getSingleByteFromPreset(const uint8_t *programArray, unsigned int offset)
{
    return pgm_read_byte(programArray + offset);
}
// Read from register
static inline void readFromRegister(uint8_t reg, int bytesToRead, uint8_t *output)
{
    return GBS::read(lastSegment, reg, output, bytesToRead);
}

void printReg(uint8_t seg, uint8_t reg)
{
    uint8_t readout;
    readFromRegister(reg, 1, &readout);

    ; // SerialMprint("0x");
    ; // SerialMprint(readout, HEX);
    ; // SerialMprint(", // s");
    ; // SerialMprint(seg);
    ; // SerialMprint("_");
    ; // SerialMprintln(reg, HEX);
}

void dumpRegisters(byte segment)
{
    if (segment > 5)
        return;
    writeOneByte(0xF0, segment);

    switch (segment) {
        case 0:
            for (int x = 0x40; x <= 0x5F; x++) {
                printReg(0, x);
            }
            for (int x = 0x90; x <= 0x9F; x++) {
                printReg(0, x);
            }
            break;
        case 1:
            for (int x = 0x0; x <= 0x2F; x++) {
                printReg(1, x);
            }
            break;
        case 2:
            for (int x = 0x0; x <= 0x3F; x++) {
                printReg(2, x);
            }
            break;
        case 3:
            for (int x = 0x0; x <= 0x7F; x++) {
                printReg(3, x);
            }
            break;
        case 4:
            for (int x = 0x0; x <= 0x5F; x++) {
                printReg(4, x);
            }
            break;
        case 5:
            for (int x = 0x0; x <= 0x6F; x++) {
                printReg(5, x);
            }
            break;
    }
}

void resetPLLAD()
{
    GBS::PLLAD_VCORST::write(1);
    GBS::PLLAD_PDZ::write(1);
    latchPLLAD();
    GBS::PLLAD_VCORST::write(0);
    delay(1);
    latchPLLAD();
    rto->clampPositionIsSet = 0;     
    rto->continousStableCounter = 1; 
}

void latchPLLAD()
{
    GBS::PLLAD_LAT::write(0);
    ESP.wdtFeed();
    delayMicroseconds(128);
    GBS::PLLAD_LAT::write(1);
}

void resetPLL()
{
    GBS::PLL_VCORST::write(1);
    delay(1);
    GBS::PLL_VCORST::write(0);
    delay(1);
    rto->clampPositionIsSet = 0;     
    rto->continousStableCounter = 1; 
}

void ResetSDRAM()
{
    GBS::SDRAM_RESET_CONTROL::write(0x02);
    GBS::SDRAM_RESET_SIGNAL::write(1);
    GBS::SDRAM_RESET_SIGNAL::write(0);
    GBS::SDRAM_RESET_CONTROL::write(0x82);
}

void resetDigital()
{
    boolean keepBypassActive = 0;
    if (GBS::SFTRST_HDBYPS_RSTZ::read() == 1) {
        keepBypassActive = 1;
    }
    GBS::RESET_CONTROL_0x47::write(0x17);
    if (rto->outModeHdBypass) {
        GBS::RESET_CONTROL_0x46::write(0x00);
        GBS::RESET_CONTROL_0x47::write(0x1F);
        return;
    }
    GBS::RESET_CONTROL_0x46::write(0x41);
    if (keepBypassActive == 1) {
        GBS::RESET_CONTROL_0x47::write(0x1F);
    }
    GBS::RESET_CONTROL_0x46::write(0x7f);
}

void resetSyncProcessor()
{
    GBS::SFTRST_SYNC_RSTZ::write(0);
    ESP.wdtFeed();
    delayMicroseconds(10);
    GBS::SFTRST_SYNC_RSTZ::write(1);
}

void resetModeDetect()
{
    GBS::SFTRST_MODE_RSTZ::write(0);
    delay(1);
    GBS::SFTRST_MODE_RSTZ::write(1);
}

void shiftHorizontal(uint16_t amountToShift, bool subtracting)
{
    uint16_t hrst = GBS::VDS_HSYNC_RST::read();
    uint16_t hbst = GBS::VDS_HB_ST::read();
    uint16_t hbsp = GBS::VDS_HB_SP::read();

    if (subtracting) {
        if ((int16_t)hbst - amountToShift >= 0) {
            hbst -= amountToShift;
        } else {
            hbst = hrst - (amountToShift - hbst);
        }
        if ((int16_t)hbsp - amountToShift >= 0) {
            hbsp -= amountToShift;
        } else {
            hbsp = hrst - (amountToShift - hbsp);
        }
    } else {
        if ((int16_t)hbst + amountToShift <= hrst) {
            hbst += amountToShift;

            if (hbst > GBS::VDS_DIS_HB_ST::read()) {
                GBS::VDS_DIS_HB_ST::write(hbst);
            }
        } else {
            hbst = 0 + (amountToShift - (hrst - hbst));
        }
        if ((int16_t)hbsp + amountToShift <= hrst) {
            hbsp += amountToShift;
        } else {
            hbsp = 0 + (amountToShift - (hrst - hbsp));
        }
    }

    GBS::VDS_HB_ST::write(hbst);
    GBS::VDS_HB_SP::write(hbsp);
}

void shiftHorizontalLeft()
{
    shiftHorizontal(4, true);
}

void shiftHorizontalRight()
{
    shiftHorizontal(4, false);
}

void shiftHorizontalLeftIF(uint8_t amount)
{
    uint16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() + amount;
    uint16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() + amount;
    uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

    if (rto->videoStandardInput <= 2) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD / 2);
    } else if (rto->videoStandardInput <= 7) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD);
    }
    uint16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

    GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

    if (IF_HB_ST2 < IF_HSYNC_RST) {
        GBS::IF_HB_ST2::write(IF_HB_ST2);
    } else {
        GBS::IF_HB_ST2::write(IF_HB_ST2 - IF_HSYNC_RST);
    }

    if (IF_HB_SP2 < IF_HSYNC_RST) {
        GBS::IF_HB_SP2::write(IF_HB_SP2);
    } else {
        GBS::IF_HB_SP2::write((IF_HB_SP2 - IF_HSYNC_RST) + 1);
    }
}

void shiftHorizontalRightIF(uint8_t amount)
{
    int16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() - amount;
    int16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() - amount;
    uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

    if (rto->videoStandardInput <= 2) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD / 2);
    } else if (rto->videoStandardInput <= 7) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD);
    }
    int16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();
    GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

    if (IF_HB_ST2 > 0) {
        GBS::IF_HB_ST2::write(IF_HB_ST2);
    } else {
        GBS::IF_HB_ST2::write(IF_HSYNC_RST - 1);
    }

    if (IF_HB_SP2 > 0) {
        GBS::IF_HB_SP2::write(IF_HB_SP2);
    } else {
        GBS::IF_HB_SP2::write(IF_HSYNC_RST - 1);
    }
}

void scaleHorizontal(uint16_t amountToScale, bool subtracting)
{
    uint16_t hscale = GBS::VDS_HSCALE::read();

    if (subtracting && (hscale == 513 || hscale == 512))
        amountToScale = 1;
    if (!subtracting && (hscale == 511 || hscale == 512))
        amountToScale = 1;

    if (subtracting && (((int)hscale - amountToScale) <= 256)) {
        hscale = 256;
        GBS::VDS_HSCALE::write(hscale);
        ; // SerialMprintln("limit");
        return;
    }

    if (subtracting && (hscale - amountToScale > 255)) {
        hscale -= amountToScale;
    } else if (hscale + amountToScale < 1023) {
        hscale += amountToScale;
    } else if (hscale + amountToScale == 1023) {
        hscale = 1023;
        GBS::VDS_HSCALE::write(hscale);
        GBS::VDS_HSCALE_BYPS::write(1);
    } else if (hscale + amountToScale > 1023) {
        hscale = 1023;
        GBS::VDS_HSCALE::write(hscale);
        GBS::VDS_HSCALE_BYPS::write(1);
        ; // SerialMprintln("limit");
        return;
    }

    GBS::VDS_HSCALE_BYPS::write(0);

    uint16_t htotal = GBS::VDS_HSYNC_RST::read();
    uint16_t toShift = 0;
    if (hscale < 540)
        toShift = 4;
    else if (hscale < 640)
        toShift = 3;
    else
        toShift = 2;

    if (subtracting) {
        shiftHorizontal(toShift, true);
        if ((GBS::VDS_HB_ST::read() + 5) < GBS::VDS_DIS_HB_ST::read()) {
            GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 5);
        } else if ((GBS::VDS_DIS_HB_ST::read() + 5) < htotal) {
            GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 5);
            GBS::VDS_HB_ST::write(GBS::VDS_DIS_HB_ST::read());
        }

        if (GBS::VDS_HB_SP::read() < (GBS::VDS_HB_ST::read() + 16)) {
            if ((GBS::VDS_HB_SP::read()) > (htotal / 2)) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_SP::read() - 16);
            }
        }
    }

    if (!subtracting) {
        shiftHorizontal(toShift, false);
        if ((GBS::VDS_HB_ST::read() - 5) > 0) {
            GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() - 5);
        }
    }

    if (hscale < 512) {
        if (hscale % 2 == 0) {
            if (GBS::VDS_HB_ST::read() % 2 == 1) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 1);
            }
            if (htotal % 2 == 1) {
                if (GBS::VDS_HB_SP::read() % 2 == 0) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            } else {
                if (GBS::VDS_HB_SP::read() % 2 == 1) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            }
        } else {
            if (GBS::VDS_HB_ST::read() % 2 == 1) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 1);
            }
            if (htotal % 2 == 0) {
                if (GBS::VDS_HB_SP::read() % 2 == 1) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            } else {
                if (GBS::VDS_HB_SP::read() % 2 == 0) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            }
        }
        if (GBS::VDS_DIS_HB_ST::read() < GBS::VDS_HB_ST::read()) {
            GBS::VDS_DIS_HB_ST::write(GBS::VDS_HB_ST::read());
        }
    }

    ; // SerialMprint("HScale: ");
    ; // SerialMprintln(hscale);
    GBS::VDS_HSCALE::write(hscale);
}

void moveHS(uint16_t amountToAdd, bool subtracting)
{
    if (rto->outModeHdBypass) {
        uint16_t SP_CS_HS_ST = GBS::SP_CS_HS_ST::read();
        uint16_t SP_CS_HS_SP = GBS::SP_CS_HS_SP::read();
        uint16_t htotal = GBS::HD_HSYNC_RST::read();

        if (videoStandardInputIsPalNtscSd()) {
            htotal -= 8;
            htotal *= 2;
        }

        if (htotal == 0)
            return;
        int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

        if (SP_CS_HS_ST + amount < 0) {
            SP_CS_HS_ST = htotal + SP_CS_HS_ST;
        }
        if (SP_CS_HS_SP + amount < 0) {
            SP_CS_HS_SP = htotal + SP_CS_HS_SP;
        }

        GBS::SP_CS_HS_ST::write((SP_CS_HS_ST + amount) % htotal);
        GBS::SP_CS_HS_SP::write((SP_CS_HS_SP + amount) % htotal);

        ; // SerialMprint("HSST: ");
        ; // SerialMprint(GBS::SP_CS_HS_ST::read());
        ; // SerialMprint(" HSSP: ");
        ; // SerialMprintln(GBS::SP_CS_HS_SP::read());
    } else {
        uint16_t VDS_HS_ST = GBS::VDS_HS_ST::read();
        uint16_t VDS_HS_SP = GBS::VDS_HS_SP::read();
        uint16_t htotal = GBS::VDS_HSYNC_RST::read();

        if (htotal == 0)
            return;
        int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

        if (VDS_HS_ST + amount < 0) {
            VDS_HS_ST = htotal + VDS_HS_ST;
        }
        if (VDS_HS_SP + amount < 0) {
            VDS_HS_SP = htotal + VDS_HS_SP;
        }

        GBS::VDS_HS_ST::write((VDS_HS_ST + amount) % htotal);
        GBS::VDS_HS_SP::write((VDS_HS_SP + amount) % htotal);
    }
    printVideoTimings();
}

void moveVS(uint16_t amountToAdd, bool subtracting)
{
    uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
    if (vtotal == 0)
        return;
    uint16_t VDS_DIS_VB_ST = GBS::VDS_DIS_VB_ST::read();
    uint16_t newVDS_VS_ST = GBS::VDS_VS_ST::read();
    uint16_t newVDS_VS_SP = GBS::VDS_VS_SP::read();

    if (subtracting) {
        if ((newVDS_VS_ST - amountToAdd) > VDS_DIS_VB_ST) {
            newVDS_VS_ST -= amountToAdd;
            newVDS_VS_SP -= amountToAdd;
        } else
            ; // SerialMprintln("limit");
    } else {
        if ((newVDS_VS_SP + amountToAdd) < vtotal) {
            newVDS_VS_ST += amountToAdd;
            newVDS_VS_SP += amountToAdd;
        } else
            ; // SerialMprintln("limit");
    }

    GBS::VDS_VS_ST::write(newVDS_VS_ST);
    GBS::VDS_VS_SP::write(newVDS_VS_SP);
}

void invertHS()
{
    uint8_t high, low;
    uint16_t newST, newSP;

    writeOneByte(0xf0, 3);
    readFromRegister(0x0a, 1, &low);
    readFromRegister(0x0b, 1, &high);
    newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
    readFromRegister(0x0b, 1, &low);
    readFromRegister(0x0c, 1, &high);
    newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

    uint16_t temp = newST;
    newST = newSP;
    newSP = temp;

    writeOneByte(0x0a, (uint8_t)(newST & 0x00ff));
    writeOneByte(0x0b, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
    writeOneByte(0x0c, (uint8_t)((newSP & 0x0ff0) >> 4));
}

void invertVS()
{
    uint8_t high, low;
    uint16_t newST, newSP;

    writeOneByte(0xf0, 3);
    readFromRegister(0x0d, 1, &low);
    readFromRegister(0x0e, 1, &high);
    newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
    readFromRegister(0x0e, 1, &low);
    readFromRegister(0x0f, 1, &high);
    newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

    uint16_t temp = newST;
    newST = newSP;
    newSP = temp;

    writeOneByte(0x0d, (uint8_t)(newST & 0x00ff));
    writeOneByte(0x0e, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
    writeOneByte(0x0f, (uint8_t)((newSP & 0x0ff0) >> 4));
}

void scaleVertical(uint16_t amountToScale, bool subtracting)
{
    uint16_t vscale = GBS::VDS_VSCALE::read();

    if (vscale == 1023) {
        GBS::VDS_VSCALE_BYPS::write(0);
    }

    if (subtracting && (vscale == 513 || vscale == 512))
        amountToScale = 1;

    if (subtracting && (vscale == 684 || vscale == 683))
        amountToScale = 1;

    if (!subtracting && (vscale == 511 || vscale == 512))
        amountToScale = 1;

    if (!subtracting && (vscale == 682 || vscale == 683))
        amountToScale = 1;

    if (subtracting && (vscale - amountToScale > 128)) {
        vscale -= amountToScale;
    } else if (subtracting) {
        vscale = 128;
    } else if (vscale + amountToScale <= 1023) {
        vscale += amountToScale;
    } else if (vscale + amountToScale > 1023) {
        vscale = 1023;
    }

    ; // SerialMprint("VScale: ");
    ; // SerialMprintln(vscale);
    GBS::VDS_VSCALE::write(vscale);
}

void shiftVertical(uint16_t amountToAdd, bool subtracting)
{
    typedef GBS::Tie<GBS::VDS_VB_ST, GBS::VDS_VB_SP> Regs;
    uint16_t vrst = GBS::VDS_VSYNC_RST::read() - FrameSync::getSyncLastCorrection();
    uint16_t vbst = 0, vbsp = 0;
    int16_t newVbst = 0, newVbsp = 0;

    Regs::read(vbst, vbsp);
    newVbst = vbst;
    newVbsp = vbsp;

    if (subtracting) {

        newVbsp -= amountToAdd;
    } else {

        newVbsp += amountToAdd;
    }

    if (newVbst < 0) {
        newVbst = vrst + newVbst;
    }
    if (newVbsp < 0) {
        newVbsp = vrst + newVbsp;
    }

    if (newVbst > (int16_t)vrst) {
        newVbst = newVbst - vrst;
    }
    if (newVbsp > (int16_t)vrst) {
        newVbsp = newVbsp - vrst;
    }

    if (newVbsp < (newVbst + 2)) {
        newVbsp = newVbst + 2;
    }

    newVbst = newVbsp - 2;

    Regs::write(newVbst, newVbsp);
}

void shiftVerticalUp()
{
    shiftVertical(1, true);
}

void shiftVerticalDown()
{
    shiftVertical(1, false);
}

void shiftVerticalUpIF()
{

    uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
    uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;

    if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    }
    int16_t stop = GBS::IF_VB_SP::read();
    int16_t start = GBS::IF_VB_ST::read();

    if (stop < (sourceLines - 1) && start < (sourceLines - 1)) {
        stop += 2;
        start += 2;
    } else {
        start = 0;
        stop = 2;
    }
    GBS::IF_VB_SP::write(stop);
    GBS::IF_VB_ST::write(start);
}

void shiftVerticalDownIF()
{
    uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
    uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;

    if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    }

    int16_t stop = GBS::IF_VB_SP::read();
    int16_t start = GBS::IF_VB_ST::read();

    if (stop > 1 && start > 1) {
        stop -= 2;
        start -= 2;
    } else {
        start = sourceLines - 2;
        stop = sourceLines;
    }
    GBS::IF_VB_SP::write(stop);
    GBS::IF_VB_ST::write(start);
}

void setHSyncStartPosition(uint16_t value)
{
    if (rto->outModeHdBypass) {

        GBS::SP_CS_HS_ST::write(value);
    } else {
        GBS::VDS_HS_ST::write(value);
    }
}

void setHSyncStopPosition(uint16_t value)
{
    if (rto->outModeHdBypass) {

        GBS::SP_CS_HS_SP::write(value);
    } else {
        GBS::VDS_HS_SP::write(value);
    }
}

void setMemoryHblankStartPosition(uint16_t value)
{
    GBS::VDS_HB_ST::write(value);
    GBS::HD_HB_ST::write(value);
}

void setMemoryHblankStopPosition(uint16_t value)
{
    GBS::VDS_HB_SP::write(value);
    GBS::HD_HB_SP::write(value);
}

void setDisplayHblankStartPosition(uint16_t value)
{
    GBS::VDS_DIS_HB_ST::write(value);
}

void setDisplayHblankStopPosition(uint16_t value)
{
    GBS::VDS_DIS_HB_SP::write(value);
}

void setVSyncStartPosition(uint16_t value)
{
    GBS::VDS_VS_ST::write(value);
    GBS::HD_VS_ST::write(value);
}

void setVSyncStopPosition(uint16_t value)
{
    GBS::VDS_VS_SP::write(value);
    GBS::HD_VS_SP::write(value);
}

void setMemoryVblankStartPosition(uint16_t value)
{
    GBS::VDS_VB_ST::write(value);
    GBS::HD_VB_ST::write(value);
}

void setMemoryVblankStopPosition(uint16_t value)
{
    GBS::VDS_VB_SP::write(value);
    GBS::HD_VB_SP::write(value);
}

void setDisplayVblankStartPosition(uint16_t value)
{
    GBS::VDS_DIS_VB_ST::write(value);
}

void setDisplayVblankStopPosition(uint16_t value)
{
    GBS::VDS_DIS_VB_SP::write(value);
}

uint16_t getCsVsStart()
{
    return (GBS::SP_SDCS_VSST_REG_H::read() << 8) + GBS::SP_SDCS_VSST_REG_L::read();
}

uint16_t getCsVsStop()
{
    return (GBS::SP_SDCS_VSSP_REG_H::read() << 8) + GBS::SP_SDCS_VSSP_REG_L::read();
}

void setCsVsStart(uint16_t start)
{
    GBS::SP_SDCS_VSST_REG_H::write(start >> 8);
    GBS::SP_SDCS_VSST_REG_L::write(start & 0xff);
}

void setCsVsStop(uint16_t stop)
{
    GBS::SP_SDCS_VSSP_REG_H::write(stop >> 8);
    GBS::SP_SDCS_VSSP_REG_L::write(stop & 0xff);
}

void printVideoTimings()
{
    if (rto->presetID < 0x20) {
        ; // SerialMprintln("");
        ; // SerialMprint(F("HT / scale   : "));
        ; // SerialMprint(GBS::VDS_HSYNC_RST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_HSCALE::read());
        ; // SerialMprint(F("HS ST/SP     : "));
        ; // SerialMprint(GBS::VDS_HS_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_HS_SP::read());
        ; // SerialMprint(F("HB ST/SP(d)  : "));
        ; // SerialMprint(GBS::VDS_DIS_HB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_DIS_HB_SP::read());
        ; // SerialMprint(F("HB ST/SP     : "));
        ; // SerialMprint(GBS::VDS_HB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_HB_SP::read());
        ; // SerialMprintln(F("------"));
        // vertical
        ; // SerialMprint(F("VT / scale   : "));
        ; // SerialMprint(GBS::VDS_VSYNC_RST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_VSCALE::read());
        ; // SerialMprint(F("VS ST/SP     : "));
        ; // SerialMprint(GBS::VDS_VS_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_VS_SP::read());
        ; // SerialMprint(F("VB ST/SP(d)  : "));
        ; // SerialMprint(GBS::VDS_DIS_VB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_DIS_VB_SP::read());
        ; // SerialMprint(F("VB ST/SP     : "));
        ; // SerialMprint(GBS::VDS_VB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::VDS_VB_SP::read());
        // IF V offset
        ; // SerialMprint(F("IF VB ST/SP  : "));
        ; // SerialMprint(GBS::IF_VB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::IF_VB_SP::read());
    } else {
        ; // SerialMprintln("");
        ; // SerialMprint(F("HD_HSYNC_RST : "));
        ; // SerialMprintln(GBS::HD_HSYNC_RST::read());
        ; // SerialMprint(F("HD_INI_ST    : "));
        ; // SerialMprintln(GBS::HD_INI_ST::read());
        ; // SerialMprint(F("HS ST/SP     : "));
        ; // SerialMprint(GBS::SP_CS_HS_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::SP_CS_HS_SP::read());
        ; // SerialMprint(F("HB ST/SP     : "));
        ; // SerialMprint(GBS::HD_HB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::HD_HB_SP::read());
        ; // SerialMprintln(F("------"));
        // vertical
        ; // SerialMprint(F("VS ST/SP     : "));
        ; // SerialMprint(GBS::HD_VS_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::HD_VS_SP::read());
        ; // SerialMprint(F("VB ST/SP     : "));
        ; // SerialMprint(GBS::HD_VB_ST::read());
        ; // SerialMprint(" ");
        ; // SerialMprintln(GBS::HD_VB_SP::read());
    }

    ; // SerialMprint(F("CsVT         : "));
    ; // SerialMprintln(GBS::STATUS_SYNC_PROC_VTOTAL::read());
    ; // SerialMprint(F("CsVS_ST/SP   : "));
    ; // SerialMprint(getCsVsStart());
    ; // SerialMprint(F(" "));
    ; // SerialMprintln(getCsVsStop());
}

void set_htotal(uint16_t htotal)
{

    uint16_t h_blank_display_start_position = htotal - 1;
    uint16_t h_blank_display_stop_position = htotal - ((htotal * 3) / 4);
    uint16_t center_blank = ((h_blank_display_stop_position / 2) * 3) / 4;
    uint16_t h_sync_start_position = center_blank - (center_blank / 2);
    uint16_t h_sync_stop_position = center_blank + (center_blank / 2);
    uint16_t h_blank_memory_start_position = h_blank_display_start_position - 1;
    uint16_t h_blank_memory_stop_position = h_blank_display_stop_position - (h_blank_display_stop_position / 50);

    GBS::VDS_HSYNC_RST::write(htotal);
    GBS::VDS_HS_ST::write(h_sync_start_position);
    GBS::VDS_HS_SP::write(h_sync_stop_position);
    GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
    GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
    GBS::VDS_HB_ST::write(h_blank_memory_start_position);
    GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
}

void set_vtotal(uint16_t vtotal)
{
    uint16_t VDS_DIS_VB_ST = vtotal - 2;
    uint16_t VDS_DIS_VB_SP = (vtotal >> 6) + 8;
    uint16_t VDS_VB_ST = ((uint16_t)(vtotal * 0.016f)) & 0xfffe;
    uint16_t VDS_VB_SP = VDS_VB_ST + 2;
    uint16_t v_sync_start_position = 1;
    uint16_t v_sync_stop_position = 5;

    if ((vtotal < 530) || (vtotal >= 803 && vtotal <= 809) || (vtotal >= 793 && vtotal <= 798)) {
        uint16_t temp = v_sync_start_position;
        v_sync_start_position = v_sync_stop_position;
        v_sync_stop_position = temp;
    }

    GBS::VDS_VSYNC_RST::write(vtotal);
    GBS::VDS_VS_ST::write(v_sync_start_position);
    GBS::VDS_VS_SP::write(v_sync_stop_position);
    GBS::VDS_VB_ST::write(VDS_VB_ST);
    GBS::VDS_VB_SP::write(VDS_VB_SP);
    GBS::VDS_DIS_VB_ST::write(VDS_DIS_VB_ST);
    GBS::VDS_DIS_VB_SP::write(VDS_DIS_VB_SP);

    GBS::VDS_VSYN_SIZE1::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_VSYN_SIZE2::write(GBS::VDS_VSYNC_RST::read() + 2);
}

void resetDebugPort()
{
    GBS::PAD_BOUT_EN::write(1);
    GBS::IF_TEST_EN::write(1);
    GBS::IF_TEST_SEL::write(3);
    GBS::TEST_BUS_SEL::write(0xa);
    GBS::TEST_BUS_EN::write(1);
    GBS::TEST_BUS_SP_SEL::write(0x0f);
    GBS::MEM_FF_TOP_FF_SEL::write(1);

    GBS::VDS_TEST_EN::write(1);
}

void readEeprom()
{
    int addr = 0;
    const uint8_t eepromAddr = 0x50;
    Wire.beginTransmission(eepromAddr);

    Wire.write(addr >> 8);
    Wire.write((uint8_t)addr);
    Wire.endTransmission();
    Wire.requestFrom(eepromAddr, (uint8_t)128);
    uint8_t readData = 0;
    uint8_t i = 0;
    while (Wire.available()) {

        readData = Wire.read();
        Serial.println(readData, HEX);

        i++;
    }
}

void setIfHblankParameters()
{
    if (!rto->outModeHdBypass) {
        uint16_t pll_divider = GBS::PLLAD_MD::read();

        GBS::IF_HSYNC_RST::write(((pll_divider >> 1) + 13) & 0xfffe);
        GBS::IF_LINE_SP::write(GBS::IF_HSYNC_RST::read() + 1);
        if (rto->presetID == 0x05) {

            GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() + 32);
            GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() + 32);
        }
        if (rto->presetID == 0x15) {
            GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() + 20);
            GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() + 20);
        }

        if (GBS::IF_LD_RAM_BYPS::read()) {

            GBS::IF_HB_SP2::write((uint16_t)((float)pll_divider * 0.06512f) & 0xfffe);

            GBS::IF_HB_ST2::write((uint16_t)((float)pll_divider * 0.4912f) & 0xfffe);
        } else {

            GBS::IF_HB_SP2::write(4 + ((uint16_t)((float)pll_divider * 0.0224f) & 0xfffe));
            GBS::IF_HB_ST2::write((uint16_t)((float)pll_divider * 0.4550f) & 0xfffe);

            if (GBS::IF_HB_ST2::read() >= 0x420) {

                GBS::IF_HB_ST2::write(0x420);
            }

            if (rto->presetID == 0x05 || rto->presetID == 0x15) {

                GBS::IF_HB_SP2::write(0x2A);
            }
        }
    }
}

void fastGetBestHtotal()
{
    uint32_t inStart, inStop;
    signed long inPeriod = 1;
    double inHz = 1.0;
    GBS::TEST_BUS_SEL::write(0xa);
    if (FrameSync::vsyncInputSample(&inStart, &inStop)) {
        inPeriod = (inStop - inStart) >> 1;
        if (inPeriod > 1) {
            inHz = (double)1000000 / (double)inPeriod;
        }
    } else {
    }

    uint16_t newVtotal = GBS::VDS_VSYNC_RST::read();
    double bestHtotal = 108000000 / ((double)newVtotal * inHz);
    double bestHtotal50 = 108000000 / ((double)newVtotal * 50);
    double bestHtotal60 = 108000000 / ((double)newVtotal * 60);

    if (bestHtotal > 800 && bestHtotal < 3200) {
    }
}

boolean runAutoBestHTotal()
{
    if (!FrameSync::ready() && rto->autoBestHtotalEnabled == true && rto->videoStandardInput > 0 && rto->videoStandardInput < 15) {

        boolean stableNow = 1;

        for (uint8_t i = 0; i < 64; i++) {
            if (!getStatus16SpHsStable()) {
                stableNow = 0;

                break;
            }
        }

        if (stableNow) {
            if (GBS::STATUS_INT_SOG_BAD::read()) {

                resetInterruptSogBadBit();
                delay(40);
                stableNow = false;
            }
            resetInterruptSogBadBit();

            if (stableNow && (getVideoMode() == rto->videoStandardInput)) {
                uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
                uint8_t vdsBusSelBackup = GBS::VDS_TEST_BUS_SEL::read();
                uint8_t ifBusSelBackup = GBS::IF_TEST_SEL::read();

                if (testBusSelBackup != 0)
                    GBS::TEST_BUS_SEL::write(0);
                if (vdsBusSelBackup != 0)
                    GBS::VDS_TEST_BUS_SEL::write(0);
                if (ifBusSelBackup != 3)
                    GBS::IF_TEST_SEL::write(3);

                yield();
                uint16_t bestHTotal = FrameSync::init();
                yield();

                GBS::TEST_BUS_SEL::write(testBusSelBackup);
                if (vdsBusSelBackup != 0)
                    GBS::VDS_TEST_BUS_SEL::write(vdsBusSelBackup);
                if (ifBusSelBackup != 3)
                    GBS::IF_TEST_SEL::write(ifBusSelBackup);

                if (GBS::STATUS_INT_SOG_BAD::read()) {

                    stableNow = false;
                }
                for (uint8_t i = 0; i < 16; i++) {
                    if (!getStatus16SpHsStable()) {
                        stableNow = 0;

                        break;
                    }
                }
                resetInterruptSogBadBit();

                if (bestHTotal > 4095) {
                    if (!rto->forceRetime) {
                        stableNow = false;
                    } else {

                        bestHTotal = 4095;
                    }
                }

                if (stableNow) {
                    for (uint8_t i = 0; i < 24; i++) {
                        delay(1);
                        if (!getStatus16SpHsStable()) {
                            stableNow = false;

                            break;
                        }
                    }
                }

                if (bestHTotal > 0 && stableNow) {
                    boolean success = applyBestHTotal(bestHTotal);
                    if (success) {
                        rto->syncLockFailIgnore = 16;

                        return true;
                    }
                }
            }
        }

        if (!stableNow) {
            FrameSync::reset(uopt->frameTimeLockMethod);

            if (rto->syncLockFailIgnore > 0) {
                rto->syncLockFailIgnore--;
                if (rto->syncLockFailIgnore == 0) {
                    GBS::DAC_RGBS_PWDNZ::write(1); 
                    if (!uopt->wantOutputComponent) {
                        GBS::PAD_SYNC_OUT_ENZ::write(0); 
                    }
                    rto->autoBestHtotalEnabled = false;
                }
            }

            Serial.println(")");
        }
    } else if (FrameSync::ready()) {

        return true;
    }

    if (rto->continousStableCounter != 0 && rto->continousStableCounter != 255) {
        rto->continousStableCounter++;
    }

    return false;
}

boolean applyBestHTotal(uint16_t bestHTotal)
{
    if (rto->outModeHdBypass) {
        return true;
    }

    uint16_t orig_htotal = GBS::VDS_HSYNC_RST::read();
    int diffHTotal = bestHTotal - orig_htotal;
    uint16_t diffHTotalUnsigned = abs(diffHTotal);

    if (((diffHTotalUnsigned == 0) || (rto->extClockGenDetected && diffHTotalUnsigned == 1)) &&
        !rto->forceRetime) {
        if (!uopt->enableFrameTimeLock) {

            if (!rto->extClockGenDetected) {
                float sfr = getSourceFieldRate(0);
                yield();
                float ofr = getOutputFrameRate();
                if (sfr < 1.0f) {
                    sfr = getSourceFieldRate(0);
                }
                if (ofr < 1.0f) {
                    ofr = getOutputFrameRate();
                }
            }
        }
        return true;
    }

    if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() == 1) {

        return true;
    }

    boolean isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.06f)) ? true : false;

    if (isLargeDiff && (getVideoMode() == 8 || rto->videoStandardInput == 14)) {

        isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.16f)) ? true : false;
    }

    if (isLargeDiff) {
        ;
    }

    if (isLargeDiff && (rto->forceRetime == false)) {
        if (rto->videoStandardInput != 14) {
            rto->failRetryAttempts++;
            if (rto->failRetryAttempts < 8) {

                FrameSync::reset(uopt->frameTimeLockMethod);
                delay(60);
            } else {

                rto->autoBestHtotalEnabled = false;
            }
        }
        return false;
    }

    if (bestHTotal == 0) {

        return false;
    }

    if (rto->forceRetime == false) {
        if (GBS::STATUS_INT_SOG_BAD::read() == 1) {

            return false;
        }
    }

    rto->failRetryAttempts = 0; 

    uint16_t h_blank_display_start_position = GBS::VDS_DIS_HB_ST::read();
    uint16_t h_blank_display_stop_position = GBS::VDS_DIS_HB_SP::read();
    uint16_t h_blank_memory_start_position = GBS::VDS_HB_ST::read();
    uint16_t h_blank_memory_stop_position = GBS::VDS_HB_SP::read();

    if (h_blank_memory_start_position == h_blank_display_start_position) {
        h_blank_display_start_position += (diffHTotal / 2);
        h_blank_display_stop_position += (diffHTotal / 2);
        h_blank_memory_start_position = h_blank_display_start_position;
        h_blank_memory_stop_position += (diffHTotal / 2);
    } else {
        h_blank_display_start_position += (diffHTotal / 2);
        h_blank_display_stop_position += (diffHTotal / 2);
        h_blank_memory_start_position += (diffHTotal / 2);
        h_blank_memory_stop_position += (diffHTotal / 2);
    }

    if (diffHTotal < 0) {
        h_blank_display_start_position &= 0xfffe;
        h_blank_display_stop_position &= 0xfffe;
        h_blank_memory_start_position &= 0xfffe;
        h_blank_memory_stop_position &= 0xfffe;
    } else if (diffHTotal > 0) {
        h_blank_display_start_position += 1;
        h_blank_display_start_position &= 0xfffe;
        h_blank_display_stop_position += 1;
        h_blank_display_stop_position &= 0xfffe;
        h_blank_memory_start_position += 1;
        h_blank_memory_start_position &= 0xfffe;
        h_blank_memory_stop_position += 1;
        h_blank_memory_stop_position &= 0xfffe;
    }

    uint16_t h_sync_start_position = GBS::VDS_HS_ST::read();
    uint16_t h_sync_stop_position = GBS::VDS_HS_SP::read();

    if (h_blank_display_start_position > (bestHTotal - 8) || isLargeDiff) {

        h_blank_display_start_position = bestHTotal * 0.936f;
    }
    if (h_blank_display_stop_position > bestHTotal || isLargeDiff) {

        h_blank_display_stop_position = bestHTotal * 0.178f;
    }
    if ((h_blank_memory_start_position > bestHTotal) || (h_blank_memory_start_position > h_blank_display_start_position) || isLargeDiff) {

        h_blank_memory_start_position = h_blank_display_start_position * 0.971f;
    }
    if (h_blank_memory_stop_position > bestHTotal || isLargeDiff) {

        h_blank_memory_stop_position = h_blank_display_stop_position * 0.64f;
    }

    if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) {
        if (h_sync_start_position >= h_blank_display_stop_position) {
            h_sync_start_position = h_blank_display_stop_position * 0.8f;
            h_sync_stop_position = 4;
        }
    } else {
        if (h_sync_stop_position >= h_blank_display_stop_position) {
            h_sync_stop_position = h_blank_display_stop_position * 0.8f;
            h_sync_start_position = 4; //
        }
    }

    if (isLargeDiff) {
        if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) {
            h_sync_stop_position = 4;

            h_sync_start_position = 16 + (h_blank_display_stop_position * 0.3f);
        } else {
            h_sync_start_position = 4;
            h_sync_stop_position = 16 + (h_blank_display_stop_position * 0.3f);
        }
    }

    if (diffHTotal != 0) {

        uint16_t timeout = 0;
        while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400))

            while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 800))

                GBS::VDS_HSYNC_RST::write(bestHTotal);
        GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
        GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
        GBS::VDS_HB_ST::write(h_blank_memory_start_position);
        GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
        GBS::VDS_HS_ST::write(h_sync_start_position);
        GBS::VDS_HS_SP::write(h_sync_stop_position);
    }

    boolean print = 1;
    if (uopt->enableFrameTimeLock) {
        if ((GBS::GBS_RUNTIME_FTL_ADJUSTED::read() == 1) && !rto->forceRetime) {

            print = 0;
        }
        GBS::GBS_RUNTIME_FTL_ADJUSTED::write(0);
    }

    rto->forceRetime = false;

    if (print) {
        ;
        if (diffHTotal >= 0) {
        }

        if (!rto->extClockGenDetected) {
            float sfr = getSourceFieldRate(0);
            delay(0);
            float ofr = getOutputFrameRate();
            if (sfr < 1.0f) {
                sfr = getSourceFieldRate(0);
            }
            if (ofr < 1.0f) {
                ofr = getOutputFrameRate();
            }
        }
    }

    return true;
}

float getSourceFieldRate(boolean useSPBus)
{
    double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t spBusSelBackup = GBS::TEST_BUS_SP_SEL::read();
    uint8_t ifBusSelBackup = GBS::IF_TEST_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (debugPinBackup != 1)
        GBS::PAD_BOUT_EN::write(1);

    if (ifBusSelBackup != 3)
        GBS::IF_TEST_SEL::write(3);

    if (useSPBus) {
        if (rto->syncTypeCsync) {

            if (testBusSelBackup != 0xa)
                GBS::TEST_BUS_SEL::write(0xa);
        } else {

            if (testBusSelBackup != 0x0)
                GBS::TEST_BUS_SEL::write(0x0);
        }
        if (spBusSelBackup != 0x0f)
            GBS::TEST_BUS_SP_SEL::write(0x0f);
    } else {
        if (testBusSelBackup != 0)
            GBS::TEST_BUS_SEL::write(0);
    }

    float retVal = 0;

    uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
    if (fieldTimeTicks == 0) {

        fieldTimeTicks = FrameSync::getPulseTicks();
    }

    if (fieldTimeTicks > 0) {
        retVal = esp8266_clock_freq / (double)fieldTimeTicks;
        if (retVal < 47.0f || retVal > 86.0f) {

            fieldTimeTicks = FrameSync::getPulseTicks();
            if (fieldTimeTicks > 0) {
                retVal = esp8266_clock_freq / (double)fieldTimeTicks;
            }
        }
    }

    GBS::TEST_BUS_SEL::write(testBusSelBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup);
    if (spBusSelBackup != 0x0f)
        GBS::TEST_BUS_SP_SEL::write(spBusSelBackup);
    if (ifBusSelBackup != 3)
        GBS::IF_TEST_SEL::write(ifBusSelBackup);

    return retVal;
}

float getOutputFrameRate()
{
    double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (debugPinBackup != 1)
        GBS::PAD_BOUT_EN::write(1);

    if (testBusSelBackup != 2)
        GBS::TEST_BUS_SEL::write(2);

    float retVal = 0;

    uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
    if (fieldTimeTicks == 0) {

        fieldTimeTicks = FrameSync::getPulseTicks();
    }

    if (fieldTimeTicks > 0) {
        retVal = esp8266_clock_freq / (double)fieldTimeTicks;
        if (retVal < 47.0f || retVal > 86.0f) {

            fieldTimeTicks = FrameSync::getPulseTicks();
            if (fieldTimeTicks > 0) {
                retVal = esp8266_clock_freq / (double)fieldTimeTicks;
            }
        }
    }

    GBS::TEST_BUS_SEL::write(testBusSelBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup);

    return retVal;
}

uint32_t getPllRate()
{
    uint32_t esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t spBusSelBackup = GBS::TEST_BUS_SP_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (testBusSelBackup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
    }
    if (rto->syncTypeCsync) {
        if (spBusSelBackup != 0x6b)
            GBS::TEST_BUS_SP_SEL::write(0x6b);
    } else {
        if (spBusSelBackup != 0x09)
            GBS::TEST_BUS_SP_SEL::write(0x09);
    }
    GBS::PAD_BOUT_EN::write(1);
    yield();
    ESP.wdtFeed();
    delayMicroseconds(200);
    uint32_t ticks = FrameSync::getPulseTicks();

    GBS::PAD_BOUT_EN::write(debugPinBackup);
    if (testBusSelBackup != 0xa) {
        GBS::TEST_BUS_SEL::write(testBusSelBackup);
    }
    GBS::TEST_BUS_SP_SEL::write(spBusSelBackup);

    uint32_t retVal = 0;
    if (ticks > 0) {
        retVal = esp8266_clock_freq / ticks;
    }

    return retVal;
}

#define AUTO_GAIN_INIT 0x48

void doPostPresetLoadSteps()
{
    // if(Info_sate == 0)
    {
        if (uopt->enableAutoGain) {
            if (uopt->presetPreference == OutputCustomized) 
            {
                adco->r_gain = GBS::ADC_RGCTRL::read();
                adco->g_gain = GBS::ADC_GGCTRL::read();
                adco->b_gain = GBS::ADC_BGCTRL::read();
            }
        }

        if (rto->videoStandardInput == 0) {
            uint8_t videoMode = getVideoMode();
            if (videoMode > 0) {
                rto->videoStandardInput = videoMode;
            }
        }

        rto->presetID = GBS::GBS_PRESET_ID::read();
        rto->isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();

        GBS::ADC_UNUSED_64::write(0);
        GBS::ADC_UNUSED_65::write(0);
        GBS::ADC_UNUSED_66::write(0);
        GBS::ADC_UNUSED_67::write(0);
        GBS::PAD_CKIN_ENZ::write(0);

        if (!rto->isCustomPreset) {
            prepareSyncProcessor();
        }
        if (rto->videoStandardInput == 14) {

            if (rto->syncTypeCsync == false) {
                GBS::SP_SOG_SRC_SEL::write(0);  
                GBS::ADC_SOGEN::write(1);       
                GBS::SP_EXT_SYNC_SEL::write(0); 
                GBS::SP_SOG_MODE::write(0);     
                GBS::SP_NO_COAST_REG::write(1);
                GBS::SP_PRE_COAST::write(0);        
                GBS::SP_POST_COAST::write(0);       
                GBS::SP_H_PULSE_IGNOR::write(0xff); 
                GBS::SP_SYNC_BYPS::write(0);        
                GBS::SP_HS_POL_ATO::write(1);       
                GBS::SP_VS_POL_ATO::write(1);       
                GBS::SP_HS_LOOP_SEL::write(1);      
                GBS::SP_H_PROTECT::write(0);        
                rto->phaseADC = 16;
                rto->phaseSP = 8;
            } else {

                GBS::SP_SOG_SRC_SEL::write(0);  
                GBS::SP_EXT_SYNC_SEL::write(1); 
                GBS::ADC_SOGEN::write(1);       
                GBS::SP_SOG_MODE::write(1);     
                GBS::SP_NO_COAST_REG::write(0);
                GBS::SP_PRE_COAST::write(4);
                GBS::SP_POST_COAST::write(7);
                GBS::SP_SYNC_BYPS::write(0);   
                GBS::SP_HS_LOOP_SEL::write(1); 
                GBS::SP_H_PROTECT::write(1);
                rto->currentLevelSOG = 24;
                rto->phaseADC = 16;
                rto->phaseSP = 8;
            }
        }

        GBS::SP_H_PROTECT::write(0); 
        GBS::SP_COAST_INV_REG::write(0);
        if (!rto->outModeHdBypass && GBS::GBS_OPTION_SCALING_RGBHV::read() == 0) {
            updateSpDynamic(0);
        }

        GBS::SP_NO_CLAMP_REG::write(1);
        GBS::OUT_SYNC_CNTRL::write(1); // 

        GBS::ADC_AUTO_OFST_PRD::write(1);
        GBS::ADC_AUTO_OFST_DELAY::write(0);
        GBS::ADC_AUTO_OFST_STEP::write(0);
        GBS::ADC_AUTO_OFST_TEST::write(1);
        GBS::ADC_AUTO_OFST_RANGE_REG::write(0x00);

        if (rto->inputIsYpBpR == true) //&& Info_sate == 0 )//&& SeleInputSource == S_YUV)
        {
            applyYuvPatches();
        } else if (rto->inputIsYpBpR == false) //&& Info_sate == 0 )//&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs) )
        {
            applyRGBPatches();
        }

        if (rto->outModeHdBypass) {
            GBS::OUT_SYNC_SEL::write(1);
            rto->autoBestHtotalEnabled = false;
        } else {
            rto->autoBestHtotalEnabled = true;
        }

        rto->phaseADC = GBS::PA_ADC_S::read();
        rto->phaseSP = 8;

        if (rto->inputIsYpBpR) // && Info_sate == 0 )//&& SeleInputSource == S_YUV )
        {
            rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 14;
        } else if (rto->inputIsYpBpR) // == false && Info_sate == 0 )//&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs) )
        {
            rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        }

        setAndUpdateSogLevel(rto->currentLevelSOG);

        if (!rto->isCustomPreset) {

            setAdcParametersGainAndOffset();
        }

        GBS::GPIO_CONTROL_00::write(0x67);
        GBS::GPIO_CONTROL_01::write(0x00);
        rto->clampPositionIsSet = 0; // Clamp position setting
        rto->coastPositionIsSet = 0; // coast position setting
        rto->phaseIsSet = 0;
        rto->continousStableCounter = 0;              
        rto->noSyncCounter = 0;                       
        rto->motionAdaptiveDeinterlaceActive = false; 
        rto->scanlinesEnabled = false;                
        rto->failRetryAttempts = 0;                   
        rto->videoIsFrozen = true;
        rto->sourceDisconnected = false;
        rto->boardHasPower = true;

        if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            rto->isCustomPreset = 0;
        }

        if (!rto->isCustomPreset) {
            if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 ||
                rto->videoStandardInput == 8 || rto->videoStandardInput == 9) {
                GBS::IF_LD_RAM_BYPS::write(1);
            }

            GBS::IF_INI_ST::write(0);

            GBS::IF_HS_INT_LPF_BYPS::write(0);

            GBS::IF_HS_SEL_LPF::write(1);
            GBS::IF_HS_PSHIFT_BYPS::write(1);

            GBS::IF_LD_WRST_SEL::write(1);

            GBS::SP_RT_HS_ST::write(0);
            GBS::SP_RT_HS_SP::write(GBS::PLLAD_MD::read() * 0.93f);

            GBS::VDS_PK_LB_CORE::write(0);
            GBS::VDS_PK_LH_CORE::write(0);
            if (rto->presetID == 0x05 || rto->presetID == 0x15) {

                GBS::VDS_PK_LB_GAIN::write(0x16);
                GBS::VDS_PK_LH_GAIN::write(0x0A);
            } else {
                GBS::VDS_PK_LB_GAIN::write(0x16);
                GBS::VDS_PK_LH_GAIN::write(0x18);
            }
            GBS::VDS_PK_VL_HL_SEL::write(0);
            GBS::VDS_PK_VL_HH_SEL::write(0);

            GBS::VDS_STEP_GAIN::write(1);

            setOverSampleRatio(2, true);

            if (uopt->wantFullHeight) {
                if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
                } else if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
                    if (rto->presetID == 0x15) {
                        GBS::VDS_VSCALE::write(455);
                        GBS::VDS_DIS_VB_ST::write(GBS::VDS_VSYNC_RST::read());
                        GBS::VDS_DIS_VB_SP::write(42);
                        GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 22);
                        GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 22);
                    }
                }
            }

            if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {

                GBS::ADC_FLTR::write(3);
                GBS::PLLAD_KS::write(2);
                setOverSampleRatio(4, true);
                GBS::IF_SEL_WEN::write(0);
                if (rto->inputIsYpBpR) {
                    GBS::IF_HS_TAP11_BYPS::write(0);
                    GBS::IF_HS_Y_PDELAY::write(2);
                    GBS::VDS_V_DELAY::write(0);
                    GBS::VDS_Y_DELAY::write(3);
                }

                if (rto->presetID == 0x06 || rto->presetID == 0x16) {
                    setCsVsStart(2);
                    setCsVsStop(0);
                    GBS::IF_VS_SEL::write(1);
                    GBS::IF_VS_FLIP::write(0);
                    GBS::IF_LD_RAM_BYPS::write(0);
                    GBS::IF_HS_DEC_FACTOR::write(1);
                    GBS::IF_LD_SEL_PROV::write(0);
                    GBS::IF_HB_ST::write(2);
                    GBS::MADPT_Y_VSCALE_BYPS::write(0);
                    GBS::MADPT_UV_VSCALE_BYPS::write(0);
                    GBS::MADPT_PD_RAM_BYPS::write(0);
                    GBS::MADPT_VSCALE_DEC_FACTOR::write(1);
                    GBS::MADPT_SEL_PHASE_INI::write(0);
                    if (rto->videoStandardInput == 1) {
                        GBS::IF_HB_ST2::write(0x490);
                        GBS::IF_HB_SP2::write(0x80);
                        GBS::IF_HB_SP::write(0x4A);
                        GBS::IF_HBIN_SP::write(0xD0);
                    } else if (rto->videoStandardInput == 2) {
                        GBS::IF_HB_SP2::write(0x74);
                        GBS::IF_HB_SP::write(0x50);
                        GBS::IF_HBIN_SP::write(0xD0);
                    }
                }
            }
            if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 ||
                rto->videoStandardInput == 8 || rto->videoStandardInput == 9) {

                GBS::ADC_FLTR::write(3);
                GBS::PLLAD_KS::write(1); // VCO post crossover control, determined by CKO frequency

                if (rto->presetID != 0x06 && rto->presetID != 0x16) {
                    setCsVsStart(14);
                    setCsVsStop(11);
                    GBS::IF_HB_SP::write(0);
                }
                setOverSampleRatio(2, true);
                GBS::IF_HS_DEC_FACTOR::write(0);
                GBS::IF_LD_SEL_PROV::write(1);
                GBS::IF_PRGRSV_CNTRL::write(1);
                GBS::IF_SEL_WEN::write(1);
                GBS::IF_HS_SEL_LPF::write(0);
                GBS::IF_HS_TAP11_BYPS::write(0);
                GBS::IF_HS_Y_PDELAY::write(3);
                GBS::VDS_V_DELAY::write(1);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(1);
                GBS::VDS_Y_DELAY::write(3);
                if (rto->videoStandardInput == 9) {
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 650) {
                        delay(20);
                        if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 650) {
                            GBS::PLLAD_KS::write(0);
                            GBS::VDS_VSCALE_BYPS::write(1);
                        }
                    }
                }

                if (rto->presetID == 0x06 || rto->presetID == 0x16) {
                    GBS::MADPT_Y_VSCALE_BYPS::write(0);
                    GBS::MADPT_UV_VSCALE_BYPS::write(0);
                    GBS::MADPT_PD_RAM_BYPS::write(0);
                    GBS::MADPT_VSCALE_DEC_FACTOR::write(1);
                    GBS::MADPT_SEL_PHASE_INI::write(1);
                    GBS::MADPT_SEL_PHASE_INI::write(0);
                }
            }
            if (rto->videoStandardInput == 3 && rto->presetID != 0x06) {
                setCsVsStart(16);
                setCsVsStop(13); //
                GBS::IF_HB_ST::write(30);
                GBS::IF_HBIN_ST::write(0x20);
                GBS::IF_HBIN_SP::write(0x60);
                if (rto->presetID == 0x5) {
                    GBS::IF_HB_SP2::write(0xB0);
                    GBS::IF_HB_ST2::write(0x4BC);
                } else if (rto->presetID == 0x3) {
                    GBS::VDS_VSCALE::write(683);
                    GBS::IF_HB_ST2::write(0x478);
                    GBS::IF_HB_SP2::write(0x84);
                } else if (rto->presetID == 0x2) {
                    GBS::IF_HB_SP2::write(0x84);
                    GBS::IF_HB_ST2::write(0x478);
                } else if (rto->presetID == 0x1) {
                    GBS::IF_HB_SP2::write(0x84);
                    GBS::IF_HB_ST2::write(0x478);
                } else if (rto->presetID == 0x4) {
                    GBS::IF_HB_ST2::write(0x478);
                    GBS::IF_HB_SP2::write(0x90);
                }
            } else if (rto->videoStandardInput == 4 && rto->presetID != 0x16) {
                GBS::IF_HBIN_SP::write(0x40);
                GBS::IF_HBIN_ST::write(0x20);
                GBS::IF_HB_ST::write(0x30);
                if (rto->presetID == 0x15) {
                    GBS::IF_HB_ST2::write(0x4C0);
                    GBS::IF_HB_SP2::write(0xC8);
                } else if (rto->presetID == 0x13) {
                    GBS::IF_HB_ST2::write(0x478);
                    GBS::IF_HB_SP2::write(0x88);
                } else if (rto->presetID == 0x12) {

                    GBS::IF_HB_ST2::write(0x454);
                    GBS::IF_HB_SP2::write(0x88);
                } else if (rto->presetID == 0x11) {
                    GBS::IF_HB_ST2::write(0x454);
                    GBS::IF_HB_SP2::write(0x88);
                } else if (rto->presetID == 0x14) {
                    GBS::IF_HB_ST2::write(0x478);
                    GBS::IF_HB_SP2::write(0x90);
                }
            } else if (rto->videoStandardInput == 5) {
                GBS::ADC_FLTR::write(1);
                GBS::IF_PRGRSV_CNTRL::write(1);
                GBS::IF_HS_DEC_FACTOR::write(0);
                GBS::INPUT_FORMATTER_02::write(0x74);
                GBS::VDS_Y_DELAY::write(3);
            } else if (rto->videoStandardInput == 6 || rto->videoStandardInput == 7) {
                GBS::ADC_FLTR::write(1);
                GBS::PLLAD_KS::write(0);
                GBS::IF_PRGRSV_CNTRL::write(1);
                GBS::IF_HS_DEC_FACTOR::write(0);
                GBS::INPUT_FORMATTER_02::write(0x74);
                GBS::VDS_Y_DELAY::write(3);
            } else if (rto->videoStandardInput == 8) {

                uint32_t pllRate = 0;
                for (int i = 0; i < 8; i++) {
                    pllRate += getPllRate();
                }
                pllRate /= 8;
                if (pllRate > 200) {
                    if (pllRate < 1800) {
                        GBS::PLLAD_FS::write(0); 
                    }
                }
                GBS::PLLAD_ICP::write(6);
                GBS::ADC_FLTR::write(1);
                GBS::IF_HB_ST::write(30);

                GBS::IF_HBIN_SP::write(0x60);
                if (rto->presetID == 0x1) {
                    GBS::VDS_VSCALE::write(410);
                } else if (rto->presetID == 0x2) {
                    GBS::VDS_VSCALE::write(402);
                } else if (rto->presetID == 0x3) {
                    GBS::VDS_VSCALE::write(546);
                } else if (rto->presetID == 0x5) {
                    GBS::VDS_VSCALE::write(400);
                }
            }
        }

        if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            rto->isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
        }

        resetDebugPort();

        boolean avoidAutoBest = 0;
        if (rto->syncTypeCsync) {
            if (GBS::TEST_BUS_2F::read() == 0) {
                delay(4);
                if (GBS::TEST_BUS_2F::read() == 0) {
                    optimizeSogLevel();
                    avoidAutoBest = 1;
                    delay(4);
                }
            }
        }

        latchPLLAD();

        if (rto->isCustomPreset) {

            if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 || rto->videoStandardInput == 8) {
                GBS::MADPT_Y_DELAY_UV_DELAY::write(1);
            }

            if (GBS::DEC1_BYPS::read() && GBS::DEC2_BYPS::read()) {
                rto->osr = 1;
            } else if (GBS::DEC1_BYPS::read() && !GBS::DEC2_BYPS::read()) {
                rto->osr = 2;
            } else {
                rto->osr = 4;
            }

            if (GBS::PLL648_CONTROL_01::read() == 0x75 && GBS::GBS_PRESET_DISPLAY_CLOCK::read() != 0) {
                GBS::PLL648_CONTROL_01::write(GBS::GBS_PRESET_DISPLAY_CLOCK::read());
            } else if (GBS::GBS_PRESET_DISPLAY_CLOCK::read() == 0) {
            }
        }

        if (rto->presetIsPalForce60) {
            if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() != 1) {
                uint16_t vshift = 56;
                if (rto->presetID == 0x5) {
                    GBS::IF_VB_SP::write(4);
                } else {
                    GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + vshift);
                }
                GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
                GBS::GBS_OPTION_PALFORCED60_ENABLED::write(1);
            }
        }

        GBS::ADC_TEST_04::write(0x02); // 1:0 REF test resistance selection 4:2REF test current selection
        GBS::ADC_TEST_0C::write(0x12);
        GBS::ADC_TA_05_CTRL::write(0x02); // ADC test enable BIT0    ADC test bus control bit   BIT4:1

        if (uopt->enableAutoGain == 1) {
            if (adco->r_gain == 0) {
                setAdcGain(AUTO_GAIN_INIT);
                GBS::DEC_TEST_ENABLE::write(1);
            } else {
                GBS::ADC_RGCTRL::write(adco->r_gain); 
                GBS::ADC_GGCTRL::write(adco->g_gain); 
                GBS::ADC_BGCTRL::write(adco->b_gain); 
                GBS::DEC_TEST_ENABLE::write(1);
            }
        } else {
            GBS::DEC_TEST_ENABLE::write(0);
        }

        if (adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
            GBS::ADC_ROFCTRL::write(adco->r_off);
            GBS::ADC_GOFCTRL::write(adco->g_off);
            GBS::ADC_BOFCTRL::write(adco->b_off);
        }

        GBS::IF_AUTO_OFST_U_RANGE::write(0);
        GBS::IF_AUTO_OFST_V_RANGE::write(0);
        GBS::IF_AUTO_OFST_PRD::write(0);
        GBS::IF_AUTO_OFST_EN::write(0);

        if (uopt->wantVdsLineFilter) {
            GBS::VDS_D_RAM_BYPS::write(0);
        } else {
            GBS::VDS_D_RAM_BYPS::write(1);
        }

        if (uopt->wantPeaking) {
            GBS::VDS_PK_Y_H_BYPS::write(0);
        } else {
            GBS::VDS_PK_Y_H_BYPS::write(1);
        }

        GBS::VDS_TAP6_BYPS::write(0);

        if (uopt->wantStepResponse) {

            if (rto->presetID != 0x05 && rto->presetID != 0x15) {
                GBS::VDS_UV_STEP_BYPS::write(0);
            } else {
                GBS::VDS_UV_STEP_BYPS::write(1);
            }
        } else {
            GBS::VDS_UV_STEP_BYPS::write(1);
        }

        externalClockGenResetClock();

        Menu::init();
        FrameSync::cleanup();
        rto->syncLockFailIgnore = 16;

        GBS::VDS_SYNC_EN::write(0);
        GBS::VDS_FLOCK_EN::write(0);

        if (!rto->outModeHdBypass && rto->autoBestHtotalEnabled &&
            GBS::GBS_OPTION_SCALING_RGBHV::read() == 0 && !avoidAutoBest &&
            (rto->videoStandardInput >= 1 && rto->videoStandardInput <= 4)) {

            updateCoastPosition(0);
            delay(1);
            resetInterruptNoHsyncBadBit();
            resetInterruptSogBadBit();
            delay(10);

            if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
                GBS::PAD_SYNC_OUT_ENZ::write(0); //
            }
            delay(70);

            for (uint8_t i = 0; i < 4; i++) {
                if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                    optimizeSogLevel();
                    resetInterruptSogBadBit();
                    delay(40);
                } else if (getStatus16SpHsStable() && getStatus16SpHsStable()) {
                    delay(1);
                    if (getVideoMode() == rto->videoStandardInput) {
                        boolean ok = 0;
                        float sfr = getSourceFieldRate(0);

                        if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
                            if (sfr > 58.6f && sfr < 61.4f)
                                ok = 1;
                        } else if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
                            if (sfr > 49.1f && sfr < 51.1f)
                                ok = 1;
                        }
                        if (ok) {
                            runAutoBestHTotal();
                            delay(1);
                            break;
                        }
                    }
                }
                delay(10);
            }
        } else {

            delay(10);

            if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
                GBS::PAD_SYNC_OUT_ENZ::write(0); //
            }
            delay(20);
            updateCoastPosition(0);
            updateClampPosition();
        }

        if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); //
        }

        if (!rto->isCustomPreset) {
            if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {

                if (GBS::VPERIOD_IF::read() == 523) {
                    GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 4);
                    GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 4);
                }
            }
        }

        GBS::VDS_EXT_HB_ST::write(GBS::VDS_DIS_HB_ST::read());
        GBS::VDS_EXT_HB_SP::write(GBS::VDS_DIS_HB_SP::read());
        GBS::VDS_EXT_VB_ST::write(GBS::VDS_DIS_VB_ST::read());
        GBS::VDS_EXT_VB_SP::write(GBS::VDS_DIS_VB_SP::read());

        GBS::VDS_VSYN_SIZE1::write(GBS::VDS_VSYNC_RST::read() + 2);
        GBS::VDS_VSYN_SIZE2::write(GBS::VDS_VSYNC_RST::read() + 2);
        GBS::VDS_FRAME_RST::write(4);

        GBS::VDS_FRAME_NO::write(1);
        GBS::VDS_FR_SELECT::write(1);

        resetDigital();

        resetPLLAD();
        GBS::PLLAD_LEN::write(1); //

        if (!rto->isCustomPreset) {
            GBS::VDS_IN_DREG_BYPS::write(0);
            GBS::PLLAD_R::write(3);
            GBS::PLLAD_S::write(3);
            GBS::PLL_R::write(1);
            GBS::PLL_S::write(2);
            GBS::DEC_IDREG_EN::write(1);
            GBS::DEC_WEN_MODE::write(1);

            GBS::CAP_SAFE_GUARD_EN::write(0);

            GBS::PB_CUT_REFRESH::write(1);
            GBS::RFF_LREQ_CUT::write(0);
            GBS::CAP_REQ_OVER::write(0);
            GBS::CAP_STATUS_SEL::write(1);
            GBS::PB_REQ_SEL::write(3);

            GBS::RFF_WFF_OFFSET::write(0x0);
            if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) {

                GBS::PB_CAP_OFFSET::write(GBS::PB_FETCH_NUM::read() + 4);
            }
        }

        if (!rto->outModeHdBypass) {
            ResetSDRAM();
        }

        setAndUpdateSogLevel(rto->currentLevelSOG);

        if (rto->presetID != 0x06 && rto->presetID != 0x16) {

            GBS::IF_VS_SEL::write(0);
            GBS::IF_VS_FLIP::write(1);
        }

        GBS::SP_CLP_SRC_SEL::write(0);
        GBS::SP_CS_CLP_ST::write(32);
        GBS::SP_CS_CLP_SP::write(48);

        if (!uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); 
        }
        GBS::DAC_RGBS_PWDNZ::write(1); 
        GBS::DAC_RGBS_SPD::write(0);
        GBS::DAC_RGBS_S0ENZ::write(0); //
        GBS::DAC_RGBS_S1EN::write(1);

        rto->useHdmiSyncFix = 0; // 

        GBS::SP_H_PROTECT::write(0); // 
        if (rto->videoStandardInput >= 5) {
            GBS::SP_DIS_SUB_COAST::write(1); //
        }

        if (rto->syncTypeCsync) {
            GBS::SP_EXT_SYNC_SEL::write(1);
        }

        rto->coastPositionIsSet = false;
        rto->clampPositionIsSet = false;

        if (rto->outModeHdBypass) {
            GBS::INTERRUPT_CONTROL_01::write(0xff);
            GBS::INTERRUPT_CONTROL_00::write(0xff);
            GBS::INTERRUPT_CONTROL_00::write(0x00);
            unfreezeVideo();

            return;
        }

        if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) {
            rto->videoStandardInput = 14;
        }

        if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 0) {
            unsigned long timeout = millis();
            while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
                delay(4);
                handleWiFi(0);
                updateSpDynamic(0);
            }
            while ((getVideoMode() == 0) && (millis() - timeout < 1505)) {
                delay(4);
                handleWiFi(0);
                updateSpDynamic(0);
            }
            timeout = millis() - timeout;
            if (timeout > 1000) {
            }
            if (timeout >= 1500) {
                if (rto->currentLevelSOG >= 7) {
                    optimizeSogLevel();
                    delay(300);
                }
            }
        }

        updateClampPosition();
        if (rto->clampPositionIsSet) {
            if (GBS::SP_NO_CLAMP_REG::read() == 1) {
                GBS::SP_NO_CLAMP_REG::write(0);
            }
        }

        updateSpDynamic(0);

        if (!rto->syncWatcherEnabled) {
            GBS::SP_NO_CLAMP_REG::write(0);
        }

        setAndUpdateSogLevel(rto->currentLevelSOG);

        GBS::INTERRUPT_CONTROL_01::write(0xff);
        GBS::INTERRUPT_CONTROL_00::write(0xff);
        GBS::INTERRUPT_CONTROL_00::write(0x00);

        // OutputComponentOrVGA();

        if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) {
            rto->autoBestHtotalEnabled = 0;
            if (rto->applyPresetDoneStage == 11) {

                rto->applyPresetDoneStage = 1;
            } else {
                rto->applyPresetDoneStage = 10;
            }
        } else {

            rto->applyPresetDoneStage = 1;
        }

        unfreezeVideo();

        if (uopt->enableFrameTimeLock) {
            activeFrameTimeLockInitialSteps();
        }
    }
}

void applyPresets(uint8_t result)
{

    // printf("result %d \n", result);
    if (!rto->boardHasPower) {
        return;
    }

    if (result == 14) {
        if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            rto->inputIsYpBpR = 0;
            if (GBS::STATUS_SYNC_PROC_VSACT::read() == 0) {
                rto->syncTypeCsync = 1;
            } else {
                rto->syncTypeCsync = 0;
            }
        }
    }

    boolean waitExtra = 0;
    if (rto->outModeHdBypass || rto->videoStandardInput == 15 || rto->videoStandardInput == 0) {
        waitExtra = 1;
        if (result <= 4 || result == 14 || result == 8 || result == 9) {
            GBS::SFTRST_IF_RSTZ::write(1);
            GBS::SFTRST_VDS_RSTZ::write(1);
            GBS::SFTRST_DEC_RSTZ::write(1);
        }
    }
    rto->presetIsPalForce60 = 0;
    rto->outModeHdBypass = 0; // 

    GBS::GBS_PRESET_CUSTOM::write(0);
    rto->isCustomPreset = false;

    if (GBS::ADC_UNUSED_62::read() != 0x00) {

        if (uopt->presetPreference != 2) {
            serialCommand = 'D';
        }
    }

    if (result == 0) {

        result = 3;
        GBS::ADC_INPUT_SEL::write(1);
        delay(100);
        if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            rto->inputIsYpBpR = 0;
            rto->syncWatcherEnabled = 1;
            if (GBS::STATUS_SYNC_PROC_VSACT::read() == 0) {
                rto->syncTypeCsync = 1;
            } else {
                rto->syncTypeCsync = 0;
            }
        } else {
            GBS::ADC_INPUT_SEL::write(0);
            delay(100);
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                rto->inputIsYpBpR = 1;
                rto->syncTypeCsync = 1;
                rto->syncWatcherEnabled = 1;
            } else // 
            {
                // setResetParameters();
                // rto->presetID = 0;
                // printf("End \n");
                return;
            }
        }
    }

    if (uopt->PalForce60 == 1) 
    {
        if (uopt->presetPreference != 2) 
        {

            if (result == 2 || result == 4) 
            {
                Serial.println(F("PAL@50 to 60Hz"));
                rto->presetIsPalForce60 = 1;
            }
            if (result == 2) {
                result = 1;
            }
            if (result == 4) {
                result = 3;
            }
        }
    }

    auto applySavedBypassPreset = [&result]() -> bool {
        uint8_t rawPresetId = GBS::GBS_PRESET_ID::read();
        if (rawPresetId == PresetHdBypass) {

            rto->videoStandardInput = result;

            setOutModeHdBypass(true);
            return true;
        }
        if (rawPresetId == PresetBypassRGBHV) {
        }
        return false;
    };

    if (result == 1 || result == 3 || result == 8 || result == 9 || result == 14) {

        // printf(" This == 3 \n");
        if (uopt->presetPreference == 0) {
            writeProgramArrayNew(ntsc_240p, false);
        } else if (uopt->presetPreference == 1) {
            writeProgramArrayNew(ntsc_720x480, false);   //ntsc_720x480   pal_768x576
        } else if (uopt->presetPreference == 3) {
            writeProgramArrayNew(ntsc_1280x720, false);
        }
#if defined(ESP8266)
        else if (uopt->presetPreference == OutputCustomized) {
            const uint8_t *preset = loadPresetFromSPIFFS(result);
            writeProgramArrayNew(preset, false);
            if (applySavedBypassPreset()) {
                return;
            }
        } else if (uopt->presetPreference == 4) {
            if (uopt->matchPresetSource && (result != 8) && (GBS::GBS_OPTION_SCALING_RGBHV::read() == 0)) {
                writeProgramArrayNew(ntsc_240p, false);
            } else {
                writeProgramArrayNew(ntsc_1280x1024, false);
            }
        }
#endif
        else if (uopt->presetPreference == 5) {
            writeProgramArrayNew(ntsc_1920x1080, false);   //ntsc_1920x1080
        } else if (uopt->presetPreference == 6) {
            writeProgramArrayNew(ntsc_downscale, false);
        }
    } else if (result == 2 || result == 4) {

        if (uopt->presetPreference == 0) {
            if (uopt->matchPresetSource) {
                writeProgramArrayNew(pal_1280x1024, false);
            } else {
                writeProgramArrayNew(pal_240p, false);
            }
        } else if (uopt->presetPreference == 1) {
            writeProgramArrayNew(pal_768x576, false);
        } else if (uopt->presetPreference == 3) {
            writeProgramArrayNew(pal_1280x720, false);
        }
#if defined(ESP8266)
        else if (uopt->presetPreference == OutputCustomized) {
            const uint8_t *preset = loadPresetFromSPIFFS(result);
            writeProgramArrayNew(preset, false);
            if (applySavedBypassPreset()) {
                return;
            }
        } else if (uopt->presetPreference == 4) {
            writeProgramArrayNew(pal_1280x1024, false);
        }
#endif
        else if (uopt->presetPreference == 5) {
            writeProgramArrayNew(pal_1920x1080, false);
        } else if (uopt->presetPreference == 6) {
            writeProgramArrayNew(pal_downscale, false);
        }
    } else if (result == 5 || result == 6 || result == 7 || result == 13) {

        rto->videoStandardInput = result;
        setOutModeHdBypass(false);
        return;
    } else if (result == 15) {
        bypassModeSwitch_RGBHV();
        return;
    }

    rto->videoStandardInput = result;
    if (waitExtra) {

        delay(400);
    }
    doPostPresetLoadSteps();
}

void unfreezeVideo()
{
    GBS::CAPTURE_ENABLE::write(1);
}

void freezeVideo()
{
    GBS::CAPTURE_ENABLE::write(0);
}

uint8_t getVideoMode()
{
    uint8_t detectedMode = 0;

    if (rto->videoStandardInput >= 14) {
        detectedMode = GBS::STATUS_16::read();
        if ((detectedMode & 0x0a) > 0) {
            return rto->videoStandardInput;
        } else {
            return 0;
        }
    }

    detectedMode = GBS::STATUS_00::read();

    if ((detectedMode & 0x07) == 0x07) {
        if ((detectedMode & 0x80) == 0x80) {
            if ((detectedMode & 0x08) == 0x08)
                return 1;
            if ((detectedMode & 0x20) == 0x20)
                return 2;
            if ((detectedMode & 0x10) == 0x10)
                return 3;
            if ((detectedMode & 0x40) == 0x40)
                return 4;
        }

        detectedMode = GBS::STATUS_03::read();
        if ((detectedMode & 0x10) == 0x10) {
            return 5;
        }

        if (rto->videoStandardInput == 4) {
            detectedMode = GBS::STATUS_04::read();
            if ((detectedMode & 0xFF) == 0x80) {
                return 4;
            }
        }
    }

    detectedMode = GBS::STATUS_04::read();
    if ((detectedMode & 0x20) == 0x20) {
        if ((detectedMode & 0x61) == 0x61) {

            if (GBS::VPERIOD_IF::read() < 1160) {
                return 6;
            }
        }
        if ((detectedMode & 0x10) == 0x10) {
            if ((detectedMode & 0x04) == 0x04) {
                return 8;
            }
            return 7;
        }
    }

    if ((GBS::STATUS_05::read() & 0x0c) == 0x00) // 
    {
        if (GBS::STATUS_00::read() == 0x07) // 
        {
            if ((GBS::STATUS_03::read() & 0x02) == 0x02) // 
            {
                if (rto->inputIsYpBpR) // && Info_sate == 0 )//&& SeleInputSource == S_YUV )

                    return 13;
                else if (rto->inputIsYpBpR == false) // && Info_sate == 0 )//&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs) )

                    return 15;
            } else {
                static uint8_t XGA_60HZ = GBS::MD_XGA_60HZ_CNTRL::read();
                static uint8_t XGA_70HZ = GBS::MD_XGA_70HZ_CNTRL::read();
                static uint8_t XGA_75HZ = GBS::MD_XGA_75HZ_CNTRL::read();
                static uint8_t XGA_85HZ = GBS::MD_XGA_85HZ_CNTRL::read();

                static uint8_t SXGA_60HZ = GBS::MD_SXGA_60HZ_CNTRL::read();
                static uint8_t SXGA_75HZ = GBS::MD_SXGA_75HZ_CNTRL::read();
                static uint8_t SXGA_85HZ = GBS::MD_SXGA_85HZ_CNTRL::read();

                static uint8_t SVGA_60HZ = GBS::MD_SVGA_60HZ_CNTRL::read();
                static uint8_t SVGA_75HZ = GBS::MD_SVGA_75HZ_CNTRL::read();
                static uint8_t SVGA_85HZ = GBS::MD_SVGA_85HZ_CNTRL::read();

                static uint8_t VGA_75HZ = GBS::MD_VGA_75HZ_CNTRL::read();
                static uint8_t VGA_85HZ = GBS::MD_VGA_85HZ_CNTRL::read();

                short hSkew = random(-2, 2); //-2, 2


                GBS::MD_XGA_60HZ_CNTRL::write(XGA_60HZ + hSkew);
                GBS::MD_XGA_70HZ_CNTRL::write(XGA_70HZ + hSkew);
                GBS::MD_XGA_75HZ_CNTRL::write(XGA_75HZ + hSkew);
                GBS::MD_XGA_85HZ_CNTRL::write(XGA_85HZ + hSkew);
                GBS::MD_SXGA_60HZ_CNTRL::write(SXGA_60HZ + hSkew);
                GBS::MD_SXGA_75HZ_CNTRL::write(SXGA_75HZ + hSkew);
                GBS::MD_SXGA_85HZ_CNTRL::write(SXGA_85HZ + hSkew);
                GBS::MD_SVGA_60HZ_CNTRL::write(SVGA_60HZ + hSkew);
                GBS::MD_SVGA_75HZ_CNTRL::write(SVGA_75HZ + hSkew);
                GBS::MD_SVGA_85HZ_CNTRL::write(SVGA_85HZ + hSkew);
                GBS::MD_VGA_75HZ_CNTRL::write(VGA_75HZ + hSkew);
                GBS::MD_VGA_85HZ_CNTRL::write(VGA_85HZ + hSkew);
            }
        }
    }

    detectedMode = GBS::STATUS_00::read();
    if ((detectedMode & 0x2F) == 0x07) {
        detectedMode = GBS::STATUS_16::read();
        if ((detectedMode & 0x02) == 0x02) {
            uint16_t lineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
            for (uint8_t i = 0; i < 2; i++) {
                delay(2);
                if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (lineCount - 1) ||
                    GBS::STATUS_SYNC_PROC_VTOTAL::read() > (lineCount + 1)) {
                    lineCount = 0;
                    rto->notRecognizedCounter = 0; //
                    break;
                }
                detectedMode = GBS::STATUS_00::read();
                if ((detectedMode & 0x2F) != 0x07) {
                    lineCount = 0;
                    rto->notRecognizedCounter = 0; //
                    break;
                }
            }
            if (lineCount != 0 && rto->notRecognizedCounter < 255) {
                rto->notRecognizedCounter++;
            }
        } else {
            rto->notRecognizedCounter = 0; //
        }
    } else {
        rto->notRecognizedCounter = 0; //
    }

    if (rto->notRecognizedCounter == 255) {
        return 9;
    }

    return 0;
}

boolean getSyncPresent() //
{
    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(0x0f);
    }

    uint16_t readout = GBS::TEST_BUS::read();

    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }

    if (readout > 0x0180) {
        return true;
    }

    return false;
}

boolean getStatus00IfHsVsStable()
{
    return ((GBS::STATUS_00::read() & 0x04) == 0x04) ? 1 : 0;
}

boolean getStatus16SpHsStable()
{

    // printf("rto->videoStandardInput = %d \n",rto->videoStandardInput);
    if (rto->videoStandardInput == 15) {
        if (GBS::STATUS_INT_INP_NO_SYNC::read() == 0) {
            // printf("\n stable from \n");
            return true;
        } else {
            resetInterruptNoHsyncBadBit();
            // printf("\n false from 1\n");
            return false;
        }
    }

    uint8_t status16 = GBS::STATUS_16::read();
    if ((status16 & 0x02) == 0x02) {
        if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
            if ((status16 & 0x01) != 0x01) {
                // printf("\n stable from 1\n");
                return true;
            }
        } else {
            // printf("\n stable from 2\n");
            return true;
        }
    }

    // printf("\n false from 2\n");
    return false;
}

void setOverSampleRatio(uint8_t newRatio, boolean prepareOnly)
{
    uint8_t ks = GBS::PLLAD_KS::read();

    bool hi_res = rto->videoStandardInput == 8 || rto->videoStandardInput == 4 || rto->videoStandardInput == 3;
    bool bypass = rto->presetID == PresetHdBypass;

    switch (newRatio) {
        case 1:
            if (ks == 0)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 1)
                GBS::PLLAD_CKOS::write(1);
            if (ks == 2)
                GBS::PLLAD_CKOS::write(2);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(3);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::DEC1_BYPS::write(1);
            GBS::DEC2_BYPS::write(1);

            if (hi_res && !bypass) {
                GBS::ADC_CLK_ICLK1X::write(1);
            }

            rto->osr = 1;
            break;
        case 2:
            if (ks == 0) {
                setOverSampleRatio(1, false);
                return;
            }
            if (ks == 1)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 2)
                GBS::PLLAD_CKOS::write(1);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(2);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::DEC2_BYPS::write(0);
            GBS::DEC1_BYPS::write(1);

            if (hi_res && !bypass) {
                GBS::PLLAD_CKOS::write(GBS::PLLAD_CKOS::read() + 1);
            }
            rto->osr = 2;

            break;
        case 4:
            if (ks == 0) {
                setOverSampleRatio(1, false);
                return;
            }
            if (ks == 1) {
                setOverSampleRatio(1, false);
                return;
            }
            if (ks == 2)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(1);
            GBS::ADC_CLK_ICLK2X::write(1);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::DEC1_BYPS::write(0);
            GBS::DEC2_BYPS::write(0);

            rto->osr = 4;

            break;
        default:
            break;
    }

    if (!prepareOnly)
        latchPLLAD();
}

void togglePhaseAdjustUnits()
{
    GBS::PA_SP_BYPSZ::write(0);
    GBS::PA_SP_BYPSZ::write(1);
    delay(2);
    GBS::PA_ADC_BYPSZ::write(0);
    GBS::PA_ADC_BYPSZ::write(1);
    delay(2);
}

void advancePhase()
{
    rto->phaseADC = (rto->phaseADC + 1) & 0x1f;
    setAndLatchPhaseADC();
}

void movePhaseThroughRange()
{
    for (uint8_t i = 0; i < 128; i++) {
        advancePhase();
    }
}

void setAndLatchPhaseSP()
{
    GBS::PA_SP_LAT::write(0);
    GBS::PA_SP_S::write(rto->phaseSP);
    GBS::PA_SP_LAT::write(1);
}

void setAndLatchPhaseADC()
{
    GBS::PA_ADC_LAT::write(0);
    GBS::PA_ADC_S::write(rto->phaseADC);
    GBS::PA_ADC_LAT::write(1);
}

void nudgeMD()
{
    GBS::MD_VS_FLIP::write(!GBS::MD_VS_FLIP::read()); 
    GBS::MD_VS_FLIP::write(!GBS::MD_VS_FLIP::read());
}

void updateSpDynamic(boolean withCurrentVideoModeCheck)
{
    if (!rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    uint8_t vidModeReadout = getVideoMode();
    if (vidModeReadout == 0) {
        vidModeReadout = getVideoMode();
    }

    if (rto->videoStandardInput == 0 && vidModeReadout == 0) {
        if (GBS::SP_DLT_REG::read() > 0x30)
            GBS::SP_DLT_REG::write(0x30);
        else
            GBS::SP_DLT_REG::write(0xC0);
        return;
    }

    if (vidModeReadout == 0 && withCurrentVideoModeCheck) {
        if ((rto->noSyncCounter % 16) <= 8 && rto->noSyncCounter != 0) {
            GBS::SP_DLT_REG::write(0x30);
        } else if ((rto->noSyncCounter % 16) > 8 && rto->noSyncCounter != 0) {
            GBS::SP_DLT_REG::write(0xC0);
        } else {
            GBS::SP_DLT_REG::write(0x30);
        }
        GBS::SP_H_PULSE_IGNOR::write(0x02);

        GBS::SP_H_CST_ST::write(0x10);
        GBS::SP_H_CST_SP::write(0x100);
        GBS::SP_H_COAST::write(0);
        GBS::SP_H_TIMER_VAL::write(0x3a);
        if (rto->syncTypeCsync) {
            GBS::SP_COAST_INV_REG::write(1);
        }
        rto->coastPositionIsSet = false;
        return;
    }

    if (rto->syncTypeCsync) {
        GBS::SP_COAST_INV_REG::write(0);
    }

    if (rto->videoStandardInput != 0) {
        if (rto->videoStandardInput <= 2) {
            GBS::SP_PRE_COAST::write(7);
            GBS::SP_POST_COAST::write(3);
            GBS::SP_DLT_REG::write(0xC0);
            GBS::SP_H_TIMER_VAL::write(0x28);

            if (rto->syncTypeCsync) {
                uint16_t hPeriod = GBS::HPERIOD_IF::read();
                for (int i = 0; i < 16; i++) {
                    if (hPeriod == 511 || hPeriod < 200) {
                        hPeriod = GBS::HPERIOD_IF::read();
                        if (i == 15) {
                            hPeriod = 300;
                            break;
                        }
                    } else {
                        break;
                    }
                    ESP.wdtFeed();
                    delayMicroseconds(100);
                }

                uint16_t ignoreLength = hPeriod * 0.081f;
                if (hPeriod <= 200) {
                    ignoreLength = 0x18;
                }

                double ratioHs, ratioHsAverage = 0.0;
                uint8_t testOk = 0;
                for (int i = 0; i < 30; i++) {
                    ratioHs = (double)GBS::STATUS_SYNC_PROC_HLOW_LEN::read() / (double)(GBS::STATUS_SYNC_PROC_HTOTAL::read() + 1);
                    if (ratioHs > 0.041 && ratioHs < 0.152) {
                        testOk++;
                        ratioHsAverage += ratioHs;
                        if (testOk == 12) {
                            ratioHs = ratioHsAverage / testOk;
                            break;
                        }
                        ESP.wdtFeed();
                        delayMicroseconds(30);
                    }
                }
                if (testOk != 12) {
                    ratioHs = 0.032;
                }

                uint16_t pllDiv = GBS::PLLAD_MD::read();
                ignoreLength = ignoreLength + (pllDiv * (ratioHs * 0.38));

                if (ignoreLength > GBS::SP_H_PULSE_IGNOR::read() || GBS::SP_H_PULSE_IGNOR::read() >= 0x90) {
                    if (ignoreLength > 0x90) {
                        ignoreLength = 0x90;
                    }
                    if (ignoreLength >= 0x1A && ignoreLength <= 0x42) {
                        ignoreLength = 0x1A;
                    }
                    if (ignoreLength != GBS::SP_H_PULSE_IGNOR::read()) {
                        GBS::SP_H_PULSE_IGNOR::write(ignoreLength);
                        rto->coastPositionIsSet = 0; // coast position setting
                    }
                }
            }
        } else if (rto->videoStandardInput <= 4) {
            GBS::SP_PRE_COAST::write(7);
            GBS::SP_POST_COAST::write(6);

            GBS::SP_DLT_REG::write(0xA0);
            GBS::SP_H_PULSE_IGNOR::write(0x0E);
        } else if (rto->videoStandardInput == 5) {
            GBS::SP_PRE_COAST::write(7);
            GBS::SP_POST_COAST::write(7);
            GBS::SP_DLT_REG::write(0x30);
            GBS::SP_H_PULSE_IGNOR::write(0x08);
        } else if (rto->videoStandardInput <= 7) {
            GBS::SP_PRE_COAST::write(9);
            GBS::SP_POST_COAST::write(18);
            GBS::SP_DLT_REG::write(0x70);

            GBS::SP_H_PULSE_IGNOR::write(0x06);
        } else if (rto->videoStandardInput >= 13) {
            if (rto->syncTypeCsync == false) {
                GBS::SP_PRE_COAST::write(0x00);
                GBS::SP_POST_COAST::write(0x00);
                GBS::SP_H_PULSE_IGNOR::write(0xff); 
                GBS::SP_DLT_REG::write(0x00);
            } else {
                GBS::SP_PRE_COAST::write(0x04);
                GBS::SP_POST_COAST::write(0x07);
                GBS::SP_DLT_REG::write(0x70);
                GBS::SP_H_PULSE_IGNOR::write(0x02);
            }
        }
    }
}

void updateCoastPosition(boolean autoCoast) // Updated coastal locations
{
    if (((rto->videoStandardInput == 0) || (rto->videoStandardInput > 14)) ||
        !rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    uint32_t accInHlength = 0;
    uint16_t prevInHlength = GBS::HPERIOD_IF::read();
    for (uint8_t i = 0; i < 8; i++) {

        uint16_t thisInHlength = GBS::HPERIOD_IF::read();
        if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
            accInHlength += thisInHlength;
        } else {
            return;
        }
        if (!getStatus16SpHsStable()) {
            return;
        }

        prevInHlength = thisInHlength;
    }
    accInHlength = (accInHlength * 4) / 8;

    if (accInHlength >= 2040) {
        accInHlength = 1716;
    }

    if (accInHlength <= 240) {

        if (GBS::STATUS_SYNC_PROC_VTOTAL::read() <= 322) {
            delay(4);
            if (GBS::STATUS_SYNC_PROC_VTOTAL::read() <= 322) {
                accInHlength = 2000;

                if (rto->syncTypeCsync && rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                    if (GBS::PLLAD_ICP::read() >= 5 && GBS::PLLAD_FS::read() == 1) {
                        GBS::PLLAD_ICP::write(5);
                        GBS::PLLAD_FS::write(0); // FS、VCO Gain Selection
                        latchPLLAD();
                        rto->phaseIsSet = 0;
                    }
                }
            }
        }
    }

    if (accInHlength > 32) {
        if (autoCoast) {

            GBS::SP_H_CST_ST::write((uint16_t)(accInHlength * 0.0562f));
            GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.1550f));
            GBS::SP_HCST_AUTO_EN::write(1);
        } else {

            GBS::SP_H_CST_ST::write(0x10);

            GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.968f));

            GBS::SP_HCST_AUTO_EN::write(0);
        }
        rto->coastPositionIsSet = 1;
    }
}

void updateClampPosition() // Update Clamp Position
{
    if ((rto->videoStandardInput == 0) || !rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    if (getVideoMode() == 0) {
        return;
    }

    if (rto->inputIsYpBpR) // && Info_sate == 0 )//&& SeleInputSource == S_YUV )
    {
        GBS::SP_CLAMP_MANUAL::write(0);
    } else if (rto->inputIsYpBpR == false) // && Info_sate == 0 )//&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs) )
    {
        GBS::SP_CLAMP_MANUAL::write(1); 
    }

    uint32_t accInHlength = 0;
    uint16_t prevInHlength = 0;
    uint16_t thisInHlength = 0;
    if (rto->syncTypeCsync)
        prevInHlength = GBS::HPERIOD_IF::read();
    else
        prevInHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
    for (uint8_t i = 0; i < 16; i++) {
        if (rto->syncTypeCsync)
            thisInHlength = GBS::HPERIOD_IF::read();
        else
            thisInHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
            accInHlength += thisInHlength;
        } else {

            return;
        }
        if (!getStatus16SpHsStable()) {
            return;
        }

        prevInHlength = thisInHlength;
        ESP.wdtFeed();
        delayMicroseconds(100);
    }
    accInHlength = accInHlength / 16;

    if (accInHlength > 4095) {
        return;
    }

    uint16_t oldClampST = GBS::SP_CS_CLP_ST::read();
    uint16_t oldClampSP = GBS::SP_CS_CLP_SP::read();
    float multiSt = rto->syncTypeCsync == 1 ? 0.032f : 0.010f;
    float multiSp = rto->syncTypeCsync == 1 ? 0.174f : 0.058f;
    uint16_t start = 1 + (accInHlength * multiSt);
    uint16_t stop = 2 + (accInHlength * multiSp);

    if (rto->inputIsYpBpR) // && Info_sate == 0 )//&& SeleInputSource == S_YUV )

    {

        multiSt = rto->syncTypeCsync == 1 ? 0.089f : 0.032f;
        start = 1 + (accInHlength * multiSt);

        if (rto->outModeHdBypass) {
            if (videoStandardInputIsPalNtscSd()) {
                start += 0x60;
                stop += 0x60;
            }

            GBS::HD_BLK_GY_DATA::write(0x05);
            GBS::HD_BLK_BU_DATA::write(0x00);
            GBS::HD_BLK_RV_DATA::write(0x00);
        }
    }

    if ((start < (oldClampST - 1) || start > (oldClampST + 1)) ||
        (stop < (oldClampSP - 1) || stop > (oldClampSP + 1))) {
        GBS::SP_CS_CLP_ST::write(start);
        GBS::SP_CS_CLP_SP::write(stop);
    }

    rto->clampPositionIsSet = true;
}

void setOutModeHdBypass(bool regsInitialized) // Set output mode HD bypass
{
    if (!rto->boardHasPower) {
        return;
    }

    rto->autoBestHtotalEnabled = false;
    rto->outModeHdBypass = 1;

    externalClockGenResetClock();
    updateSpDynamic(0);
    loadHdBypassSection();
    if (GBS::ADC_UNUSED_62::read() != 0x00) {
        if (uopt->presetPreference != 2) {
            serialCommand = 'D';
        }
    }

    GBS::SP_NO_COAST_REG::write(0);
    GBS::SP_COAST_INV_REG::write(0);

    FrameSync::cleanup();
    GBS::ADC_UNUSED_62::write(0x00);
    GBS::RESET_CONTROL_0x46::write(0x00);
    GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::PA_ADC_BYPSZ::write(1);
    GBS::PA_SP_BYPSZ::write(1);

    GBS::GBS_PRESET_ID::write(PresetHdBypass);

    if (!regsInitialized) {
        GBS::GBS_PRESET_CUSTOM::write(0);
    }
    doPostPresetLoadSteps();

    resetDebugPort();

    rto->autoBestHtotalEnabled = false;
    GBS::OUT_SYNC_SEL::write(1);

    GBS::PLL_CKIS::write(0);
    GBS::PLL_DIVBY2Z::write(0);

    GBS::PAD_OSC_CNTRL::write(1);
    GBS::PLL648_CONTROL_01::write(0x35);
    GBS::PLL648_CONTROL_03::write(0x00);
    GBS::PLL_LEN::write(1);
    GBS::DAC_RGBS_R0ENZ::write(1); // RDAC output follows input R data
    GBS::DAC_RGBS_G0ENZ::write(1); // GDAC Output follows input G data
    GBS::DAC_RGBS_B0ENZ::write(1); // BDAC Output follows input B data
    GBS::DAC_RGBS_S1EN::write(1);

    GBS::PAD_TRI_ENZ::write(1);
    GBS::PLL_MS::write(2);
    GBS::MEM_PAD_CLK_INVERT::write(0);
    GBS::RESET_CONTROL_0x47::write(0x1f);

    GBS::DAC_RGBS_BYPS2DAC::write(1);
    GBS::SP_HS_LOOP_SEL::write(1);     
    GBS::SP_HS_PROC_INV_REG::write(0); 
    GBS::SP_CS_P_SWAP::write(0);
    GBS::SP_HS2PLL_INV_REG::write(0);

    GBS::PB_BYPASS::write(1);
    GBS::PLLAD_MD::write(2345);
    GBS::PLLAD_KS::write(2);
    setOverSampleRatio(2, true);
    GBS::PLLAD_ICP::write(5);
    GBS::PLLAD_FS::write(1);

    if (rto->inputIsYpBpR) // && Info_sate == 0 )//&& SeleInputSource == S_YUV )
    {
        GBS::DEC_MATRIX_BYPS::write(1); 
        GBS::HD_MATRIX_BYPS::write(0);
        GBS::HD_DYN_BYPS::write(0);
    } else if (rto->inputIsYpBpR == false) //&& (SeleInputSource == S_VGA || SeleInputSource == S_RGBs))
    {
        GBS::DEC_MATRIX_BYPS::write(1); 
        GBS::HD_MATRIX_BYPS::write(1);
        GBS::HD_DYN_BYPS::write(1);
    }

    GBS::HD_SEL_BLK_IN::write(0);

    GBS::SP_SDCS_VSST_REG_H::write(0);
    GBS::SP_SDCS_VSSP_REG_H::write(0);
    GBS::SP_SDCS_VSST_REG_L::write(0);
    GBS::SP_SDCS_VSSP_REG_L::write(2);

    GBS::HD_HSYNC_RST::write(0x3ff);
    GBS::HD_INI_ST::write(0);

    if (rto->videoStandardInput <= 2) {

        GBS::SP_HS2PLL_INV_REG::write(1);
        GBS::SP_CS_P_SWAP::write(1);
        GBS::SP_HS_PROC_INV_REG::write(1);

        GBS::MD_HS_FLIP::write(1);
        GBS::MD_VS_FLIP::write(1);
        GBS::OUT_SYNC_SEL::write(2);
        GBS::SP_HS_LOOP_SEL::write(0);
        GBS::ADC_FLTR::write(3);

        GBS::HD_HSYNC_RST::write((GBS::PLLAD_MD::read() / 2) + 8);
        GBS::HD_HB_ST::write(GBS::PLLAD_MD::read() * 0.945f);
        GBS::HD_HB_SP::write(0x90);
        GBS::HD_HS_ST::write(0x80);
        GBS::HD_HS_SP::write(0x00);

        GBS::SP_CS_HS_ST::write(0xA0);
        GBS::SP_CS_HS_SP::write(0x00);

        if (rto->videoStandardInput == 1) {
            setCsVsStart(250);
            setCsVsStop(1);
            GBS::HD_VB_ST::write(500);
            GBS::HD_VS_ST::write(3);
            GBS::HD_VS_SP::write(522);
            GBS::HD_VB_SP::write(16);
        }
        if (rto->videoStandardInput == 2) {
            setCsVsStart(301);
            setCsVsStop(5);
            GBS::HD_VB_ST::write(605);
            GBS::HD_VS_ST::write(1);
            GBS::HD_VS_SP::write(621);
            GBS::HD_VB_SP::write(16);
        }
    } else if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) {
        GBS::ADC_FLTR::write(2);
        GBS::PLLAD_KS::write(1); // VCO post crossover control, determined by CKO frequency
        GBS::PLLAD_CKOS::write(0);

        GBS::HD_HB_ST::write(0x864);

        GBS::HD_HB_SP::write(0xa0);
        GBS::HD_VB_ST::write(0x00);
        GBS::HD_VB_SP::write(0x40);
        if (rto->videoStandardInput == 3) {
            GBS::HD_HS_ST::write(0x54);
            GBS::HD_HS_SP::write(0x864);
            GBS::HD_VS_ST::write(0x06);
            GBS::HD_VS_SP::write(0x00);
            setCsVsStart(525 - 5);
            setCsVsStop(525 - 3);
        }
        if (rto->videoStandardInput == 4) {
            GBS::HD_HS_ST::write(0x10);
            GBS::HD_HS_SP::write(0x880);
            GBS::HD_VS_ST::write(0x06);
            GBS::HD_VS_SP::write(0x00);
            setCsVsStart(48);
            setCsVsStop(46);
        }
    } else if (rto->videoStandardInput <= 7 || rto->videoStandardInput == 13) {

        if (rto->videoStandardInput == 5) {
            GBS::PLLAD_MD::write(2474);
            GBS::HD_HSYNC_RST::write(550);

            GBS::PLLAD_KS::write(0);
            GBS::PLLAD_CKOS::write(0);
            GBS::ADC_FLTR::write(0); // ADC internal filter control 0:150M 1: 110M 2:70M 3:40M
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::DEC2_BYPS::write(1);
            GBS::PLLAD_ICP::write(6);
            GBS::PLLAD_FS::write(1);
            GBS::HD_HB_ST::write(0);
            GBS::HD_HB_SP::write(0x140);
            GBS::HD_HS_ST::write(0x20);
            GBS::HD_HS_SP::write(0x80);
            GBS::HD_VB_ST::write(0x00);
            GBS::HD_VB_SP::write(0x6c);
            GBS::HD_VS_ST::write(0x00);
            GBS::HD_VS_SP::write(0x05);
            setCsVsStart(2);
            setCsVsStop(0);
        }
        if (rto->videoStandardInput == 6) {
            GBS::HD_HSYNC_RST::write(0x710);

            GBS::PLLAD_KS::write(1); // VCO post crossover control, determined by CKO frequency
            GBS::PLLAD_CKOS::write(0);
            GBS::ADC_FLTR::write(1);
            GBS::HD_HB_ST::write(0);
            GBS::HD_HB_SP::write(0xb8);
            GBS::HD_HS_ST::write(0x04);
            GBS::HD_HS_SP::write(0x50);
            GBS::HD_VB_ST::write(0x00);
            GBS::HD_VB_SP::write(0x1e);
            GBS::HD_VS_ST::write(0x04);
            GBS::HD_VS_SP::write(0x09);
            setCsVsStart(8);
            setCsVsStop(6);
        }
        if (rto->videoStandardInput == 7) {
            GBS::PLLAD_MD::write(2749);
            GBS::HD_HSYNC_RST::write(0x710);

            GBS::PLLAD_KS::write(0);
            GBS::PLLAD_CKOS::write(0);
            GBS::ADC_FLTR::write(0); // ADC Internal Filter Control 0:150M 1: 110M 2:70M 3:40M
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::DEC2_BYPS::write(1);
            GBS::PLLAD_ICP::write(6);
            GBS::PLLAD_FS::write(1);
            GBS::HD_HB_ST::write(0x00);
            GBS::HD_HB_SP::write(0xb0);
            GBS::HD_HS_ST::write(0x20);
            GBS::HD_HS_SP::write(0x70);
            GBS::HD_VB_ST::write(0x00);
            GBS::HD_VB_SP::write(0x2f);
            GBS::HD_VS_ST::write(0x04);
            GBS::HD_VS_SP::write(0x0A);
        }
        if (rto->videoStandardInput == 13) {
            applyRGBPatches();
            rto->syncTypeCsync = true;
            GBS::DEC_MATRIX_BYPS::write(1); 
            GBS::SP_PRE_COAST::write(4);
            GBS::SP_POST_COAST::write(4);
            GBS::SP_DLT_REG::write(0x70);
            GBS::HD_MATRIX_BYPS::write(1);
            GBS::HD_DYN_BYPS::write(1);
            GBS::SP_VS_PROC_INV_REG::write(0); 

            GBS::PLLAD_KS::write(0);
            GBS::PLLAD_CKOS::write(0);
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::DEC1_BYPS::write(1);
            GBS::DEC2_BYPS::write(1);
            GBS::PLLAD_MD::write(512);
        }
    }

    if (rto->videoStandardInput == 13) {

        uint16_t vtotal = GBS::STATUS_SYNC_PROC_VTOTAL::read();
        if (vtotal < 532) {
            GBS::PLLAD_KS::write(3);
            GBS::PLLAD_FS::write(1);
        } else if (vtotal >= 532 && vtotal < 810) {
            GBS::PLLAD_FS::write(0); // FS, VCO gain selection
            GBS::PLLAD_KS::write(2);
        } else {
            GBS::PLLAD_KS::write(2);
            GBS::PLLAD_FS::write(1);
        }
    }

    GBS::DEC_IDREG_EN::write(1);
    GBS::DEC_WEN_MODE::write(1);
    rto->phaseSP = 8;
    rto->phaseADC = 24;
    setAndUpdateSogLevel(rto->currentLevelSOG);

    rto->outModeHdBypass = 1;

    unsigned long timeout = millis();
    while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
        delay(1);
    }
    while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
        delay(1);
    }

    updateSpDynamic(0);
    while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
        delay(1);
    }

    GBS::DAC_RGBS_PWDNZ::write(1);   
    GBS::PAD_SYNC_OUT_ENZ::write(0); 
    delay(200);
    optimizePhaseSP();
}

void bypassModeSwitch_RGBHV() 
{
    if (!rto->boardHasPower) {
        return;
    }

    GBS::DAC_RGBS_PWDNZ::write(0);
    GBS::PAD_SYNC_OUT_ENZ::write(1);

    loadHdBypassSection();
    externalClockGenResetClock();
    FrameSync::cleanup();
    GBS::ADC_UNUSED_62::write(0x00);
    GBS::PA_ADC_BYPSZ::write(1);
    GBS::PA_SP_BYPSZ::write(1);
    applyRGBPatches();
    resetDebugPort();
    rto->videoStandardInput = 15;
    rto->autoBestHtotalEnabled = false;
    rto->clampPositionIsSet = false;
    rto->HPLLState = 0;

    GBS::PLL_CKIS::write(0);
    GBS::PLL_DIVBY2Z::write(0);
    GBS::PLL_ADS::write(0);
    GBS::PLL_MS::write(2);
    GBS::PAD_TRI_ENZ::write(1);
    GBS::MEM_PAD_CLK_INVERT::write(0);
    GBS::PLL648_CONTROL_01::write(0x35);
    GBS::PLL648_CONTROL_03::write(0x00);
    GBS::PLL_LEN::write(1);

    GBS::DAC_RGBS_ADC2DAC::write(1);
    GBS::OUT_SYNC_SEL::write(1);

    GBS::SFTRST_HDBYPS_RSTZ::write(1);
    GBS::HD_INI_ST::write(0);

    GBS::HD_MATRIX_BYPS::write(1);
    GBS::HD_DYN_BYPS::write(1);

    GBS::PAD_SYNC1_IN_ENZ::write(0);
    GBS::PAD_SYNC2_IN_ENZ::write(0);

    GBS::SP_SOG_P_ATO::write(1);
    if (rto->syncTypeCsync == false) {
        GBS::SP_SOG_SRC_SEL::write(0);  
        GBS::ADC_SOGEN::write(1);       
        GBS::SP_EXT_SYNC_SEL::write(0); 
        GBS::SP_SOG_MODE::write(0);
        GBS::SP_NO_COAST_REG::write(1);
        GBS::SP_PRE_COAST::write(0);        
        GBS::SP_POST_COAST::write(0);       
        GBS::SP_H_PULSE_IGNOR::write(0xff); 
        GBS::SP_SYNC_BYPS::write(0);        
        GBS::SP_HS_POL_ATO::write(1);       
        GBS::SP_VS_POL_ATO::write(1);       
        GBS::SP_HS_LOOP_SEL::write(1);      
        GBS::SP_H_PROTECT::write(0);        
        rto->phaseADC = 16;
        rto->phaseSP = 8;
    } else {

        GBS::SP_SOG_SRC_SEL::write(0); 
        GBS::SP_EXT_SYNC_SEL::write(1);
        GBS::ADC_SOGEN::write(1); 
        GBS::SP_SOG_MODE::write(1);
        GBS::SP_NO_COAST_REG::write(0);
        GBS::SP_PRE_COAST::write(4);
        GBS::SP_POST_COAST::write(7);
        GBS::SP_SYNC_BYPS::write(0);   
        GBS::SP_HS_LOOP_SEL::write(1); 
        GBS::SP_H_PROTECT::write(1);
        rto->currentLevelSOG = 24;
        rto->phaseADC = 16;
        rto->phaseSP = 8;
    }
    GBS::SP_CLAMP_MANUAL::write(1);  
    GBS::SP_COAST_INV_REG::write(0); 

    GBS::SP_DIS_SUB_COAST::write(1);   
    GBS::SP_HS_PROC_INV_REG::write(0); 
    GBS::SP_VS_PROC_INV_REG::write(0); 
    GBS::PLLAD_KS::write(1);           
    setOverSampleRatio(2, true);
    GBS::DEC_MATRIX_BYPS::write(1); 
    GBS::ADC_FLTR::write(0);        
    GBS::PLLAD_ICP::write(4);       
    GBS::PLLAD_FS::write(0);        
    GBS::PLLAD_MD::write(1856);     

    GBS::ADC_TA_05_CTRL::write(0x02); 
    GBS::ADC_TEST_04::write(0x02);    
    GBS::ADC_TEST_0C::write(0x12);    
    GBS::DAC_RGBS_R0ENZ::write(1);    
    GBS::DAC_RGBS_G0ENZ::write(1);    
    GBS::DAC_RGBS_B0ENZ::write(1);    
    GBS::OUT_SYNC_CNTRL::write(1);    

    resetDigital();
    resetSyncProcessor();
    delay(2);
    ResetSDRAM();
    delay(2);
    resetPLLAD();
    togglePhaseAdjustUnits();
    delay(20);
    GBS::PLLAD_LEN::write(1);        
    GBS::DAC_RGBS_PWDNZ::write(1);   
    GBS::PAD_SYNC_OUT_ENZ::write(0); 

    setAndLatchPhaseSP();
    setAndLatchPhaseADC();
    latchPLLAD();

    if (uopt->enableAutoGain == 1 && adco->r_gain == 0) {
        setAdcGain(AUTO_GAIN_INIT);
        GBS::DEC_TEST_ENABLE::write(1);
    } else if (uopt->enableAutoGain == 1 && adco->r_gain != 0) {
        GBS::ADC_RGCTRL::write(adco->r_gain); 
        GBS::ADC_GGCTRL::write(adco->g_gain); 
        GBS::ADC_BGCTRL::write(adco->b_gain); 
        GBS::DEC_TEST_ENABLE::write(1);       
    } else {
        GBS::DEC_TEST_ENABLE::write(0); 
    }

    rto->presetID = PresetBypassRGBHV;
    GBS::GBS_PRESET_ID::write(PresetBypassRGBHV);
    delay(200);
}

void runAutoGain() //
{
    static unsigned long lastTimeAutoGain = millis();
    uint8_t limit_found = 0, greenValue = 0;
    uint8_t loopCeiling = 0;
    uint8_t status00reg = GBS::STATUS_00::read();

    if ((millis() - lastTimeAutoGain) < 30000) {
        loopCeiling = 61;
    } else {
        loopCeiling = 8;
    }

    for (uint8_t i = 0; i < loopCeiling; i++) {
        if (i % 20 == 0) {
            handleWiFi(0);
            limit_found = 0;
        }
        greenValue = GBS::TEST_BUS_2F::read();

        if (greenValue == 0x7f) {
            if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) {
                limit_found++;
            } else
                return;

            if (limit_found == 2) {
                limit_found = 0;
                uint8_t level = GBS::ADC_GGCTRL::read();
                if (level < 0xfe) {
                    setAdcGain(level + 2);

                    adco->r_gain = GBS::ADC_RGCTRL::read(); // ADC R 
                    adco->g_gain = GBS::ADC_GGCTRL::read(); // ADC G 
                    adco->b_gain = GBS::ADC_BGCTRL::read(); // ADC B 

                    printInfo();
                    delay(2);
                    lastTimeAutoGain = millis();
                }
            }
        }
    }
}

void enableScanlines() 
{
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 0) 
    {

        GBS::MADPT_UVDLY_PD_SP::write(0);                       
        GBS::MADPT_UVDLY_PD_ST::write(0);                       
        GBS::MADPT_EN_UV_DEINT::write(1);                       
        GBS::MADPT_UV_MI_DET_BYPS::write(1);                    
        GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength); 

        GBS::MADPT_MO_ADP_UV_EN::write(1); 

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(0);
        GBS::MADPT_PD_RAM_BYPS::write(0);
        GBS::RFF_YUV_DEINTERLACE::write(1);
        GBS::MADPT_Y_MI_DET_BYPS::write(1);
        GBS::VDS_WLEV_GAIN::write(0x08);
        GBS::VDS_W_LEV_BYPS::write(0);
        GBS::MADPT_VIIR_COEF::write(0x08);                     
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength); 
        GBS::MADPT_VIIR_BYPS::write(0);
        GBS::RFF_LINE_FLIP::write(1);

        GBS::MAPDT_VT_SEL_PRGV::write(0);
        GBS::GBS_OPTION_SCANLINES_ENABLED::write(1);
    }
    rto->scanlinesEnabled = 1;
}

void disableScanlines() //
{
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
        GBS::MAPDT_VT_SEL_PRGV::write(1);

        GBS::MADPT_UVDLY_PD_SP::write(4);
        GBS::MADPT_UVDLY_PD_ST::write(4);
        GBS::MADPT_EN_UV_DEINT::write(0);
        GBS::MADPT_UV_MI_DET_BYPS::write(0);
        GBS::MADPT_UV_MI_OFFSET::write(4);
        GBS::MADPT_MO_ADP_UV_EN::write(0);

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(1);
        GBS::VDS_W_LEV_BYPS::write(1);

        GBS::MADPT_Y_MI_OFFSET::write(0xff);
        GBS::MADPT_VIIR_BYPS::write(1);
        GBS::MADPT_PD_RAM_BYPS::write(1);
        GBS::RFF_LINE_FLIP::write(0);

        GBS::GBS_OPTION_SCANLINES_ENABLED::write(0);
    }
    rto->scanlinesEnabled = 0;
}

void enableMotionAdaptDeinterlace() //
{
    // freezeVideo();
    GBS::DEINT_00::write(0x19);
    GBS::MADPT_Y_MI_OFFSET::write(0x00);

    GBS::MADPT_Y_MI_DET_BYPS::write(0);

    if (rto->videoStandardInput == 1)
        GBS::MADPT_VTAP2_COEFF::write(6);
    if (rto->videoStandardInput == 2)
        GBS::MADPT_VTAP2_COEFF::write(4);

    GBS::RFF_ADR_ADD_2::write(1);
    GBS::RFF_REQ_SEL::write(3);

    GBS::RFF_FETCH_NUM::write(0x80);
    GBS::RFF_WFF_OFFSET::write(0x100);
    GBS::RFF_YUV_DEINTERLACE::write(0);
    GBS::WFF_FF_STA_INV::write(0);

    GBS::WFF_ENABLE::write(1);
    GBS::RFF_ENABLE::write(1);

    unfreezeVideo();
    delay(60);
    GBS::MAPDT_VT_SEL_PRGV::write(0);
    rto->motionAdaptiveDeinterlaceActive = true;
}

void disableMotionAdaptDeinterlace() // 
{
    GBS::MAPDT_VT_SEL_PRGV::write(1);
    GBS::DEINT_00::write(0xff);

    GBS::RFF_FETCH_NUM::write(0x1);
    GBS::RFF_WFF_OFFSET::write(0x1);
    delay(2);
    GBS::WFF_ENABLE::write(0);
    GBS::RFF_ENABLE::write(0);

    GBS::WFF_FF_STA_INV::write(1);

    GBS::MADPT_Y_MI_OFFSET::write(0x7f);

    GBS::MADPT_Y_MI_DET_BYPS::write(1);

    rto->motionAdaptiveDeinterlaceActive = false; 
}

boolean snapToIntegralFrameRate(void) // Capture Equalized Frame Rate
{

    float ofr = getOutputFrameRate();

    if (ofr < 1.0f) {
        delay(1);
        ofr = getOutputFrameRate();
    }

    float target;
    if (ofr > 56.5f && ofr < 64.5f) {
        target = 60.0f;
    } else if (ofr > 46.5f && ofr < 54.5f) {
        target = 50.0f;
    } else {

        return false;
    }

    uint16_t currentHTotal = GBS::VDS_HSYNC_RST::read();
    uint16_t closestHTotal = currentHTotal;

    float closestDifference = fabs(target - ofr);

    for (;;) {

        delay(0);

        if (target > ofr) {
            if (currentHTotal > 0 && applyBestHTotal(currentHTotal - 1)) {
                --currentHTotal;
            } else {
                return false;
            }
        } else if (target < ofr) {
            if (currentHTotal < 4095 && applyBestHTotal(currentHTotal + 1)) {
                ++currentHTotal;
            } else {
                return false;
            }
        } else {
            return true;
        }

        ofr = getOutputFrameRate();

        if (ofr < 1.0f) {
            delay(1);
            ofr = getOutputFrameRate();
        }
        if (ofr < 1.0f) {
            return false;
        }

        float newDifference = fabs(target - ofr);
        if (newDifference < closestDifference) {
            closestDifference = newDifference;
            closestHTotal = currentHTotal;
        } else {
            break;
        }
    }

    if (closestHTotal != currentHTotal) {
        applyBestHTotal(closestHTotal);
    }

    return true;
}

void printInfo()
{
    static char print[121];
    static uint8_t clearIrqCounter = 0;
    static uint8_t lockCounterPrevious = 0;
    uint8_t lockCounter = 0;

    int32_t wifi = 0;
    if ((WiFi.status() == WL_CONNECTED) || (WiFi.getMode() == WIFI_AP)) {
        wifi = WiFi.RSSI();
    }

    uint16_t hperiod = GBS::HPERIOD_IF::read();
    uint16_t vperiod = GBS::VPERIOD_IF::read();
    uint8_t stat0FIrq = GBS::STATUS_0F::read();
    char HSp = GBS::STATUS_SYNC_PROC_HSPOL::read() ? '+' : '-';
    char VSp = GBS::STATUS_SYNC_PROC_VSPOL::read() ? '+' : '-';
    char h = 'H', v = 'V';
    if (!GBS::STATUS_SYNC_PROC_HSACT::read()) {
        h = HSp = ' ';
    }
    if (!GBS::STATUS_SYNC_PROC_VSACT::read()) {
        v = VSp = ' ';
    }

    printf(print, "h:%4u v:%4u PLL:%01u A:%02x%02x%02x S:%02x.%02x.%02x %c%c%c%c I:%02x D:%04x m:%hu ht:%4d vt:%4d hpw:%4d u:%3x s:%2x S:%2d W:%2d\n",
           hperiod, vperiod, lockCounterPrevious,
           GBS::ADC_RGCTRL::read(), GBS::ADC_GGCTRL::read(), GBS::ADC_BGCTRL::read(),
           GBS::STATUS_00::read(), GBS::STATUS_05::read(), GBS::SP_CS_0x3E::read(),
           h, HSp, v, VSp, stat0FIrq, GBS::TEST_BUS::read(), getVideoMode(),
           GBS::STATUS_SYNC_PROC_HTOTAL::read(), GBS::STATUS_SYNC_PROC_VTOTAL::read() /*+ 1*/,
           GBS::STATUS_SYNC_PROC_HLOW_LEN::read(), rto->noSyncCounter, rto->continousStableCounter,
           rto->currentLevelSOG, wifi);
    if (stat0FIrq != 0x00) {
        clearIrqCounter++;
        if (clearIrqCounter >= 50) {
            clearIrqCounter = 0;
            GBS::INTERRUPT_CONTROL_00::write(0xff);
            GBS::INTERRUPT_CONTROL_00::write(0x00);
        }
    }

    yield();
    if (GBS::STATUS_SYNC_PROC_HSACT::read()) {
        for (uint8_t i = 0; i < 9; i++) {
            if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
                lockCounter++;
            } else {
                for (int i = 0; i < 10; i++) {
                    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
                        lockCounter++;
                        break;
                    }
                }
            }
        }
    }
    lockCounterPrevious = getMovingAverage(lockCounter);
}

void stopWire()
{
    pinMode(SCL, INPUT);
    pinMode(SDA, INPUT);
    ESP.wdtFeed();
    delayMicroseconds(80);
}

void startWire()
{
    Wire.begin();

    pinMode(SCL, OUTPUT_OPEN_DRAIN);
    pinMode(SDA, OUTPUT_OPEN_DRAIN);

    // Wire.setClock(400000);
}

void fastSogAdjust() // 
{
    if (rto->noSyncCounter <= 5) {
        uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
        uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
        if (debug_backup != 0xa) {
            GBS::TEST_BUS_SEL::write(0xa);
        }
        if (debug_backup_SP != 0x0f) {
            GBS::TEST_BUS_SP_SEL::write(0x0f);
        }

        if ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
            while ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
                if (rto->currentLevelSOG >= 4) {
                    rto->currentLevelSOG -= 2;
                } else {
                    rto->currentLevelSOG = 13;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(40);
                    break;
                }
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(28);
            }
            delay(10);
        }

        if (debug_backup != 0xa) {
            GBS::TEST_BUS_SEL::write(debug_backup);
        }
        if (debug_backup_SP != 0x0f) {
            GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
        }
    }
}

void runSyncWatcher() // 
{
    if (!rto->boardHasPower) {
        return;
    }

    static uint8_t newVideoModeCounter = 0;
    static uint16_t activeStableLineCount = 0;
    static unsigned long lastSyncDrop = millis();
    static unsigned long lastLineCountMeasure = millis();

    uint16_t thisStableLineCount = 0;
    uint8_t detectedVideoMode = getVideoMode();
    boolean status16SpHsStable = getStatus16SpHsStable();

    if (rto->outModeHdBypass && status16SpHsStable) {
        if (videoStandardInputIsPalNtscSd()) {
            if (millis() - lastLineCountMeasure > 765) {
                thisStableLineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                for (uint8_t i = 0; i < 3; i++) {
                    delay(2);
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (thisStableLineCount - 3) ||
                        GBS::STATUS_SYNC_PROC_VTOTAL::read() > (thisStableLineCount + 3)) {
                        thisStableLineCount = 0;
                        break;
                    }
                }

                if (thisStableLineCount != 0) {
                    if (thisStableLineCount < (activeStableLineCount - 3) ||
                        thisStableLineCount > (activeStableLineCount + 3)) {
                        activeStableLineCount = thisStableLineCount;
                        if (activeStableLineCount < 230 || activeStableLineCount > 340) {

                            setCsVsStart(1);
                            if (getCsVsStop() == 1) {
                                setCsVsStop(2);
                            }

                            nudgeMD();
                        } else {
                            setCsVsStart(thisStableLineCount - 9);
                        }
                        delay(150);
                    }
                }
                lastLineCountMeasure = millis();
            }
        }
    }

    if (rto->videoStandardInput == 13) {
        if (detectedVideoMode == 0) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                detectedVideoMode = 13;
            }
        }
    }

    static unsigned long preemptiveSogWindowStart = millis();
    static const uint16_t sogWindowLen = 3000;
    static uint16_t badHsActive = 0;
    static boolean lastAdjustWasInActiveWindow = 0;

    if (rto->syncTypeCsync && !rto->inputIsYpBpR && (newVideoModeCounter == 0)) {

        if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_INT_SOG_SW::read() == 1) {
            resetInterruptSogSwitchBit();
            if ((millis() - preemptiveSogWindowStart) > sogWindowLen) {

                preemptiveSogWindowStart = millis();
                badHsActive = 0;
            }
            lastVsyncLock = millis();
        }

        if ((millis() - preemptiveSogWindowStart) < sogWindowLen) {
            for (uint8_t i = 0; i < 16; i++) {
                if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
                    resetInterruptSogBadBit();
                    uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
                    if (rto->videoStandardInput == 0)
                        hlowStart = 777;
                    for (int a = 0; a < 20; a++) {
                        if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {

                            badHsActive++;
                            lastVsyncLock = millis();
                            break;
                        }
                    }
                }
                if ((i % 3) == 0) {
                    delay(1);
                } else {
                    delay(0);
                }
            }

            if (badHsActive >= 17) {
                if (rto->currentLevelSOG >= 2) {
                    rto->currentLevelSOG -= 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                    updateSpDynamic(0);
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                } else if (badHsActive > 40) {
                    optimizeSogLevel();
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                }
                preemptiveSogWindowStart = millis();
            }
        } else if (lastAdjustWasInActiveWindow) {
            lastAdjustWasInActiveWindow = 0;
            if (rto->currentLevelSOG >= 8) {
                rto->currentLevelSOG -= 1;
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(30);
                updateSpDynamic(0);
                badHsActive = 0;
                rto->phaseIsSet = 0;
            }
        }
    }

    if ((detectedVideoMode == 0 || !status16SpHsStable) && rto->videoStandardInput != 15) {
        rto->noSyncCounter++;            // 
        rto->continousStableCounter = 0; // 
        lastVsyncLock = millis();
        if (rto->noSyncCounter == 1) {
            // freezeVideo(); 
            return;
        }

        rto->phaseIsSet = 0;

        if (rto->noSyncCounter <= 3 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
            // freezeVideo(); 
        }

        if (newVideoModeCounter == 0) {

            if (rto->noSyncCounter == 2) {

                if ((millis() - lastSyncDrop) > 1500) {
                    if (rto->printInfos == false) {
                        ;
                    }
                } else {
                    if (rto->printInfos == false) {
                        ;
                    }
                }

                if (rto->currentLevelSOG <= 1 && videoStandardInputIsPalNtscSd()) {
                    rto->currentLevelSOG += 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                }
                lastSyncDrop = millis();
            }
        }

        if (rto->noSyncCounter == 8) {
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100);

            if (videoStandardInputIsPalNtscSd()) {

                GBS::SP_PRE_COAST::write(9);
                GBS::SP_POST_COAST::write(9);

                uint8_t ignore = GBS::SP_H_PULSE_IGNOR::read();
                if (ignore >= 0x33) {
                    GBS::SP_H_PULSE_IGNOR::write(ignore / 2);
                }
            }
            rto->coastPositionIsSet = 0; // coast position setting
        }

        if (rto->noSyncCounter % 27 == 0) {

            updateSpDynamic(1);
        }

        if (rto->noSyncCounter % 32 == 0) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                unfreezeVideo();
            } else {
                // freezeVideo();
            }
        }

        if (rto->inputIsYpBpR && (rto->noSyncCounter == 34) && Info_sate == 0) //&& SeleInputSource == S_YUV )
        {
            GBS::SP_NO_CLAMP_REG::write(1);
            rto->clampPositionIsSet = false;
        }

        if (rto->noSyncCounter == 38) {
            nudgeMD();
        }

        if (rto->syncTypeCsync) {
            if (rto->noSyncCounter > 47) {
                if (rto->noSyncCounter % 16 == 0) {
                    GBS::SP_H_PROTECT::write(!GBS::SP_H_PROTECT::read());
                }
            }
        }

        if (rto->noSyncCounter % 150 == 0) {
            if (rto->noSyncCounter == 150 || rto->noSyncCounter % 900 == 0) {

                uint8_t extSyncBackup = GBS::SP_EXT_SYNC_SEL::read();
                GBS::SP_EXT_SYNC_SEL::write(0); // Extension 2 Setting Hs_Hs Selection
                delay(240);
                printInfo();
                if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                    delay(10);
                    if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                        rto->noSyncCounter = 0x07fe;
                        printf("noSyncCounter max2 \n");
                    }
                }
                GBS::SP_EXT_SYNC_SEL::write(extSyncBackup);
            }
            GBS::SP_H_COAST::write(0);
            GBS::SP_H_PROTECT::write(0); 
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100);
            GBS::SP_CS_CLP_ST::write(32);
            GBS::SP_CS_CLP_SP::write(48); //
            updateSpDynamic(1);           
            nudgeMD();
            delay(80);

            uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
            if (GBS::PLLAD_VCORST::read() == 1) {

                hlowStart = 777;
            }
            for (int a = 0; a < 128; a++) {
                if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {

                    if (rto->noSyncCounter % 450 == 0) {
                        rto->currentLevelSOG = 0;
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                    } else {
                        optimizeSogLevel();
                    }
                    break;
                } else if (a == 127) {

                    rto->currentLevelSOG = 5; // Current level SOG
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                }
                delay(0);
            }

            resetSyncProcessor();
            delay(8);
            resetModeDetect();
            delay(8);
        }

        if (rto->noSyncCounter % 413 == 0) {
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                GBS::ADC_INPUT_SEL::write(0);
            } else {
                GBS::ADC_INPUT_SEL::write(1);
            }
            delay(40);
            unsigned long timeout = millis();
            while (millis() - timeout <= 210) {
                if (getStatus16SpHsStable()) {
                    rto->noSyncCounter = 0x07fe;
                    printf("noSyncCounter max1 \n");
                    break;
                }
                handleWiFi(0);
                delay(1);
            }

            if (millis() - timeout > 210) {
                if (GBS::ADC_INPUT_SEL::read() == 1) {
                    GBS::ADC_INPUT_SEL::write(0);
                } else {
                    GBS::ADC_INPUT_SEL::write(1);
                }
            }
        }

        newVideoModeCounter = 0;
    }

    if (((detectedVideoMode != 0 && detectedVideoMode != rto->videoStandardInput) ||
         (detectedVideoMode != 0 && rto->videoStandardInput == 0)) &&
        rto->videoStandardInput != 15) {

        if (newVideoModeCounter < 255) {
            newVideoModeCounter++;
            rto->continousStableCounter = 0; 
            if (newVideoModeCounter > 1) {
                if (newVideoModeCounter == 2) {
                    ;
                }
            }
            if (newVideoModeCounter == 3) {
                // freezeVideo();
                GBS::SP_H_CST_ST::write(0x10);
                GBS::SP_H_CST_SP::write(0x100);
                rto->coastPositionIsSet = 0; // coast position setting
                delay(10);
                if (getVideoMode() == 0) {
                    updateSpDynamic(1);
                    delay(40);
                }
            }
        }

        if (newVideoModeCounter >= 8) {
            uint8_t vidModeReadout = 0;
            for (int a = 0; a < 30; a++) {
                vidModeReadout = getVideoMode();
                if (vidModeReadout == 13) {
                    newVideoModeCounter = 5;
                }
                if (vidModeReadout != detectedVideoMode) {
                    newVideoModeCounter = 0;
                }
            }
            if (newVideoModeCounter != 0) {
                rto->videoIsFrozen = false; 

                if (GBS::SP_SOG_MODE::read() == 1) {
                    rto->syncTypeCsync = true;
                } else {
                    rto->syncTypeCsync = false; 
                }
                boolean wantPassThroughMode = uopt->presetPreference == 10;

                if (((rto->videoStandardInput == 1 || rto->videoStandardInput == 3) && (detectedVideoMode == 2 || detectedVideoMode == 4)) ||
                    rto->videoStandardInput == 0 ||
                    ((rto->videoStandardInput == 2 || rto->videoStandardInput == 4) && (detectedVideoMode == 1 || detectedVideoMode == 3))) {
                    rto->useHdmiSyncFix = 1;
                } else {
                    rto->useHdmiSyncFix = 0; 
                }

                if (!wantPassThroughMode) {

                    applyPresets(detectedVideoMode);
                } else {
                    rto->videoStandardInput = detectedVideoMode;
                    setOutModeHdBypass(false);
                }
                rto->videoStandardInput = detectedVideoMode;
                rto->noSyncCounter = 0;          
                rto->continousStableCounter = 0; 
                newVideoModeCounter = 0;
                activeStableLineCount = 0;
                delay(20);
                badHsActive = 0;
                preemptiveSogWindowStart = millis();
            } else {
                unfreezeVideo();
                printInfo();
                newVideoModeCounter = 0;
                if (rto->videoStandardInput == 0) {
                    rto->noSyncCounter = 0x05ff;
                }
            }
        }
    } else if (getStatus16SpHsStable() && detectedVideoMode != 0 && rto->videoStandardInput != 15 && (rto->videoStandardInput == detectedVideoMode)) {

        if (rto->continousStableCounter < 255) {
            rto->continousStableCounter++;
        }

        static boolean doFullRestore = 0;
        if (rto->noSyncCounter >= 150) {

            rto->coastPositionIsSet = false;
            rto->phaseIsSet = false;
            FrameSync::reset(uopt->frameTimeLockMethod);
            doFullRestore = 1;
        }

        rto->noSyncCounter = 0; 
        newVideoModeCounter = 0;

        if (rto->continousStableCounter == 1 && !doFullRestore) {
            rto->videoIsFrozen = true;
            unfreezeVideo();
        }

        if (rto->continousStableCounter == 2) {
            updateSpDynamic(0);
            if (doFullRestore) {
                delay(20);
                optimizeSogLevel();
                doFullRestore = 0;
            }
            rto->videoIsFrozen = true;
            unfreezeVideo();
        }

        if (rto->continousStableCounter == 4) {
        }

        if (!rto->phaseIsSet) {
            if (rto->continousStableCounter >= 10 && rto->continousStableCounter < 61) {

                if ((rto->continousStableCounter % 10) == 0) {
                    rto->phaseIsSet = optimizePhaseSP();
                }
            }
        }

        if (rto->continousStableCounter == 160) {
            resetInterruptSogBadBit();
        }

        if (rto->continousStableCounter == 45) {
            GBS::ADC_UNUSED_67::write(0);

            rto->clampPositionIsSet = 0; // Clamp position setting
        }

        if (rto->continousStableCounter % 31 == 0) {
            updateSpDynamic(0);
        }

        if (rto->continousStableCounter >= 3) {
            if ((rto->videoStandardInput == 1 || rto->videoStandardInput == 2) &&
                !rto->outModeHdBypass && rto->noSyncCounter == 0) {

                static uint8_t timingAdjustDelay = 0;
                static uint8_t oddEvenWhenArmed = 0;
                boolean preventScanlines = 0;

                if (rto->deinterlaceAutoEnabled) {
                    uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
                    static uint8_t filteredLineCountMotionAdaptiveOn = 0, filteredLineCountMotionAdaptiveOff = 0;
                    static uint16_t VPERIOD_IF_OLD = VPERIOD_IF;

                    if (VPERIOD_IF_OLD != VPERIOD_IF) {

                        preventScanlines = 1;
                        filteredLineCountMotionAdaptiveOn = 0;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (uopt->enableFrameTimeLock || rto->extClockGenDetected) {
                            if (uopt->deintMode == 1) {
                                timingAdjustDelay = 11;
                                oddEvenWhenArmed = VPERIOD_IF % 2;
                            }
                        }
                    }

                    if (VPERIOD_IF == 522 || VPERIOD_IF == 524 || VPERIOD_IF == 526 ||
                        VPERIOD_IF == 622 || VPERIOD_IF == 624 || VPERIOD_IF == 626) {
                        filteredLineCountMotionAdaptiveOn++;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (filteredLineCountMotionAdaptiveOn >= 2) {
                            if (uopt->deintMode == 0 && !rto->motionAdaptiveDeinterlaceActive) {
                                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
                                    disableScanlines();
                                }
                                enableMotionAdaptDeinterlace();
                                if (timingAdjustDelay == 0) {
                                    timingAdjustDelay = 11;
                                    oddEvenWhenArmed = VPERIOD_IF % 2;
                                } else {
                                    timingAdjustDelay = 0;
                                }
                                preventScanlines = 1;
                            }
                            filteredLineCountMotionAdaptiveOn = 0;
                        }
                    } else if (VPERIOD_IF == 521 || VPERIOD_IF == 523 || VPERIOD_IF == 525 ||
                               VPERIOD_IF == 623 || VPERIOD_IF == 625 || VPERIOD_IF == 627) {
                        filteredLineCountMotionAdaptiveOff++;
                        filteredLineCountMotionAdaptiveOn = 0;
                        if (filteredLineCountMotionAdaptiveOff >= 2) {
                            if (uopt->deintMode == 0 && rto->motionAdaptiveDeinterlaceActive) {
                                disableMotionAdaptDeinterlace();
                                if (timingAdjustDelay == 0) {
                                    timingAdjustDelay = 11;
                                    oddEvenWhenArmed = VPERIOD_IF % 2;
                                } else {
                                    timingAdjustDelay = 0;
                                }
                            }
                            filteredLineCountMotionAdaptiveOff = 0;
                        }
                    } else {
                        filteredLineCountMotionAdaptiveOn = filteredLineCountMotionAdaptiveOff = 0;
                    }
                    VPERIOD_IF_OLD = VPERIOD_IF;

                    if (uopt->deintMode == 1) {
                        if (rto->motionAdaptiveDeinterlaceActive) {
                            disableMotionAdaptDeinterlace();
                            FrameSync::reset(uopt->frameTimeLockMethod);
                            GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                            lastVsyncLock = millis();
                        }
                        if (uopt->wantScanlines && !rto->scanlinesEnabled) {
                            enableScanlines();
                        } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                            disableScanlines();
                        }
                    }

                    if (timingAdjustDelay != 0) {
                        if ((VPERIOD_IF % 2) == oddEvenWhenArmed) {
                            timingAdjustDelay--;
                            if (timingAdjustDelay == 0) {
                                if (uopt->enableFrameTimeLock) {
                                    FrameSync::reset(uopt->frameTimeLockMethod);
                                    GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                                    delay(10);
                                    lastVsyncLock = millis();
                                }
                                externalClockGenSyncInOutRate();
                            }
                        }
                    }
                }

                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive && !preventScanlines) {
                        enableScanlines();
                    } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                        disableScanlines();
                    }
                }
            }
        }
    }

    if (rto->videoStandardInput >= 14) {
        static uint16_t RGBHVNoSyncCounter = 0;

        if (uopt->preferScalingRgbhv && rto->continousStableCounter >= 2) {
            boolean needPostAdjust = 0;
            static uint16_t activePresetLineCount = 0;

            uint16 sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
            if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 15) {
                uint16_t firstDetectedSourceLines = sourceLines;
                boolean moveOn = 1;
                for (int i = 0; i < 30; i++) {
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();

                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    rto->isValidForScalingRGBHV = true;
                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    rto->autoBestHtotalEnabled = 1;

                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_NO_COAST_REG::write(1);
                        GBS::ADC_5_00::write(0x10);
                        GBS::PLL_IS::write(0);
                        GBS::PLL_VCORST::write(1);
                        delay(320);
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10);
                        GBS::SP_H_CST_SP::write(0x80);
                        GBS::SP_H_PROTECT::write(1);
                    }
                    delay(4);

                    float sourceRate = getSourceFieldRate(1);
                    Serial.printf("sourceRate: ");
                    Serial.println(sourceRate);

                    if (uopt->presetPreference == 2) {

                        rto->videoStandardInput = 14;
                    } else {
                        if (sourceLines < 280) {

                            rto->videoStandardInput = 1;
                        } else if (sourceLines < 380) {

                            rto->videoStandardInput = 2;
                        } else if (sourceRate > 44.0f && sourceRate < 53.8f) {

                            rto->videoStandardInput = 4;
                            needPostAdjust = 1;
                        } else {

                            rto->videoStandardInput = 3;
                            needPostAdjust = 1;
                        }
                    }

                    if (uopt->presetPreference == 10)
                        uopt->presetPreference = Output1080P;

                    activePresetLineCount = sourceLines;
                    applyPresets(rto->videoStandardInput);

                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    GBS::IF_INI_ST::write(16);
                    GBS::SP_SOG_P_ATO::write(1);

                    GBS::SP_SDCS_VSST_REG_L::write(2);
                    GBS::SP_SDCS_VSSP_REG_L::write(0);

                    rto->coastPositionIsSet = rto->clampPositionIsSet = 0; // Clamp position setting
                    rto->videoStandardInput = 14;

                    if (GBS::PLLAD_ICP::read() >= 6) {
                        GBS::PLLAD_ICP::write(5);
                        latchPLLAD();
                        delay(40);
                    }

                    updateSpDynamic(1);
                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_CLAMP_MANUAL::write(1); 
                        GBS::SP_NO_COAST_REG::write(1);
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10);
                        GBS::SP_H_CST_SP::write(0x80);
                        GBS::SP_H_PROTECT::write(1);
                    }
                    delay(300);

                    if (rto->extClockGenDetected) {

                        if (!rto->outModeHdBypass) {
                            if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {

                                GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());

                                Si.enable(0);
                                ESP.wdtFeed();
                                delayMicroseconds(800);
                                GBS::PLL648_CONTROL_01::write(0x75);
                            }
                        }

                        externalClockGenSyncInOutRate();
                    }

                    if (needPostAdjust) {

                        GBS::IF_HB_ST2::write(0x08);
                        GBS::IF_HB_SP2::write(0x68);
                        GBS::IF_HBIN_SP::write(0x50);
                        if (rto->presetID == 0x05) {
                            GBS::IF_HB_ST2::write(0x480);
                            GBS::IF_HB_SP2::write(0x8E);
                        }

                        float sfr = getSourceFieldRate(0);
                        if (sfr >= 69.0) {

                            GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);
                        } else {

                            GBS::IF_VB_SP::write(8);
                            GBS::IF_VB_ST::write(6);
                        }
                    }
                }
            }

            else if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 14) {

                if (sourceLines < 280 && activePresetLineCount > 280) {
                    rto->videoStandardInput = 1;
                } else if (sourceLines < 380 && activePresetLineCount > 380) {
                    rto->videoStandardInput = 2;
                } else if (sourceLines > 380 && activePresetLineCount < 380) {
                    rto->videoStandardInput = 3;
                    needPostAdjust = 1;
                }

                if (rto->videoStandardInput != 14) {

                    uint16_t firstDetectedSourceLines = sourceLines;
                    boolean moveOn = 1;
                    for (int i = 0; i < 30; i++) {
                        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                        if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                            moveOn = 0;
                            break;
                        }
                        delay(10);
                    }

                    if (moveOn) {

                        if (rto->videoStandardInput <= 2) {
                            ;
                        }

                        if (uopt->presetPreference == 10) {
                            uopt->presetPreference = Output720P;
                        }

                        activePresetLineCount = sourceLines;
                        applyPresets(rto->videoStandardInput);

                        GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                        GBS::IF_INI_ST::write(16);
                        GBS::SP_SOG_P_ATO::write(1);

                        GBS::SP_SDCS_VSST_REG_L::write(2);
                        GBS::SP_SDCS_VSSP_REG_L::write(0);

                        rto->coastPositionIsSet = rto->clampPositionIsSet = 0; // Clamp position setting
                        rto->videoStandardInput = 14;

                        if (GBS::PLLAD_ICP::read() >= 6) {
                            GBS::PLLAD_ICP::write(5);
                            latchPLLAD();
                        }

                        updateSpDynamic(1);
                        if (rto->syncTypeCsync == false) {
                            GBS::SP_SOG_MODE::write(0);
                            GBS::SP_CLAMP_MANUAL::write(1); 
                            GBS::SP_NO_COAST_REG::write(1);
                        } else {
                            GBS::SP_SOG_MODE::write(1);
                            GBS::SP_H_CST_ST::write(0x10);
                            GBS::SP_H_CST_SP::write(0x80);
                            GBS::SP_H_PROTECT::write(1);
                        }
                        delay(300);

                        if (rto->extClockGenDetected) {

                            if (!rto->outModeHdBypass) {
                                if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {

                                    GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());

                                    Si.enable(0);
                                    ESP.wdtFeed();
                                    delayMicroseconds(800);
                                    GBS::PLL648_CONTROL_01::write(0x75);
                                }
                            }

                            externalClockGenSyncInOutRate();
                        }

                        if (needPostAdjust) {

                            GBS::IF_HB_ST2::write(0x08);
                            GBS::IF_HB_SP2::write(0x68);
                            GBS::IF_HBIN_SP::write(0x50);
                            if (rto->presetID == 0x05) {
                                GBS::IF_HB_ST2::write(0x480);
                                GBS::IF_HB_SP2::write(0x8E);
                            }

                            float sfr = getSourceFieldRate(0);
                            if (sfr >= 69.0) {

                                GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);
                            } else {

                                GBS::IF_VB_SP::write(8);
                                GBS::IF_VB_ST::write(6);
                            }
                        }
                    } else {

                        rto->videoStandardInput = 14;
                    }
                }
            }

            else if ((sourceLines > 535) && rto->videoStandardInput == 14) {
                uint16_t firstDetectedSourceLines = sourceLines;
                boolean moveOn = 1;
                for (int i = 0; i < 30; i++) {
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();

                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    ;
                    rto->videoStandardInput = 15;
                    rto->isValidForScalingRGBHV = false; 

                    activePresetLineCount = 0;
                    applyPresets(rto->videoStandardInput);

                    delay(300);
                }
            }
        }

        if (!uopt->preferScalingRgbhv && rto->videoStandardInput == 14) {
            rto->videoStandardInput = 15;
            rto->isValidForScalingRGBHV = false; 
            applyPresets(rto->videoStandardInput);
            delay(300);
        }

        uint16_t limitNoSync = 0;
        uint8_t VSHSStatus = 0;
        boolean stable = 0;
        if (rto->syncTypeCsync == true) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                resetModeDetect();
                stable = 0;
                delay(10);
                resetInterruptSogBadBit();
            } else {
                stable = 1;
                VSHSStatus = GBS::STATUS_00::read();

                stable = ((VSHSStatus & 0x04) == 0x04);
            }
            limitNoSync = 200;
        } else {
            // SYNC_PROC_STATUS_[0]   HS polarity  
            // SYNC_PROC_STATUS_[1]   HS active
            // SYNC_PROC_STATUS_[2]   VS polarity  
            // SYNC_PROC_STATUS_[3]   VS active
            // SYNC_PROC_STATUS_[7:4] Reserved
            VSHSStatus = GBS::STATUS_16::read();
            stable = ((VSHSStatus & 0x0a) == 0x0a);
            limitNoSync = 300;
        }
        // printf("0x%02x \n",stable);
        if (!stable) {

            RGBHVNoSyncCounter++;
            rto->continousStableCounter = 0; 
                                             // if (RGBHVNoSyncCounter % 2 == 0)
                                             // {
                                             //     printf("count:0x%02x\n",RGBHVNoSyncCounter);
                                             //     ESP.wdtFeed();
                                             // }
        } else {
            RGBHVNoSyncCounter = 0;

            if (rto->continousStableCounter < 255) {
                rto->continousStableCounter++;
                if (rto->continousStableCounter == 6) {
                    updateSpDynamic(1); 
                }
            }
        }

        if (RGBHVNoSyncCounter > limitNoSync && rto->noSyncCounter < 100) {
            RGBHVNoSyncCounter = 0;
            // if (!rto->isInLowPowerMode)
            if (!rto->HdmiHoldDetection) {
                setResetParameters();   
                prepareSyncProcessor(); 
                resetSyncProcessor();   
            }
            rto->noSyncCounter = 0; 
            Serial.println("RGBHV limit no sync");
            // No Signal Out
        }

        // if (RGBHVNoSyncCounter > limitNoSync)
        // {
        //   RGBHVNoSyncCounter = 0;
        //   setResetParameters();
        //   prepareSyncProcessor();
        //   resetSyncProcessor();

        //   rto->noSyncCounter = 0;
        //   Serial.println("RGBHV limit no sync");
        // }

        static unsigned long lastTimeSogAndPllRateCheck = millis();
        if ((millis() - lastTimeSogAndPllRateCheck) > 900) {
            if (rto->videoStandardInput == 15) {
                updateHVSyncEdge();
                delay(100);
            }

            static uint8_t runsWithSogBadStatus = 0; 
            static uint8_t oldHPLLState = 0;
            if (rto->syncTypeCsync == false) {
                if (GBS::STATUS_INT_SOG_BAD::read()) 
                {
                    runsWithSogBadStatus++;
                    if (runsWithSogBadStatus >= 4) {
                        rto->syncTypeCsync = true;
                        rto->HPLLState = runsWithSogBadStatus = RGBHVNoSyncCounter = 0;
                        rto->noSyncCounter = 0x07fe;
                        printf("noSyncCounter max \n");
                    }
                } else {
                    runsWithSogBadStatus = 0;
                }
            }

            uint32_t currentPllRate = 0;
            static uint32_t oldPllRate = 10;

            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                currentPllRate = getPllRate();

                if (currentPllRate > 100 && currentPllRate < 7500) {
                    if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                        delay(40);
                        if (GBS::STATUS_INT_SOG_BAD::read() == 1)
                            delay(100);
                        currentPllRate = getPllRate();

                        if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                            oldPllRate = currentPllRate;
                        }
                    }
                } else {
                    currentPllRate = 0;
                }
            }

            resetInterruptSogBadBit();

            oldHPLLState = rto->HPLLState;
            if (currentPllRate != 0) {
                if (currentPllRate < 1030) {
                    rto->HPLLState = 1;
                } else if (currentPllRate < 2300) {
                    rto->HPLLState = 2;
                } else if (currentPllRate < 3200) {
                    rto->HPLLState = 3;
                } else if (currentPllRate < 3800) {
                    rto->HPLLState = 4;
                } else {
                    rto->HPLLState = 5;
                }
            }

            if (rto->videoStandardInput == 15) {
                if (oldHPLLState != rto->HPLLState) {
                    if (rto->HPLLState == 1) {
                        GBS::PLLAD_KS::write(2);
                        GBS::PLLAD_FS::write(0); // FS, VCO Gain Selection
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 2) {
                        GBS::PLLAD_KS::write(1); // VCO post crossover control, determined by CKO frequency
                        GBS::PLLAD_FS::write(0); // FS, VCO Gain Selection
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 3) {
                        GBS::PLLAD_KS::write(1); // VCO post crossover control, determined by CKO frequency
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 4) {
                        GBS::PLLAD_KS::write(0);
                        GBS::PLLAD_FS::write(0); // FS、VCO Gain Selection
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 5) {
                        GBS::PLLAD_KS::write(0);
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6);
                    }

                    latchPLLAD();
                    delay(2);
                    setOverSampleRatio(4, false);
                    delay(100);
                }
            } else if (rto->videoStandardInput == 14) {
                if (oldHPLLState != rto->HPLLState) {
                }
            }

            if (rto->videoStandardInput == 14) {

                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive) {
                        if (GBS::IF_LD_RAM_BYPS::read() == 0) {
                            enableScanlines();
                        }
                    } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                        disableScanlines();
                    }
                }
            }

            rto->clampPositionIsSet = false;
            lastTimeSogAndPllRateCheck = millis();
        }
    }

    // if (((Info == InfoRGBs || Info == InfoRGsB || Info == InfoVGA)))
    // {
    //   // Osd_Display(0xFF, "RGB ");

    //   if (GBS::STATUS_SYNC_PROC_VSACT::read())
    //   {
    //     if (GBS::STATUS_SYNC_PROC_HSACT::read())
    //     {
    //       // Osd_Display(0xFF, "HV   ");
    //       if( (Info == InfoVGA || Info == InfoRGsB) && rto->HdmiHoldDetection == true)
    //         {
    //           rto->HdmiHoldDetection = false;
    //           printf(" VGA Detection : false\n");
    //         }
    //     }
    //   }
    //   else if((Info == InfoRGBs )&& rto->HdmiHoldDetection == true)
    //   {
    //     rto->HdmiHoldDetection = false;
    //     printf(" RGBS Detection : false\n");
    //   }
    // }

    if ((rto->noSyncCounter >= 0x07fe)) 
    {
        rto->noSyncCounter = 0; 
        // prepareSyncProcessor(); 
        printf("No Signal Out\n");
        // rto->isInLowPowerMode = true;  
        rto->HdmiHoldDetection = true;
    }
    /*
    if (rto->noSyncCounter >= 0x07fe)
    {
      GBS::DAC_RGBS_PWDNZ::write(0);
      rto->noSyncCounter = 0;
      goLowPowerWithInputDetection();
      printf("No Signal Out\n");
    }
  */
}

boolean checkBoardPower()
{
    GBS::ADC_UNUSED_69::write(0x6a);
    if (GBS::ADC_UNUSED_69::read() == 0x6a) {
        GBS::ADC_UNUSED_69::write(0);
        return 1;
    }

    GBS::ADC_UNUSED_69::write(0);
    if (rto->boardHasPower == true) {
        Serial.println(F("! power / i2c lost !"));
    }

    return 0;
}

void calibrateAdcOffset()
{
    GBS::PAD_BOUT_EN::write(0);
    GBS::PLL648_CONTROL_01::write(0xA5);
    GBS::ADC_INPUT_SEL::write(2);
    GBS::DEC_MATRIX_BYPS::write(1); 
    GBS::DEC_TEST_ENABLE::write(1);
    GBS::ADC_5_03::write(0x31);
    GBS::ADC_TEST_04::write(0x00);
    GBS::SP_CS_CLP_ST::write(0x00);
    GBS::SP_CS_CLP_SP::write(0x00);
    GBS::SP_5_56::write(0x05);
    GBS::SP_5_57::write(0x80);
    GBS::ADC_5_00::write(0x02);
    GBS::TEST_BUS_SEL::write(0x0b);
    GBS::TEST_BUS_EN::write(1);
    resetDigital();

    uint16_t hitTargetCounter = 0;
    uint16_t readout16 = 0;
    uint8_t missTargetCounter = 0;
    uint8_t readout = 0;

    GBS::ADC_RGCTRL::write(0x7F);
    GBS::ADC_GGCTRL::write(0x7F);
    GBS::ADC_BGCTRL::write(0x7F);

    GBS::ADC_ROFCTRL::write(0x7F);
    GBS::ADC_GOFCTRL::write(0x3D);
    GBS::ADC_BOFCTRL::write(0x7F);
    GBS::DEC_TEST_SEL::write(1);

    unsigned long startTimer = 0;
    for (uint8_t i = 0; i < 3; i++) {
        missTargetCounter = 0;
        hitTargetCounter = 0;
        delay(20);
        startTimer = millis();

        while ((millis() - startTimer) < 800) {
            readout16 = GBS::TEST_BUS::read() & 0x7fff;

            if (readout16 < 7) {
                hitTargetCounter++;
                missTargetCounter = 0;
            } else if (missTargetCounter++ > 2) {
                if (i == 0) {
                    GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL::read() + 1);
                    readout = GBS::ADC_GOFCTRL::read();
                } else if (i == 1) {
                    GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL::read() + 1);
                    readout = GBS::ADC_ROFCTRL::read();
                } else if (i == 2) {
                    GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL::read() + 1);
                    readout = GBS::ADC_BOFCTRL::read();
                }

                if (readout >= 0x52) {

                    break;
                }

                delay(10);
                hitTargetCounter = 0;
                missTargetCounter = 0;
                startTimer = millis();
            }
            if (hitTargetCounter > 1500) {
                break;
            }
        }
        if (i == 0) {

            adco->g_off = GBS::ADC_GOFCTRL::read();
            GBS::ADC_GOFCTRL::write(0x7F);
            GBS::ADC_ROFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(2);
        }
        if (i == 1) {
            adco->r_off = GBS::ADC_ROFCTRL::read();
            GBS::ADC_ROFCTRL::write(0x7F);
            GBS::ADC_BOFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(3);
        }
        if (i == 2) {
            adco->b_off = GBS::ADC_BOFCTRL::read();
        }
    }

    if (readout >= 0x52) {
        adco->r_off = adco->g_off = adco->b_off = 0x40;
    }

    GBS::ADC_GOFCTRL::write(adco->g_off);
    GBS::ADC_ROFCTRL::write(adco->r_off);
    GBS::ADC_BOFCTRL::write(adco->b_off);
}

void loadDefaultUserOptions()
{
    uopt->presetPreference = Output1080P;
    uopt->enableFrameTimeLock = 0;
    uopt->presetSlot = 'A';        //
    uopt->frameTimeLockMethod = 0; 
    uopt->enableAutoGain = 0;      
    uopt->wantScanlines = 0;       
    uopt->wantOutputComponent = 0; 
    uopt->deintMode = 0;           
    uopt->wantVdsLineFilter = 1;
    uopt->wantPeaking = 1;
    uopt->preferScalingRgbhv = 1;
    uopt->wantTap6 = 1;
    uopt->PalForce60 = 0;
    uopt->matchPresetSource = 1; 
    uopt->wantStepResponse = 1;
    uopt->wantFullHeight = 1;
    uopt->enableCalibrationADC = 1;
    uopt->scanlineStrength = 0x30;
    uopt->disableExternalClockGenerator = 0;
}

#if USE_NEW_OLED_MENU




enum EncoderState {
  STATE_00 = 0b00,  
  STATE_01 = 0b01,  
  STATE_11 = 0b11,  
  STATE_10 = 0b10   
};
volatile EncoderState encoderState = STATE_00;  
void ICACHE_RAM_ATTR isrRotaryEncoderRotateForNewMenu()
{   

    unsigned long interruptTime = millis();
    static unsigned long lastInterruptTime = 0;
    static unsigned long lastNavUpdateTime = 0;
    static OLEDMenuNav lastNav;
    EncoderState newState = static_cast<EncoderState>((digitalRead(pin_a) << 1) | digitalRead(pin_b));
    OLEDMenuNav newNav;
    switch (encoderState) {
    case STATE_00:
      if (newState == STATE_01) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::DOWN : OLEDMenuNav::UP;
      else if (newState == STATE_10) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::UP : OLEDMenuNav::DOWN;
      break;
    case STATE_01:
      if (newState == STATE_11) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::DOWN : OLEDMenuNav::UP;
      else if (newState == STATE_00) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::UP : OLEDMenuNav::DOWN;
      break;
    case STATE_11:
      if (newState == STATE_10) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::DOWN : OLEDMenuNav::UP;
      else if (newState == STATE_01) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::UP : OLEDMenuNav::DOWN;
      break;
    case STATE_10:
      if (newState == STATE_00) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::DOWN : OLEDMenuNav::UP;
      else if (newState == STATE_11) newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::UP : OLEDMenuNav::DOWN;
      break;
  }
  encoderState = newState;  // 
    if (interruptTime - lastInterruptTime > 100) 
    {   
      
              
  
        // if (!digitalRead(pin_b) && digitalRead(pin_a)) 
        // {
        //     newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::DOWN : OLEDMenuNav::UP;
        // } 
        // else if (digitalRead(pin_b) && !digitalRead(pin_a)) 
        // {
        //     newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::UP : OLEDMenuNav::DOWN;
        // }

        if ((newNav != lastNav && (interruptTime - lastNavUpdateTime < 120)) || (oled_menuItem != 0)) 
        {
            oledNav = lastNav = OLEDMenuNav::IDLE;   //
            
        } 
        else 
        {
            lastNav = oledNav = newNav;
            ++rotaryIsrID;
            lastNavUpdateTime = interruptTime;
        }
        lastInterruptTime = interruptTime;
    }
}

void handlePress() {

	  static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if ((interruptTime - lastInterruptTime > 500) && (oled_menuItem == 0))   //Minimum Repeat Press Interval
    {
        oledNav = OLEDMenuNav::ENTER;
        ++rotaryIsrID;
    }
    lastInterruptTime = interruptTime;
}
void ICACHE_RAM_ATTR isrRotaryEncoderPushForNewMenu()
{
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if ((interruptTime - lastInterruptTime > 500) && (oled_menuItem == 0))
    {
        oledNav = OLEDMenuNav::ENTER;
        ++rotaryIsrID;
    }
    lastInterruptTime = interruptTime;
}
#endif

#if HAVE_BUTTONS
#define INPUT_SHIFT 0
#define DOWN_SHIFT 1
#define UP_SHIFT 2
#define MENU_SHIFT 3
#define BACK_SHIFT 4

static const uint8_t historySize = 32;
static const uint16_t buttonPollInterval = 10; // Encode
static uint8_t buttonHistory[historySize];
static uint8_t buttonIndex;
static uint8_t buttonState;
static uint8_t buttonChanged;

// uint8_t readButtons(void)
// {
//     return ~(
//               (digitalRead(pin_b) << INPUT_SHIFT) |
//               (digitalRead(pin_a) << DOWN_SHIFT) |
//               (digitalRead(pin_b) << UP_SHIFT) |
//               (digitalRead(pin_switch) << MENU_SHIFT)
//             );
// }

// void debounceButtons(void) 
// {
//     buttonHistory[buttonIndex++ % historySize] = readButtons();
//     buttonChanged = 0xFF;
//     for (uint8_t i = 0; i < historySize; ++i)
//         buttonChanged &= buttonState ^ buttonHistory[i];
//     buttonState ^= buttonChanged;
// }

// bool buttonDown(uint8_t pos)
// {
//     return (buttonState & (1 << pos)) && (buttonChanged & (1 << pos));
// }

// void handleButtons(void)
// {
// #if USE_NEW_OLED_MENU
//     OLEDMenuNav btn = OLEDMenuNav::IDLE;
//     debounceButtons();
//     if (buttonDown(MENU_SHIFT))
//         btn = OLEDMenuNav::ENTER;
//     if (buttonDown(DOWN_SHIFT))
//         btn = OLEDMenuNav::UP;
//     if (buttonDown(UP_SHIFT))
//         btn = OLEDMenuNav::DOWN;
//     oledMenu.tick(btn);
// #else
//     debounceButtons();
//     if (buttonDown(INPUT_SHIFT))
//         Menu::run(MenuInput::BACK);
//     if (buttonDown(DOWN_SHIFT))
//         Menu::run(MenuInput::DOWN);

//     if (buttonDown(MENU_SHIFT))
//         Menu::run(MenuInput::FORWARD);
// #endif
// }
#endif

void discardSerialRxData()
{
    uint16_t maxThrowAway = 0x1fff;
    while (Serial.available() && maxThrowAway > 0) {
        Serial.read();
        maxThrowAway--;
    }
}

void updateWebSocketData()
{
    if (rto->webServerEnabled && rto->webServerStarted) {
        if (webSocket.connectedClients() > 0) {

            constexpr size_t MESSAGE_LEN = 6;
            char toSend[MESSAGE_LEN] = {0};
            toSend[0] = '#';

            if (rto->isCustomPreset) {
                toSend[1] = '9';
            } else
                switch (rto->presetID) {
                    case 0x01:
                    case 0x11:
                        toSend[1] = '1';
                        break;
                    case 0x02:
                    case 0x12:
                        toSend[1] = '2';
                        break;
                    case 0x03:
                    case 0x13:
                        toSend[1] = '3';
                        break;
                    case 0x04:
                    case 0x14:
                        toSend[1] = '4';
                        break;
                    case 0x05:
                    case 0x15:
                        toSend[1] = '5';
                        break;
                    case 0x06:
                    case 0x16:
                        toSend[1] = '6';
                        break;
                    case PresetHdBypass:
                    case PresetBypassRGBHV:
                        toSend[1] = '8';
                        break;
                    default:
                        toSend[1] = '0';
                        break;
                }

            toSend[2] = (char)uopt->presetSlot;

            toSend[3] = '@';
            toSend[4] = '@';
            toSend[5] = '@';

            if (uopt->enableAutoGain) {
                toSend[3] |= (1 << 0);
            }
            if (uopt->wantScanlines) {
                toSend[3] |= (1 << 1);
            }
            if (uopt->wantVdsLineFilter) {
                toSend[3] |= (1 << 2);
            }
            if (uopt->wantPeaking) {
                toSend[3] |= (1 << 3);
            }
            if (uopt->PalForce60) {
                toSend[3] |= (1 << 4);
            }
            if (uopt->wantOutputComponent) {
                toSend[3] |= (1 << 5);
            }

            if (uopt->matchPresetSource) {
                toSend[4] |= (1 << 0);
            }
            if (uopt->enableFrameTimeLock) {
                toSend[4] |= (1 << 1);
            }
            if (uopt->deintMode) {
                toSend[4] |= (1 << 2);
            }
            if (uopt->wantTap6) {
                toSend[4] |= (1 << 3);
            }
            if (uopt->wantStepResponse) {
                toSend[4] |= (1 << 4);
            }
            if (uopt->wantFullHeight) {
                toSend[4] |= (1 << 5);
            }

            if (uopt->enableCalibrationADC) {
                toSend[5] |= (1 << 0);
            }
            if (uopt->preferScalingRgbhv) {
                toSend[5] |= (1 << 1);
            }
            if (uopt->disableExternalClockGenerator) {
                toSend[5] |= (1 << 2);
            }

            if (ESP.getFreeHeap() > 14000) {
                webSocket.broadcastTXT(toSend, MESSAGE_LEN);
            } else {
                webSocket.disconnect();
            }
        }
    }
}

void handleWiFi(boolean instant)
{
    static unsigned long lastTimePing = millis();
    if (rto->webServerEnabled && rto->webServerStarted) {
        MDNS.update();
        persWM.handleWiFi();
        dnsServer.processNextRequest();

        if ((millis() - lastTimePing) > 953) {
            webSocket.broadcastPing();
        }
        if (((millis() - lastTimePing) > 973) || instant) {
            if ((webSocket.connectedClients(false) > 0) || instant) {
                updateWebSocketData();
            }
            lastTimePing = millis();
        }
    }

    if (rto->allowUpdatesOTA) {
        ArduinoOTA.handle();
    }
    yield();
}

void myLog(char const *type, char command)
{

    printf("%s command %c at settings source %d, custom slot %d, status %x\n",
           type, command, uopt->presetPreference, uopt->presetSlot, rto->presetID);
}

void setup()
{
    system_update_cpu_freq(160);
    // delay(700);
    // ESP.wdtDisable();

    display.init();

    display.flipScreenVertically();

    irrecv.enableIRIn();
    OSD_clear();
    OSD();
    PT_MUTE(0x78);
    PT_2257(70); // audible



    pinMode(pin_a, INPUT_PULLUP);
    pinMode(pin_b, INPUT_PULLUP);
    pinMode(pin_switch, INPUT_PULLUP);

#if USE_NEW_OLED_MENU
    // versatile_encoder = new Versatile_RotaryEncoder(pin_a, pin_b, pin_switch);
    // versatile_encoder->setHandleRotate(handleRotate);  //
    // versatile_encoder->setHandlePress(handlePress);//

    attachInterrupt(digitalPinToInterrupt(pin_a),   isrRotaryEncoderRotateForNewMenu, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pin_b),   isrRotaryEncoderRotateForNewMenu, CHANGE);   //isrRotaryEncoderPushForNewMenu
    attachInterrupt(digitalPinToInterrupt(pin_switch), isrRotaryEncoderPushForNewMenu, FALLING);
    initOLEDMenu();
    initOSD();
#else
    attachInterrupt(digitalPinToInterrupt(pin_a), isrRotaryEncoder, RISING);
#endif

    rto->webServerEnabled = true;
    rto->webServerStarted = false;

    Serial.begin(115200);
    Serial.setTimeout(10);

    WiFi.hostname(device_hostname_partial);

    startWire();
    Wire.setClock(400000);
    GBS::SP_SOG_MODE::read();
    writeOneByte(0xF0, 0);
    writeOneByte(0x00, 0);
    GBS::STATUS_00::read();

    if (rto->webServerEnabled) {
        rto->allowUpdatesOTA = false;
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
        WiFi.setOutputPower(16.0f);
        startWebserver();
        rto->webServerStarted = true;
    } else {
        WiFi.mode(WIFI_OFF);
        WiFi.forceSleepBegin();
    }
#ifdef HAVE_PINGER_LIBRARY
    pingLastTime = millis();
#endif

    ; // SerialMprintln(F("\nstartup"));

    loadDefaultUserOptions();

    rto->allowUpdatesOTA = false;      
    rto->enableDebugPings = false;     
    rto->autoBestHtotalEnabled = true; 
    rto->syncLockFailIgnore = 16;      
    rto->forceRetime = false;          
    rto->syncWatcherEnabled = true;    
    rto->phaseADC = 16;
    rto->phaseSP = 16;
    rto->failRetryAttempts = 0;  
    rto->presetID = 0;           
    rto->isCustomPreset = false; 
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false; 
    rto->deinterlaceAutoEnabled = true;           
    rto->scanlinesEnabled = false;                
    rto->boardHasPower = true;                    
    rto->presetIsPalForce60 = false;
    rto->syncTypeCsync = false;          
    rto->isValidForScalingRGBHV = false; 
    rto->medResLineCount = 0x33;
    rto->osr = 0;                  
    rto->useHdmiSyncFix = 0;       
    rto->notRecognizedCounter = 0; 

    rto->inputIsYpBpR = false;   
    rto->videoStandardInput = 0; 
    rto->outModeHdBypass = false;
    rto->videoIsFrozen = false;  
    if (!rto->webServerEnabled)
        rto->webServerStarted = false;
    rto->printInfos = false;          
    rto->sourceDisconnected = true;   
    rto->isInLowPowerMode = false;    
    rto->applyPresetDoneStage = 0;     
    rto->presetVlineShift = 0;         
    rto->clampPositionIsSet = 0;       
    rto->coastPositionIsSet = 0;       
    rto->continousStableCounter = 0;   
    rto->currentLevelSOG = 5;          
    rto->thisSourceMaxLevelSOG = 31;   

    adco->r_gain = 0;
    adco->g_gain = 0;
    adco->b_gain = 0;
    adco->r_off = 0;
    adco->g_off = 0;
    adco->b_off = 0;

    serialCommand = '@';
    userCommand = '@';

    pinMode(DEBUG_IN_PIN, INPUT);
    pinMode(15, OUTPUT);

    display.init();
    display.flipScreenVertically();

    // Start-up Logo
    unsigned long initDelay = millis();
    while (millis() - initDelay < 1500) {
        display.drawXbm(2, 2, gbsicon_width, gbsicon_height, gbsicon_bits);
        display.display();
        handleWiFi(0);
        delay(1);
    }
    display.clear();

    GBS::RESET_CONTROL_0x46::write(0);
    GBS::RESET_CONTROL_0x47::write(0);
    GBS::PLLAD_VCORST::write(1);
    GBS::PLLAD_PDZ::write(0);

    if (!SPIFFS.begin()) {
        ; // SerialMprintln(F("SPIFFS mount failed! ((1M SPIFFS) selected?)"));
    } else {
        File f = SPIFFS.open("/preferencesv2.txt", "r");
        if (!f) {
            //  SerialMprintln(F("no preferences file yet, create new"));
            printf("no preferences file yet, create new\n");
            loadDefaultUserOptions();
            saveUserPrefs();
        } else {
            uopt->presetPreference = (PresetPreference)(f.read() - '0'); 
            if (uopt->presetPreference > 10)
                uopt->presetPreference = Output1080P;

            uopt->enableFrameTimeLock = (uint8_t)(f.read() - '0');
            if (uopt->enableFrameTimeLock > 1)
                uopt->enableFrameTimeLock = 0;

            uopt->presetSlot = lowByte(f.read());

            uopt->frameTimeLockMethod = (uint8_t)(f.read() - '0'); 
            if (uopt->frameTimeLockMethod > 1)
                uopt->frameTimeLockMethod = 0;

            uopt->enableAutoGain = (uint8_t)(f.read() - '0'); 
            if (uopt->enableAutoGain > 1)
                uopt->enableAutoGain = 0;

            uopt->wantScanlines = (uint8_t)(f.read() - '0'); 
            if (uopt->wantScanlines > 1)
                uopt->wantScanlines = 0;

            uopt->wantOutputComponent = (uint8_t)(f.read() - '0'); 
            if (uopt->wantOutputComponent > 1)
                uopt->wantOutputComponent = 0;

            uopt->deintMode = (uint8_t)(f.read() - '0'); 
            if (uopt->deintMode > 2)
                uopt->deintMode = 0;

            uopt->wantVdsLineFilter = (uint8_t)(f.read() - '0'); 
            if (uopt->wantVdsLineFilter > 1)
                uopt->wantVdsLineFilter = 1;

            uopt->wantPeaking = (uint8_t)(f.read() - '0'); 
            if (uopt->wantPeaking > 1)
                uopt->wantPeaking = 1;

            uopt->preferScalingRgbhv = (uint8_t)(f.read() - '0'); 
            if (uopt->preferScalingRgbhv > 1)
                uopt->preferScalingRgbhv = 1;

            uopt->wantTap6 = (uint8_t)(f.read() - '0');
            if (uopt->wantTap6 > 1)
                uopt->wantTap6 = 1;

            uopt->PalForce60 = (uint8_t)(f.read() - '0'); 
            if (uopt->PalForce60 > 1)
                uopt->PalForce60 = 1;

            uopt->matchPresetSource = (uint8_t)(f.read() - '0'); 
            if (uopt->matchPresetSource > 1)
                uopt->matchPresetSource = 1;

            uopt->wantStepResponse = (uint8_t)(f.read() - '0');
            if (uopt->wantStepResponse > 1)
                uopt->wantStepResponse = 1;

            uopt->wantFullHeight = (uint8_t)(f.read() - '0');
            if (uopt->wantFullHeight > 1)
                uopt->wantFullHeight = 1;

            uopt->enableCalibrationADC = (uint8_t)(f.read() - '0');
            if (uopt->enableCalibrationADC > 1)
                uopt->enableCalibrationADC = 1;

            uopt->scanlineStrength = (uint8_t)(f.read() - '0');
            if (uopt->scanlineStrength > 0x60)
                uopt->enableCalibrationADC = 0x30;

            uopt->disableExternalClockGenerator = (uint8_t)(f.read() - '0');
            if (uopt->disableExternalClockGenerator > 1)
                uopt->disableExternalClockGenerator = 0;

            Volume = (uint8_t)(f.read() - '0');
            // InCurrent = (uint8_t)(f.read() - '0');
            SeleInputSource = (uint8_t)(f.read() - '0');

            // GBS::SP_EXT_SYNC_SEL::write((uint8_t)(f.read() - '0'));
            // GBS::ADC_INPUT_SEL::write((uint8_t)(f.read() - '0'));

            SvModeOption = (uint8_t)(f.read() - '0') * 10 + (uint8_t)(f.read() - '0');
            if (SvModeOption > MODEOPTION_MAX - 1)
                SvModeOption = 0;
            AvModeOption = (uint8_t)(f.read() - '0') * 10 + (uint8_t)(f.read() - '0');
            if (AvModeOption > MODEOPTION_MAX - 1)
                AvModeOption = 0;

            // printf(" SV AV: %d  %d \n",SvModeOption,AvModeOption);
            SmoothOption = (uint8_t)(f.read() - '0');
            if (SmoothOption > 1)
                SmoothOption = 0;

            LineOption = (uint8_t)(f.read() - '0');
            // LineOption = 1;
            if (LineOption > 1)
                LineOption = 1;

            BriorCon = (uint8_t)(f.read() - '0');
            if (BriorCon > 2)
                BriorCon = 1;

            Info = (uint8_t)(f.read() - '0');

            RGB_Com = (uint8_t)(f.read() - '0');
            if (RGB_Com > 1)
                RGB_Com = 0;


            Bright = (uint8_t)(f.read() - '0') * 100 + (uint8_t)(f.read() - '0') * 10 + (uint8_t)(f.read() - '0');
            if ((Bright > 0xFF - 1) || (Bright == 0))
                Bright = 0x80;

            Contrast = (uint8_t)(f.read() - '0') * 100 + (uint8_t)(f.read() - '0') * 10 + (uint8_t)(f.read() - '0');
            if ((Contrast > 0xFF - 1) || (Contrast == 0))
                Contrast = 0x80;

            Saturation = (uint8_t)(f.read() - '0') * 100 + (uint8_t)(f.read() - '0') * 10 + (uint8_t)(f.read() - '0');
            if ((Saturation > 0xFF - 1) || (Saturation == 0))
                Saturation = 0x80;
            // RGBs_Com = (uint8_t)(f.read() - '0');
            // RGsB_Com = (uint8_t)(f.read() - '0');
            // VGA_Com = (uint8_t)(f.read() - '0');

            f.close();
        }
    }

    // ReadUserIRRemote();

    GBS::PAD_CKIN_ENZ::write(1);
    externalClockGenDetectAndInitialize();

    startWire();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    initDelay = millis();
    while (millis() - initDelay < 1000) {
        handleWiFi(0);
        delay(1);
    }

    if (WiFi.status() == WL_CONNECTED) {
    } else if (WiFi.SSID().length() == 0) {
        ; // SerialMprintln(FPSTR(ap_info_string));
    } else {
        ; // SerialMprintln(F("(WiFi): still connecting.."));
        WiFi.reconnect();
    }

    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    boolean powerOrWireIssue = 0;
    if (!checkBoardPower()) {
        stopWire();
        for (int i = 0; i < 40; i++) {

            startWire();
            GBS::STATUS_00::read();
            digitalWrite(SCL, 0);
            // ESP.wdtFeed();
            delayMicroseconds(12);
            stopWire();
            if (digitalRead(SDA) == 1) {
                break;
            }
            if ((i % 7) == 0) {
                delay(1);
            }
        }

        startWire();
        delay(1);
        GBS::STATUS_00::read();
        delay(1);

        if (!checkBoardPower()) {
            stopWire();
            powerOrWireIssue = 1;
            rto->boardHasPower = false;
            rto->syncWatcherEnabled = false;
        } else {
            rto->syncWatcherEnabled = true;
            rto->boardHasPower = true;
            ; // SerialMprintln(F("recovered"));
        }
    }

    if (powerOrWireIssue == 0) {

        if (!rto->extClockGenDetected) {
            externalClockGenDetectAndInitialize();
        }
        if (rto->extClockGenDetected == 1) {
            Serial.println(F("ext clockgen detected"));
        } else {
            Serial.println(F("no ext clockgen"));
        }

        zeroAll();
        setResetParameters();
        prepareSyncProcessor();

        uint8_t productId = GBS::CHIP_ID_PRODUCT::read();
        uint8_t revisionId = GBS::CHIP_ID_REVISION::read();
        ; // SerialMprint(F("Chip ID: "));
        ; // SerialMprint(productId, HEX);
        ; // SerialMprint(" ");
        ; // SerialMprintln(revisionId, HEX);

        if (uopt->enableCalibrationADC) {

            calibrateAdcOffset();
        }
        setResetParameters();

        delay(4);
        handleWiFi(1);
        delay(4);
    } else {
        ; // SerialMprintln(F("Please check board power and cabling or restart!"));
    }

    if (Serial.available()) {
        discardSerialRxData();
    }
    halfPeriod.reset();
}
void loop()
{
    static uint8_t readout = 0;
    static uint8_t segmentCurrent = 255;
    static uint8_t registerCurrent = 255;
    static uint8_t inputToogleBit = 0;
    static uint8_t inputStage = 0;
    static unsigned long lastTimeSyncWatcher = millis();
    static unsigned long lastTimeSourceCheck = 500;
    static unsigned long lastTimeCheck = 500;
    static unsigned long lastTimeInterruptClear = millis();
    // static unsigned long lastTim_signal;
    // static unsigned long lastTim_sys;
    // static unsigned long lastTim_web;
    // Tim_signal = millis();
    // Tim_sys = millis();
    // Tim_web = millis();
    static bool dir = 0;

    // static unsigned long OneSec = millis();
    // if (millis() - OneSec >= 3000)
    //   {
    //     // printf("bit[0]:%d  \n",GBS::PAD_CKIN_ENZ::read());
    //     // printf("bit[4 3 1 0]:%d %d %d %d \n",GBS::PAD_TRI_ENZ::read(),GBS::PAD_BLK_OUT_ENZ::read(),GBS::PAD_CKOUT_ENZ::read(),GBS::PAD_CKIN_ENZ::read());
    //     OneSec = millis();
    //   }

    // #if HAVE_BUTTONS
    //   static unsigned long lastButton = micros();
    //   if (micros() - lastButton > buttonPollInterval)  // 10
    //   {
    //       printf(" IDLESTART \n");
    //     lastButton = micros();
    //     handleButtons();
    //   }
    // #endif
    
    // versatile_encoder->ReadEncoder();

    OSD_selectOption();
    OSD_IR();

    uint8_t oldIsrID = rotaryIsrID;
    if (NEW_OLED_MENU == true) {
        oledMenu.tick(oledNav);
        if (oldIsrID == rotaryIsrID) {
            oledNav = OLEDMenuNav::IDLE;
        }
    }

    if ((millis() - Tim_sys) >= 400) {
        PT_2257(Volume + 12);
        Tim_sys = millis();
    }

    handleWiFi(0);

    if ((millis() - Tim_signal) >= 100) {
        Mode_Option();
        Tim_signal = millis();
    }
    web_service(inputStage, segmentCurrent, registerCurrent, readout, inputToogleBit);
    if (uopt->enableFrameTimeLock &&
        rto->sourceDisconnected == false &&
        rto->autoBestHtotalEnabled &&
        rto->syncWatcherEnabled &&
        FrameSync::ready() &&
        millis() - lastVsyncLock > FrameSyncAttrs::lockInterval &&
        rto->continousStableCounter > 20 &&
        rto->noSyncCounter == 0) {
        uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        uint16_t pllad = GBS::PLLAD_MD::read();

        if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
            uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(0x0);
            }

            fsDebugPrintf("running frame sync, clock gen enabled = %d\n", rto->extClockGenDetected);

            bool success = rto->extClockGenDetected ? FrameSync::runFrequency() : FrameSync::runVsync(uopt->frameTimeLockMethod);

            if (!success) {
                if (rto->syncLockFailIgnore-- == 0) {
                    FrameSync::reset(uopt->frameTimeLockMethod);
                }
            }
            else if (rto->syncLockFailIgnore > 0) {
                rto->syncLockFailIgnore = 16;
            }

            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(debug_backup);
            }
        }

        lastVsyncLock = millis();
    }

          
    if (rto->syncWatcherEnabled && rto->boardHasPower) {
        if ((millis() - lastTimeInterruptClear) > 3000) {
            GBS::INTERRUPT_CONTROL_00::write(0xfe);
            GBS::INTERRUPT_CONTROL_00::write(0x00);
            lastTimeInterruptClear = millis();
        }
    }

    if (rto->printInfos == true) {
        printInfo();
    }

    if (rto->sourceDisconnected == false && rto->syncWatcherEnabled == true && (millis() - lastTimeSyncWatcher) > 20) { 
        runSyncWatcher();                                                                                               
        lastTimeSyncWatcher = millis();

        if (uopt->enableAutoGain == 1 && !rto->sourceDisconnected && rto->videoStandardInput > 0 && rto->clampPositionIsSet && rto->noSyncCounter == 0 && rto->continousStableCounter > 90 && rto->boardHasPower) {
            uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
            uint16_t pllad = GBS::PLLAD_MD::read();
            if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                uint8_t debugRegBackup = 0, debugPinBackup = 0;
                debugPinBackup = GBS::PAD_BOUT_EN::read();
                debugRegBackup = GBS::TEST_BUS_SEL::read();
                GBS::PAD_BOUT_EN::write(0);
                GBS::DEC_TEST_SEL::write(1);
                GBS::TEST_BUS_SEL::write(0xb);
                if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                    runAutoGain();
                }
                GBS::TEST_BUS_SEL::write(debugRegBackup);
                GBS::PAD_BOUT_EN::write(debugPinBackup);
            }
        }
    }

    if (rto->autoBestHtotalEnabled && !FrameSync::ready() && rto->syncWatcherEnabled) {
        if (rto->continousStableCounter >= 10 && rto->coastPositionIsSet &&
            ((millis() - lastVsyncLock) > 500)) {
            if ((rto->continousStableCounter % 5) == 0) {
                uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
                uint16_t pllad = GBS::PLLAD_MD::read();
                if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                    runAutoBestHTotal();
                }
            }
        }
    }

    if ((rto->videoStandardInput <= 14 && rto->videoStandardInput != 0) &&
        rto->syncWatcherEnabled && !rto->coastPositionIsSet) {
        if (rto->continousStableCounter >= 7) {
            if ((getStatus16SpHsStable() == 1) && (getVideoMode() == rto->videoStandardInput)) {
                updateCoastPosition(0);
                if (rto->coastPositionIsSet) {
                    if (videoStandardInputIsPalNtscSd()) 
                    {

                        GBS::SP_DIS_SUB_COAST::write(0);
                        GBS::SP_H_PROTECT::write(0); 
                    }
                }
            }
        }
    }

    if ((rto->videoStandardInput != 0) && (rto->continousStableCounter >= 4) &&
        !rto->clampPositionIsSet && rto->syncWatcherEnabled) {
        updateClampPosition();
        if (rto->clampPositionIsSet) {
            if (GBS::SP_NO_CLAMP_REG::read() == 1) {
                GBS::SP_NO_CLAMP_REG::write(0);
            }
        }
    }


    if ((rto->applyPresetDoneStage == 1) &&
        ((rto->continousStableCounter > 35 && rto->continousStableCounter < 45) ||
         !rto->syncWatcherEnabled)) {
        if (rto->applyPresetDoneStage == 1) {

            GBS::DAC_RGBS_PWDNZ::write(1); 
            if (!uopt->wantOutputComponent) {
                GBS::PAD_SYNC_OUT_ENZ::write(0); 
            }
            if (!rto->syncWatcherEnabled) {
                updateClampPosition();
                GBS::SP_NO_CLAMP_REG::write(0);
            }

            if (rto->extClockGenDetected && rto->videoStandardInput != 14) {
                if (!rto->outModeHdBypass) {
                    if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                        GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                        Si.enable(0);
                        ESP.wdtFeed();
                        delayMicroseconds(800);
                        GBS::PLL648_CONTROL_01::write(0x75);
                    }
                }
                externalClockGenSyncInOutRate();
            }
            rto->applyPresetDoneStage = 0;
        }
    } 
    else if (rto->applyPresetDoneStage == 1 && (rto->continousStableCounter > 35)) {

        GBS::DAC_RGBS_PWDNZ::write(1);  // 
        if (!uopt->wantOutputComponent) // 
        {
            GBS::PAD_SYNC_OUT_ENZ::write(0); //
        }

        externalClockGenSyncInOutRate();
        rto->applyPresetDoneStage = 0;
    }

    if (rto->applyPresetDoneStage == 10) // 
    {
        rto->applyPresetDoneStage = 11;
        setOutModeHdBypass(false);
    }

    if (rto->syncWatcherEnabled == true && rto->sourceDisconnected == true && rto->boardHasPower) {
        if ((millis() - lastTimeSourceCheck) >= 500) {
            if (checkBoardPower()) {
                inputAndSyncDetect();
            } else {
                rto->boardHasPower = false;      // 
                rto->continousStableCounter = 0; // 
                rto->syncWatcherEnabled = false; // 
            }
            lastTimeSourceCheck = millis();

            uint8_t currentSOG = GBS::ADC_SOGCTRL::read();
            if (currentSOG >= 3) {
                rto->currentLevelSOG = currentSOG - 1;
                GBS::ADC_SOGCTRL::write(rto->currentLevelSOG);
            } else {
                rto->currentLevelSOG = 6;
                GBS::ADC_SOGCTRL::write(rto->currentLevelSOG);
            }
        }
    } else if ((rto->syncWatcherEnabled == true && rto->sourceDisconnected == false && rto->boardHasPower)) {
        if ((millis() - lastTimeSourceCheck) >= 500) {
            // if (CheckInputFrequency() && rto->HdmiHoldDetection)
            if (CheckInputFrequency()) {
                uint8_t videoMode = getVideoMode();
                if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read()) {
                    videoMode = rto->videoStandardInput;
                }
                if (uopt->presetPreference != 2) // 
                {
                    rto->useHdmiSyncFix = 1;
                    if (rto->videoStandardInput == 14) {
                        rto->videoStandardInput = 15;
                    } else {
                        applyPresets(videoMode);
                    }
                } else {
                    // printf("out off-1\n");
                    setOutModeHdBypass(false); //    false
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
            }

            lastTimeSourceCheck = millis();
        }
    }

    if ((rto->noSyncCounter == 61 || rto->noSyncCounter == 62) && rto->boardHasPower) // 
    {
        if (!checkBoardPower()) {
            rto->noSyncCounter = 1;
            rto->boardHasPower = false;
            rto->continousStableCounter = 0; // 

            stopWire(); // 
                        // printf("out off-2\n");
        } else {

            rto->noSyncCounter = 63;
        }
    }

    if (!rto->boardHasPower && rto->syncWatcherEnabled) 
    {
        if (digitalRead(SCL) && digitalRead(SDA)) {
            delay(50);
            if (digitalRead(SCL) && digitalRead(SDA)) {
                Serial.println(F("power good"));
                delay(350);
                startWire();
                {

                    GBS::SP_SOG_MODE::read();
                    GBS::SP_SOG_MODE::read();
                    writeOneByte(0xF0, 0);
                    writeOneByte(0x00, 0);
                    GBS::STATUS_00::read();
                }
                rto->syncWatcherEnabled = true;
                rto->boardHasPower = true;
                delay(100);
                goLowPowerWithInputDetection();
            }
        }
    }

#ifdef HAVE_PINGER_LIBRARY

    if (WiFi.status() == WL_CONNECTED) {
        if (rto->enableDebugPings && millis() - pingLastTime > 1000) {

            if (pinger.Ping(WiFi.gatewayIP(), 1, 750) == false) {
                Serial.println("Error during last ping command.");
            }
            pingLastTime = millis();
        }
    }
#endif
}
/////////
/////////
/////////
/////////
/////////
/////////
/////////
//////////////////
/////////
/////////
/////////
/////////
/////////
/////////
//////////////////
/////////
/////////
/////////
/////////
/////////
/////////
//////////////////
/////////
/////////
/////////
/////////
/////////
/////////
//////////////////
/////////
/////////
/////////
/////////
/////////
/////////
//////////////////
/////////
/////////
/////////
/////////
/////////
/////////
//////////////////
/////////
/////////
/////////
/////////
/////////
/////////
/////////

void web_service(uint8_t inputStage, uint8_t segmentCurrent, uint8_t registerCurrent, uint8_t readout, uint8_t inputToogleBit)
{

    if ((millis() - Tim_web) >= 300) {
        if (Serial.available()) {
            serialCommand = Serial.read();
        } else if (inputStage > 0) {
            ; // SerialMprintln(F(" abort"));
            discardSerialRxData();
            serialCommand = ' ';
        }
        if (serialCommand != '@') {

            if (inputStage > 0) {
                if (serialCommand != 's' && serialCommand != 't' && serialCommand != 'g') {
                    discardSerialRxData();
                    ; // SerialMprintln(F(" abort"));
                    serialCommand = ' ';
                }
            }
            // myLog("serial", serialCommand);

            switch (serialCommand) {
                case ' ':

                    inputStage = segmentCurrent = registerCurrent = 0;
                    break;
                case 'd': {

                    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
                        disableScanlines();
                    }

                    if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                        FrameSync::reset(uopt->frameTimeLockMethod);
                    }

                    for (int segment = 0; segment <= 5; segment++) {
                        dumpRegisters(segment);
                    }; // SerialMprintln("};");
                } break;
                case '+':; // SerialMprintln("hor. +");
                    shiftHorizontalRight();
                    break;
                case '-':; // SerialMprintln("hor. -");
                    shiftHorizontalLeft();
                    break;
                case '*':
                    shiftVerticalUpIF();
                    break;
                case '/':
                    shiftVerticalDownIF();
                    break;
                case 'z':; // SerialMprintln(F("scale+"));
                    scaleHorizontal(2, true);
                    break;
                case 'h':; // SerialMprintln(F("scale-"));
                    scaleHorizontal(2, false);
                    break;
                case 'q':
                    resetDigital();
                    delay(2);
                    ResetSDRAM();
                    delay(2);
                    togglePhaseAdjustUnits();
                    break;
                case 'D':; // SerialMprint(F("debug view: "));
                    if (GBS::ADC_UNUSED_62::read() == 0x00) {
                        GBS::VDS_PK_LB_GAIN::write(0x3f);
                        GBS::VDS_PK_LH_GAIN::write(0x3f);
                        GBS::ADC_UNUSED_60::write(GBS::VDS_Y_OFST::read());
                        GBS::ADC_UNUSED_61::write(GBS::HD_Y_OFFSET::read());
                        GBS::ADC_UNUSED_62::write(1);
                        GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 0x24);
                        GBS::HD_Y_OFFSET::write(GBS::HD_Y_OFFSET::read() + 0x24);
                        if (!rto->inputIsYpBpR) {

                            GBS::HD_DYN_BYPS::write(0);
                            GBS::HD_U_OFFSET::write(GBS::HD_U_OFFSET::read() + 0x24);
                            GBS::HD_V_OFFSET::write(GBS::HD_V_OFFSET::read() + 0x24);
                        }; // SerialMprintln("on");
                    } else {
                        if (rto->presetID == 0x05) {
                            GBS::VDS_PK_LB_GAIN::write(0x16);
                            GBS::VDS_PK_LH_GAIN::write(0x0A);
                        } else {
                            GBS::VDS_PK_LB_GAIN::write(0x16);
                            GBS::VDS_PK_LH_GAIN::write(0x18);
                        }
                        GBS::VDS_Y_OFST::write(GBS::ADC_UNUSED_60::read());
                        GBS::HD_Y_OFFSET::write(GBS::ADC_UNUSED_61::read());
                        if (!rto->inputIsYpBpR) {

                            GBS::HD_DYN_BYPS::write(1);
                            GBS::HD_U_OFFSET::write(0);
                            GBS::HD_V_OFFSET::write(0);
                        }

                        GBS::ADC_UNUSED_60::write(0);
                        GBS::ADC_UNUSED_61::write(0);
                        GBS::ADC_UNUSED_62::write(0);
                        ; // SerialMprintln("off");
                    }
                    serialCommand = '@';
                    break;
                case 'C':; // SerialMprintln(F("PLL: ICLK"));

                    GBS::PLL648_CONTROL_01::write(0x85);
                    GBS::PLL_CKIS::write(1);
                    latchPLLAD();

                    rto->syncLockFailIgnore = 16;
                    FrameSync::reset(uopt->frameTimeLockMethod);
                    rto->forceRetime = true;

                    delay(200);
                    break;
                case 'Y':
                    writeProgramArrayNew(ntsc_1280x720, false);
                    doPostPresetLoadSteps();
                    break;
                case 'y':
                    writeProgramArrayNew(pal_1280x720, false);
                    doPostPresetLoadSteps();
                    break;
                case 'P':; // SerialMprint(F("auto deinterlace: "));
                    rto->deinterlaceAutoEnabled = !rto->deinterlaceAutoEnabled;
                    if (rto->deinterlaceAutoEnabled) {
                        ; // SerialMprintln("on");
                    } else {
                        ; // SerialMprintln("off");
                    }
                    break;
                case 'p':
                    if (!rto->motionAdaptiveDeinterlaceActive) {
                        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
                            disableScanlines();
                        }
                        enableMotionAdaptDeinterlace();
                    } else {
                        disableMotionAdaptDeinterlace();
                    }
                    break;
                case 'k':
                    bypassModeSwitch_RGBHV();
                    break;
                case 'K':
                    setOutModeHdBypass(false);
                    uopt->presetPreference = OutputBypass;
                    saveUserPrefs();
                    printf("pass \n");
                    break;
                case 'T':; // SerialMprint(F("auto gain "));
                    if (uopt->enableAutoGain == 0) {
                        uopt->enableAutoGain = 1;
                        setAdcGain(AUTO_GAIN_INIT);
                        GBS::DEC_TEST_ENABLE::write(1);
                        ; // SerialMprintln("on");
                    } else {
                        uopt->enableAutoGain = 0;
                        GBS::DEC_TEST_ENABLE::write(0);
                        ; // SerialMprintln("off");
                    }
                    saveUserPrefs();
                    break;
                case 'e':
                    writeProgramArrayNew(ntsc_240p, false);
                    doPostPresetLoadSteps();
                    break;
                case 'r':
                    writeProgramArrayNew(pal_240p, false);
                    doPostPresetLoadSteps();
                    break;
                case '.': {
                    if (!rto->outModeHdBypass) {

                        rto->autoBestHtotalEnabled = true;
                        rto->syncLockFailIgnore = 16;
                        rto->forceRetime = true;
                        FrameSync::reset(uopt->frameTimeLockMethod);

                        if (!rto->syncWatcherEnabled) {
                            boolean autoBestHtotalSuccess = 0;
                            delay(30);
                            autoBestHtotalSuccess = runAutoBestHTotal();
                            if (!autoBestHtotalSuccess) {
                                ; // SerialMprintln(F("(unchanged)"));
                            }
                        }
                    }
                } break;
                case '!':
                    Serial.print(F("sfr: "));
                    Serial.println(getSourceFieldRate(1));
                    Serial.print(F("pll: "));
                    Serial.println(getPllRate());
                    break;
                case '$': {

                    uint16_t writeAddr = 0x54;
                    const uint8_t eepromAddr = 0x50;
                    for (; writeAddr < 0x56; writeAddr++) {
                        Wire.beginTransmission(eepromAddr);
                        Wire.write(writeAddr >> 8);
                        Wire.write((uint8_t)writeAddr);
                        Wire.write(0x10);
                        Wire.endTransmission();
                        delay(5);
                    }
                    Serial.println("done");
                } break;
                case 'j':
                    latchPLLAD();
                    break;
                case 'J':
                    resetPLLAD();
                    break;
                case 'v':
                    rto->phaseSP += 1;
                    rto->phaseSP &= 0x1f;
                    ; // SerialMprint("SP: ");
                    ; // SerialMprintln(rto->phaseSP);
                    setAndLatchPhaseSP();
                    break;
                case 'b':
                    advancePhase();
                    latchPLLAD();
                    ; // SerialMprint("ADC: ");
                    ; // SerialMprintln(rto->phaseADC);
                    break;
                case '#':
                    rto->videoStandardInput = 13;
                    applyPresets(13);
                    break;
                case 'n': {
                    uint16_t pll_divider = GBS::PLLAD_MD::read();
                    pll_divider += 1;
                    GBS::PLLAD_MD::write(pll_divider);
                    GBS::IF_HSYNC_RST::write((pll_divider / 2));
                    GBS::IF_LINE_SP::write(((pll_divider / 2) + 1) + 0x40);
                    ; // SerialMprint(F("PLL div: "));
                    ; // SerialMprint(pll_divider, HEX);
                    ; // SerialMprint(" ");
                    ; // SerialMprintln(pll_divider);

                    latchPLLAD();
                    delay(1);
                    updateClampPosition();
                    updateCoastPosition(0);
                } break;
                case 'N': {
                    if (rto->scanlinesEnabled) {
                        rto->scanlinesEnabled = false; // 
                        disableScanlines();
                    } else {
                        rto->scanlinesEnabled = true;
                        enableScanlines();
                    }
                } break;
                case 'a':; // SerialMprint(F("HTotal++: "));
                    ;      // SerialMprintln(GBS::VDS_HSYNC_RST::read() + 1);
                    if (GBS::VDS_HSYNC_RST::read() < 4095) {
                        if (uopt->enableFrameTimeLock) {

                            FrameSync::reset(uopt->frameTimeLockMethod);
                        }
                        rto->forceRetime = 1;
                        applyBestHTotal(GBS::VDS_HSYNC_RST::read() + 1);
                    }
                    break;
                case 'A':; // SerialMprint(F("HTotal--: "));
                    ;      // SerialMprintln(GBS::VDS_HSYNC_RST::read() - 1);
                    if (GBS::VDS_HSYNC_RST::read() > 0) {
                        if (uopt->enableFrameTimeLock) {

                            FrameSync::reset(uopt->frameTimeLockMethod);
                        }
                        rto->forceRetime = 1;
                        applyBestHTotal(GBS::VDS_HSYNC_RST::read() - 1);
                    }
                    break;
                case 'M': {
                } break;
                case 'm':; // SerialMprint(F("syncwatcher "));
                    if (rto->syncWatcherEnabled == true) {
                        rto->syncWatcherEnabled = false;
                        if (rto->videoIsFrozen) {
                            unfreezeVideo();
                        }; // SerialMprintln("off");
                    } else {
                        rto->syncWatcherEnabled = true;
                        ; // SerialMprintln("on");
                    }
                    break;
                case ',':
                    printVideoTimings();
                    break;
                case 'i':
                    rto->printInfos = !rto->printInfos;
                    printf("printfInfo\n");
                    break;
                case 'c':; // SerialMprintln(F("OTA Updates on"));
                    initUpdateOTA();
                    rto->allowUpdatesOTA = true;
                    break;
                case 'G':; // SerialMprint(F("Debug Pings "));
                    if (!rto->enableDebugPings) {
                        ; // SerialMprintln("on");
                        rto->enableDebugPings = 1;
                    } else {
                        ; // SerialMprintln("off");
                        rto->enableDebugPings = 0;
                    }
                    break;
                case 'u':
                    ResetSDRAM();
                    break;
                case 'f':; // SerialMprint(F("peaking "));
                    if (uopt->wantPeaking == 0) {
                        uopt->wantPeaking = 1;
                        GBS::VDS_PK_Y_H_BYPS::write(0);
                    } else {
                        if (GBS::VDS_PK_LB_GAIN::read() == 0x16) {
                            uopt->wantPeaking = 0;
                            GBS::VDS_PK_Y_H_BYPS::write(1);
                        }
                    }
                    saveUserPrefs();
                    break;
                case 'F':; // SerialMprint(F("ADC filter "));
                    if (GBS::ADC_FLTR::read() > 0) {
                        GBS::ADC_FLTR::write(0); // ADC Internal Filter Control  0:150M 1: 110M 2:70M 3:40M
                        ;                        // SerialMprintln("off");
                    } else {
                        GBS::ADC_FLTR::write(3);
                        ; // SerialMprintln("on");
                    }
                    break;
                case 'L': {

                    // uopt->wantOutputComponent = !uopt->wantOutputComponent;
                    // // OutputComponentOrVGA();
                    // saveUserPrefs();

                    // uint8_t videoMode = getVideoMode();
                    // if (videoMode == 0)
                    //   videoMode = rto->videoStandardInput;
                    // PresetPreference backup = uopt->presetPreference;
                    // uopt->presetPreference = Output720P;
                    // rto->videoStandardInput = 0;
                    // applyPresets(videoMode);
                    // uopt->presetPreference = backup;
                } break;
                case 'l':; // SerialMprintln(F("resetSyncProcessor"));
                    resetSyncProcessor();
                    break;
                case 'Z': {
                    uopt->matchPresetSource = !uopt->matchPresetSource;
                    saveUserPrefs();
                    uint8_t vidMode = getVideoMode();
                    if (uopt->presetPreference == 0 && GBS::GBS_PRESET_ID::read() == 0x11) {
                        applyPresets(vidMode);
                    } else if (uopt->presetPreference == 4 && GBS::GBS_PRESET_ID::read() == 0x02) {
                        applyPresets(vidMode);
                    }
                } break;
                case 'W':
                    uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
                    break;
                case 'E':
                    writeProgramArrayNew(ntsc_1280x1024, false);
                    doPostPresetLoadSteps();
                    break;
                case 'R':
                    writeProgramArrayNew(pal_1280x1024, false);
                    doPostPresetLoadSteps();
                    break;
                case '0':
                    moveHS(4, true);
                    break;
                case '1':
                    moveHS(4, false);
                    break;
                case '2':
                    writeProgramArrayNew(pal_768x576, false);
                    doPostPresetLoadSteps();
                    break;
                case '3':
                    //
                    break;
                case '4': {

                    if (GBS::VDS_VSCALE::read() <= 256) {
                        ; // SerialMprintln("limit");
                        break;
                    }
                    scaleVertical(2, true);
                } break;
                case '5': {

                    if (GBS::VDS_VSCALE::read() == 1023) {
                        ; // SerialMprintln("limit");
                        break;
                    }
                    scaleVertical(2, false);
                } break;
                case '6':
                    if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
                        if (GBS::IF_HBIN_SP::read() >= 10) {
                            GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() - 8);
                            if ((GBS::IF_HSYNC_RST::read() - 4) > ((GBS::PLLAD_MD::read() >> 1) + 5)) {
                                GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() - 4);
                                GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 4);
                            }
                        } else {
                            ; // SerialMprintln("limit");
                        }
                    } else if (!rto->outModeHdBypass) {
                        if (GBS::IF_HB_SP2::read() >= 4)
                            GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() - 4);
                        else
                            GBS::IF_HB_SP2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                        if (GBS::IF_HB_ST2::read() >= 4)
                            GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() - 4);
                        else
                            GBS::IF_HB_ST2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                        ; // SerialMprint(F("IF_HB_ST2: "));
                        ; // SerialMprint(GBS::IF_HB_ST2::read(), HEX);
                        ; // SerialMprint(F(" IF_HB_SP2: "));
                        ; // SerialMprintln(GBS::IF_HB_SP2::read(), HEX);
                    }
                    break;
                case '7':
                    if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
                        if (GBS::IF_HBIN_SP::read() < 0x150) {
                            GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() + 8);
                        } else {
                            ; // SerialMprintln("limit");
                        }
                    } else if (!rto->outModeHdBypass) {
                        if (GBS::IF_HB_SP2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                            GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() + 4);
                        else
                            GBS::IF_HB_SP2::write(0);
                        if (GBS::IF_HB_ST2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                            GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() + 4);
                        else
                            GBS::IF_HB_ST2::write(0);
                        ; // SerialMprint(F("IF_HB_ST2: "));
                        ; // SerialMprint(GBS::IF_HB_ST2::read(), HEX);
                        ; // SerialMprint(F(" IF_HB_SP2: "));
                        ; // SerialMprintln(GBS::IF_HB_SP2::read(), HEX);
                    }
                    break;
                case '8':

                    invertHS();
                    invertVS();

                    break;
                case '9':
                    writeProgramArrayNew(ntsc_720x480, false);
                    doPostPresetLoadSteps();
                    break;
                case 'o': {
                    if (rto->osr == 1) {
                        setOverSampleRatio(2, false);
                    } else if (rto->osr == 2) {
                        setOverSampleRatio(4, false);
                    } else if (rto->osr == 4) {
                        setOverSampleRatio(1, false);
                    }
                    delay(4);
                    optimizePhaseSP();
                    ; // SerialMprint("OSR ");
                    ; // SerialMprint(rto->osr);
                    ; // SerialMprintln("x");
                    rto->phaseIsSet = 0;
                } break;
                case 'g':
                    inputStage++;

                    if (inputStage > 0) {
                        if (inputStage == 1) {
                            segmentCurrent = Serial.parseInt();
                            ; // SerialMprint("G");
                            ; // SerialMprint(segmentCurrent);
                        } else if (inputStage == 2) {
                            char szNumbers[3];
                            szNumbers[0] = Serial.read();
                            szNumbers[1] = Serial.read();
                            szNumbers[2] = '\0';

                            registerCurrent = strtol(szNumbers, NULL, 16);
                            ; // SerialMprint("R");
                            ; // SerialMprint(registerCurrent, HEX);
                            if (segmentCurrent <= 5) {
                                writeOneByte(0xF0, segmentCurrent);
                                readFromRegister(registerCurrent, 1, &readout);
                                ; // SerialMprint(" value: 0x");
                                ; // SerialMprintln(readout, HEX);
                            } else {
                                discardSerialRxData();
                                ; // SerialMprintln("abort");
                            }
                            inputStage = 0;
                        }
                    }
                    break;
                case 's':
                    inputStage++;

                    if (inputStage > 0) {
                        if (inputStage == 1) {
                            segmentCurrent = Serial.parseInt();
                            ; // SerialMprint("S");
                            ; // SerialMprint(segmentCurrent);
                        } else if (inputStage == 2) {
                            char szNumbers[3];
                            for (uint8_t a = 0; a <= 1; a++) {

                                if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                    (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                    (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                    szNumbers[a] = Serial.read();
                                } else {
                                    szNumbers[a] = 0;
                                    Serial.read();
                                }
                            }
                            szNumbers[2] = '\0';

                            registerCurrent = strtol(szNumbers, NULL, 16);
                            ; // SerialMprint("R");
                            ; // SerialMprint(registerCurrent, HEX);
                        } else if (inputStage == 3) {
                            char szNumbers[3];
                            for (uint8_t a = 0; a <= 1; a++) {
                                if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                    (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                    (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                    szNumbers[a] = Serial.read();
                                } else {
                                    szNumbers[a] = 0;
                                    Serial.read();
                                }
                            }
                            szNumbers[2] = '\0';

                            inputToogleBit = strtol(szNumbers, NULL, 16);
                            if (segmentCurrent <= 5) {
                                writeOneByte(0xF0, segmentCurrent);
                                readFromRegister(registerCurrent, 1, &readout);
                                ; // SerialMprint(" (was 0x");
                                ; // SerialMprint(readout, HEX);
                                ; // SerialMprint(")");
                                writeOneByte(registerCurrent, inputToogleBit);
                                readFromRegister(registerCurrent, 1, &readout);
                                ; // SerialMprint(" is now: 0x");
                                ; // SerialMprintln(readout, HEX);
                            } else {
                                discardSerialRxData();
                                ; // SerialMprintln("abort");
                            }
                            inputStage = 0;
                        }
                    }
                    break;
                case 't':
                    inputStage++;

                    if (inputStage > 0) {
                        if (inputStage == 1) {
                            segmentCurrent = Serial.parseInt();
                            ; // SerialMprint("T");
                            ; // SerialMprint(segmentCurrent);
                        } else if (inputStage == 2) {
                            char szNumbers[3];
                            for (uint8_t a = 0; a <= 1; a++) {

                                if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                    (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                    (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                    szNumbers[a] = Serial.read();
                                } else {
                                    szNumbers[a] = 0;
                                    Serial.read();
                                }
                            }
                            szNumbers[2] = '\0';

                            registerCurrent = strtol(szNumbers, NULL, 16);
                            ; // SerialMprint("R");
                            ; // SerialMprint(registerCurrent, HEX);
                        } else if (inputStage == 3) {
                            if (Serial.peek() >= '0' && Serial.peek() <= '7') {
                                inputToogleBit = Serial.parseInt();
                            } else {
                                inputToogleBit = 255;
                            }; // SerialMprint(" Bit: ");
                            ; // SerialMprint(inputToogleBit);
                            inputStage = 0;
                            if ((segmentCurrent <= 5) && (inputToogleBit <= 7)) {
                                writeOneByte(0xF0, segmentCurrent);
                                readFromRegister(registerCurrent, 1, &readout);
                                ; // SerialMprint(" (was 0x");
                                ; // SerialMprint(readout, HEX);
                                ; // SerialMprint(")");
                                writeOneByte(registerCurrent, readout ^ (1 << inputToogleBit));
                                readFromRegister(registerCurrent, 1, &readout);
                                ; // SerialMprint(" is now: 0x");
                                ; // SerialMprintln(readout, HEX);
                            } else {
                                discardSerialRxData();
                                inputToogleBit = registerCurrent = 0;
                                ; // SerialMprintln("abort");
                            }
                        }
                    }
                    break;
                case '<': {
                    if (segmentCurrent != 255 && registerCurrent != 255) {
                        writeOneByte(0xF0, segmentCurrent);
                        readFromRegister(registerCurrent, 1, &readout);
                        writeOneByte(registerCurrent, readout - 1);
                        Serial.print("S");
                        Serial.print(segmentCurrent);
                        Serial.print("_");
                        Serial.print(registerCurrent, HEX);
                        readFromRegister(registerCurrent, 1, &readout);
                        Serial.print(" : ");
                        Serial.println(readout, HEX);
                    }
                } break;
                case '>': {
                    if (segmentCurrent != 255 && registerCurrent != 255) {
                        writeOneByte(0xF0, segmentCurrent);
                        readFromRegister(registerCurrent, 1, &readout);
                        writeOneByte(registerCurrent, readout + 1);
                        Serial.print("S");
                        Serial.print(segmentCurrent);
                        Serial.print("_");
                        Serial.print(registerCurrent, HEX);
                        readFromRegister(registerCurrent, 1, &readout);
                        Serial.print(" : ");
                        Serial.println(readout, HEX);
                    }
                } break;
                case '_': {
                    uint32_t ticks = FrameSync::getPulseTicks();
                    Serial.println(ticks);
                } break;
                case '~':
                    goLowPowerWithInputDetection();
                    break;
                case 'w': {

                    uint16_t value = 0;
                    String what = Serial.readStringUntil(' ');

                    if (what.length() > 5) {
                        ; // SerialMprintln(F("abort"));
                        inputStage = 0;
                        break;
                    }
                    if (what.equals("f")) {
                        if (rto->extClockGenDetected) {
                            Serial.print(F("old freqExtClockGen: "));
                            Serial.println((uint32_t)rto->freqExtClockGen);
                            rto->freqExtClockGen = Serial.parseInt();

                            if (rto->freqExtClockGen >= 1000000 && rto->freqExtClockGen <= 250000000) {
                                Si.setFreq(0, rto->freqExtClockGen);
                                rto->clampPositionIsSet = 0; // Clamp position setting
                                rto->coastPositionIsSet = 0; // coast position setting
                            }
                            Serial.print(F("set freqExtClockGen: "));
                            Serial.println((uint32_t)rto->freqExtClockGen);
                        }
                        break;
                    }

                    value = Serial.parseInt();
                    if (value < 4096) {
                        ; // SerialMprint("set ");
                        ; // SerialMprint(what);
                        ; // SerialMprint(" ");
                        ; // SerialMprintln(value);
                        if (what.equals("ht")) {

                            if (!rto->outModeHdBypass) {
                                rto->forceRetime = 1;
                                applyBestHTotal(value);
                            } else {
                                GBS::VDS_HSYNC_RST::write(value);
                            }
                        } else if (what.equals("vt")) {
                            set_vtotal(value);
                        } else if (what.equals("hsst")) {
                            setHSyncStartPosition(value);
                        } else if (what.equals("hssp")) {
                            setHSyncStopPosition(value);
                        } else if (what.equals("hbst")) {
                            setMemoryHblankStartPosition(value);
                        } else if (what.equals("hbsp")) {
                            setMemoryHblankStopPosition(value);
                        } else if (what.equals("hbstd")) {
                            setDisplayHblankStartPosition(value);
                        } else if (what.equals("hbspd")) {
                            setDisplayHblankStopPosition(value);
                        } else if (what.equals("vsst")) {
                            setVSyncStartPosition(value);
                        } else if (what.equals("vssp")) {
                            setVSyncStopPosition(value);
                        } else if (what.equals("vbst")) {
                            setMemoryVblankStartPosition(value);
                        } else if (what.equals("vbsp")) {
                            setMemoryVblankStopPosition(value);
                        } else if (what.equals("vbstd")) {
                            setDisplayVblankStartPosition(value);
                        } else if (what.equals("vbspd")) {
                            setDisplayVblankStopPosition(value);
                        } else if (what.equals("sog")) {
                            setAndUpdateSogLevel(value);
                        } else if (what.equals("ifini")) {
                            GBS::IF_INI_ST::write(value);
                        } else if (what.equals("ifvst")) {
                            GBS::IF_VB_ST::write(value);
                        } else if (what.equals("ifvsp")) {
                            GBS::IF_VB_SP::write(value);
                        } else if (what.equals("vsstc")) {
                            setCsVsStart(value);
                        } else if (what.equals("vsspc")) {
                            setCsVsStop(value);
                        }
                    } else {
                        ; // SerialMprintln("abort");
                    }
                } break;
                case 'x': {
                    uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
                    GBS::IF_HBIN_SP::write(if_hblank_scale_stop + 1);
                    ; // SerialMprint("1_26: ");
                    ; // SerialMprintln((if_hblank_scale_stop + 1), HEX);
                } break;
                case 'X': {
                    uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
                    GBS::IF_HBIN_SP::write(if_hblank_scale_stop - 1);
                    ; // SerialMprint("1_26: ");
                    ; // SerialMprintln((if_hblank_scale_stop - 1), HEX);
                } break;
                case '(': {
                    writeProgramArrayNew(ntsc_1920x1080, false);
                    doPostPresetLoadSteps();
                } break;
                case ')': {
                    writeProgramArrayNew(pal_1920x1080, false);
                    doPostPresetLoadSteps();
                } break;
                case 'V': {
                    ; // SerialMprint(F("step response "));
                    uopt->wantStepResponse = !uopt->wantStepResponse;
                    if (uopt->wantStepResponse) {
                        GBS::VDS_UV_STEP_BYPS::write(0);
                        ; // SerialMprintln("on");
                    } else {
                        GBS::VDS_UV_STEP_BYPS::write(1);
                        ; // SerialMprintln("off");
                    }
                    saveUserPrefs();
                } break;
                case 'S': {
                    snapToIntegralFrameRate();
                    break;
                }
                case ':':
                    externalClockGenSyncInOutRate();
                    break;
                case ';':
                    externalClockGenResetClock();
                    if (rto->extClockGenDetected) {
                        rto->extClockGenDetected = 0;
                        Serial.println(F("ext clock gen bypass"));
                    } else {
                        rto->extClockGenDetected = 1;
                        Serial.println(F("ext clock gen active"));
                        externalClockGenSyncInOutRate();
                    }
                    //{

                    //}
                    break;
                default:
                    Serial.print(F("unknown command "));
                    Serial.println(serialCommand, HEX);
                    break;
            }

            delay(1);

            lastVsyncLock = millis();

            if (!Serial.available()) {

                if (serialCommand != 'D') {
                    serialCommand = '@';
                }
                handleWiFi(1);
            }
        }

        if (userCommand != '@') {
            // printf("Web %c \n", userCommand);

            handleType2Command(userCommand);
            userCommand = '@';
            lastVsyncLock = millis();
            handleWiFi(1);

            // printf("uopt->presetSlot %d  \n", uopt->presetSlot);
        }
        Tim_web = millis();
    }
}

#if defined(ESP8266)
#include "webui_html.h"

void handleType2Command(char argument)
{
    // myLog("user", argument);
    switch (argument) {
        case '0':

            if (uopt->PalForce60 == 0) {
                uopt->PalForce60 = 1;
            } else {
                uopt->PalForce60 = 0;
            }
            saveUserPrefs();

            break;
        case '1':
            // reset to defaults button
            webSocket.close();
            loadDefaultUserOptions();
            saveUserPrefs();
            Serial.println(F("options set to defaults, restarting"));
            delay(60);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            //
            break;
        case '2':
            //
            break;
        case '3': // load custom preset
        {
            uopt->presetPreference = OutputCustomized; // custom
            if (rto->videoStandardInput == 14) {
                //
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            // printf("This load \n");
        } break;
        case '4': // Saving customized presets
            savePresetToSPIFFS();
            uopt->presetPreference = OutputCustomized; // custom
            saveUserPrefs();
            break;
        case '5':
            // 
            uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
            saveUserPrefs();
            if (uopt->enableFrameTimeLock) {
                ; // SerialMprintln(F("FTL on"));
            } else {
                ; // SerialMprintln(F("FTL off"));
            }
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->enableFrameTimeLock) {
                activeFrameTimeLockInitialSteps();
            }
            break;
        case '6':
            //
            break;
        case '7':
            uopt->wantScanlines = !uopt->wantScanlines;
            //  SerialMprint(F("scanlines: "));
            if (uopt->wantScanlines) {
                //  SerialMprintln(F("on (Line Filter recommended)"));
            } else {
                disableScanlines();
                // SerialMprintln("off");
            }
            saveUserPrefs();
            break;
        case '9':
            //
            break;
        case 'a':
            webSocket.close();
            Serial.println(F("restart"));
            delay(60);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            break;
        case 'e': // print files on spiffs
        {
            Dir dir = SPIFFS.openDir("/");
            while (dir.next()) {
                ;         // SerialMprint(dir.fileName());
                ;         // SerialMprint(" ");
                ;         // SerialMprintln(dir.fileSize());
                delay(1); // wifi stack
            }
            ////
            File f = SPIFFS.open("/preferencesv2.txt", "r");
            if (!f) {
                ; // SerialMprintln(F("failed opening preferences file"));
            } else {
                ; // SerialMprint(F("preset preference = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("frame time lock = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("preset slot = "));
                ; // SerialMprintln((uint8_t)(f.read()));
                ; // SerialMprint(F("frame lock method = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("auto gain = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("scanlines = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("component output = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("deinterlacer mode = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("line filter = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("peaking = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("preferScalingRgbhv = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("6-tap = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("pal force60 = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("matched = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("step response = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));
                ; // SerialMprint(F("disable external clock generator = "));
                ; // SerialMprintln((uint8_t)(f.read() - '0'));

                f.close();
            }
        } break;
        case 'f':
        case 'g':
        case 'h':
        case 'p':
        case 's':
        case 'L': {
            // Loading presets via webui
            uint8_t videoMode = getVideoMode();
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput; // 

            if (argument == 'f')
                uopt->presetPreference = Output960P; //Output960P; // 1280x960
            if (argument == 'g')
                uopt->presetPreference = Output720P; // 1280x720
            if (argument == 'h')
                uopt->presetPreference = Output480P; // 720x480/768x576
            if (argument == 'p')
                uopt->presetPreference = Output1024P; // 1280x1024
            if (argument == 's')
                uopt->presetPreference = Output1080P; // 1920x1080
            // if (argument == 'L')
            //   uopt->presetPreference = OutputDownscale; // downscale

            rto->useHdmiSyncFix = 1; // 
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(videoMode);
            }
            saveUserPrefs();
        } break;
        case 'i':
            // toggle active frametime lock method
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->frameTimeLockMethod == 0) {
                uopt->frameTimeLockMethod = 1;
            } else if (uopt->frameTimeLockMethod == 1) {
                uopt->frameTimeLockMethod = 0;
            }
            saveUserPrefs();
            activeFrameTimeLockInitialSteps();
            break;
        case 'l':
            // cycle through available SDRAM clocks
            {
                uint8_t PLL_MS = GBS::PLL_MS::read();
                uint8_t memClock = 0;

                if (PLL_MS == 0)
                    PLL_MS = 2;
                else if (PLL_MS == 2)
                    PLL_MS = 7;
                else if (PLL_MS == 7)
                    PLL_MS = 4;
                else if (PLL_MS == 4)
                    PLL_MS = 3;
                else if (PLL_MS == 3)
                    PLL_MS = 5;
                else if (PLL_MS == 5)
                    PLL_MS = 0;

                switch (PLL_MS) {
                    case 0:
                        memClock = 108;
                        break;
                    case 1:
                        memClock = 81;
                        break; // goes well with 4_2C = 0x14, 4_2D = 0x27
                    case 2:
                        memClock = 10;
                        break; // feedback clock
                    case 3:
                        memClock = 162;
                        break;
                    case 4:
                        memClock = 144;
                        break;
                    case 5:
                        memClock = 185;
                        break; // slight OC
                    case 6:
                        memClock = 216;
                        break; // !OC!
                    case 7:
                        memClock = 129;
                        break;
                    default:
                        break;
                }
                GBS::PLL_MS::write(PLL_MS);
                ResetSDRAM();
                if (memClock != 10) {
                    ; // SerialMprint(F("SDRAM clock: "));
                    ; // SerialMprint(memClock);
                    ; // SerialMprintln("Mhz");
                } else {
                    ; // SerialMprint(F("SDRAM clock: "));
                    ; // SerialMprintln(F("Feedback clock"));
                }
            }
            break;
        case 'm':; // SerialMprint(F("Line Filter: "));
            if (uopt->wantVdsLineFilter) {
                uopt->wantVdsLineFilter = 0;
                GBS::VDS_D_RAM_BYPS::write(1);
                ; // SerialMprintln("off");
            } else {
                uopt->wantVdsLineFilter = 1;
                GBS::VDS_D_RAM_BYPS::write(0);
                ; // SerialMprintln("on");
            }
            saveUserPrefs();
            break;
        case 'n':; // SerialMprint(F("ADC gain++ : "));
            uopt->enableAutoGain = 0;
            setAdcGain(GBS::ADC_RGCTRL::read() - 1);
            ; // SerialMprintln(GBS::ADC_RGCTRL::read(), HEX);
            break;
        case 'o':; // SerialMprint(F("ADC gain-- : "));
            uopt->enableAutoGain = 0;
            setAdcGain(GBS::ADC_RGCTRL::read() + 1);
            ; // SerialMprintln(GBS::ADC_RGCTRL::read(), HEX);
            break;
        case 'A': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd > 4) && (hbspd < (htotal - 4))) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
            } else {
                ; // SerialMprintln("limit");
            }
        } break;
        case 'B': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd < (htotal - 4)) && (hbspd > 4)) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
            } else {
                ; // SerialMprintln("limit");
            }
        } break;
        case 'C': {
            // vert mask +
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd > 6) && (vbspd < (vtotal - 4))) {
                GBS::VDS_DIS_VB_ST::write(vbstd - 2);
                GBS::VDS_DIS_VB_SP::write(vbspd + 2);
            } else {
                ; // SerialMprintln("limit");
            }
        } break;
        case 'D': {
            // vert mask -
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd < (vtotal - 4)) && (vbspd > 6)) {
                GBS::VDS_DIS_VB_ST::write(vbstd + 2);
                GBS::VDS_DIS_VB_SP::write(vbspd - 2);
            } else {
                ; // SerialMprintln("limit");
            }
        } break;
        case 'q':
            if (uopt->deintMode != 1) {
                uopt->deintMode = 1;
                disableMotionAdaptDeinterlace();
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read()) {
                    disableScanlines();
                }
                saveUserPrefs();
            }; // SerialMprintln(F("Deinterlacer: Bob"));
            break;
        case 'r':
            if (uopt->deintMode != 0) {
                uopt->deintMode = 0;
                saveUserPrefs();
                // will enable next loo p()
            }; // SerialMprintln(F("Deinterlacer: Motion Adaptive"));
            break;
        case 't':
            // unused now
            ; // SerialMprint(F("6-tap: "));
            if (uopt->wantTap6 == 0) {
                uopt->wantTap6 = 1;
                GBS::VDS_TAP6_BYPS::write(0);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() - 1);
                ; // SerialMprintln("on");
            } else {
                uopt->wantTap6 = 0;
                GBS::VDS_TAP6_BYPS::write(1);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
                ; // SerialMprintln("off");
            }
            saveUserPrefs();
            break;
        case 'u':
            // restart to attempt wifi station mode connect
            printf("RestWiFi.Sta : %d\n", WIFI_STA);
            delay(30);
            WiFi.mode(WIFI_STA);
            WiFi.hostname(device_hostname_partial); // _full
            delay(30);
            ESP.reset();
            break;
        case 'v': {
            uopt->wantFullHeight = !uopt->wantFullHeight;
            saveUserPrefs();
            uint8_t vidMode = getVideoMode();
            if (uopt->presetPreference == 5) {
                if (GBS::GBS_PRESET_ID::read() == 0x05 || GBS::GBS_PRESET_ID::read() == 0x15) {
                    applyPresets(vidMode);
                }
            }
        } break;
        case 'w':
            uopt->enableCalibrationADC = !uopt->enableCalibrationADC;
            saveUserPrefs();
            break;
        case 'x':
            uopt->preferScalingRgbhv = !uopt->preferScalingRgbhv;
            ; // SerialMprint(F("preferScalingRgbhv: "));
            if (uopt->preferScalingRgbhv) {
                ; // SerialMprintln("on");
                  // printf("on\n");
            } else {
                ; // SerialMprintln("off");
                  // printf("off\n");
            }
            saveUserPrefs();
            break;
        case 'X':; // SerialMprint(F("ExternalClockGenerator "));
            if (uopt->disableExternalClockGenerator == 0) {
                uopt->disableExternalClockGenerator = 1;
                // printf("disabled\n");
            } else {
                uopt->disableExternalClockGenerator = 0;
                // printf("enabled\n");
            }
            saveUserPrefs();
            break;
        case 'z':
            // sog slicer level
            if (rto->currentLevelSOG > 0) {
                rto->currentLevelSOG -= 1;
            } else {
                rto->currentLevelSOG = 16;
            }
            setAndUpdateSogLevel(rto->currentLevelSOG);
            optimizePhaseSP();
            ; // SerialMprint("Phase: ");
            ; // SerialMprint(rto->phaseSP);
            ; // SerialMprint(" SOG: ");
            ; // SerialMprint(rto->currentLevelSOG);
            ; // SerialMprintln();
            break;
        case 'E':
            // test option for now
            ; // SerialMprint(F("IF Auto Offset: "));
            toggleIfAutoOffset();
            if (GBS::IF_AUTO_OFST_EN::read()) {
                ; // SerialMprintln("on");
            } else {
                ; // SerialMprintln("off");
            }
            break;
        case 'F':
            // freeze pic
            if (GBS::CAPTURE_ENABLE::read()) {
                GBS::CAPTURE_ENABLE::write(0);
            } else {
                GBS::CAPTURE_ENABLE::write(1);
            }
            break;
        case 'K':
            // scanline strength
            if (uopt->scanlineStrength >= 0x10) {
                uopt->scanlineStrength -= 0x10;
            } else {
                uopt->scanlineStrength = 0x50;
            }
            if (rto->scanlinesEnabled) {
                GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
                GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength);
            }
            saveUserPrefs();
            break;
        case 'W':
            // Четкость
            if (GBS::VDS_PK_LB_GAIN::read() == 0x16) {
                GBS::VDS_PK_Y_H_BYPS::write(0);
                GBS::VDS_PK_LB_GAIN::write(0x5f); // 3_45
                GBS::VDS_PK_LH_GAIN::write(0x5f); // 3_47
                ;                                 // SerialMprintln("Sharpness - Medium");
                ;                                 // SerialMprint(F("LB_GAIN :"));
                ;                                 // SerialMprintln(GBS::VDS_PK_LB_GAIN::read(), HEX);
                ;                                 // SerialMprint(F("LH_GAIN :"));
                ;                                 // SerialMprintln(GBS::VDS_PK_LH_GAIN::read(), HEX);
            } else {
                GBS::VDS_PK_Y_H_BYPS::write(1);
                GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                GBS::VDS_PK_LH_GAIN::write(0x0A); // 3_47
                ;                                 // SerialMprintln("Sharpness - Norm");
                ;                                 // SerialMprint(F("LB_GAIN :"));
                ;                                 // SerialMprintln(GBS::VDS_PK_LB_GAIN::read(), HEX);
                ;                                 // SerialMprint(F("LH_GAIN :"));
                ;                                 // SerialMprintln(GBS::VDS_PK_LH_GAIN::read(), HEX);
            }
            break;
        case 'Z':
            // Y_offset +
            GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 1);
            if (GBS::VDS_Y_OFST::read() == 0x80)
                GBS::VDS_Y_OFST::write(0x00);
            break;
        case 'T':
            // Y_offset -
            GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 1);
            if (GBS::VDS_Y_OFST::read() == 0x7F) {
                GBS::VDS_Y_OFST::write(0x00);
            }
            break;
        case 'N':
            // U_offset +
            GBS::VDS_U_OFST::write(GBS::VDS_U_OFST::read() + 1);
            ; // SerialMprint(F("U_offset + : "));
            ; // SerialMprintln(GBS::VDS_U_OFST::read(), DEC);
            if (GBS::VDS_U_OFST::read() == 0x80) {
                GBS::VDS_U_OFST::write(0x00);
            }
            break;
        case 'M':
            // U_offset -
            GBS::VDS_U_OFST::write(GBS::VDS_U_OFST::read() - 1);
            ; // SerialMprint(F("U_offset - : "));
            ; // SerialMprintln(GBS::VDS_U_OFST::read(), DEC);
            if (GBS::VDS_U_OFST::read() == 0x7F) {
                GBS::VDS_U_OFST::write(0x00);
            }
            break;
        case 'Q':
            // V_offset +
            GBS::VDS_V_OFST::write(GBS::VDS_V_OFST::read() + 1);
            ; // SerialMprint(F("V_offset + : "));
            ; // SerialMprintln(GBS::VDS_V_OFST::read(), DEC);
            if (GBS::VDS_V_OFST::read() == 0x80) {
                GBS::VDS_V_OFST::write(0x00);
            }
            break;
        case 'H':
            // V_offset -
            GBS::VDS_V_OFST::write(GBS::VDS_V_OFST::read() - 1);
            ; // SerialMprint(F("V_offset - : "));
            ; // SerialMprintln(GBS::VDS_V_OFST::read(), DEC);
            if (GBS::VDS_V_OFST::read() == 0x7F) {
                GBS::VDS_V_OFST::write(0x00);
            }
            break;
            // case 'P':
            //     // Y_gain +
            //     GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 1);
            //     ; // SerialMprint(F("Y_gain + : "));
            //     ; // SerialMprintln(GBS::VDS_Y_GAIN::read(), DEC);
            //     break;
            // case 'S':
            //     // Y_gain -
            //     GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 1);
            //     ; // SerialMprint(F("Y_gain - : "));
            //     ; // SerialMprintln(GBS::VDS_Y_GAIN::read(), DEC);
            break;
        case 'V':
            // Цвет +
            GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() + 1);
            GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() + 1);
            ; // SerialMprint(F("Color + : "));
            ; // SerialMprintln(GBS::VDS_VCOS_GAIN::read(), DEC); // потолок 3D
            if (GBS::VDS_UCOS_GAIN::read() >= 0x39) {
                GBS::VDS_UCOS_GAIN::write(0x1C);
                GBS::VDS_VCOS_GAIN::write(0x29);
            }
            break;
        case 'R':
            // Цвет -
            GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() - 1);
            GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() - 1);
            ; // SerialMprint(F("Color - : "));
            ; // SerialMprintln(GBS::VDS_VCOS_GAIN::read(), DEC); // потолок 14
            if (GBS::VDS_UCOS_GAIN::read() <= 0x07) {
                GBS::VDS_UCOS_GAIN::write(0x1C);
            }
            if (GBS::VDS_VCOS_GAIN::read() <= 0x14) {
                GBS::VDS_VCOS_GAIN::write(0x29);
            }
            break;
        case 'O':
            // Инфо
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                ; // SerialMprintln("RGB timings");
                ; // SerialMprintln(F("------------ "));
                ; // SerialMprint(F("U_OFFSET "));
                ; // SerialMprintln(GBS::VDS_U_OFST::read(), DEC);
                ; // SerialMprint(F("V_OFFSET "));
                ; // SerialMprintln(GBS::VDS_V_OFST::read(), DEC);
                ; // SerialMprint(F("Y_OFFSET "));
                ; // SerialMprintln(GBS::VDS_Y_OFST::read(), DEC);
                ; // SerialMprintln(F("------------ "));
                ; // SerialMprint(F("UCOS_GAIN "));
                ; // SerialMprintln(GBS::VDS_UCOS_GAIN::read(), DEC);
                ; // SerialMprint(F("VCOS_GAIN "));
                ; // SerialMprintln(GBS::VDS_VCOS_GAIN::read(), DEC);
                ; // SerialMprint(F("Y_GAIN "));
                ; // SerialMprintln(GBS::VDS_Y_GAIN::read(), DEC);
            } else {
                ; // SerialMprintln("YPbPr timings");
                ; // SerialMprintln(F("------------ "));
                ; // SerialMprint(F("U_OFFSET "));
                ; // SerialMprintln(GBS::VDS_U_OFST::read(), DEC);
                ; // SerialMprint(F("V_OFFSET "));
                ; // SerialMprintln(GBS::VDS_V_OFST::read(), DEC);
                ; // SerialMprint(F("Y_OFFSET "));
                ; // SerialMprintln(GBS::VDS_Y_OFST::read(), DEC);
                ; // SerialMprintln(F("------------ "));
                ; // SerialMprint(F("UCOS_GAIN "));
                ; // SerialMprintln(GBS::VDS_UCOS_GAIN::read(), DEC);
                ; // SerialMprint(F("VCOS_GAIN "));
                ; // SerialMprintln(GBS::VDS_VCOS_GAIN::read(), DEC);
                ; // SerialMprint(F("Y_GAIN "));
                ; // SerialMprintln(GBS::VDS_Y_GAIN::read(), DEC);
            }
            break;
        case 'U': // Default
            // Сброс
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                GBS::VDS_Y_GAIN::write(128);
                GBS::VDS_UCOS_GAIN::write(28);
                GBS::VDS_VCOS_GAIN::write(41);

                GBS::VDS_Y_OFST::write(0);
                GBS::VDS_U_OFST::write(0);
                GBS::VDS_V_OFST::write(0);

                GBS::ADC_ROFCTRL::write(adco->r_off);
                GBS::ADC_GOFCTRL::write(adco->g_off);
                GBS::ADC_BOFCTRL::write(adco->b_off);
                ; // SerialMprintln("RGB:defauit");
            } else {
                GBS::VDS_Y_GAIN::write(128);
                GBS::VDS_UCOS_GAIN::write(28);
                GBS::VDS_VCOS_GAIN::write(41);
                // GBS::VDS_Y_OFST::write(254);
                GBS::VDS_U_OFST::write(3); //+16
                GBS::VDS_V_OFST::write(3);
                GBS::ADC_ROFCTRL::write(adco->r_off);
                GBS::ADC_GOFCTRL::write(adco->g_off);
                GBS::ADC_BOFCTRL::write(adco->b_off);

                if (BriorCon == 1) // RGB
                {
                    GBS::VDS_Y_OFST::write(0x0E);
                } else if (BriorCon == 2) // YUV
                {
                    // GBS::VDS_Y_OFST::write(-0x22);
                    GBS::VDS_Y_OFST::write(0x0E);
                };

                // (signed char)GBS::VDS_Y_OFST::read()  = (signed char)GBS::VDS_Y_OFST::read();
                // (signed char)GBS::VDS_U_OFST::read()  = (signed char)GBS::VDS_U_OFST::read();
                // (signed char)GBS::VDS_V_OFST::read()  = (signed char)GBS::VDS_V_OFST::read();

                R_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.402 * (signed char)((signed char)GBS::VDS_V_OFST::read())))) + 128;
                G_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) - (float)(0.344136 * (signed char)((signed char)GBS::VDS_U_OFST::read())) - 0.714136 * (signed char)((signed char)GBS::VDS_V_OFST::read()))) + 128;
                B_VAL = (((signed char)((signed char)GBS::VDS_Y_OFST::read()) + (float)(1.772 * (signed char)((signed char)GBS::VDS_U_OFST::read())))) + 128;
            }
            break;
        case 'I':
            if (IR == 0) {
                oled_menuItem = 5;
                ; // SerialMprintln("IR remote: DEC");
            } else if (IR == 1) {
                oled_menuItem = 0;
                ; // SerialMprintln("IR remote: OFF");
                IR = 0;
            }
            break;
        default:
            break;
    }
}

WiFiEventHandler disconnectedEventHandler;

void startWebserver()
{

    persWM.setApCredentials(final_ssid, ap_password);
    persWM.onConnect([]() {
    if (MDNS.begin(device_hostname_partial, WiFi.localIP()))
    { 
      
      MDNS.addService("http", "tcp", 80); 
      MDNS.announce();
    } });
    persWM.onAp([]() { ; });

    disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event) {
    Serial.print("Station disconnected, reason: ");
    Serial.println(event.reason); });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    
    if (ESP.getFreeHeap() > 10000)
    {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", webui_html, webui_html_len);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    } });

    server.on("/sc", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() > 10000)
    {
      int params = request->params();
      
      if (params > 0)
      {
        AsyncWebParameter *p = request->getParam(0);
        
        serialCommand = p->name().charAt(0);

        
        if (serialCommand == ' ')
        {
          serialCommand = '+';
        }
      }
      request->send(200); 
    } });

    server.on("/uc", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() > 10000)
    {
      int params = request->params();
      
      if (params > 0)
      {
        AsyncWebParameter *p = request->getParam(0);
        
        userCommand = p->name().charAt(0);
      }
      request->send(200); 
    } });

    server.on("/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "true");
    request->send(response);

    if (request->arg("n").length())
    {     
      if (request->arg("p").length())
      { 
        
        WiFi.begin(request->arg("n").c_str(), request->arg("p").c_str(), 0, 0, false);
      }
      else
      {
        WiFi.begin(request->arg("n").c_str(), emptyString, 0, 0, false);
      }
    }
    else
    {
      WiFi.begin();
    }
    // printf("Rest ESP\n");
    userCommand = 'u'; });

    server.on("/bin/slots.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() > 10000)
    {
      SlotMetaArray slotsObject;
      File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");

      if (!slotsBinaryFileRead)
      {
        File slotsBinaryFileWrite = SPIFFS.open(SLOTS_FILE, "w");
        for (int i = 0; i < SLOTS_TOTAL; i++)
        {
          slotsObject.slot[i].slot = i;
          slotsObject.slot[i].presetID = 0;
          slotsObject.slot[i].scanlines = 0;
          slotsObject.slot[i].scanlinesStrength = 0;
          slotsObject.slot[i].wantVdsLineFilter = false;
          slotsObject.slot[i].wantStepResponse = true;
          slotsObject.slot[i].wantPeaking = true;
          char emptySlotName[25] = "Empty                   ";
          strncpy(slotsObject.slot[i].name, emptySlotName, 25);
        }
        slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileWrite.close();
      }
      else
      {
        slotsBinaryFileRead.close();
      }

      request->send(SPIFFS, "/slots.bin", "application/octet-stream");
    } });

    server.on("/slot/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool result = false;

    if (ESP.getFreeHeap() > 10000)
    {
      int params = request->params();

      if (params > 0)
      {
        AsyncWebParameter *slotParam = request->getParam(0);
        String slotParamValue = slotParam->value();
        char slotValue[2];
        slotParamValue.toCharArray(slotValue, sizeof(slotValue));
        uopt->presetSlot = (uint8_t)slotValue[0];
        uopt->presetPreference = OutputCustomized;
        saveUserPrefs();
        result = true;
      }
    }

    request->send(200, "application/json", result ? "true" : "false"); });

    server.on("/slot/save", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool result = false;

    if (ESP.getFreeHeap() > 10000)
    {
      int params = request->params();

      if (params > 0)
      {
        SlotMetaArray slotsObject;
        File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");

        if (slotsBinaryFileRead)
        {
          slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
          slotsBinaryFileRead.close();
        }
        else
        {
          File slotsBinaryFileWrite = SPIFFS.open(SLOTS_FILE, "w");

          for (int i = 0; i < SLOTS_TOTAL; i++)
          {
            slotsObject.slot[i].slot = i;
            slotsObject.slot[i].presetID = 0;
            slotsObject.slot[i].scanlines = 0;
            slotsObject.slot[i].scanlinesStrength = 0;
            slotsObject.slot[i].wantVdsLineFilter = false;
            slotsObject.slot[i].wantStepResponse = true;
            slotsObject.slot[i].wantPeaking = true;
            char emptySlotName[25] = "Empty                   ";
            strncpy(slotsObject.slot[i].name, emptySlotName, 25);
          }

          slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
          slotsBinaryFileWrite.close();
        }

        AsyncWebParameter *slotIndexParam = request->getParam(0);
        String slotIndexString = slotIndexParam->value();
        uint8_t slotIndex = lowByte(slotIndexString.toInt());
        if (slotIndex >= SLOTS_TOTAL)
        {
          goto fail;
        }

        AsyncWebParameter *slotNameParam = request->getParam(1);
        String slotName = slotNameParam->value();

        char emptySlotName[25] = "                        ";
        strncpy(slotsObject.slot[slotIndex].name, emptySlotName, 25);

        slotsObject.slot[slotIndex].slot = slotIndex;
        slotName.toCharArray(slotsObject.slot[slotIndex].name, sizeof(slotsObject.slot[slotIndex].name));
        slotsObject.slot[slotIndex].presetID = rto->presetID;
        slotsObject.slot[slotIndex].scanlines = uopt->wantScanlines;
        slotsObject.slot[slotIndex].scanlinesStrength = uopt->scanlineStrength;
        slotsObject.slot[slotIndex].wantVdsLineFilter = uopt->wantVdsLineFilter;
        slotsObject.slot[slotIndex].wantStepResponse = uopt->wantStepResponse;
        slotsObject.slot[slotIndex].wantPeaking = uopt->wantPeaking;

        File slotsBinaryOutputFile = SPIFFS.open(SLOTS_FILE, "w");
        slotsBinaryOutputFile.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryOutputFile.close();

        result = true;
      }
    }

fail:
    request->send(200, "application/json", result ? "true" : "false"); });

    server.on("/slot/remove", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool result = false;
    int params = request->params();
    AsyncWebParameter *p = request->getParam(0);
    char param = p->name().charAt(0);
    if (params > 0)
    {
      if (param == '0')
      {
        result = true;
      }
      else
      {
        Ascii8 slot = uopt->presetSlot;
        Ascii8 nextSlot;
        auto currentSlot = slotIndexMap.indexOf(slot);

        SlotMetaArray slotsObject;
        File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");
        slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileRead.close();
        String slotName = slotsObject.slot[currentSlot].name;

        
        SPIFFS.remove("/preset_ntsc." + String((char)slot));
        SPIFFS.remove("/preset_pal." + String((char)slot));
        SPIFFS.remove("/preset_ntsc_480p." + String((char)slot));
        SPIFFS.remove("/preset_pal_576p." + String((char)slot));
        SPIFFS.remove("/preset_ntsc_720p." + String((char)slot));
        SPIFFS.remove("/preset_ntsc_1080p." + String((char)slot));
        SPIFFS.remove("/preset_medium_res." + String((char)slot));
        SPIFFS.remove("/preset_vga_upscale." + String((char)slot));
        SPIFFS.remove("/preset_unknown." + String((char)slot));

        uint8_t loopCount = 0;
        uint8_t flag = 1;
        while (flag != 0)
        {
          slot = slotIndexMap[currentSlot + loopCount];
          nextSlot = slotIndexMap[currentSlot + loopCount + 1];
          flag = 0;
          flag += SPIFFS.rename("/preset_ntsc." + String((char)(nextSlot)), "/preset_ntsc." + String((char)slot));
          flag += SPIFFS.rename("/preset_pal." + String((char)(nextSlot)), "/preset_pal." + String((char)slot));
          flag += SPIFFS.rename("/preset_ntsc_480p." + String((char)(nextSlot)), "/preset_ntsc_480p." + String((char)slot));
          flag += SPIFFS.rename("/preset_pal_576p." + String((char)(nextSlot)), "/preset_pal_576p." + String((char)slot));
          flag += SPIFFS.rename("/preset_ntsc_720p." + String((char)(nextSlot)), "/preset_ntsc_720p." + String((char)slot));
          flag += SPIFFS.rename("/preset_ntsc_1080p." + String((char)(nextSlot)), "/preset_ntsc_1080p." + String((char)slot));
          flag += SPIFFS.rename("/preset_medium_res." + String((char)(nextSlot)), "/preset_medium_res." + String((char)slot));
          flag += SPIFFS.rename("/preset_vga_upscale." + String((char)(nextSlot)), "/preset_vga_upscale." + String((char)slot));
          flag += SPIFFS.rename("/preset_unknown." + String((char)(nextSlot)), "/preset_unknown." + String((char)slot));

          slotsObject.slot[currentSlot + loopCount].slot = slotsObject.slot[currentSlot + loopCount + 1].slot;
          slotsObject.slot[currentSlot + loopCount].presetID = slotsObject.slot[currentSlot + loopCount + 1].presetID;
          slotsObject.slot[currentSlot + loopCount].scanlines = slotsObject.slot[currentSlot + loopCount + 1].scanlines;
          slotsObject.slot[currentSlot + loopCount].scanlinesStrength = slotsObject.slot[currentSlot + loopCount + 1].scanlinesStrength;
          slotsObject.slot[currentSlot + loopCount].wantVdsLineFilter = slotsObject.slot[currentSlot + loopCount + 1].wantVdsLineFilter;
          slotsObject.slot[currentSlot + loopCount].wantStepResponse = slotsObject.slot[currentSlot + loopCount + 1].wantStepResponse;
          slotsObject.slot[currentSlot + loopCount].wantPeaking = slotsObject.slot[currentSlot + loopCount + 1].wantPeaking;
          
          strncpy(slotsObject.slot[currentSlot + loopCount].name, slotsObject.slot[currentSlot + loopCount + 1].name, 25);
          loopCount++;
        }

        File slotsBinaryFileWrite = SPIFFS.open(SLOTS_FILE, "w");
        slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileWrite.close();
        result = true;
      }
    }

fail:
    request->send(200, "application/json", result ? "true" : "false"); });

    server.on("/spiffs/upload", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "application/json", "true"); });

    server.on(
        "/spiffs/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "true");
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                request->_tempFile = SPIFFS.open("/" + filename, "w");
            }
            if (len) {
                request->_tempFile.write(data, len);
            }
            if (final) {
                request->_tempFile.close();
            }
        });

    server.on("/spiffs/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() > 10000)
    {
      int params = request->params();
      if (params > 0)
      {
        request->send(SPIFFS, request->getParam(0)->value(), String(), true);
      }
      else
      {
        request->send(200, "application/json", "false");
      }
    }
    else
    {
      request->send(200, "application/json", "false");
    } });

    server.on("/spiffs/dir", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() > 10000)
    {
      Dir dir = SPIFFS.openDir("/");
      String output = "[";

      while (dir.next())
      {
        output += "\"";
        output += dir.fileName();
        output += "\",";
        delay(1); 
      }

      output += "]";

      output.replace(",]", "]");

      request->send(200, "application/json", output);
      return;
    }
    request->send(200, "application/json", "false"); });

    server.on("/spiffs/format", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "application/json", SPIFFS.format() ? "true" : "false"); });

    server.on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    WiFiMode_t wifiMode = WiFi.getMode();
    request->send(200, "application/json", wifiMode == WIFI_AP ? "{\"mode\":\"ap\"}" : "{\"mode\":\"sta\",\"ssid\":\"" + WiFi.SSID() + "\"}"); });

    server.on("/gbs/restore-filters", HTTP_GET, [](AsyncWebServerRequest *request) {
    SlotMetaArray slotsObject;
    File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");
    bool result = false;
    if (slotsBinaryFileRead)
    {
      slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
      slotsBinaryFileRead.close();
      auto currentSlot = slotIndexMap.indexOf(uopt->presetSlot);
      if (currentSlot == -1)
      {
        goto fail;
      }

      uopt->wantScanlines = slotsObject.slot[currentSlot].scanlines;
      if (uopt->wantScanlines)
      {
      }
      else
      {
        disableScanlines();
      }
      saveUserPrefs();

      uopt->scanlineStrength = slotsObject.slot[currentSlot].scanlinesStrength;
      uopt->wantVdsLineFilter = slotsObject.slot[currentSlot].wantVdsLineFilter;
      uopt->wantStepResponse = slotsObject.slot[currentSlot].wantStepResponse;
      uopt->wantPeaking = slotsObject.slot[currentSlot].wantPeaking;
      result = true;
    }

fail:
    request->send(200, "application/json", result ? "true" : "false"); });

    persWM.setConnectNonBlock(true);
    if (WiFi.SSID().length() == 0) {
        persWM.setupWiFiHandlers();
        persWM.startApMode();
    } else {
        persWM.begin();
    }

    server.begin();
    webSocket.begin();
    yield();

#ifdef HAVE_PINGER_LIBRARY

    pinger.OnReceive([](const PingerResponse &response) {
    if (response.ReceivedResponse)
{
      Serial.printf(
        "Reply from %s: time=%lums\n",
        response.DestIPAddress.toString().c_str(),
        response.ResponseTime);

      pingLastTime = millis() - 900; 
    }
    else
    {
      Serial.printf("Request timed out.\n");
    }

    
    
    return true; });

    pinger.OnEnd([](const PingerResponse &response) { return true; });
#endif
}

void initUpdateOTA()
{
    ArduinoOTA.setHostname("GBS OTA");

    ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else 
      type = "filesystem";

    
    SPIFFS.end();
    ; });
    ArduinoOTA.onEnd([]() { ; });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { ; });
    ArduinoOTA.onError([](ota_error_t error) {
    ;
    if (error == OTA_AUTH_ERROR)
      ;
    else if (error == OTA_BEGIN_ERROR)
      ;
    else if (error == OTA_CONNECT_ERROR)
      ;
    else if (error == OTA_RECEIVE_ERROR)
      ;
    else if (error == OTA_END_ERROR)
      ; });
    ArduinoOTA.begin();
    yield();
}

void StrClear(char *str, uint16_t length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

const uint8_t *loadPresetFromSPIFFS(byte forVideoMode) // 
{

    static uint8_t preset[432]; //
    String s = "";
    Ascii8 slot = 0;
    File f;

    f = SPIFFS.open("/preferencesv2.txt", "r");
    if (f) {
        uint8_t result[3];
        uint8_t dump = 0;
        result[0] = f.read();
        result[1] = f.read();
        result[2] = f.read();
        f.close();
        slot = result[2];
    } else {
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    }

    if (forVideoMode == 1) {
        f = SPIFFS.open("/preset_ntsc." + String((char)slot), "r");
    } else if (forVideoMode == 2) {
        f = SPIFFS.open("/preset_pal." + String((char)slot), "r");
    } else if (forVideoMode == 3) {
        f = SPIFFS.open("/preset_ntsc_480p." + String((char)slot), "r");
    } else if (forVideoMode == 4) {
        f = SPIFFS.open("/preset_pal_576p." + String((char)slot), "r");
    } else if (forVideoMode == 5) {
        f = SPIFFS.open("/preset_ntsc_720p." + String((char)slot), "r");
    } else if (forVideoMode == 6) {
        f = SPIFFS.open("/preset_ntsc_1080p." + String((char)slot), "r");
    } else if (forVideoMode == 8) {
        f = SPIFFS.open("/preset_medium_res." + String((char)slot), "r");
    } else if (forVideoMode == 14) {
        f = SPIFFS.open("/preset_vga_upscale." + String((char)slot), "r");
    } else if (forVideoMode == 0) {
        f = SPIFFS.open("/preset_unknown." + String((char)slot), "r");
    }

    if (!f) // open failed
    {
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    } else {
        s = f.readStringUntil('}'); // 
        f.close();
    }

    char *tmp;
    uint16_t i = 0;
    tmp = strtok(&s[0], ","); // 
    while (tmp) {
        preset[i++] = (uint8_t)atoi(tmp); 
        tmp = strtok(NULL, ",");
        yield();
    }
    return preset;
}

void savePresetToSPIFFS()
{

    uint8_t readout = 0;
    File f;
    Ascii8 slot = 0;

    f = SPIFFS.open("/preferencesv2.txt", "r");
    if (f) {
        uint8_t result[3];
        result[0] = f.read();
        result[1] = f.read();
        result[2] = f.read();
        f.close();
        slot = result[2];
    } else {
        return;
    }

    if (rto->videoStandardInput == 1) {
        f = SPIFFS.open("/preset_ntsc." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 2) {
        f = SPIFFS.open("/preset_pal." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 3) {
        f = SPIFFS.open("/preset_ntsc_480p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 4) {
        f = SPIFFS.open("/preset_pal_576p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 5) {
        f = SPIFFS.open("/preset_ntsc_720p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 6) {
        f = SPIFFS.open("/preset_ntsc_1080p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 8) {
        f = SPIFFS.open("/preset_medium_res." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 14) {
        f = SPIFFS.open("/preset_vga_upscale." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 0) {
        f = SPIFFS.open("/preset_unknown." + String((char)slot), "w");
    }

    if (f) {
        GBS::GBS_PRESET_CUSTOM::write(1);

        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
            disableScanlines();
        }

        if (!rto->extClockGenDetected) {
            if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
        }

        for (int i = 0; i <= 5; i++) {
            writeOneByte(0xF0, i);
            switch (i) {
                case 0:
                    for (int x = 0x40; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    for (int x = 0x90; x <= 0x9F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 1:
                    for (int x = 0x0; x <= 0x2F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 2:

                    break;
                case 3:
                    for (int x = 0x0; x <= 0x7F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 4:
                    for (int x = 0x0; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 5:
                    for (int x = 0x0; x <= 0x6F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
            }
        }
        f.println("};");
        f.close();
        // delay(100);
        // printf("Info: 0x%02x \n", Info);
        // delay(100);
    }
}
// void SaveUserIRRemote()
// {

//   File f = SPIFFS.open("/IRmote.txt", "w");
//   if (!f)
//   {
//     return;
//   }
//   f.write((uint8_t *)IRKeyMenu, 4);
//   f.write((uint8_t *)IRKeySave, 4);
//   f.write((uint8_t *)IRKeyInfo, 4);
//   f.write((uint8_t *)IRKeyRight, 4);
//   f.write((uint8_t *)IRKeyLeft, 4);
//   f.write((uint8_t *)IRKeyUp, 4);
//   f.write((uint8_t *)IRKeyDown, 4);
//   f.write((uint8_t *)IRKeyOk, 4);
//   f.write((uint8_t *)IRKeyExit, 4);
//   f.write((uint8_t *)IRKeyMute, 4);
//   f.write((uint8_t *)kRecv2, 4);
//   f.write((uint8_t *)kRecv3, 4);

//   f.close();
// }

void ReadUserIRRemote()
{

    File f = SPIFFS.open("/IRmote.txt", "r");
    if (!f) {
        return;
    }
    f.read((uint8_t *)IRKeyMenu, 4);
    f.read((uint8_t *)IRKeySave, 4);
    f.read((uint8_t *)IRKeyInfo, 4);
    f.read((uint8_t *)IRKeyRight, 4);
    f.read((uint8_t *)IRKeyLeft, 4);
    f.read((uint8_t *)IRKeyUp, 4);
    f.read((uint8_t *)IRKeyDown, 4);
    f.read((uint8_t *)IRKeyOk, 4);
    f.read((uint8_t *)IRKeyExit, 4);
    f.read((uint8_t *)IRKeyMute, 4);
    f.read((uint8_t *)kRecv2, 4);
    f.read((uint8_t *)kRecv3, 4);
    f.close();
}
// = (uint8_t)(f.read() - '0');
void saveUserPrefs()
{

    File f = SPIFFS.open("/preferencesv2.txt", "w");
    if (!f) {
        return;
    }
    f.write(uopt->presetPreference + '0');
    f.write(uopt->enableFrameTimeLock + '0');
    f.write(uopt->presetSlot);
    f.write(uopt->frameTimeLockMethod + '0');
    f.write(uopt->enableAutoGain + '0');
    f.write(uopt->wantScanlines + '0');
    f.write(uopt->wantOutputComponent + '0');
    f.write(uopt->deintMode + '0');
    f.write(uopt->wantVdsLineFilter + '0');
    f.write(uopt->wantPeaking + '0');
    f.write(uopt->preferScalingRgbhv + '0');
    f.write(uopt->wantTap6 + '0');
    f.write(uopt->PalForce60 + '0');
    f.write(uopt->matchPresetSource + '0');
    f.write(uopt->wantStepResponse + '0');
    f.write(uopt->wantFullHeight + '0');
    f.write(uopt->enableCalibrationADC + '0');
    f.write(uopt->scanlineStrength + '0');
    f.write(uopt->disableExternalClockGenerator + '0');
    f.write(Volume + '0');

    // f.write(InCurrent + '0');
    f.write(SeleInputSource + '0');

    // f.write(GBS::SP_EXT_SYNC_SEL::read() + '0');
    // f.write(GBS::ADC_INPUT_SEL::read() + '0');

    f.write(SvModeOption / 10 + '0');
    f.write(SvModeOption % 10 + '0');
    f.write(AvModeOption / 10 + '0');
    f.write(AvModeOption % 10 + '0');
    // printf(" SV AV: %d  %d \n",SvModeOption,AvModeOption);
    f.write(SmoothOption + '0');
    f.write(LineOption + '0');
    f.write(BriorCon + '0'); // 26
    f.write(Info + '0');     // 27
    f.write(RGB_Com + '0');

    f.write((Bright / 100) + '0');
    f.write((Bright % 100) / 10 + '0');
    f.write(Bright % 10 + '0');

    f.write((Contrast / 100) + '0');
    f.write((Contrast % 100) / 10 + '0');
    f.write(Contrast % 10 + '0');

    f.write((Saturation / 100) + '0');
    f.write((Saturation % 100) / 10 + '0');
    f.write(Saturation % 10 + '0');
    f.close();
}

#endif

void OSD_selectOption()
{

    if (oled_menuItem == 0) {
        NEW_OLED_MENU = true;
    } else {
        NEW_OLED_MENU = false;
    }



    if (oled_menuItem == OSD_Resolution) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->>>");
        display.drawString(1, 28, "Output Resolution");
        display.display();

        if (results.value == IRKeyUp || results.value == IRKeyDown) {
            COl_L = 2;
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('1');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('1');
                    oled_menuItem = OSD_ScreenSettings;
                    break;
                case IRKeyOk:
                    oled_menuItem = OSD_Resolution_1080;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('3');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_ScreenSettings) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.drawString(1, 0, "Menu->>>");
        display.drawString(1, 28, "Screen Settings");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('1');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyOk:
                    oled_menuItem = 75;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('6');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_ColorSettings) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.drawString(1, 0, "Menu->>>");
        display.drawString(1, 28, "Picture Settings");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            COl_L = 2;
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('2');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyUp:
                    COl_L = 1;
                    // OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    oled_menuItem = OSD_ResetDefault;
                    // OSD_menu_F('2');

                    // oled_menuItem = OSD_ResetDefault;
                    // COl_L = 3;
                    // OSD_menu_F(OSD_CROSS_BOTTOM);
                    // OSD_menu_F('2');

                    break;
                case IRKeyOk:
                    // oled_menuItem = 82;
                    oled_menuItem = 88;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('d');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.drawString(1, 0, "Menu->>>");
        display.drawString(1, 28, "System Settings");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('2');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyUp:
                    oled_menuItem = OSD_ScreenSettings;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('1');
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    // OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyOk:
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    // if (oled_menuItem == 66)
    // {

    //     if(OLED_clear_flag)
    // display.clear();
    // OLED_clear_flag = ~0;display.setColor(OLEDDISPLAY_COLOR::WHITE);
    //     display.drawString(1, 0, "Menu-");
    //     display.drawString(1, 28, "Developer");
    //     display.display();

    //     if (results.value == IRKeyDown  ||  results.value == IRKeyUp)
    //     {
    //         OSD_c1(icon4, P0, blue_fill);
    //         OSD_c3(icon4, P0, blue_fill);
    //         OSD_c2(icon4, P0, yellow);
    //     }
    //     else if (results.value == IRKeyUp)
    //     {
    //         OSD_c1(icon4, P0, blue_fill);
    //         OSD_c3(icon4, P0, blue_fill);
    //         OSD_c2(icon4, P0, yellow);
    //     }

    //     if (irrecv.decode(&results))
    //     {
    //         switch (results.value)
    //         {
    //         case IRKeyUp:
    //             COl_L = 1;
    //             OSD_menu_F('2');
    //             oled_menuItem = OSD_SystemSettings;
    //             break;
    //         case IRKeyDown:
    //             COl_L = 3;
    //             OSD_menu_F('2');
    //             oled_menuItem = OSD_ResetDefault;
    //             break;
    //         case IRKeyOk:
    //             oled_menuItem = 104;
    //             OSD_menu_F(OSD_CROSS_TOP);
    //             OSD_menu_F('q');
    //             break;
    //         case IRKeyExit:
    //             oled_menuItem = 0;
    //             OSD_clear();
    //             OSD();
    //             break;
    //         }
    //         irrecv.resume();
    //     }
    // }

    else if (oled_menuItem == OSD_ResetDefault) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.drawString(1, 0, "Menu-");
        display.drawString(1, 28, "Reset Settings");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyOk) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);
            OSD_menu_F('2');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;

                    // oled_menuItem = OSD_ColorSettings;
                    // COl_L = 2;
                    // OSD_menu_F(OSD_CROSS_MID);
                    // OSD_menu_F('2');

                    break;
                case IRKeyOk:
                    userCommand = '1';
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_Resolution_1080) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Output");
        display.drawString(1, 28, "1920x1080");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('1');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('3');
                    oled_menuItem = OSD_Resolution_1024;
                    break;
                case IRKeyOk:
                    // uopt->preferScalingRgbhv = true;
                    userCommand = 's';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_Resolution_1024) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Output");
        display.drawString(1, 28, "1280x1024");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('1');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('3');
                    oled_menuItem = OSD_Resolution_1080;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('3');
                    oled_menuItem = OSD_Resolution_960;
                    break;
                case IRKeyOk:
                    // uopt->preferScalingRgbhv = true;
                    userCommand = 'p';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_Resolution_960) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Output");
        display.drawString(1, 28, "1280x960");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('3');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('1');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('3');
                    oled_menuItem = OSD_Resolution_1024;
                    break;
                case IRKeyDown:
                    oled_menuItem = OSD_Resolution_720;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('4');
                    break;
                case IRKeyOk:
                    // uopt->preferScalingRgbhv = true;
                    userCommand = 'f';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_Resolution_720) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Output");
        display.drawString(1, 28, "1280x720");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('4');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('1');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = OSD_Resolution_960;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('3');
                    break;
                // case IRKeyDown:
                //   COl_L = 2;
                //   OSD_menu_F('4');
                //   oled_menuItem = OSD_Resolution_pass;

                //   break;
                case IRKeyOk:
                    // uopt->preferScalingRgbhv = true;
                    userCommand = 'g';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    // if (oled_menuItem == 72)
    // {

    //     if(OLED_clear_flag)
    // display.clear();
    // OLED_clear_flag = ~0;display.setColor(OLEDDISPLAY_COLOR::WHITE);
    // display.setTextAlignment(TEXT_ALIGN_LEFT);
    //     display.setFont(ArialMT_Plain_10);
    //     display.drawString(1, 0, "Menu->Output");
    //     display.setFont(ArialMT_Plain_16);
    //     display.drawString(1, 28, "480p/576p");
    //     display.display();

    //     if (results.value == IRKeyDown  ||  results.value == IRKeyUp)
    //     {
    //         OSD_c1(icon4, P0, blue_fill);
    //         OSD_c3(icon4, P0, blue_fill);
    //         OSD_c2(icon4, P0, yellow);
    //     }

    //     if (irrecv.decode(&results))
    //     {
    //         switch (results.value)
    //         {
    //         case IRKeyMenu:
    //             OSD_menu_F(OSD_CROSS_MID);
    //             OSD_menu_F('1');
    //             oled_menuItem = 62;
    //             break;
    //         case IRKeyUp:
    //             COl_L = 1;
    //             OSD_menu_F('4');
    //             oled_menuItem = OSD_Resolution_720;
    //             break;
    //         case IRKeyDown:
    //             COl_L = 3;
    //             OSD_menu_F('4');
    //             oled_menuItem = 73;
    //             break;
    //         case IRKeyOk:
    //             userCommand = 'h';
    //             break;
    //         case IRKeyExit:
    //             oled_menuItem = OSD_Input;
    //             OSD_clear();
    //             OSD();
    //             break;
    //         }
    //         irrecv.resume();
    //     }
    // }

    // if (oled_menuItem == 73)
    // {

    //     if(OLED_clear_flag)
    // display.clear();
    // OLED_clear_flag = ~0;display.setColor(OLEDDISPLAY_COLOR::WHITE);
    //     display.drawString(1, 0, "Menu-");
    //     display.drawString(1, 28, "Downscale");
    //     display.display();

    //     if (results.value == IRKeyDown  ||  results.value == IRKeyUp)
    //     {
    //         OSD_c3(icon4, P0, yellow);
    //         OSD_c1(icon4, P0, blue_fill);
    //         OSD_c2(icon4, P0, blue_fill);
    //         OSD_menu_F('4');
    //     }

    //     if (irrecv.decode(&results))
    //     {
    //         switch (results.value)
    //         {
    //         case IRKeyMenu:
    //             OSD_menu_F(OSD_CROSS_MID);
    //             OSD_menu_F('1');
    //             oled_menuItem = 62;
    //             break;
    //         case IRKeyUp:
    //             COl_L = 2;
    //             OSD_menu_F('4');
    //             oled_menuItem = 72;
    //             break;
    //         case IRKeyDown:
    //             oled_menuItem = OSD_Resolution_pass;
    //             OSD_menu_F(OSD_CROSS_TOP);
    //             OSD_menu_F('5');
    //             break;
    //         case IRKeyOk:
    //             userCommand = 'L';
    //             break;
    //         case IRKeyExit:
    //             oled_menuItem = OSD_Input;
    //             OSD_clear();
    //             OSD();
    //             break;
    //         }
    //         irrecv.resume();
    //     }
    // }

    else if (oled_menuItem == OSD_Resolution_pass) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Output");
        display.drawString(1, 28, "Pass Through");
        display.display();
        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('4');
        }
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('1');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('4');
                    oled_menuItem = OSD_Resolution_720;
                    break;
                // case IRKeyDown:
                //     oled_menuItem = OSD_Resolution_1080;
                //     OSD_menu_F(OSD_CROSS_TOP);
                //     OSD_menu_F('3');
                //     break;
                case IRKeyOk:
                    // tentative = uopt->presetPreference;
                    // if(Info == InfoVGA)
                    // {
                    //     uopt->preferScalingRgbhv = false;
                    // }
                    // else
                    // {
                    //   // uopt->preferScalingRgbhv = true;
                    //   serialCommand = 'K';
                    // }

                    // saveUserPrefs();

                    // OSD_menu_F(OSD_CROSS_TOP);
                    // OSD_menu_F('!');
                    // oled_menuItem = OSD_Resolution_RetainedSettings;
                    // Keep_S = 0;
                    // Tim_Resolution_Start = millis();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }
    else if (oled_menuItem == OSD_Resolution_RetainedSettings) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Retained settings?");
        // display.drawString(1, 20, "settings?");
        display.display();
        // if (results.value == IRKeyOk)
        // {
        //   OSD_menu_F('·');  
        // }
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('4');
                    oled_menuItem = OSD_Resolution_pass;
                    break;
                case IRKeyRight:
                    Keep_S = 0;
                    OSD_menu_F('!');
                    break;
                case IRKeyLeft:
                    Keep_S = 1;
                    OSD_menu_F('!');
                    break;
                case IRKeyOk:
                    if (Keep_S) {
                        saveUserPrefs();
                        // printf("Save \n");
                    } else {
                        if (tentative == Output960P) // 1280x960
                            userCommand = 'f';
                        else if (tentative == Output720P) // 1280x720
                            userCommand = 'g';
                        else if (tentative == Output1024P) // 1280x1024
                            userCommand = 'p';
                        else if (tentative == Output1080P) // 1920x1080
                            userCommand = 's';
                        else
                            userCommand = 'g';
                    }

                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('4');
                    oled_menuItem = OSD_Resolution_pass;
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 75) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Screen");
        display.drawString(1, 28, "Move");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_ScreenSettings;
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('6');
                    oled_menuItem = 76;
                    break;
                case IRKeyOk:
                    oled_menuItem = 79;
                    OSD_menu_F('6');
                    OSD_c1(0x3E, P5, main0);
                    OSD_c1(0x3E, P6, main0);
                    OSD_c1(0x3E, P7, main0);
                    OSD_c1(0x3E, P8, main0);
                    OSD_c1(0x3E, P9, main0);
                    OSD_c1(0x3E, P10, main0);
                    OSD_c1(0x3E, P11, main0);
                    OSD_c1(0x3E, P12, main0);
                    OSD_c1(0x3E, P13, main0);
                    OSD_c1(0x08, P15, yellow);
                    OSD_c1(0x18, P16, yellow);
                    OSD_c1(0x03, P14, yellow);
                    OSD_c1(0x13, P17, yellow);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 76) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Screen");
        display.drawString(1, 28, "Scale");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_ScreenSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('6');
                    oled_menuItem = 75;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('6');
                    oled_menuItem = 77;
                    break;
                case IRKeyOk:
                    oled_menuItem = 80;
                    OSD_menu_F('6');
                    OSD_c2(0x3E, P6, main0);
                    OSD_c2(0x3E, P7, main0);
                    OSD_c2(0x3E, P8, main0);
                    OSD_c2(0x3E, P9, main0);
                    OSD_c2(0x3E, P10, main0);
                    OSD_c2(0x3E, P11, main0);
                    OSD_c2(0x3E, P12, main0);
                    OSD_c2(0x3E, P13, main0);
                    OSD_c2(0x08, P15, yellow);
                    OSD_c2(0x18, P16, yellow);
                    OSD_c2(0x03, P14, yellow);
                    OSD_c2(0x13, P17, yellow);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 77) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Screen");
        display.drawString(1, 28, "Borders");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_ScreenSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('6');
                    oled_menuItem = 76;
                    break;
                case IRKeyDown:
                    oled_menuItem = 95;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('o');
                    break;
                case IRKeyOk:
                    oled_menuItem = 81;
                    OSD_menu_F('6');
                    OSD_c3(0x3E, P8, main0);
                    OSD_c3(0x3E, P9, main0);
                    OSD_c3(0x3E, P10, main0);
                    OSD_c3(0x3E, P11, main0);
                    OSD_c3(0x3E, P12, main0);
                    OSD_c3(0x3E, P13, main0);
                    OSD_c3(0x08, P15, yellow);
                    OSD_c3(0x18, P16, yellow);
                    OSD_c3(0x03, P14, yellow);
                    OSD_c3(0x13, P17, yellow);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 79) {

        if (results.value == IRKeyOk) {
            OSD_menu_F('6');
            OSD_c1(0x3E, P5, main0);
            OSD_c1(0x3E, P6, main0);
            OSD_c1(0x3E, P7, main0);
            OSD_c1(0x3E, P8, main0);
            OSD_c1(0x3E, P9, main0);
            OSD_c1(0x3E, P10, main0);
            OSD_c1(0x3E, P11, main0);
            OSD_c1(0x3E, P12, main0);
            OSD_c1(0x3E, P13, main0);
            OSD_c1(0x08, P15, yellow);
            OSD_c1(0x18, P16, yellow);
            OSD_c1(0x03, P14, yellow);
            OSD_c1(0x13, P17, yellow);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('6');
                    oled_menuItem = 75;
                    break;
                case IRKeyOk:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('6');
                    oled_menuItem = 75;
                    break;
                case IRKeyRight:
                    Tim_menuItem = millis();
                    serialCommand = '6';
                    if (GBS::IF_HBIN_SP::read() >= 10) {
                    } else {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca1;
                            __(l, _20);
                            __(i, _21);
                            __(m, _22);
                            __(i, _23);
                            __(t, _24);
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca1;
                    __(l, _20);
                    __(i, _21);
                    __(m, _22);
                    __(i, _23);
                    __(t, _24);
                    __(0x0d, _25);
                    break;
                case IRKeyLeft:
                    Tim_menuItem = millis();
                    serialCommand = '7';
                    if (GBS::IF_HBIN_SP::read() < 0x150) {
                    } else {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca1;
                            __(l, _20);
                            __(i, _21);
                            __(m, _22);
                            __(i, _23);
                            __(t, _24);
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca1;
                    __(l, _20);
                    __(i, _21);
                    __(m, _22);
                    __(i, _23);
                    __(t, _24);
                    __(0x0d, _25);
                    break;
                case IRKeyUp:
                    Tim_menuItem = millis();
                    shiftVerticalUpIF();
                    break;
                case IRKeyDown:
                    Tim_menuItem = millis();
                    shiftVerticalDownIF();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 80) {

        if (results.value == IRKeyOk) {
            OSD_menu_F('6');
            OSD_c2(0x3E, P6, main0);
            OSD_c2(0x3E, P7, main0);
            OSD_c2(0x3E, P8, main0);
            OSD_c2(0x3E, P9, main0);
            OSD_c2(0x3E, P10, main0);
            OSD_c2(0x3E, P11, main0);
            OSD_c2(0x3E, P12, main0);
            OSD_c2(0x3E, P13, main0);
            OSD_c2(0x08, P15, yellow);
            OSD_c2(0x18, P16, yellow);
            OSD_c2(0x03, P14, yellow);
            OSD_c2(0x13, P17, yellow);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('6');
                    oled_menuItem = 76;
                    break;
                case IRKeyOk:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('6');
                    oled_menuItem = 76;
                    break;
                case IRKeyRight:
                    Tim_menuItem = millis();
                    serialCommand = 'h';
                    if (GBS::VDS_HSCALE::read() == 1023) {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca2;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca2;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyLeft:
                    Tim_menuItem = millis();
                    serialCommand = 'z';
                    if (GBS::VDS_HSCALE::read() <= 256) {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca2;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca2;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyUp:
                    Tim_menuItem = millis();
                    serialCommand = '5';
                    if (GBS::VDS_VSCALE::read() == 1023) {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca2;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca2;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyDown:
                    Tim_menuItem = millis();
                    serialCommand = '4';
                    if (GBS::VDS_VSCALE::read() <= 256) {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca2;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca2;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 81) {

        if (results.value == IRKeyOk) {
            OSD_menu_F('6');
            OSD_c3(0x3E, P8, main0);
            OSD_c3(0x3E, P9, main0);
            OSD_c3(0x3E, P10, main0);
            OSD_c3(0x3E, P11, main0);
            OSD_c3(0x3E, P12, main0);
            OSD_c3(0x3E, P13, main0);
            OSD_c3(0x08, P15, yellow);
            OSD_c3(0x18, P16, yellow);
            OSD_c3(0x03, P14, yellow);
            OSD_c3(0x13, P17, yellow);
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('6');
                    oled_menuItem = 77;
                    break;
                case IRKeyOk:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('6');
                    oled_menuItem = 77;
                    break;
                case IRKeyRight:

                    userCommand = 'A';
                    if ((GBS::VDS_DIS_HB_ST::read() > 4) && (GBS::VDS_DIS_HB_SP::read() < (GBS::VDS_HSYNC_RST::read() - 4))) {
                    } else {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca3;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca3;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyLeft:

                    userCommand = 'B';
                    if ((GBS::VDS_DIS_HB_ST::read() < (GBS::VDS_HSYNC_RST::read() - 4)) && (GBS::VDS_DIS_HB_SP::read() > 4)) {
                    } else {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca3;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca3;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyUp:

                    userCommand = 'C';
                    if ((GBS::VDS_DIS_VB_ST::read() > 6) && (GBS::VDS_DIS_VB_SP::read() < (GBS::VDS_VSYNC_RST::read() - 4))) {
                    } else {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca3;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca3;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyDown:

                    userCommand = 'D';
                    if ((GBS::VDS_DIS_VB_ST::read() < (GBS::VDS_VSYNC_RST::read() - 4)) && (GBS::VDS_DIS_VB_SP::read() > 6)) {
                    } else {
                        for (int p = 0; p <= 400; p++) {
                            colour1 = 0x14;
                            number_stroca = stroca3;
                            Osd_Display(20, "limit");
                            __(0x0d, _25);
                        }
                    }
                    colour1 = blue_fill;
                    number_stroca = stroca3;
                    Osd_Display(20, "limit");
                    __(0x0d, _25);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 82) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 22, "ADC gain");
        // display.drawStringf(40,44,num,"ADC :%d");
        if (uopt->enableAutoGain) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('a');
        }

        OSD_menu_F('e');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;

                    break;
                case IRKeyUp:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('d');
                    oled_menuItem = 90;

                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('a');
                    oled_menuItem = 83;
                    break;
                case IRKeyRight:
                    userCommand = 'n';
                    break;
                case IRKeyLeft:
                    userCommand = 'o';
                    break;
                case IRKeyOk:
                    serialCommand = 'T';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 83) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 22, "Scanlines");
        // uopt->scanlineStrength
        if (uopt->wantScanlines) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
        }

        OSD_menu_F('e');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('a'); //
                    oled_menuItem = 82;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('a'); //
                    oled_menuItem = 84;
                    break;
                case IRKeyRight:
                    userCommand = 'K';
                    break;
                case IRKeyLeft:
                    userCommand = 'K';
                    break;
                case IRKeyOk:
                    userCommand = '7';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 84) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 22, "Line filter");
        if (uopt->wantVdsLineFilter) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('a');
        }

        OSD_menu_F('e');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('a'); //
                    oled_menuItem = 83;
                    break;
                case IRKeyDown:
                    oled_menuItem = 85;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('b');
                    break;
                case IRKeyOk:
                    userCommand = 'm';
                    break;
                // case IRKeyLeft:
                //   userCommand = 'm';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 85) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 22, "Sharpness");
        if (GBS::VDS_PK_LB_GAIN::read() != 0x16) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('b');
        }

        OSD_menu_F('f');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    oled_menuItem = 84;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('a');
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('b'); //
                    oled_menuItem = 86;
                    break;
                case IRKeyOk:
                    userCommand = 'W';
                    break;
                // case IRKeyLeft:
                //   userCommand = 'W';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 86) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 22, "Peaking");
        if (uopt->wantPeaking) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('f');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('b'); //
                    oled_menuItem = 85;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('b'); //
                    oled_menuItem = 87;
                    break;
                case IRKeyOk:
                    serialCommand = 'f';
                    break;
                // case IRKeyLeft:
                //   serialCommand = 'f';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 87) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 22, "Step response");
        if (uopt->wantStepResponse) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('b');
        }

        OSD_menu_F('f');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('b'); //
                    oled_menuItem = 86;
                    break;
                case IRKeyDown:
                    oled_menuItem = 93;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('c');
                    break;
                case IRKeyOk:
                    serialCommand = 'V';
                    break;
                // case IRKeyLeft:
                //   serialCommand = 'V';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 88) // D
    {
        // int8_t cur = GBS::VDS_Y_OFST::read();
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 28, "R ");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('d');
        }
        OSD_menu_F('g');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                // case IRKeyUp:
                //     oled_menuItem = 87;
                //     OSD_menu_F(OSD_CROSS_BOTTOM);
                //     OSD_menu_F('b');
                //     break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('d'); //
                    oled_menuItem = 89;
                    break;
                case IRKeyRight:
                    // Y_offset +
                    R_VAL = MIN(R_VAL + STEP, 255);
                    // GBS::VDS_Y_OFST::write(cur);

                    Color_Conversion();
                    PR_rgb();
                    break;
                case IRKeyLeft:
                    // userCommand = 'T';
                    // Y_offset -
                    R_VAL = MAX(0, R_VAL - STEP);
                    // GBS::VDS_Y_OFST::write(cur);
                    Color_Conversion();
                    PR_rgb();
                    break;
                case IRKeyOk:
                    // turnOffWiFi();
                    saveUserPrefs();
                    // serialCommand = 'K';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 89) {
        // int8_t cur = GBS::VDS_U_OFST::read();
        if (uopt->enableAutoGain == 1) {
            uopt->enableAutoGain = 0;
            saveUserPrefs();
        } else {
            uopt->enableAutoGain = 0;
        }
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 28, "G ");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }
        OSD_menu_F('g');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('d'); //
                    oled_menuItem = 88;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('d'); //
                    oled_menuItem = 90;
                    break;
                case IRKeyRight:
                    // userCommand = 'N';
                    G_VAL = MIN(G_VAL + STEP, 255);

                    Color_Conversion();
                    PR_rgb();
                    break;
                case IRKeyLeft:
                    // userCommand = 'M';
                    G_VAL = MAX(0, G_VAL - STEP);
                    Color_Conversion();
                    PR_rgb();
                    break;

                case IRKeyOk:
                    // turnOnWiFi();
                    saveUserPrefs();
                    break;

                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 90) {
        // int8_t cur = GBS::VDS_V_OFST::read();
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 28, "B");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('d');
        }
        // (signed char)GBS::VDS_Y_OFST::read() = GBS::VDS_Y_OFST::read();
        // (signed char)GBS::VDS_U_OFST::read() = GBS::VDS_U_OFST::read();
        // (signed char)GBS::VDS_V_OFST::read() = GBS::VDS_V_OFST::read();
        OSD_menu_F('g');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('d'); //
                    oled_menuItem = 89;
                    break;
                case IRKeyDown:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('a');
                    oled_menuItem = 82;
                    break;
                case IRKeyRight:
                    // userCommand = 'Q';
                    // cur = MIN(cur + STEP, 255);
                    // GBS::VDS_V_OFST::write(cur);

                    B_VAL = MIN(B_VAL + STEP, 255);

                    Color_Conversion();
                    PR_rgb();
                    break;
                case IRKeyLeft:
                    // userCommand = 'H';
                    // cur = MAX(0, cur - STEP);
                    // GBS::VDS_V_OFST::write(cur);
                    B_VAL = MAX(0, B_VAL - STEP);
                    Color_Conversion();
                    PR_rgb();
                    break;
                case IRKeyOk:
                    saveUserPrefs();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 91) {
        uint8_t cur = GBS::VDS_Y_GAIN::read();
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 28, "Y gain");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('c');
        }

        OSD_menu_F('h');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    oled_menuItem = 90;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('d');
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('c'); //
                    oled_menuItem = 92;
                    break;
                case IRKeyRight:
                    // userCommand = 'P';
                    cur = MIN(cur + STEP, 255);
                    GBS::VDS_Y_GAIN::write(cur);
                    break;
                case IRKeyLeft:
                    // userCommand = 'S';
                    cur = MAX(0, cur - STEP);
                    GBS::VDS_Y_GAIN::write(cur);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 92) {
        uint8_t curU;
        // = GBS::VDS_UCOS_GAIN::read();
        uint8_t curV = GBS::VDS_VCOS_GAIN::read();
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 28, "Color");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }
        OSD_menu_F('h');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('c'); //
                    oled_menuItem = 91;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('c'); //
                    oled_menuItem = 93;
                    break;
                case IRKeyRight:
                    userCommand = 'V'; //++
                    // curU = MIN(curU + STEP/8, 0X41);

                    // if (GBS::VDS_UCOS_GAIN::read() < 0x39)
                    // {
                    //     curV = MIN(curV + STEP/8, 0x46);
                    //     curU = curV - 13;
                    //     GBS::VDS_VCOS_GAIN::write(curV);
                    //     GBS::VDS_UCOS_GAIN::write(curU);
                    // }
                    break;
                case IRKeyLeft:
                    userCommand = 'R';
                    // curU = MAX(0X00, curU - STEP/8);
                    // curV = MAX(0x1c, curV - STEP/8);
                    // curU = curV - 13;
                    // GBS::VDS_VCOS_GAIN::write(curV);
                    // GBS::VDS_UCOS_GAIN::write(curU);
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 93) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Color");
        display.drawString(1, 28, "Default Color");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('c');
        }

        // OSD_menu_F('h');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ColorSettings;
                    break;
                case IRKeyUp:
                    oled_menuItem = 87;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('b');
                    break;
                // case IRKeyDown:
                //   oled_menuItem = 82;
                //   OSD_menu_F(OSD_CROSS_TOP);
                //   OSD_menu_F('a');
                // break;
                case IRKeyOk:
                    userCommand = 'U';
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 94) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Matched presets");
        if (uopt->matchPresetSource) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyUp || results.value == IRKeyDown) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);

            OSD_menu_F('i');
        }

        OSD_menu_F(j);

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('i');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Compatibility;
                    break;
                case IRKeyDown:
                    oled_menuItem = 103;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('k');
                    break;
                case IRKeyOk:
                    serialCommand = 'Z';
                    break;
                // case IRKeyLeft:
                //   serialCommand = 'Z';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 95) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Screen");
        display.drawString(1, 22, "Full height");
        if (uopt->wantFullHeight) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('o');
        }
        OSD_menu_F('p');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_ScreenSettings;
                    break;
                case IRKeyUp:
                    oled_menuItem = 77;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('6');
                    break;
                // case IRKeyDown:
                //     COl_L = 3;
                //     OSD_menu_F('i'); //
                //     oled_menuItem = 96;
                //     break;
                case IRKeyOk: {
                    uopt->wantFullHeight = !uopt->wantFullHeight;
                    saveUserPrefs();
                    uint8_t vidMode = getVideoMode();
                    if (uopt->presetPreference == 5) {
                        if (GBS::GBS_PRESET_ID::read() == 0x05 || GBS::GBS_PRESET_ID::read() == 0x15) {
                            applyPresets(vidMode);
                        }
                    }
                    UpDisplay();
                } break;
                // case IRKeyRight:
                //   userCommand = 'v';
                //   break;
                // case IRKeyLeft:
                //   userCommand = 'v';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 96) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Use upscaling");
        if (uopt->preferScalingRgbhv) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('i');
        }

        OSD_menu_F('j');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('i');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Compatibility;
                    break;
                case IRKeyDown:
                    oled_menuItem = 103;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('k');
                    break;
                case IRKeyOk:
                    // userCommand = 'x';
                    // uopt->preferScalingRgbhv = !uopt->preferScalingRgbhv;
                    // UpDisplay();
                    break;
                // case IRKeyLeft:
                //   userCommand = 'x';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    // else if (oled_menuItem == 97)
    // {

    //     display.clear();
    //     display.drawString(1, 0, "Menu-");
    //     display.drawString(1, 28, "Component/VGA");
    //     display.display();

    //     if (results.value == IRKeyUp)
    //     {
    //         OSD_c1(icon4, P0, yellow);
    //         OSD_c2(icon4, P0, blue_fill);
    //         OSD_c3(icon4, P0, blue_fill);
    //         OSD_menu_F('k');
    //     }

    //     OSD_menu_F('l');

    //     if (irrecv.decode(&results))
    //     {
    //         switch (results.value)
    //         {
    //         case IRKeyMenu:
    //             OSD_menu_F(OSD_CROSS_TOP);
    //             OSD_menu_F('2');
    //             oled_menuItem = OSD_SystemSettings;
    //             break;
    // case IRKeyUp:
    //     oled_menuItem = 96;
    //     OSD_menu_F(OSD_CROSS_BOTTOM);
    //     OSD_menu_F('i');
    //     break;
    // case IRKeyDown:
    //     COl_L = 2;
    //     OSD_menu_F('k'); //
    //     oled_menuItem = 98;
    //     break;
    //         case IRKeyRight:
    //             serialCommand = 'L';
    //             break;
    //         case IRKeyLeft:
    //             serialCommand = 'L';
    //             break;
    //         case IRKeyExit:
    //             oled_menuItem = OSD_Input;
    //             OSD_clear();
    //             OSD();
    //             break;
    //         }
    //         irrecv.resume();
    //     }
    // }

    else if (oled_menuItem == 98) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Force 50 / 60Hz");
        if (uopt->PalForce60) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('l');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('k'); //
                    oled_menuItem = 103;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('k'); //
                    oled_menuItem = 102;
                    break;
                case IRKeyOk:
                    if (uopt->PalForce60 == 0) {
                        uopt->PalForce60 = 1;
                    } else {
                        uopt->PalForce60 = 0;
                    }
                    saveUserPrefs();
                    UpDisplay();
                    break;
                // case IRKeyLeft:
                //   userCommand = '0';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 99) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Clock generator");
        if (uopt->disableExternalClockGenerator) {
            display.drawString(1, 44, "OFF");
        } else {
            display.drawString(1, 44, "ON");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);
            OSD_menu_F('m');
        }
        OSD_menu_F('n');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('m'); //
                    oled_menuItem = 101;
                    break;
                // case IRKeyDown:
                //   oled_menuItem = 100;
                //   OSD_menu_F(OSD_CROSS_TOP);
                //   OSD_menu_F('m');
                //   break;
                case IRKeyOk:
                    userCommand = 'X';
                    break;
                // case IRKeyLeft:
                //   userCommand = 'X';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 100) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "ADC calibration");
        if (uopt->enableCalibrationADC) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('m');
        }
        OSD_menu_F('n');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    oled_menuItem = 102;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('k');
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('m'); //
                    oled_menuItem = 101;
                    break;
                case IRKeyOk:
                    userCommand = 'w';
                    break;
                // case IRKeyLeft:
                //   userCommand = 'w';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 101) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Frame Time Lock");
        if (uopt->enableFrameTimeLock) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('n');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('m'); //
                    oled_menuItem = 100;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('m'); //
                    oled_menuItem = 99;
                    break;
                case IRKeyOk:
                    userCommand = '5';
                    break;
                // case IRKeyLeft:
                //   userCommand = '5';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 102) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Lock Method");
        if (uopt->frameTimeLockMethod) {
            display.drawString(1, 44, "1Vtotal only");
        } else {
            display.drawString(1, 44, "0Vtotal+VSST");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c3(icon4, P0, yellow);
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('k');
        }

        OSD_menu_F('l');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('k'); //
                    oled_menuItem = 98;
                    break;
                case IRKeyDown:
                    oled_menuItem = 100;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('m');
                    break;
                case IRKeyOk:
                    userCommand = 'i';
                    break;
                // case IRKeyLeft:
                //   userCommand = 'i';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 103) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Deinterlace");
        if (uopt->deintMode) {
            display.drawString(1, 44, "Bob");
        } else {
            display.drawString(1, 44, "Adaptive");
        }
        display.display();

        if (results.value == IRKeyUp || results.value == IRKeyDown) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('k');
        }

        OSD_menu_F('l');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    oled_menuItem = 94;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('i');
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('k'); //
                    oled_menuItem = 98;
                    break;
                case IRKeyOk:
                    if (uopt->deintMode != 1) {
                        uopt->deintMode = 1;
                        disableMotionAdaptDeinterlace();
                        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read()) {
                            disableScanlines();
                        }
                        saveUserPrefs();
                    } else if (uopt->deintMode != 0) {
                        uopt->deintMode = 0;
                        saveUserPrefs();
                    }
                    UpDisplay();
                    break;
                // case IRKeyRight:
                //   userCommand = 'q';
                //   break;
                // case IRKeyLeft:
                //   userCommand = 'r';
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    // else if (oled_menuItem == 104)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "MEM left / righ");
    //   display.display();

    //   if (results.value == IRKeyUp)
    //   {
    //     OSD_c1(icon4, P0, yellow);
    //     OSD_c2(icon4, P0, blue_fill);
    //     OSD_c3(icon4, P0, blue_fill);
    //     OSD_menu_F('q');
    //   }

    //   OSD_menu_F('r');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_MID);
    //       OSD_menu_F('2');
    //       oled_menuItem = 66;
    //       break;
    //     case IRKeyDown:
    //       COl_L = 2;
    //       OSD_menu_F('q'); //
    //       oled_menuItem = 105;
    //       break;
    //     case IRKeyRight:
    //       serialCommand = '+';
    //       break;
    //     case IRKeyLeft:
    //       serialCommand = '-';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    // else if (oled_menuItem == 105)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "HS left / right");
    //   display.display();

    //   if (results.value == IRKeyDown || results.value == IRKeyUp)
    //   {
    //     OSD_c1(icon4, P0, blue_fill);
    //     OSD_c3(icon4, P0, blue_fill);
    //     OSD_c2(icon4, P0, yellow);
    //   }

    //   OSD_menu_F('r');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_MID);
    //       OSD_menu_F('2');
    //       oled_menuItem = 66;
    //       break;
    //     case IRKeyUp:
    //       COl_L = 1;
    //       OSD_menu_F('q'); //
    //       oled_menuItem = 104;
    //       break;
    //     case IRKeyDown:
    //       COl_L = 3;
    //       OSD_menu_F('q'); //
    //       oled_menuItem = 106;
    //       break;
    //     case IRKeyRight:
    //       serialCommand = '0';
    //       break;
    //     case IRKeyLeft:
    //       serialCommand = '1';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    // else if (oled_menuItem == 106)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "HTotal - / +");
    //   display.display();

    //   if (results.value == IRKeyDown || results.value == IRKeyUp)
    //   {
    //     OSD_c3(icon4, P0, yellow);
    //     OSD_c1(icon4, P0, blue_fill);
    //     OSD_c2(icon4, P0, blue_fill);
    //     OSD_menu_F('q');
    //   }

    //   OSD_menu_F('r');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_MID);
    //       OSD_menu_F('2');
    //       oled_menuItem = 66;
    //       break;
    //     case IRKeyUp:
    //       COl_L = 2;
    //       OSD_menu_F('q'); //
    //       oled_menuItem = 105;
    //       break;
    //     case IRKeyDown:
    //       oled_menuItem = 107;
    //       OSD_menu_F(OSD_CROSS_TOP);
    //       OSD_menu_F('s');
    //       break;
    //     case IRKeyRight:
    //       serialCommand = 'a';
    //       break;
    //     case IRKeyLeft:
    //       serialCommand = 'A';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    // else if (oled_menuItem == 107)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "Debug view");
    //   display.display();

    //   if (results.value == IRKeyUp)
    //   {
    //     OSD_c1(icon4, P0, yellow);
    //     OSD_c2(icon4, P0, blue_fill);
    //     OSD_c3(icon4, P0, blue_fill);
    //     OSD_menu_F('s');
    //   }

    //   OSD_menu_F('t');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_MID);
    //       OSD_menu_F('2');
    //       oled_menuItem = 66;
    //       break;
    //     case IRKeyUp:
    //       oled_menuItem = 106;
    //       OSD_menu_F(OSD_CROSS_BOTTOM);
    //       OSD_menu_F('q');
    //       break;
    //     case IRKeyDown:
    //       COl_L = 2;
    //       OSD_menu_F('s'); //
    //       oled_menuItem = 108;
    //       break;
    //     case IRKeyRight:
    //       serialCommand = 'D';
    //       break;
    //     case IRKeyLeft:
    //       serialCommand = 'D';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    // else if (oled_menuItem == 108)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "ADC filter");
    //   display.display();

    //   if (results.value == IRKeyDown || results.value == IRKeyUp)
    //   {
    //     OSD_c1(icon4, P0, blue_fill);
    //     OSD_c3(icon4, P0, blue_fill);
    //     OSD_c2(icon4, P0, yellow);
    //   }

    //   OSD_menu_F('t');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_MID);
    //       OSD_menu_F('2');
    //       oled_menuItem = 66;
    //       break;
    //     case IRKeyUp:
    //       COl_L = 1;
    //       OSD_menu_F('s'); //
    //       oled_menuItem = 107;
    //       break;
    //     case IRKeyDown:
    //       COl_L = 3;
    //       OSD_menu_F('s');
    //       oled_menuItem = 153;
    //       break;
    //     case IRKeyRight:
    //       serialCommand = 'F';
    //       break;
    //     case IRKeyLeft:
    //       serialCommand = 'F';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    // else if (oled_menuItem == 153)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "Freeze capture");
    //   display.display();

    //   if (results.value == IRKeyDown || results.value == IRKeyUp)
    //   {
    //     OSD_c3(icon4, P0, yellow);
    //     OSD_c1(icon4, P0, blue_fill);
    //     OSD_c2(icon4, P0, blue_fill);
    //     OSD_menu_F('s'); //
    //   }

    //   OSD_menu_F('t');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_MID);
    //       OSD_menu_F('2');
    //       oled_menuItem = 66;
    //       break;
    //     case IRKeyUp:
    //       COl_L = 2;
    //       OSD_menu_F('s'); //
    //       oled_menuItem = 108;
    //       break;
    //     case IRKeyDown:
    //       oled_menuItem = 104;
    //       OSD_menu_F(OSD_CROSS_TOP);
    //       OSD_menu_F('q');
    //       break;
    //     case IRKeyRight:
    //       userCommand = 'F';
    //       break;
    //     case IRKeyLeft:
    //       userCommand = 'F';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    // else if (oled_menuItem == 109)
    // {

    //   display.clear();
    //   display.drawString(1, 0, "Menu-");
    //   display.drawString(1, 28, "Enable OTA");
    //   display.display();

    //   if (results.value == IRKeyUp)
    //   {
    //     OSD_c1(icon4, P0, yellow);
    //     OSD_c2(icon4, P0, blue_fill);
    //     OSD_c3(icon4, P0, blue_fill);
    //     OSD_menu_F('u');
    //   }

    //   OSD_menu_F('v');

    //   if (irrecv.decode(&results))
    //   {
    //     decode_flag = 1;
    //     switch (results.value)
    //     {
    //     case IRKeyMenu:
    //       OSD_menu_F(OSD_CROSS_BOTTOM);
    //       OSD_menu_F('2');
    //       oled_menuItem = OSD_ResetDefault;
    //       break;
    //     case IRKeyDown:
    //       COl_L = 2;
    //       OSD_menu_F('u'); //
    //       oled_menuItem = 110;
    //       break;
    //     case IRKeyRight:
    //       serialCommand = 'c';
    //       break;
    //     case IRKeyLeft:
    //       serialCommand = 'c';
    //       break;
    //     case IRKeyExit:
    //       oled_menuItem = OSD_Input;
    //       OSD_clear();
    //       OSD();
    //       break;
    //     }
    //     irrecv.resume();
    //   }
    // }

    else if (oled_menuItem == 110) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.drawString(1, 0, "Menu-");
        display.drawString(1, 28, "Restart");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('v');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_ResetDefault;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('u'); //
                    oled_menuItem = 109;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('u'); //
                    oled_menuItem = 111;
                    break;
                case IRKeyOk:
                    userCommand = 'a';
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    // else if (oled_menuItem == 111)
    // {

    //     display.clear();
    //     display.drawString(1, 0, "Menu-");
    //     display.drawString(1, 28, "Reset defaults");
    //     display.display();

    //     if (results.value == IRKeyDown  ||  results.value == IRKeyUp)
    //     {
    //         OSD_c3(icon4, P0, yellow);
    //         OSD_c1(icon4, P0, blue_fill);
    //         OSD_c2(icon4, P0, blue_fill);
    //     }

    //     OSD_menu_F('v');

    //     if (irrecv.decode(&results))
    //     {
    //         switch (results.value)
    //         {
    //         case IRKeyMenu:
    //             OSD_menu_F(OSD_CROSS_BOTTOM);
    //             OSD_menu_F('2');
    //             oled_menuItem = OSD_ResetDefault;
    //             break;
    //         case IRKeyUp:
    //             COl_L = 2;
    //             OSD_menu_F('u'); //
    //             oled_menuItem = 110;
    //             break;
    //         case IRKeyOk:
    //             userCommand = '1';
    //             break;
    //         case IRKeyExit:
    //             oled_menuItem = 0;
    //             OSD_clear();
    //             OSD();
    //             break;
    //         }
    //         irrecv.resume();
    //     }
    // }

    else if (oled_menuItem == OSD_Input) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->>>");
        display.drawString(1, 28, "Input");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            COl_L = 1;
            OSD_c1(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_menu_F('1');
        }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('1');
                    oled_menuItem = 62;

                    break;
                case IRKeyOk:
                    oled_menuItem = OSD_Input_RGBs;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('@');
                    COl_L = 1;
                    break;
                case IRKeyExit:
                    oled_menuItem = 0; // 154
                    OSD_clear();
                    OSD();
                    break;

                // case IRKeyInfo:
                //     OSD_clear();
                //     OSD();
                //     Tim_menuItem = millis();
                //     NEW_OLED_MENU = false;
                //     background_up(stroca1, _27, blue_fill);
                //     background_up(stroca2, _27, blue_fill);
                //     oled_menuItem = 152;
                //     break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_Input_RGBs) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Input");
        display.drawString(1, 28, "RGBs");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('@');
        }
        // OSD_menu_F('*');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyOk:
                    Info_sate = 0;
                    RGB_Com = 1;
                    InputRGBs_mode(RGB_Com);
                    rto->isInLowPowerMode = false; 
                    break;

                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1'); //
                    oled_menuItem = OSD_Input;
                    break;
                // case IRKeyUp:
                //     COl_L = 3;
                //     OSD_menu_F(OSD_CROSS_BOTTOM);
                //     OSD_menu_F('#');
                //     oled_menuItem = OSD_Input_AV;
                //     break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('@');
                    oled_menuItem = OSD_Input_RBsB;
                    break;

                    // case IRKeyLeft:
                    //     RGBs_Com = !RGBs_Com;
                    //     RGBs_CompatibilityChanged = 1;
                    //     break;
                    // case IRKeyRight:
                    //     RGBs_Com = !RGBs_Com;
                    //     RGBs_CompatibilityChanged = 1;
                    //     break;

                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_Input_RBsB) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Input");
        display.drawString(1, 28, "RGsB");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('@');
        }
        // OSD_menu_F('*');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyOk:
                    Info_sate = 0;
                    RGB_Com = 1;
                    InputRGsB_mode(RGB_Com);
                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1'); //
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('@');
                    oled_menuItem = OSD_Input_RGBs;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('@');
                    oled_menuItem = OSD_Input_VGA;
                    break;

                    // case IRKeyLeft:
                    //     RGsB_Com = !RGsB_Com;
                    //     RGsB_CompatibilityChanged = 1;
                    //     break;
                    // case IRKeyRight:
                    //     RGsB_Com = !RGsB_Com;
                    //     RGsB_CompatibilityChanged = 1;
                    //     break;

                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }
    else if (oled_menuItem == OSD_Input_VGA) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Input");
        display.drawString(1, 28, "VGA");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);

            OSD_menu_F('@');
        }
        // OSD_menu_F('*');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyOk:
                    Info_sate = 0;
                    RGB_Com = 0;
                    InputVGA_mode(RGB_Com);
                    // printf("\n\n RGB_Com %d \n\n", RGB_Com);
                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1'); //
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('@');
                    oled_menuItem = OSD_Input_RBsB;
                    break;
                case IRKeyDown:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('#');
                    oled_menuItem = OSD_Input_YPBPR;
                    break;

                    // case IRKeyLeft:
                    //     VGA_Com = !VGA_Com;
                    //     VGA_CompatibilityChanged = 1;
                    //     break;
                    // case IRKeyRight:
                    //     VGA_Com = !VGA_Com;
                    //     VGA_CompatibilityChanged = 1;
                    //     break;

                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }
    else if (oled_menuItem == OSD_Input_YPBPR) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Input");
        display.drawString(1, 28, "YPBPR");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {

            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('#');
        }
        OSD_menu_F('$');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {

                case IRKeyOk:
                    Info_sate = 0;
                    InputYUV();
                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1'); //
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyUp:
                    COl_L = 3;
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('@');
                    oled_menuItem = OSD_Input_VGA;
                    break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('#');
                    oled_menuItem = OSD_Input_SV;
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }
    else if (oled_menuItem == OSD_Input_SV) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Input");
        display.drawString(1, 22, "SV");
        // display.drawString(1, 45, "Format:");
        switch (SvModeOption) {
            case 0: {
                display.drawString(1, 44, "Auto");
            } break;
            case 1: {
                display.drawString(1, 44, "PAL");
            } break;
            case 2: {
                display.drawString(1, 44, "NTSC-M");
            } break;
            case 3: {
                display.drawString(1, 44, "PAL-60");
            } break;
            case 4: {
                display.drawString(1, 44, "NTSC443");
            } break;
            case 5: {
                display.drawString(1, 44, "NTSC-J");
            } break;
            case 6: {
                display.drawString(1, 44, "PAL-N w/ p");
            } break;
            case 7: {
                display.drawString(1, 44, "PAL-M w/o p");
            } break;
            case 8: {
                display.drawString(1, 44, "PAL-M");
            } break;
            case 9: {
                display.drawString(1, 44, "PAL Cmb -N");
            } break;
            case 10: {
                display.drawString(1, 44, "PAL Cmb -N w/ p");
            } break;
            case 11: {
                display.drawString(1, 44, "SECAM");
            } break;
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {

            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('#');
        }
        OSD_menu_F('$');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {

                case IRKeyOk:
                    Info_sate = 0;
                    InputSV_mode(SvModeOption + 1);
                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1'); //
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('#');
                    oled_menuItem = OSD_Input_YPBPR;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('#');
                    oled_menuItem = OSD_Input_AV;
                    break;
                case IRKeyLeft:
                    if (SvModeOption <= MODEOPTION_MIN)
                        SvModeOption = MODEOPTION_MAX;
                    SvModeOption--;
                    SvModeOptionChanged = 1;
                    break;
                case IRKeyRight:
                    SvModeOption++;
                    if (SvModeOption >= MODEOPTION_MAX)
                        SvModeOption = MODEOPTION_MIN;
                    SvModeOptionChanged = 1;
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }
    else if (oled_menuItem == OSD_Input_AV) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->Input");
        display.drawString(1, 22, "AV");
        // display.drawString(1, 45, "Format:");
        switch (AvModeOption) {
            case 0: {
                display.drawString(1, 44, "Auto");
            } break;
            case 1: {
                display.drawString(1, 44, "PAL");
            } break;
            case 2: {
                display.drawString(1, 44, "NTSC-M");
            } break;
            case 3: {
                display.drawString(1, 44, "PAL-60");
            } break;
            case 4: {
                display.drawString(1, 44, "NTSC443");
            } break;
            case 5: {
                display.drawString(1, 44, "NTSC-J");
            } break;
            case 6: {
                display.drawString(1, 44, "PAL-N w/ p");
            } break;
            case 7: {
                display.drawString(1, 44, "PAL-M w/o p");
            } break;
            case 8: {
                display.drawString(1, 44, "PAL-M");
            } break;
            case 9: {
                display.drawString(1, 44, "PAL Cmb -N");
            } break;
            case 10: {
                display.drawString(1, 44, "PAL Cmb -N w/ p");
            } break;
            case 11: {
                display.drawString(1, 44, "SECAM");
            } break;
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {

            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);

            OSD_menu_F('#');
        }
        OSD_menu_F('$');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyOk:
                    Info_sate = 0;
                    InputAV_mode(AvModeOption + 1);
                    // rto->isInLowPowerMode = false;  

                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1'); //
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('#');
                    oled_menuItem = OSD_Input_SV;
                    break;
                // case IRKeyDown:
                //     oled_menuItem = OSD_Input_RGBs;
                //     COl_L = 1;
                //     OSD_menu_F(OSD_CROSS_TOP);
                //     OSD_menu_F('@');
                //     break;
                case IRKeyLeft:
                    if (AvModeOption <= MODEOPTION_MIN)
                        AvModeOption = MODEOPTION_MAX;
                    AvModeOption--;
                    AvModeOptionChanged = 1;
                    break;
                case IRKeyRight:
                    AvModeOption++;
                    if (AvModeOption >= MODEOPTION_MAX)
                        AvModeOption = MODEOPTION_MIN;
                    AvModeOptionChanged = 1;
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 28, "Sv-Av InPutSet");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);

            OSD_menu_F('i');
        }

        OSD_menu_F('j');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyOk:
                    if (Info == InfoSV || Info == InfoAV) {
                        COl_L = 1;
                        OSD_menu_F(OSD_CROSS_TOP);
                        OSD_menu_F('^');
                        oled_menuItem = OSD_SystemSettings_SVAVInput_DoubleLine;
                    }
                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                // case IRKeyUp:
                //     COl_L = 2;
                //     OSD_menu_F(OSD_CROSS_MID);
                //     OSD_menu_F('o');
                //     oled_menuItem = 94;
                //     break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('i');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Compatibility;
                    break;

                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_DoubleLine) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "M>Sys>SvAv Set");
        display.drawString(1, 22, "DoubleLine");
        if (LineOption) {
            display.drawString(1, 44, "2X");
        } else {
            display.drawString(1, 44, "1X");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);

            OSD_menu_F('^');
        }
        OSD_menu_F('&');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    break;

                // case IRKeyUp:
                //   COl_L = 2;
                //   OSD_menu_F('^');
                //   oled_menuItem = OSD_SystemSettings_SVAVInput_Smooth;
                //   break;
                case IRKeyDown:
                    COl_L = 2;
                    OSD_menu_F('^');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Smooth;
                    break;

                case IRKeyOk:
                    LineOption = !LineOption;
                    SettingLineOptionChanged = 1;
                    break;
                // case IRKeyRight:
                //   LineOption = !LineOption;
                //   SettingLineOptionChanged = 1;
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_Smooth) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "M>Sys>SvAv Set");
        display.drawString(1, 22, "Smooth");
        if (SmoothOption) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);

            OSD_menu_F('^');
        }
        OSD_menu_F('&');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('^');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_DoubleLine;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('^');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Bright;
                    break;
                case IRKeyOk:
                    if (LineOption) {
                        SmoothOption = !SmoothOption;
                        SettingSmoothOptionChanged = 1;
                    }
                    break;
                // case IRKeyRight:
                //   SmoothOption = !SmoothOption;
                //   SettingSmoothOptionChanged = 1;
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_Bright) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "M>Sys>SvAv Set");
        display.drawString(1, 22, "Bright");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);

            OSD_menu_F('^');
        }
        OSD_menu_F('&');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    saveUserPrefs();
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('^');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Smooth;
                    break;
                case IRKeyDown:

                    // COl_L = 3;
                    // OSD_menu_F('^');
                    // oled_menuItem = OSD_SystemSettings_SVAVInput_contrast;

                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('z');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_contrast;
                    break;
                case IRKeyRight:
                    Bright = MIN(Bright + STEP, 254);
                    SetReg(0x0a, Bright - 128);
                    // printf("Bright: 0x%02x \n",Bright);
                    break;
                case IRKeyLeft:
                    Bright = MAX(Bright - STEP, 0);
                    SetReg(0x0a, Bright - 128);
                    // printf("Bright: 0x%02x \n",Bright);
                    break;
                case IRKeyOk:
                    saveUserPrefs();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_contrast) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "M>Sys>SvAv Set");
        display.drawString(1, 22, "Contrast");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);

            OSD_menu_F('z');
        }
        OSD_menu_F('A');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    break;
                case IRKeyUp:
                    OSD_menu_F(OSD_CROSS_BOTTOM);
                    OSD_menu_F('^');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_Bright;
                    // COl_L = 2;
                    // OSD_menu_F('^');
                    // oled_menuItem = OSD_SystemSettings_SVAVInput_Bright;
                    break;

                case IRKeyDown:
                    // OSD_menu_F(OSD_CROSS_TOP);
                    // OSD_menu_F('z');
                    // oled_menuItem = OSD_SystemSettings_SVAVInput_saturation;

                    COl_L = 2;
                    OSD_menu_F('z');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_saturation;

                    break;
                case IRKeyRight:
                    Contrast = MIN(Contrast + STEP, 254);
                    SetReg(0x08, Contrast);
                    // printf("contrast: 0x%02x \n",Contrast);
                    break;
                case IRKeyLeft:
                    Contrast = MAX(Contrast - STEP, 0);
                    SetReg(0x08, Contrast);
                    // printf("contrast: 0x%02x \n",Contrast);
                    break;
                case IRKeyOk:
                    saveUserPrefs();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_saturation) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "M>Sys>SvAv Set");
        display.drawString(1, 22, "Saturation");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);

            OSD_menu_F('z');
        }
        OSD_menu_F('A');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('z');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_contrast;

                    // OSD_menu_F(OSD_CROSS_BOTTOM);
                    // OSD_menu_F('^');
                    // oled_menuItem = OSD_SystemSettings_SVAVInput_contrast;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('z');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_default;    
                    break;
                case IRKeyRight:
                    Saturation = MIN(Saturation + STEP, 254);
                    SetReg(0xe3, Saturation);
                    // printf("saturation: 0x%02x \n",Saturation);
                    break;
                case IRKeyLeft:
                    Saturation = MAX(Saturation - STEP, 0);       
                    SetReg(0xe3, Saturation);
                    // printf("saturation: 0x%02x \n",Saturation);
                    break;
                case IRKeyOk:
                    saveUserPrefs();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }


    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_default) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "M>Sys>SvAv Set");
        display.drawString(1, 22, "Default");
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, yellow);

            OSD_menu_F('z');
        }
        OSD_menu_F('A');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    break;
                case IRKeyUp:
                    COl_L = 2;
                    OSD_menu_F('z');
                    oled_menuItem = OSD_SystemSettings_SVAVInput_saturation;
                    break;
                case IRKeyOk:
                    SetReg('D', 'E');
                    Bright = 128;
                    Contrast = 128;
                    Saturation = 128;
                    saveUserPrefs();
                    break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == OSD_SystemSettings_SVAVInput_Compatibility) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu->System");
        display.drawString(1, 22, "Compatibility");
        if (RGB_Com == 1) {
            display.drawString(1, 44, "ON");
        } else {
            display.drawString(1, 44, "OFF");
        }
        display.display();

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
            OSD_c3(icon4, P0, blue_fill);

            OSD_menu_F('i');
        }
        OSD_menu_F('j');
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('2');
                    oled_menuItem = OSD_SystemSettings;
                    break;
                case IRKeyUp:
                    COl_L = 1;
                    OSD_menu_F('i'); //
                    oled_menuItem = OSD_SystemSettings_SVAVInput;
                    break;
                case IRKeyDown:
                    COl_L = 3;
                    OSD_menu_F('i'); //
                    oled_menuItem = 94;
                    break;
                case IRKeyOk:
                    RGB_Com = !RGB_Com;
                    if (RGB_Com > 1)
                        RGB_Com = 0;
                    Send_Compatibility(RGB_Com);
                    if (GBS::ADC_INPUT_SEL::read())
                        UpDisplay();
                    break;
                // case IRKeyRight:
                //   RGB_Com = !RGB_Com;
                //   if (RGB_Com > 1)
                //     RGB_Com = 0;
                //   Send_Compatibility(RGB_Com);
                //   break;
                case IRKeyExit:
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('1');
                    oled_menuItem = OSD_Input;
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 112) {

        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu-");
        display.drawString(1, 28, "Profile");
        display.display();

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 113;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 138;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'A';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 113) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 114;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 112;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'B';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 114) // save
    {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 115;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 113;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'C';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 115) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 116;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 114;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'D';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 116) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 117;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 115;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'E';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 117) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 118;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 116;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'F';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 118) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 126;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 117;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'G';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 119) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 120;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 151;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'A';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 120) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 121;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 119;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'B';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 121) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 122;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 120;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'C';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 122) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 123;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 121;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'D';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 123) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 124;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 122;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'E';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 124) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 125;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 123;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'F';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 125) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 139;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 124;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'G';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 126) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 127;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 118;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'H';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 127) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 128;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 126;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'I';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 128) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 129;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 127;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'J';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 129) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 130;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 128;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'K';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 130) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 131;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 129;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'L';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 131) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 132;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 130;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'M';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 132) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 133;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 131;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'N';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 133) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 134;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 132;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'O';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 134) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 135;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 133;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'P';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 135) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 136;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 134;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'Q';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 136) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 137;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 135;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'R';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 137) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 138;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 136;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'S';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 138) {

        if (results.value == IRKeyUp) {
            OSD_c1(icon4, P0, yellow);
            OSD_c2(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_menu_F('w');
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyDown:
                    oled_menuItem = 119;
                    OSD_menu_F(OSD_CROSS_MID);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 112;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 137;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'T';
                    OSD_menu_F('y');
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 139) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 140;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 125;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'H';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 140) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 141;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 139;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'I';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 141) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 142;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 140;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'J';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 142) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 143;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 141;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'K';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 143) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 144;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 142;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'L';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 144) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 145;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 143;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'M';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 145) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 146;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 144;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'N';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 146) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 147;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 145;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'O';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 147) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 148;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 146;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'P';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 148) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 149;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 147;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'Q';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 149) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 150;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 148;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'R';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 150) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 151;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 149;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'S';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 151) {

        if (results.value == IRKeyDown || results.value == IRKeyUp) {
            OSD_c1(icon4, P0, blue_fill);
            OSD_c3(icon4, P0, blue_fill);
            OSD_c2(icon4, P0, yellow);
        }

        OSD_menu_F('x');

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;
                case IRKeyUp:
                    oled_menuItem = 112;
                    OSD_menu_F(OSD_CROSS_TOP);
                    OSD_menu_F('w');
                    break;
                case IRKeyRight:
                    oled_menuItem = 119;
                    break;
                case IRKeyLeft:
                    oled_menuItem = 150;
                    break;
                case IRKeyOk:
                    uopt->presetSlot = 'T';
                    uopt->presetPreference = OutputCustomized;
                    saveUserPrefs();
                    userCommand = '4';
                    for (int i = 0; i <= 800; i++) {
                        OSD_c2(O, P25, 0x14);
                        OSD_c2(K, P26, 0x14);
                    }
                    OSD_c2(O, P25, blue_fill);
                    OSD_c2(K, P26, blue_fill);
                    break;
                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 1) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(8, 15, "Volume - / + dB");
        display.display();

        adl = 50 - Volume;
        colour1 = yellowT;
        number_stroca = stroca1;
        Osd_Display(1, "Line input volume");
        colour1 = main0;
        sequence_number1 = _21;
        sequence_number2 = _20;
        sequence_number3 = 0x3D;
        // __(d, _23), __(B, _24);
        OSD_c1(o, _19, blue_fill);

        OSD_c1(o, _22, blue_fill);
        OSD_c1(o, _23, blue_fill);
        OSD_c1(o, _24, blue_fill);
        Typ(adl);
        // if (Volume <= 0)
        // {
        //   OSD_c1(o, _19, blue_fill);
        // }
        // else if (Volume > 0)
        // {
        //   __(0x3E, _19);
        // }

        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case kRecv2: // ++
                    Volume = MAX(Volume - 1, 0);
                    adl = 50 - Volume;
                    PT_2257(Volume + 12);
                    break;
                case kRecv3: // --
                    Volume = MIN(Volume + 1, 50);
                    adl = 50 - Volume;
                    PT_2257(Volume + 12);
                    break;
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = 62;
                    break;

                case IRKeyOk:
                    saveUserPrefs();
                    for (int z = 0; z <= 800; z++) {
                        OSD_c1(s, _19, 0x14);
                        OSD_c1(a, _20, 0x14);
                        OSD_c1(v, _21, 0x14);
                        OSD_c1(i, _22, 0x14);
                        OSD_c1(n, _23, 0x14);
                        OSD_c1(g, _24, 0x14);
                    }
                    break;

                case IRKeyExit:
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    else if (oled_menuItem == 152) {
        if (OLED_clear_flag)
            display.clear();
        OLED_clear_flag = ~0;
        display.setColor(OLEDDISPLAY_COLOR::WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(1, 0, "Menu-");
        display.drawString(1, 28, "Info");
        display.display();

        boolean vsyncActive = 0;
        boolean hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        rto->presetID = GBS::GBS_PRESET_ID::read();

        colour1 = yellow;
        number_stroca = stroca1;
        Osd_Display(0, "Info:");
        colour1 = main0;
        Osd_Display(26, "Hz");

        if (rto->presetID == 0x01 || rto->presetID == 0x11) {
            // OSD_writeString(6,1,"1280x960 ");

            OSD_c1(n1, P6, main0);
            OSD_c1(n2, P7, main0);
            OSD_c1(n8, P8, main0);
            OSD_c1(n0, P9, main0);
            OSD_c1(x, P10, main0);
            OSD_c1(n9, P11, main0);
            OSD_c1(n6, P12, main0);
            OSD_c1(n0, P13, main0);
            OSD_c1(n4, P14, blue_fill);
        } else if (rto->presetID == 0x02 || rto->presetID == 0x12) {
            // OSD_writeString(6,1,"1280x1024");
            OSD_c1(n1, P6, main0);
            OSD_c1(n2, P7, main0);
            OSD_c1(n8, P8, main0);
            OSD_c1(n0, P9, main0);
            OSD_c1(x, P10, main0);
            OSD_c1(n1, P11, main0);
            OSD_c1(n0, P12, main0);
            OSD_c1(n2, P13, main0);
            OSD_c1(n4, P14, main0);
        } else if (rto->presetID == 0x03 || rto->presetID == 0x13) {
            // OSD_writeString(6,1,"1280x720 ");
            OSD_c1(n1, P6, main0);
            OSD_c1(n2, P7, main0);
            OSD_c1(n8, P8, main0);
            OSD_c1(n0, P9, main0);
            OSD_c1(x, P10, main0);
            OSD_c1(n7, P11, main0);
            OSD_c1(n2, P12, main0);
            OSD_c1(n0, P13, main0);
            OSD_c1(n4, P14, blue_fill);
        } else if (rto->presetID == 0x05 || rto->presetID == 0x15) {
            // OSD_writeString(6,1,"1920x1080");
            OSD_c1(n1, P6, main0);
            OSD_c1(n9, P7, main0);
            OSD_c1(n2, P8, main0);
            OSD_c1(n0, P9, main0);
            OSD_c1(x, P10, main0);
            OSD_c1(n1, P11, main0);
            OSD_c1(n0, P12, main0);
            OSD_c1(n8, P13, main0);
            OSD_c1(n0, P14, main0);
        } else if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            // OSD_writeString(6,1,"Downscale");
            OSD_c1(D, P6, main0);
            OSD_c1(o, P7, main0);
            OSD_c1(w, P8, main0);
            OSD_c1(n, P9, main0);
            OSD_c1(s, P10, main0);
            OSD_c1(c, P11, main0);
            OSD_c1(a, P12, main0);
            OSD_c1(l, P13, main0);
            OSD_c1(e, P14, main0);
        } else if (rto->presetID == 0x04) {
            // OSD_writeString(6,1,"720x480  ");
            OSD_c1(n7, P6, main0);
            OSD_c1(n2, P7, main0);
            OSD_c1(n0, P8, main0);
            OSD_c1(x, P9, main0);
            OSD_c1(n4, P10, main0);
            OSD_c1(n8, P11, main0);
            OSD_c1(n0, P12, main0);
            OSD_c1(n8, P13, blue_fill);
            OSD_c1(n0, P14, blue_fill);
        } else if (rto->presetID == 0x14) {
            // OSD_writeString(6,1,"768x576  ");
            OSD_c1(n7, P6, main0);
            OSD_c1(n6, P7, main0);
            OSD_c1(n8, P8, main0);
            OSD_c1(x, P9, main0);
            OSD_c1(n5, P10, main0);
            OSD_c1(n7, P11, main0);
            OSD_c1(n6, P12, main0);
            OSD_c1(n8, P13, blue_fill);
            OSD_c1(n0, P14, blue_fill);
        } else {
            // OSD_writeString(6,1,"Bypass   ");
            OSD_c1(B, P6, main0);
            OSD_c1(y, P7, main0);
            OSD_c1(p, P8, main0);
            OSD_c1(a, P9, main0);
            OSD_c1(s, P10, main0);
            OSD_c1(s, P11, main0);
            OSD_c1(n6, P12, blue_fill);
            OSD_c1(n8, P13, blue_fill);
            OSD_c1(n0, P14, blue_fill);
        }

        if (Info == InfoRGBs) {
            // OSD_writeString(17,1," RGBs");
            OSD_c1(r, P17, blue_fill);
            OSD_c1(R, P18, main0);
            OSD_c1(G, P19, main0);
            OSD_c1(B, P20, main0);
            OSD_c1(s, P21, main0);
        } else if (Info == InfoRGsB) {
            // OSD_writeString(17,1," RGsB ");
            OSD_c1(r, P17, blue_fill);
            OSD_c1(R, P18, main0);
            OSD_c1(G, P19, main0);
            OSD_c1(s, P20, main0);
            OSD_c1(B, P21, main0);
            OSD_c1(B, P22, blue_fill);
        } else if (Info == InfoVGA) {
            // OSD_writeString(17,1," VGA  ");
            OSD_c1(r, P17, blue_fill);
            OSD_c1(V, P18, main0);
            OSD_c1(G, P19, main0);
            OSD_c1(A, P20, main0);
            OSD_c1(B, P21, blue_fill);
            OSD_c1(B, P22, blue_fill);
        } else if (Info == InfoYUV) {
            OSD_c1(r, P17, blue_fill);
            OSD_c1(Y, P18, main0);
            OSD_c1(P, P19, main0);
            OSD_c1(B, P20, main0);
            OSD_c1(P, P21, main0);
            OSD_c1(R, P22, main0);
        } else if (Info == InfoSV) {
            OSD_c1(r, P17, blue_fill);
            OSD_c1(Y, P18, blue_fill);
            OSD_c1(S, P19, main0);
            OSD_c1(V, P20, main0);
            OSD_c1(B, P21, blue_fill);
            OSD_c1(B, P22, blue_fill);
        } else if (Info == InfoAV) {
            OSD_c1(r, P17, blue_fill);
            OSD_c1(Y, P18, blue_fill);
            OSD_c1(A, P19, main0);
            OSD_c1(V, P20, main0);
            OSD_c1(B, P21, blue_fill);
            OSD_c1(B, P22, blue_fill);
        } else {
            OSD_c1(Y, P17, blue_fill);
            OSD_c1(P, P18, blue_fill);
            OSD_c1(b, P19, blue_fill);
            OSD_c1(P, P20, blue_fill);
            OSD_c1(r, P21, blue_fill);
            OSD_c1(B, P22, blue_fill);
        }

        adl = ofr;
        colour1 = main0;
        number_stroca = stroca1;
        sequence_number1 = _25;
        sequence_number2 = _24; //_24
        sequence_number3 = 0x3D;
        Typ(adl);

        clean_up(stroca2, 31, 0); // 17  31

        colour1 = yellow;
        number_stroca = stroca2;

        Osd_Display(0, "Current:");

        colour1 = main0;
        number_stroca = stroca2;

        Osd_Display(0xFF, " ");
        // if (( rto->sourceDisconnected || !rto->boardHasPower || Info_sate == 1) && rto->HdmiHoldDetection)
        if ((rto->sourceDisconnected || !rto->boardHasPower || Info_sate == 1)) {
            Osd_Display(0xFF, "No Input");
        } else if (((currentInput == 1) || (Info == InfoRGBs || Info == InfoRGsB || Info == InfoVGA))) {
            OSD_c2(B, P16, blue_fill);
            Osd_Display(0xFF, "RGB ");
            vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
            if (vsyncActive) {
                // Osd_Display(0xFF,"H");
                hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                if (hsyncActive) {
                    Osd_Display(0xFF, "HV   ");
                }
            } else if ((Info == InfoVGA) && ((!vsyncActive || !hsyncActive))) {
                OSD_c2(B, P11, blue_fill);
                Osd_Display(0x09, "No Input");
            }
        } else if ((rto->continousStableCounter > 35 || currentInput != 1) || (Info == InfoYUV || Info == InfoSV || Info == InfoAV)) {
            OSD_c2(B, P16, blue_fill);
            if (Info == InfoYUV)
                Osd_Display(0xFF, "  YPBPR  ");
            else if (Info == InfoSV)
                Osd_Display(0xFF, "   SV    ");
            else if (Info == InfoAV)
                Osd_Display(0xFF, "   AV    ");
        } else {
            Osd_Display(0xFF, "No Input");
        }
#if 1
        static uint8_t S0_Read_Resolution;
        static unsigned long Tim_info = 0;
        if ((millis() - Tim_info) >= 1000) {
            S0_Read_Resolution = GBS::REG_S0_00::read();

            // GBS::IF_LD_RAM_BYPS::write(1);
            // printf( "Scanning method: %d\n",GBS::STATUS_SYNC_PROC_VTOTAL::read() );   // 0x%02x  
            // printf( "Scanning method: %d\n",GBS::STATUS_VDS_VERT_COUNT::read() );
            // printf( "H_TOTAL: %d      ",(GBS::H_TOTAL_HIGH::read() << 8) + GBS::H_TOTAL_LOW::read() *4  );

            // printf( "V_TOTAL: %d\n",(GBS::V_TOTAL_HIGH::read() << 7) + GBS::V_TOTAL_LOW::read() );

            Tim_info = millis();
        }

        if (S0_Read_Resolution & 0x80) 
        {
            if (S0_Read_Resolution & 0x40) 
            {
                Osd_Display(0xFF, "   576p");
            } 
            else if (S0_Read_Resolution & 0x20) 
            {
                if( abs(GBS::STATUS_SYNC_PROC_VTOTAL::read() - 312) <= 10)
                  Osd_Display(0xFF, "   288p");
                else  
                  Osd_Display(0xFF, "   576i");
            } 
            else if (S0_Read_Resolution & 0x10) 
            {
                Osd_Display(0xFF, "   480p");
            } 
            else if (S0_Read_Resolution & 0x08)   
            {
                if( abs(GBS::STATUS_SYNC_PROC_VTOTAL::read() - 262) <= 10)
                  Osd_Display(0xFF, "   240p");
                else
                  Osd_Display(0xFF, "   480i");
            } 
            else 
                Osd_Display(0xFF, "   Err");
        } 
        else
            Osd_Display(0xFF, "   Err");




        // clean_up(stroca3, 17, 0); // 17  31
        // colour1 = yellow;
        // number_stroca = stroca3;
        // Osd_Display(0, "version:");
        // colour1 = main0;
        // number_stroca = stroca3;
        // Osd_Display(0xFF, "   ");
        // Osd_Display(0xFF, final_version);
        
#endif
        if (irrecv.decode(&results)) {
            decode_flag = 1;
            switch (results.value) {
                case IRKeyMenu:
                    COl_L = 1;
                    OSD_menu_F('0');
                    oled_menuItem = OSD_Input;
                    break;
                case IRKeyExit:
                    if (Info_sate) {
                        GBS::VDS_DIS_HB_ST::write(St);
                        GBS::VDS_DIS_HB_SP::write(Sp);
                        Info_sate = 0;
                    }
                    // Info_sate = 0;
                    oled_menuItem = 0;
                    OSD_clear();
                    OSD();
                    break;
            }
            irrecv.resume();
        }
    }

    if (
        (
            (results.value == IRKeyMenu) ||
            (results.value == IRKeySave) ||
            (results.value == IRKeyInfo) ||
            (results.value == IRKeyRight) ||
            (results.value == IRKeyLeft) ||
            (results.value == IRKeyUp) ||
            (results.value == IRKeyDown) ||
            (results.value == IRKeyOk) ||
            (results.value == IRKeyExit) ||
            (results.value == IRKeyMute) ||
            (results.value == kRecv2) ||
            (results.value == kRecv3)) &&
        (decode_flag == 1) &&
        (oled_menuItem != 0)) {
        // printf("Delay success \n");
        Tim_menuItem = millis();
        decode_flag = 0;
        OledUpdataTime = 1;
    }

    if ((millis() - Tim_Resolution) >= OSD_RESOLUTION_UP_TIME && oled_menuItem == OSD_Resolution_RetainedSettings) {
        Tim_menuItem = millis(); // updata osd close
        Tim_Resolution = millis();
        // printf(" status:%02x  \n",GBS::PAD_CONTROL_01_0x49::read());
        // printf(" %02x \n",GBS::STATUS_MISC::read());
        uint8_t T_tim = OSD_RESOLUTION_CLOSE_TIME / 1000 - ((Tim_Resolution - Tim_Resolution_Start) / 1000);
        // colour1 = A2_main0;
        number_stroca = stroca2;
        if (T_tim >= 10) {
            OSD_c2((T_tim / 10) + '0', P11, main0);
            OSD_c2((T_tim % 10) + '0', P12, main0);

            Osd_Display(14, " s ");
        } else {
            OSD_c2('0', P12, blue_fill);
            OSD_c2(T_tim + '0', P11, main0); //

            Osd_Display(13, " s ");
        }
        // printf(" TIM :%d \n",(uint8_t)((Tim_Resolution - Tim_Resolution_Start)/10));

        if ((Tim_Resolution - Tim_Resolution_Start) >= OSD_RESOLUTION_CLOSE_TIME) {

            // uopt->preferScalingRgbhv = true;
            if (tentative == Output960P) // 1280x960
                userCommand = 'f';
            else if (tentative == Output720P) // 1280x720
                userCommand = 'g';
            else if (tentative == Output1024P) // 1280x1024
                userCommand = 'p';
            else if (tentative == Output1080P) // 1920x1080
                userCommand = 's';
            else
                userCommand = 'g';
            // printf("%c \n",userCommand);

            OSD_menu_F(OSD_CROSS_MID);
            OSD_menu_F('4');
            oled_menuItem = OSD_Resolution_pass;
        }
    }

    if (oled_menuItem_last != oled_menuItem && oled_menuItem != 0) {
        Tim_menuItem = millis();
        OLED_clear_flag = 1;
        // printf("freq:%d \n", system_get_cpu_freq());
        // printf("oled_menuItem:%d \n", oled_menuItem);
        // printf("Info_sate:%d \n", Info_sate);
    }
    if ((millis() - Tim_menuItem) >= OSD_CLOSE_TIME && oled_menuItem != 0) {
        // 菜单关闭
        if (Info_sate) {
            GBS::VDS_DIS_HB_ST::write(St);
            GBS::VDS_DIS_HB_SP::write(Sp);
            Info_sate = 0;
        }
        oled_menuItem = 0;
        oled_menuItem_last = 0;
        OSD_clear();
        OSD();
    }
    oled_menuItem_last = oled_menuItem;
}

void OSD_menu_F(char incomingByte)
{
    const size_t tableSize = sizeof(menuTable) / sizeof(menuTable[0]);
    const unsigned char key = (unsigned char)incomingByte;

    // 线性查找实�?
    for (size_t i = 0; i < tableSize; i++) {
        if (menuTable[i].key == key) {
            menuTable[i].handler();
            return;
        }
    }


    // OSD_default_F();
}

void OSD_IR()
{
    if (irrecv.decode(&results)) {
        decode_flag = 1;
        if (results.value == IRKeyMenu) {
            Tim_menuItem = millis();
            if (rto->sourceDisconnected || !rto->boardHasPower || GBS::PAD_CKIN_ENZ::read()) // || !GBS::STATUS_MISC_VSYNC::read()
            {

                NEW_OLED_MENU = false;
                background_up(stroca1, _27, blue_fill);
                background_up(stroca2, _27, blue_fill);
                
                oled_menuItem = 152;

                // InputINFO();
                //////////new
                Info_sate = 1;
                St = GBS::VDS_DIS_HB_ST::read();
                Sp = GBS::VDS_DIS_HB_SP::read();

                /////////new
                // loadDefaultUserOptions();
                writeProgramArrayNew(ntsc_720x480, false); 
                doPostPresetLoadSteps();
                GBS::VDS_DIS_HB_ST::write(0x00);
                GBS::VDS_DIS_HB_SP::write(0xffff);
                freezeVideo();                  
                GBS::SP_CLAMP_MANUAL::write(1); 
                                                // GBS::VDS_U_OFST::write(GBS::VDS_U_OFST::read() + 100);
            } else {
                NEW_OLED_MENU = false;
                COl_L = 1;
                OSD_menu_F('0');
                oled_menuItem = 154;
                display.clear();
                // display.init();
                // display.flipScreenVertically();   
                // printf("Oled Init\n");
            }
        }

        // if (results.value == kRecv14)
        // {
        //     NEW_OLED_MENU = false;
        //     background_up(stroca1, _10, blue_fill);
        //     for (int i = 0; i <= 800; i++)
        //     {
        //         colour1 = yellowT;
        //         number_stroca = stroca1;
        //         __(R, _2), __(e, _3), __(s, _4), __(t, _5), __(a, _6), __(r, _7), __(t, _8);
        //         display.clear();
        //         display.setTextAlignment(TEXT_ALIGN_LEFT);
        //         display.setFont(ArialMT_Plain_16);
        //         display.drawString(8, 15, "Resetting GBS");
        //         display.drawString(8, 35, "Please Wait...");
        //         display.display();
        //     }
        //     webSocket.close();
        //     delay(60);
        //     ESP.reset();
        //     oled_menuItem = 0;
        //     PT_MUTE(0x79);
        // }

        if (results.value == IRKeySave) {
            Tim_menuItem = millis();
            NEW_OLED_MENU = false;
            OSD_menu_F(OSD_CROSS_TOP);
            OSD_menu_F('w');
            oled_menuItem = 112;
        }

        if (results.value == IRKeyInfo) {
            Tim_menuItem = millis();
            NEW_OLED_MENU = false;
            background_up(stroca1, _27, blue_fill);
            background_up(stroca2, _27, blue_fill);
            oled_menuItem = 152;
        }

        switch (results.value) {
            case IRKeyMute:
                Tim_menuItem = millis();
                if (MUTE_R == 0) {
                    PT_MUTE(0x79);
                    NEW_OLED_MENU = false;
                    background_up(stroca1, _9, blue_fill);
                    for (int i = 0; i <= 800; i++) {
                        colour1 = yellowT;
                        number_stroca = stroca1;
                        __(M, _1), __(U, _2), __(T, _3), __(E, _4);
                        colour1 = main0;
                        __(O, _6), __(N, _7);
                        display.clear();
                        display.flipScreenVertically();
                        display.setTextAlignment(TEXT_ALIGN_LEFT);
                        display.setFont(ArialMT_Plain_16);
                        display.drawString(8, 15, "MUTE ON");
                        display.display();
                    }
                    oled_menuItem = 0;
                    background_up(stroca1, _9, blue_fill);
                    OSD_Cut_0x01();
                    OSD();
                    MUTE_R = 1;
                } else if (MUTE_R == 1) {

                    PT_MUTE(0x78);
                    NEW_OLED_MENU = false;
                    background_up(stroca1, _9, blue_fill);
                    for (int i = 0; i <= 800; i++) {
                        colour1 = yellowT;
                        number_stroca = stroca1;
                        __(M, _1), __(U, _2), __(T, _3), __(E, _4);
                        colour1 = main0;
                        __(O, _6), __(F, _7), __(F, _8);
                        display.clear();
                        display.setTextAlignment(TEXT_ALIGN_LEFT);
                        display.setFont(ArialMT_Plain_16);
                        display.drawString(8, 15, "MUTE OFF");
                        display.display();
                    }
                    oled_menuItem = 0;
                    background_up(stroca1, _9, blue_fill);
                    OSD_Cut_0x01();
                    OSD();
                    MUTE_R = 0;
                }
                break;
            case kRecv2:
                Tim_menuItem = millis();
                NEW_OLED_MENU = false;
                background_up(stroca1, _25, blue_fill);
                oled_menuItem = 1;
                break;
            case kRecv3:
                Tim_menuItem = millis();
                NEW_OLED_MENU = false;
                background_up(stroca1, _25, blue_fill);
                oled_menuItem = 1;
                break;
        }

        irrecv.resume();
        delay(5);
    }
}


void handle_0(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    // OSD_c2(0x15, P9 , blue_fill);
    // OSD_c3(0x15, P18, blue_fill);

    OSD_background();
    colour1 = blue_fill;
    number_stroca = stroca2;
    __(icon4, _0);
    number_stroca = stroca3;
    __(icon4, _0);
    colour1 = yellow;
    number_stroca = stroca1;
    __(icon4, _0);

    colour1 = blue;

    // number_stroca = stroca1;
    // __(icon5, _27);
    number_stroca = stroca2;
    __('1', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "1 Input");
    OSD_c1(0x15, P8, yellowT);

    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "2 Output Resolution");

    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "3 Screen Settings");
};
void handle_1(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;

        OSD_c1(0x15, P8, yellowT);
        OSD_c2(0x15, P20, blue_fill);
        OSD_c3(0x15, P18, blue_fill);
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;

        OSD_c1(0x15, P8, blue_fill);
        OSD_c2(0x15, P20, yellowT);
        OSD_c3(0x15, P18, blue_fill);
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;

        OSD_c1(0x15, P8, blue_fill);
        OSD_c2(0x15, P20, blue_fill);
        OSD_c3(0x15, P18, yellowT);
    }

    colour1 = blue;
    // number_stroca = stroca1;
    // __(icon5, _27);
    number_stroca = stroca2;
    __('1', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "1 Input");

    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "2 Output Resolution"); //__(0X15, _9);

    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "3 Screen Settings"); //__(0X15, _18);
};
void handle_2(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;

        OSD_c1(0x15, P18, yellowT);
        OSD_c2(0x15, P19, blue_fill);
        // OSD_c3(0x15, P15 , blue_fill  );
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;

        OSD_c1(0x15, P18, blue_fill);
        OSD_c2(0x15, P19, yellowT);
        // OSD_c3(0x15, P15 , blue_fill  );
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;

        OSD_c1(0x15, P18, blue_fill);
        OSD_c2(0x15, P19, blue_fill);
        // OSD_c3(0x15, P15 , blue_fill  );
    }

    colour1 = blue;
    number_stroca = stroca1;
    __(icon5, _27);
    number_stroca = stroca2;
    __('2', _27);
    // number_stroca = stroca3;
    // __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "4 System Settings");
    colour1 = A2_main0;
    number_stroca = stroca2;
    // Osd_Display(1, "5 Color Settings");
    Osd_Display(1, "5 Picture Settings");
    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "6 Reset Settings");
};
void handle_3(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;
    // number_stroca = stroca1;
    // __(icon5, _27);
    number_stroca = stroca2;
    __('1', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "1920x1080");
    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "1280x1024");
    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "1280x960");
};
void handle_4(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    }
    // else if (COl_L == 2)
    // {
    //   A1_yellow = main0;
    //   // if(Info == InfoVGA)
    //     A2_main0 = yellowT;
    //   // else
    //   //   A2_main0 = 0x14;
    //   A3_main0 = main0;
    // }
    // else if (COl_L == 3)
    // {
    //     A1_yellow = main0;
    //     A2_main0 = main0;
    //     A3_main0 = yellowT;
    // }

    colour1 = blue;

    number_stroca = stroca1;
    __(icon5, _27);

    number_stroca = stroca2;
    __('2', _27);

    // number_stroca = stroca3;
    // __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "1280x720");

    // colour1 = A2_main0;
    // number_stroca = stroca2;
    // Osd_Display(1, "Pass through");
    // colour1 = A3_main0;
    // number_stroca = stroca3;
    // __(D, _1), __(o, _2), __(w, _3), __(n, _4), __(s, _5), __(c, _6), __(a, _7), __(l, _8), __(e, _9), __(n1, _11), __(n5, _12), __(K, _13), __(H, _14), __(z, _15);
};
void handle_5(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;
    number_stroca = stroca1;
    __(icon5, _27);
    number_stroca = stroca2;
    __(I, _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "Pass through");
};
void handle_6(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;

    // number_stroca = stroca1;
    // __(icon5, _27);
    number_stroca = stroca2;
    __('1', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "Move");
    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "Scale");
    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "Borders");
};
void handle_7(void)
{
    OSD_background();
    OSD_c1(icon4, P0, yellow);
    OSD_c2(icon4, P0, blue_fill);
    OSD_c3(icon4, P0, blue_fill);
    COl_L = 1;
};
void handle_8(void)
{
    OSD_background();
    OSD_c1(icon4, P0, blue_fill);
    OSD_c2(icon4, P0, yellow);
    OSD_c3(icon4, P0, blue_fill);
    COl_L = 2;
};
void handle_9(void)
{
    OSD_background();
    OSD_c1(icon4, P0, blue_fill);
    OSD_c2(icon4, P0, blue_fill);
    OSD_c3(icon4, P0, yellow);
    COl_L = 3;
};
void handle_a(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;

    number_stroca = stroca1;
    __(icon5, _27);
    number_stroca = stroca2;
    __('2', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "ADC gain");
    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "Scanlines");
    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "Line filter");
};
void handle_b(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;
    number_stroca = stroca1;
    __(icon5, _27);
    number_stroca = stroca2;
    __('3', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "Sharpness");
    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "Peaking");
    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "Step response");
};
void handle_c(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;
    number_stroca = stroca1;
    __(icon5, _27);
    number_stroca = stroca2;
    __('4', _27);
    // number_stroca = stroca3;
    // __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "Default Color");
    // Osd_Display(1, "Y gain");
    // colour1 = A2_main0;
    // number_stroca = stroca2;
    // Osd_Display(1, "Color");
    // colour1 = A3_main0;
    // number_stroca = stroca3;
};
void handle_d(void)
{
    if (COl_L == 1) {
        A1_yellow = yellowT;
        A2_main0 = main0;
        A3_main0 = main0;
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;
    }

    colour1 = blue;
    // number_stroca = stroca1;
    // __(icon5, _27);
    number_stroca = stroca2;
    __('1', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "R");
    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "G");
    colour1 = A3_main0;
    number_stroca = stroca3;
    Osd_Display(1, "B");
};
void handle_e(void)
{
    OSD_c1(0x3E, P9, main0);
    OSD_c1(0x3E, P10, main0);
    OSD_c1(0x3E, P11, main0);
    OSD_c1(0x3E, P12, main0);
    OSD_c1(0x3E, P13, main0);
    OSD_c1(0x3E, P14, main0);
    OSD_c1(0x3E, P15, main0);
    OSD_c1(0x3E, P16, main0);
    OSD_c1(0x3E, P17, main0);
    OSD_c1(0x3E, P18, main0);
    OSD_c1(0x3E, P22, main0);

    OSD_c2(0x3E, P10, main0);
    OSD_c2(0x3E, P11, main0);
    OSD_c2(0x3E, P12, main0);
    OSD_c2(0x3E, P13, main0);
    OSD_c2(0x3E, P14, main0);
    OSD_c2(0x3E, P15, main0);
    OSD_c2(0x3E, P16, main0);
    OSD_c2(0x3E, P17, main0);
    OSD_c2(0x3E, P18, main0);
    OSD_c2(0x3E, P19, main0);
    OSD_c2(0x3E, P22, main0);
    OSD_c3(0x3E, P12, main0);
    OSD_c3(0x3E, P13, main0);
    OSD_c3(0x3E, P14, main0);
    OSD_c3(0x3E, P15, main0);
    OSD_c3(0x3E, P16, main0);
    OSD_c3(0x3E, P17, main0);
    OSD_c3(0x3E, P18, main0);
    OSD_c3(0x3E, P19, main0);
    OSD_c3(0x3E, P20, main0);
    OSD_c3(0x3E, P21, main0);
    OSD_c3(0x3E, P22, main0);

    if (uopt->wantScanlines) {
        OSD_c2(O, P23, main0);
        OSD_c2(N, P24, main0);
        OSD_c2(F, P25, blue_fill);
    } else {
        OSD_c2(O, P23, main0);
        OSD_c2(F, P24, main0);
        OSD_c2(F, P25, main0);
    }

    if (uopt->wantVdsLineFilter) {
        OSD_c3(O, P23, main0);
        OSD_c3(N, P24, main0);
        OSD_c3(F, P25, blue_fill);
    } else {
        OSD_c3(O, P23, main0);
        OSD_c3(F, P24, main0);
        OSD_c3(F, P25, main0);
    }
    adl = GBS::ADC_RGCTRL::read();
    Type4(adl);

    adl = uopt->scanlineStrength;
    if (adl == 0x00) {
        OSD_c2(n0, P21, main0);
        OSD_c2(n0, P20, main0);
    } else if (adl == 0x10) {
        OSD_c2(n0, P21, main0);
        OSD_c2(n1, P20, main0);
    } else if (adl == 0x20) {
        OSD_c2(n0, P21, main0);
        OSD_c2(n2, P20, main0);
    } else if (adl == 0x30) {
        OSD_c2(n0, P21, main0);
        OSD_c2(n3, P20, main0);
    } else if (adl == 0x40) {
        OSD_c2(n0, P21, main0);
        OSD_c2(n4, P20, main0);
    } else if (adl == 0x50) {
        OSD_c2(n0, P21, main0);
        OSD_c2(n5, P20, main0);
    }

    if (uopt->enableAutoGain == 0) {
        OSD_c1(O, P23, main0);
        OSD_c1(F, P24, main0);
        OSD_c1(F, P25, main0);
    } else {
        OSD_c1(O, P23, main0);
        OSD_c1(N, P24, main0);
        OSD_c1(0x3E, P25, blue_fill);
    }
};
void handle_f(void)
{
    OSD_c1(0x3E, P10, main0);
    OSD_c1(0x3E, P11, main0);
    OSD_c1(0x3E, P12, main0);
    OSD_c1(0x3E, P13, main0);
    OSD_c1(0x3E, P14, main0);
    OSD_c1(0x3E, P15, main0);
    OSD_c1(0x3E, P16, main0);
    OSD_c1(0x3E, P17, main0);
    OSD_c1(0x3E, P18, main0);
    OSD_c1(0x3E, P19, main0);
    OSD_c1(0x3E, P20, main0);
    OSD_c1(0x3E, P21, main0);
    OSD_c1(0x3E, P22, main0);
    OSD_c2(0x3E, P8, main0);
    OSD_c2(0x3E, P9, main0);
    OSD_c2(0x3E, P10, main0);
    OSD_c2(0x3E, P11, main0);
    OSD_c2(0x3E, P12, main0);
    OSD_c2(0x3E, P13, main0);
    OSD_c2(0x3E, P14, main0);
    OSD_c2(0x3E, P15, main0);
    OSD_c2(0x3E, P16, main0);
    OSD_c2(0x3E, P17, main0);
    OSD_c2(0x3E, P18, main0);
    OSD_c2(0x3E, P19, main0);
    OSD_c2(0x3E, P20, main0);
    OSD_c2(0x3E, P21, main0);
    OSD_c2(0x3E, P22, main0);
    OSD_c3(0x3E, P14, main0);
    OSD_c3(0x3E, P15, main0);
    OSD_c3(0x3E, P16, main0);
    OSD_c3(0x3E, P17, main0);
    OSD_c3(0x3E, P18, main0);
    OSD_c3(0x3E, P19, main0);
    OSD_c3(0x3E, P20, main0);
    OSD_c3(0x3E, P21, main0);
    OSD_c3(0x3E, P22, main0);

    if (GBS::VDS_PK_LB_GAIN::read() == 0x16) {
        OSD_c1(O, P23, main0);
        OSD_c1(F, P24, main0);
        OSD_c1(F, P25, main0);
    } else {
        OSD_c1(O, P23, main0);
        OSD_c1(N, P24, main0);
        OSD_c1(F, P25, blue_fill);
    }

    if (uopt->wantPeaking == 0) {
        OSD_c2(O, P23, main0);
        OSD_c2(F, P24, main0);
        OSD_c2(F, P25, main0);
    } else {
        OSD_c2(O, P23, main0);
        OSD_c2(N, P24, main0);
        OSD_c2(F, P25, blue_fill);
    }

    if (uopt->wantStepResponse) {
        OSD_c3(O, P23, main0);
        OSD_c3(N, P24, main0);
        OSD_c3(F, P25, blue_fill);
    } else {
        OSD_c3(O, P23, main0);
        OSD_c3(F, P24, main0);
        OSD_c3(F, P25, main0);
    }
};
void handle_g(void)
{ // OSD_c1(0x3E, P2, main0);
    // OSD_c1(0x3E, P3, main0);
    // OSD_c1(0x3E, P4, main0);
    OSD_c1(0x3E, P5, main0);
    OSD_c1(0x3E, P6, main0);
    OSD_c1(0x3E, P7, main0);
    OSD_c1(0x3E, P8, main0);
    OSD_c1(0x3E, P9, main0);
    OSD_c1(0x3E, P10, main0);
    OSD_c1(0x3E, P11, main0);
    OSD_c1(0x3E, P12, main0);
    OSD_c1(0x3E, P13, main0);
    OSD_c1(0x3E, P14, main0);
    OSD_c1(0x3E, P15, main0);
    OSD_c1(0x3E, P16, main0);
    OSD_c1(0x3E, P17, main0);
    OSD_c1(0x3E, P18, main0);
    OSD_c1(0x3E, P19, main0);
    OSD_c1(0x3E, P20, main0);
    OSD_c1(0x3E, P21, main0);
    OSD_c1(0x3E, P22, main0);

    // OSD_c2(0x3E, P2, main0);
    // OSD_c2(0x3E, P3, main0);
    // OSD_c2(0x3E, P4, main0);
    OSD_c2(0x3E, P5, main0);
    OSD_c2(0x3E, P6, main0);
    OSD_c2(0x3E, P7, main0);
    OSD_c2(0x3E, P8, main0);
    OSD_c2(0x3E, P9, main0);
    OSD_c2(0x3E, P10, main0);
    OSD_c2(0x3E, P11, main0);
    OSD_c2(0x3E, P12, main0);
    OSD_c2(0x3E, P13, main0);
    OSD_c2(0x3E, P14, main0);
    OSD_c2(0x3E, P15, main0);
    OSD_c2(0x3E, P16, main0);
    OSD_c2(0x3E, P17, main0);
    OSD_c2(0x3E, P18, main0);
    OSD_c2(0x3E, P19, main0);
    OSD_c2(0x3E, P20, main0);
    OSD_c2(0x3E, P21, main0);
    OSD_c2(0x3E, P22, main0);

    // OSD_c3(0x3E, P2, main0);
    // OSD_c3(0x3E, P3, main0);
    // OSD_c3(0x3E, P4, main0);
    OSD_c3(0x3E, P5, main0);
    OSD_c3(0x3E, P6, main0);
    OSD_c3(0x3E, P7, main0);
    OSD_c3(0x3E, P8, main0);
    OSD_c3(0x3E, P9, main0);
    OSD_c3(0x3E, P10, main0);
    OSD_c3(0x3E, P11, main0);
    OSD_c3(0x3E, P12, main0);
    OSD_c3(0x3E, P13, main0);
    OSD_c3(0x3E, P14, main0);
    OSD_c3(0x3E, P15, main0);
    OSD_c3(0x3E, P16, main0);
    OSD_c3(0x3E, P17, main0);
    OSD_c3(0x3E, P18, main0);
    OSD_c3(0x3E, P19, main0);
    OSD_c3(0x3E, P20, main0);
    OSD_c3(0x3E, P21, main0);
    OSD_c3(0x3E, P22, main0);

    // adl = (128 + GBS::VDS_Y_OFST::read());  //R
    // adl = ((signed char)GBS::VDS_Y_OFST::read() + 1.402 * ((signed char)GBS::VDS_V_OFST::read()-128));  //R
    // adl = ((signed char)GBS::VDS_Y_OFST::read() + 1.5 * ((signed char)GBS::VDS_V_OFST::read()));  //R
    // adl= (signed char)GBS::VDS_Y_OFST::read()+1.402*((signed char)GBS::VDS_V_OFST::read()-128);
    // adl = R_VAL;
    colour1 = main0;
    number_stroca = stroca1;
    sequence_number1 = _25;
    sequence_number2 = _24;
    sequence_number3 = _23;
    // Typ(((signed char)((signed char)GBS::VDS_Y_OFST::read()) +(float)( 1.402     * (signed char)((signed char)GBS::VDS_V_OFST::read()) )) + 128);
    Typ(R_VAL);
    // adl = (128 + GBS::VDS_U_OFST::read());  //G
    // adl = ((signed char)GBS::VDS_Y_OFST::read() - 0.88 * ((signed char)GBS::VDS_U_OFST::read()) - 0.764 * ((signed char)GBS::VDS_V_OFST::read()));  //G
    // adl = (signed char)GBS::VDS_Y_OFST::read()-0.344136*((signed char)GBS::VDS_U_OFST::read()-128)-0.714136*((signed char)GBS::VDS_V_OFST::read()-128);
    // adl = G_VAL;
    colour1 = main0;
    number_stroca = stroca2;
    sequence_number1 = _25;
    sequence_number2 = _24;
    sequence_number3 = _23;
    // Typ(((signed char)((signed char)GBS::VDS_Y_OFST::read()) -(float)( 0.344136  * (signed char)((signed char)GBS::VDS_U_OFST::read()) )- 0.714136 * (signed char)((signed char)GBS::VDS_V_OFST::read()) ) + 128);
    Typ(G_VAL);

    // adl = (128 + GBS::VDS_V_OFST::read());  //B
    // adl = ((signed char)GBS::VDS_Y_OFST::read() + 2 * ((signed char)GBS::VDS_U_OFST::read()));  //B
    // adl = (signed char)GBS::VDS_Y_OFST::read()+1.772*((signed char)GBS::VDS_U_OFST::read()-128);
    // adl = B_VAL;
    colour1 = main0;
    number_stroca = stroca3;
    sequence_number1 = _25;
    sequence_number2 = _24;
    sequence_number3 = _23;
    // Typ(((signed char)((signed char)GBS::VDS_Y_OFST::read()) +(float)( 1.772     * (signed char)((signed char)GBS::VDS_U_OFST::read()) )) + 128);
    Typ(B_VAL);
};
void handle_h(void)
{
    OSD_c1(0x3E, P7, main0);
    OSD_c1(0x3E, P8, main0);
    OSD_c1(0x3E, P9, main0);
    OSD_c1(0x3E, P10, main0);
    OSD_c1(0x3E, P11, main0);
    OSD_c1(0x3E, P12, main0);
    OSD_c1(0x3E, P13, main0);
    OSD_c1(0x3E, P14, main0);
    OSD_c1(0x3E, P15, main0);
    OSD_c1(0x3E, P16, main0);
    OSD_c1(0x3E, P17, main0);
    OSD_c1(0x3E, P18, main0);
    OSD_c1(0x3E, P19, main0);
    OSD_c1(0x3E, P20, main0);
    OSD_c1(0x3E, P21, main0);
    OSD_c1(0x3E, P22, main0);
    OSD_c2(0x3E, P6, main0);
    OSD_c2(0x3E, P7, main0);
    OSD_c2(0x3E, P8, main0);
    OSD_c2(0x3E, P9, main0);
    OSD_c2(0x3E, P10, main0);
    OSD_c2(0x3E, P11, main0);
    OSD_c2(0x3E, P12, main0);
    OSD_c2(0x3E, P13, main0);
    OSD_c2(0x3E, P14, main0);
    OSD_c2(0x3E, P15, main0);
    OSD_c2(0x3E, P16, main0);
    OSD_c2(0x3E, P17, main0);
    OSD_c2(0x3E, P18, main0);
    OSD_c2(0x3E, P19, main0);
    OSD_c2(0x3E, P20, main0);
    OSD_c2(0x3E, P21, main0);
    OSD_c2(0x3E, P22, main0);

    adl = GBS::VDS_Y_GAIN::read();
    colour1 = main0;
    number_stroca = stroca1;
    sequence_number1 = _25;
    sequence_number2 = _24;
    sequence_number3 = _23;
    Typ(adl);
    adl = GBS::VDS_VCOS_GAIN::read();
    colour1 = main0;
    number_stroca = stroca2;
    sequence_number1 = _25;
    sequence_number2 = _24;
    sequence_number3 = _23;
    Typ(adl);
};
void handle_i(void)
{
    if (COl_L == 1) {
        if ((Info != InfoSV) && (Info != InfoAV)) {
            A1_yellow = 0X14;
        } else {
            A1_yellow = yellowT;
        }

        A2_main0 = main0;
        A3_main0 = main0;

        OSD_c1(0x15, P21, yellowT);
    } else if (COl_L == 2) {
        A1_yellow = main0;
        A2_main0 = yellowT;
        A3_main0 = main0;

        OSD_c1(0x15, P21, blue_fill);
    } else if (COl_L == 3) {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = yellowT;

        OSD_c1(0x15, P21, blue_fill);
    }

    colour1 = blue;
    // number_stroca = stroca1;
    // __(icon5, _27);
    number_stroca = stroca2;
    __('1', _27);
    number_stroca = stroca3;
    __(icon6, _27);

    colour1 = A1_yellow;
    number_stroca = stroca1;
    Osd_Display(1, "SV/AV Input Settings");

    colour1 = A2_main0;
    number_stroca = stroca2;
    Osd_Display(1, "Compatibility Mode");

    colour1 = A3_main0;
    number_stroca = stroca3;
    // Osd_Display(1, "Lowres:use upscaling");
    Osd_Display(1, "Matched presets");
};
void handle_j(void)
{
    // OSD_c2(0x3E, P16, main0);
    // OSD_c2(0x3E, P17, main0);
    // OSD_c2(0x3E, P18, main0);
    OSD_c2(0x3E, P19, main0);
    OSD_c2(0x3E, P20, main0);
    OSD_c2(0x3E, P21, main0);
    OSD_c2(0x3E, P22, main0);
    if (RGB_Com == 1) {
        OSD_c2(O, P23, main0);
        OSD_c2(N, P24, main0);
        OSD_c2(F, P25, blue_fill);
    } else {
        OSD_c2(O, P23, main0);
        OSD_c2(F, P24, main0);
        OSD_c2(F, P25, main0);
    }
    OSD_c3(0x3E, P16, main0);
    OSD_c3(0x3E, P17, main0);
    OSD_c3(0x3E, P18, main0);
    OSD_c3(0x3E, P19, main0);
    OSD_c3(0x3E, P20, main0);
    OSD_c3(0x3E, P21, main0);
    OSD_c3(0x3E, P22, main0);
    if (uopt->matchPresetSource) {
        OSD_c3(O, P23, main0);
        OSD_c3(N, P24, main0);     // ON
        OSD_c3(F, P25, blue_fill); // ON
    } else {
        OSD_c3(O, P23, main0);
        OSD_c3(F, P24, main0);
        OSD_c3(F, P25, main0); // OFF
    }
    /*
    upscaling
        // OSD_c3(0x3E, P21, main0);
        // OSD_c3(0x3E, P22, main0);
        // if (uopt->preferScalingRgbhv)
        // {
        //   OSD_c3(O, P23, main0);
        //   OSD_c3(N, P24, main0);
        //   OSD_c3(F, P25, blue_fill);
        // }
        // else
        // {
        //   OSD_c3(O, P23, main0);
        //   OSD_c3(F, P24, main0);
        //   OSD_c3(F, P25, main0);
        // }
    */};
    void handle_k(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __('2', _27);
        number_stroca = stroca3;
        __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Deinterlace");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "Force:50Hz to 60Hz");
        colour1 = A3_main0;
        number_stroca = stroca3;
        // Osd_Display(1, "Clock generator");
        Osd_Display(1, "Lock method");
    };
    void handle_l(void)
    {
        OSD_c1(0x3E, P12, main0);
        OSD_c1(0x3E, P13, main0);
        OSD_c1(0x3E, P14, main0);
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        if (uopt->deintMode == 0) {
            OSD_c1(A, P18, main0);
            OSD_c1(d, P19, main0);
            OSD_c1(a, P20, main0);
            OSD_c1(p, P21, main0);
            OSD_c1(t, P22, main0);
            OSD_c1(i, P23, main0);
            OSD_c1(v, P24, main0);
            OSD_c1(e, P25, main0);
        } else {
            OSD_c1(0x3E, P18, main0);
            OSD_c1(0x3E, P19, main0);
            OSD_c1(0x3E, P20, main0);
            OSD_c1(0x3E, P21, main0);
            OSD_c1(0x3E, P22, main0);
            OSD_c1(B, P23, main0);
            OSD_c1(o, P24, main0);
            OSD_c1(b, P25, main0);
        }

        // OSD_c1(0x3E, P21, main0);
        // OSD_c1(0x3E, P22, main0);
        OSD_c2(0x3E, P19, main0);
        OSD_c2(0x3E, P20, main0);
        OSD_c2(0x3E, P21, main0);
        OSD_c2(0x3E, P22, main0);
        // OSD_c3(0x3E, P16, main0);
        // OSD_c3(0x3E, P17, main0);
        // OSD_c3(0x3E, P18, main0);
        // OSD_c3(0x3E, P19, main0);
        // OSD_c3(0x3E, P20, main0);
        // OSD_c3(0x3E, P21, main0);
        // OSD_c3(0x3E, P22, main0);
        OSD_c3(0x3E, P12, main0);
        OSD_c3(0x3E, P13, main0);

        // if (uopt->wantOutputComponent)
        // {
        //     OSD_c1(O, P23, main0);
        //     OSD_c1(N, P24, main0);
        //     OSD_c1(F, P25, blue_fill);
        // }
        // else
        // {
        //     OSD_c1(O, P23, main0);
        //     OSD_c1(F, P24, main0);
        //     OSD_c1(F, P25, main0);
        // }

        if (uopt->PalForce60) {
            OSD_c2(O, P23, main0);
            OSD_c2(N, P24, main0);
            OSD_c2(F, P25, blue_fill);
        } else {
            OSD_c2(O, P23, main0);
            OSD_c2(F, P24, main0);
            OSD_c2(F, P25, main0);
        }

        // if (uopt->disableExternalClockGenerator)
        // {
        //   OSD_c3(O, P23, main0);
        //   OSD_c3(F, P24, main0);
        //   OSD_c3(F, P25, main0);
        // }
        // else
        // {
        //   OSD_c3(O, P23, main0);
        //   OSD_c3(N, P24, main0);
        //   OSD_c3(F, P25, blue_fill);
        // }

        if (uopt->frameTimeLockMethod == 0) {
            OSD_c3(n0, P14, main0);
            OSD_c3(V, P15, main0);
            OSD_c3(t, P16, main0);
            OSD_c3(o, P17, main0);
            OSD_c3(t, P18, main0);
            OSD_c3(a, P19, main0);
            OSD_c3(l, P20, main0);
            OSD_c3(0x3C, P21, main0);
            OSD_c3(V, P22, main0);
            OSD_c3(S, P23, main0);
            OSD_c3(S, P24, main0);
            OSD_c3(T, P25, main0);
        } else {
            OSD_c3(n1, P14, main0);
            OSD_c3(V, P15, main0);
            OSD_c3(t, P16, main0);
            OSD_c3(o, P17, main0);
            OSD_c3(t, P18, main0);
            OSD_c3(a, P19, main0);
            OSD_c3(l, P20, main0);
            OSD_c3(o, P22, main0);
            OSD_c3(n, P23, main0);
            OSD_c3(l, P24, main0);
            OSD_c3(y, P25, main0);
            OSD_c3(F, P21, blue_fill);
        }
    };
    void handle_m(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __('3', _27);
        // number_stroca = stroca3;
        // __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "ADC calibration");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "Frame Time lock");
        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "EnableFrameTimeLock");
    };
    void handle_n(void)
    {
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);
        OSD_c1(0x3E, P19, main0);
        OSD_c1(0x3E, P20, main0);
        OSD_c1(0x3E, P21, main0);
        OSD_c1(0x3E, P22, main0);
        OSD_c2(0x3E, P16, main0);
        OSD_c2(0x3E, P17, main0);
        OSD_c2(0x3E, P18, main0);
        OSD_c2(0x3E, P19, main0);
        OSD_c2(0x3E, P20, main0);
        OSD_c2(0x3E, P21, main0);
        OSD_c2(0x3E, P22, main0);
        // OSD_c3(0x3E, P16, main0);
        // OSD_c3(0x3E, P17, main0);
        // OSD_c3(0x3E, P18, main0);
        // OSD_c3(0x3E, P19, main0);
        OSD_c3(0x3E, P20, main0);
        OSD_c3(0x3E, P21, main0);
        OSD_c3(0x3E, P22, main0);

        if (uopt->enableCalibrationADC) {
            OSD_c1(O, P23, main0);
            OSD_c1(N, P24, main0);
            OSD_c1(F, P25, blue_fill);
        } else {
            OSD_c1(O, P23, main0);
            OSD_c1(F, P24, main0);
            OSD_c1(F, P25, main0);
        }

        if (uopt->enableFrameTimeLock) {
            OSD_c2(O, P23, main0);
            OSD_c2(N, P24, main0);
            OSD_c2(F, P25, blue_fill);
        } else {
            OSD_c2(O, P23, main0);
            OSD_c2(F, P24, main0);
            OSD_c2(F, P25, main0);
        }

        if (uopt->disableExternalClockGenerator) {
            OSD_c3(O, P23, main0);
            OSD_c3(F, P24, main0);
            OSD_c3(F, P25, main0);
        } else {
            OSD_c3(O, P23, main0);
            OSD_c3(N, P24, main0);
            OSD_c3(F, P25, blue_fill);
        }
    };
    void handle_o(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __('2', _27);
        // number_stroca = stroca3;
        // __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Full height");

        // colour1 = A2_main0;
        // number_stroca = stroca2;
        // __(M, _1), __(a, _2), __(t, _3), __(c, _4), __(h, _5), __(e, _6), __(d, _7), __(p, _9), __(r, _10), __(e, _11), __(s, _12), __(e, _13), __(t, _14), __(s, _15);
    };
    void handle_p(void)
    {
        OSD_c1(0x3E, P12, main0);
        OSD_c1(0x3E, P13, main0);
        OSD_c1(0x3E, P14, main0);
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);
        OSD_c1(0x3E, P19, main0);
        OSD_c1(0x3E, P20, main0);
        OSD_c1(0x3E, P21, main0);
        OSD_c1(0x3E, P22, main0);
        // OSD_c3(0x3E, P22, main0);

        if (uopt->wantFullHeight) {
            OSD_c1(O, P23, main0);
            OSD_c1(N, P24, main0);
            OSD_c1(F, P25, blue_fill);
        } else {
            OSD_c1(O, P23, main0);
            OSD_c1(F, P24, main0);
            OSD_c1(F, P25, main0);
        }
    };
    void handle_q(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __(I, _27);
        number_stroca = stroca3;
        __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "MEM left/right");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "HS left/right");
        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "HTotal");
    };
    void handle_r(void)
    {
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);
        OSD_c1(0x3E, P19, main0);
        OSD_c1(0x3E, P20, main0);
        OSD_c1(0x3E, P21, main0);
        OSD_c1(0x3E, P22, main0);
        OSD_c1(0x03, P23, yellow);
        OSD_c1(0x13, P24, yellow);
        OSD_c2(0x3E, P14, main0);
        OSD_c2(0x3E, P15, main0);
        OSD_c2(0x3E, P16, main0);
        OSD_c2(0x3E, P17, main0);
        OSD_c2(0x3E, P18, main0);
        OSD_c2(0x3E, P19, main0);
        OSD_c2(0x3E, P20, main0);
        OSD_c2(0x3E, P21, main0);
        OSD_c2(0x3E, P22, main0);
        OSD_c2(0x03, P23, yellow);
        OSD_c2(0x13, P24, yellow);
        OSD_c3(0x3E, P7, main0);
        OSD_c3(0x3E, P8, main0);
        OSD_c3(0x3E, P9, main0);
        OSD_c3(0x3E, P10, main0);
        OSD_c3(0x3E, P11, main0);
        OSD_c3(0x3E, P12, main0);
        OSD_c3(0x3E, P13, main0);
        OSD_c3(0x3E, P14, main0);
        OSD_c3(0x3E, P15, main0);
        OSD_c3(0x3E, P16, main0);
        OSD_c3(0x3E, P17, main0);
        OSD_c3(0x3E, P18, main0);
        OSD_c3(0x3E, P19, main0);
        OSD_c3(0x3E, P20, main0);
        OSD_c3(0x3E, P21, main0);
        OSD_c3(0x3E, P22, main0);
        adl = GBS::VDS_HSYNC_RST::read();
        colour1 = main0;
        number_stroca = stroca3;
        sequence_number1 = _25;
        sequence_number2 = _24;
        sequence_number3 = _23;
        Typ(adl);
    };
    void handle_s(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __(I, _27);
        number_stroca = stroca3;
        __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Debug view");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "ADC filter");
        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "Freeze capture");
    };
    void handle_t(void)
    {
        OSD_c1(0x3E, P11, main0);
        OSD_c1(0x3E, P12, main0);
        OSD_c1(0x3E, P13, main0);
        OSD_c1(0x3E, P14, main0);
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);
        OSD_c1(0x3E, P19, main0);
        OSD_c1(0x3E, P20, main0);
        OSD_c1(0x3E, P21, main0);
        OSD_c1(0x3E, P22, main0);
        OSD_c2(0x3E, P11, main0);
        OSD_c2(0x3E, P12, main0);
        OSD_c2(0x3E, P13, main0);
        OSD_c2(0x3E, P14, main0);
        OSD_c2(0x3E, P15, main0);
        OSD_c2(0x3E, P16, main0);
        OSD_c2(0x3E, P17, main0);
        OSD_c2(0x3E, P18, main0);
        OSD_c2(0x3E, P19, main0);
        OSD_c2(0x3E, P20, main0);
        OSD_c2(0x3E, P21, main0);
        OSD_c2(0x3E, P22, main0);
        colour1 = main0;
        number_stroca = stroca3;
        __(0x3E, _15), __(0x3E, _16), __(0x3E, _17), __(0x3E, _18), __(0x3E, _19), __(0x3E, _20), __(0x3E, _21), __(0x3E, _22);

        if (GBS::ADC_UNUSED_62::read() == 0x00) {
            OSD_c1(O, P23, main0);
            OSD_c1(F, P24, main0);
            OSD_c1(F, P25, main0);
        } else {
            OSD_c1(O, P23, main0);
            OSD_c1(N, P24, main0);
            OSD_c1(F, P25, blue_fill);
        }

        if (GBS::ADC_FLTR::read() > 0) {
            OSD_c2(O, P23, main0);
            OSD_c2(N, P24, main0);
            OSD_c2(F, P25, blue_fill);
        } else {
            OSD_c2(O, P23, main0);
            OSD_c2(F, P24, main0);
            OSD_c2(F, P25, main0);
        }

        if (GBS::CAPTURE_ENABLE::read() > 0) {
            OSD_c3(O, P23, main0);
            OSD_c3(F, P24, main0);
            OSD_c3(F, P25, main0);
        } else {
            OSD_c3(O, P23, main0);
            OSD_c3(N, P24, main0);
            OSD_c3(F, P25, blue_fill);
        }
    };
    void handle_u(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __(I, _27);
        number_stroca = stroca3;
        __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Enable OTA");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "Restart");
        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "Reset defaults");
    };
    void handle_v(void)
    {
        OSD_c1(0x3E, P11, main0);
        OSD_c1(0x3E, P12, main0);
        OSD_c1(0x3E, P13, main0);
        OSD_c1(0x3E, P14, main0);
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);
        OSD_c1(0x3E, P19, main0);
        OSD_c1(0x3E, P20, main0);
        OSD_c1(0x3E, P21, main0);
        OSD_c1(0x3E, P22, main0);

        if (rto->allowUpdatesOTA) {
            OSD_c1(O, P23, main0);
            OSD_c1(N, P24, main0);
            OSD_c1(F, P25, blue_fill);
        } else {
            OSD_c1(O, P23, main0);
            OSD_c1(F, P24, main0);
            OSD_c1(F, P25, main0);
        }
    };
    void handle_w(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        }
        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Loadprofile:");

        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "Saveprofile:");

        colour1 = yellowT;
        number_stroca = stroca3;
        Osd_Display(1, "Active save:");
    };
    void handle_x(void)
    {
        if (oled_menuItem == 112) {
            colour1 = main0;
            number_stroca = stroca1;
            sending1();
            nameP();
        } else if (oled_menuItem == 113) {
            colour1 = main0;
            number_stroca = stroca1;
            sending2();
            nameP();
        } else if (oled_menuItem == 114) {
            colour1 = main0;
            number_stroca = stroca1;
            sending3();
            nameP();
        } else if (oled_menuItem == 115) {
            colour1 = main0;
            number_stroca = stroca1;
            sending4();
            nameP();
        } else if (oled_menuItem == 116) {
            colour1 = main0;
            number_stroca = stroca1;
            sending5();
            nameP();
        } else if (oled_menuItem == 117) {
            colour1 = main0;
            number_stroca = stroca1;
            sending6();
            nameP();
        } else if (oled_menuItem == 118) {
            colour1 = main0;
            number_stroca = stroca1;
            sending7();
            nameP();
        } else if (oled_menuItem == 126) {
            colour1 = main0;
            number_stroca = stroca1;
            sending8();
            nameP();
        } else if (oled_menuItem == 127) {
            colour1 = main0;
            number_stroca = stroca1;
            sending9();
            nameP();
        } else if (oled_menuItem == 128) {
            colour1 = main0;
            number_stroca = stroca1;
            sending10();
            nameP();
        } else if (oled_menuItem == 129) {
            colour1 = main0;
            number_stroca = stroca1;
            sending11();
            nameP();
        } else if (oled_menuItem == 130) {
            colour1 = main0;
            number_stroca = stroca1;
            sending12();
            nameP();
        } else if (oled_menuItem == 131) {
            colour1 = main0;
            number_stroca = stroca1;
            sending13();
            nameP();
        } else if (oled_menuItem == 132) {
            colour1 = main0;
            number_stroca = stroca1;
            sending14();
            nameP();
        } else if (oled_menuItem == 133) {
            colour1 = main0;
            number_stroca = stroca1;
            sending15();
            nameP();
        } else if (oled_menuItem == 134) {
            colour1 = main0;
            number_stroca = stroca1;
            sending16();
            nameP();
        } else if (oled_menuItem == 135) {
            colour1 = main0;
            number_stroca = stroca1;
            sending17();
            nameP();
        } else if (oled_menuItem == 136) {
            colour1 = main0;
            number_stroca = stroca1;
            sending18();
            nameP();
        } else if (oled_menuItem == 137) {
            colour1 = main0;
            number_stroca = stroca1;
            sending19();
            nameP();
        } else if (oled_menuItem == 138) {
            colour1 = main0;
            number_stroca = stroca1;
            sending20();
            nameP();
        }

        if (uopt->presetSlot == 'A') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending1a();
            nameP();
        } else if (uopt->presetSlot == 'B') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending2a();
            nameP();
        } else if (uopt->presetSlot == 'C') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending3a();
            nameP();
        } else if (uopt->presetSlot == 'D') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending4a();
            nameP();
        } else if (uopt->presetSlot == 'E') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending5a();
            nameP();
        } else if (uopt->presetSlot == 'F') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending6a();
            nameP();
        } else if (uopt->presetSlot == 'G') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending7a();
            nameP();
        } else if (uopt->presetSlot == 'H') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending8a();
            nameP();
        } else if (uopt->presetSlot == 'I') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending9a();
            nameP();
        } else if (uopt->presetSlot == 'J') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending10a();
            nameP();
        } else if (uopt->presetSlot == 'K') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending11a();
            nameP();
        } else if (uopt->presetSlot == 'L') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending12a();
            nameP();
        } else if (uopt->presetSlot == 'M') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending13a();
            nameP();
        } else if (uopt->presetSlot == 'N') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending14a();
            nameP();
        } else if (uopt->presetSlot == 'O') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending15a();
            nameP();
        } else if (uopt->presetSlot == 'P') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending16a();
            nameP();
        } else if (uopt->presetSlot == 'Q') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending17a();
            nameP();
        } else if (uopt->presetSlot == 'R') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending18a();
            nameP();
        } else if (uopt->presetSlot == 'S') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending19a();
            nameP();
        } else if (uopt->presetSlot == 'T') {
            colour1 = yellowT;
            number_stroca = stroca3;
            sending20a();
            nameP();
        }

        if (oled_menuItem == 119) {
            colour1 = main0;
            number_stroca = stroca2;
            sending1b();
            nameP();
        } else if (oled_menuItem == 120) {
            colour1 = main0;
            number_stroca = stroca2;
            sending2b();
            nameP();
        } else if (oled_menuItem == 121) {
            colour1 = main0;
            number_stroca = stroca2;
            sending3b();
            nameP();
        } else if (oled_menuItem == 122) {
            colour1 = main0;
            number_stroca = stroca2;
            sending4b();
            nameP();
        } else if (oled_menuItem == 123) {
            colour1 = main0;
            number_stroca = stroca2;
            sending5b();
            nameP();
        } else if (oled_menuItem == 124) {
            colour1 = main0;
            number_stroca = stroca2;
            sending6b();
            nameP();
        } else if (oled_menuItem == 125) {
            colour1 = main0;
            number_stroca = stroca2;
            sending7b();
            nameP();
        } else if (oled_menuItem == 139) {
            colour1 = main0;
            number_stroca = stroca2;
            sending8b();
            nameP();
        } else if (oled_menuItem == 140) {
            colour1 = main0;
            number_stroca = stroca2;
            sending9b();
            nameP();
        } else if (oled_menuItem == 141) {
            colour1 = main0;
            number_stroca = stroca2;
            sending10b();
            nameP();
        } else if (oled_menuItem == 142) {
            colour1 = main0;
            number_stroca = stroca2;
            sending11b();
            nameP();
        } else if (oled_menuItem == 143) {
            colour1 = main0;
            number_stroca = stroca2;
            sending12b();
            nameP();
        } else if (oled_menuItem == 144) {
            colour1 = main0;
            number_stroca = stroca2;
            sending13b();
            nameP();
        } else if (oled_menuItem == 145) {
            colour1 = main0;
            number_stroca = stroca2;
            sending14b();
            nameP();
        } else if (oled_menuItem == 146) {
            colour1 = main0;
            number_stroca = stroca2;
            sending15b();
            nameP();
        } else if (oled_menuItem == 147) {
            colour1 = main0;
            number_stroca = stroca2;
            sending16b();
            nameP();
        } else if (oled_menuItem == 148) {
            colour1 = main0;
            number_stroca = stroca2;
            sending17b();
            nameP();
        } else if (oled_menuItem == 149) {
            colour1 = main0;
            number_stroca = stroca2;
            sending18b();
            nameP();
        } else if (oled_menuItem == 150) {
            colour1 = main0;
            number_stroca = stroca2;
            sending19b();
            nameP();
        } else if (oled_menuItem == 151) {
            colour1 = main0;
            number_stroca = stroca2;
            sending20b();
            nameP();
        }
    };
    void handle_y(void)
    {
        uopt->presetPreference = OutputCustomized;
        saveUserPrefs();
        uopt->presetPreference = OutputCustomized;
        if (rto->videoStandardInput == 14) {
            rto->videoStandardInput = 15;
        } else {
            applyPresets(rto->videoStandardInput);
        }
        saveUserPrefs();
    };

    void handle_z(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        }else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        }else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Contrast");
        // Osd_Display(1, "Saturation");

        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "Saturation");

        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "Default");
    }

    void handle_A(void)
    {


        OSD_c1(0x3E, P13, main0);
        OSD_c1(0x3E, P14, main0);
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);


        colour1 = main0;
        number_stroca = stroca1;
        sequence_number1 = _25;
        sequence_number2 = _24;
        sequence_number3 = _23;
        Typ(Contrast);


        OSD_c2(0x3E, P13, main0);
        OSD_c2(0x3E, P14, main0);
        OSD_c2(0x3E, P15, main0);
        OSD_c2(0x3E, P16, main0);
        OSD_c2(0x3E, P17, main0);
        OSD_c2(0x3E, P18, main0);


        colour1 = main0;
        number_stroca = stroca2;
        sequence_number1 = _25;
        sequence_number2 = _24;
        sequence_number3 = _23;
        Typ(Saturation);

    };


    void handle_caret(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            if (!LineOption)
                A2_main0 = 0x14;
            else
                A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        // colour1 = blue;
        // number_stroca = stroca1;
        // __(icon5, _27);
        // number_stroca = stroca2;
        // __(I, _27);
        // number_stroca = stroca3;
        // __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "DoubleLine");
        // Osd_Display(1, "Smooth");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "Smooth");
        // Osd_Display(1, "Bright");

        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "Bright");
        // Osd_Display(1, "Contrast");
    };
    void handle_at(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        // number_stroca = stroca1;
        // __(icon5, _27);
        number_stroca = stroca2;
        __('1', _27);
        number_stroca = stroca3;
        __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "RGBs");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "RGsB");
        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "VGA");
    };
    void handle_exclamation(void)
    {
        A1_yellow = main0;
        A2_main0 = main0;
        A3_main0 = main0;

        colour1 = blue;
        // number_stroca = stroca1;
        // __(icon5, _27);
        // number_stroca = stroca2;
        // __('1', _27);
        // number_stroca = stroca3;
        // __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(0, "Whether to keep the settings");

        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(0, "Restore in ");

        if (Keep_S) {
            colour1 = yellowT;
            OSD_c3(0x15, P2, yellowT);
        } else {
            colour1 = A3_main0;
            OSD_c3(0x15, P2, blue_fill);
        }
        number_stroca = stroca3;
        Osd_Display(3, "Changes");

        if (!Keep_S) {
            colour1 = yellowT;
            OSD_c3(0x15, P13, yellowT);
        } else {
            colour1 = A3_main0;
            OSD_c3(0x15, P13, blue_fill);
        }
        Osd_Display(0xff, "    Recover");

        // OSD_c3(0x15, P2, blue_fill);
    };
    void handle_hash(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __('2', _27);
        // number_stroca = stroca3;
        // __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "YPBPR");
        colour1 = A2_main0;
        number_stroca = stroca2;
        Osd_Display(1, "SV");
        colour1 = A3_main0;
        number_stroca = stroca3;
        Osd_Display(1, "AV");
    };
    void handle_dollar(void)
    {
        if (oled_menuItem == OSD_Input_SV) {
            OSD_writeString(4, 2, "Format:");
            OSD_writeString(4, 3, "                      ");
        } else if (oled_menuItem == OSD_Input_AV) {
            OSD_writeString(4, 3, "Format:");
            OSD_writeString(4, 2, "                      ");
        } else {
            OSD_writeString(4, 2, "                      ");
            OSD_writeString(4, 3, "                      ");
        }
        switch (SvModeOption) {
            case 0: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "Auto           ");
            } break;
            case 1: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL            ");
            } break;
            case 2: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "NTSC-M         ");
            } break;
            case 3: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL-60         ");
            } break;
            case 4: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "NTSC443        ");
            } break;
            case 5: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "NTSC-J          ");
            } break;
            case 6: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL-N w/ p      ");
            } break;
            case 7: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL-M w/o p    ");
            } break;
            case 8: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL-M          ");
            } break;
            case 9: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL Cmb -N     ");
            } break;
            case 10: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "PAL Cmb -N w/ p");
            } break;
            case 11: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "SECAM          ");
            } break;
            default: {
                if (oled_menuItem == OSD_Input_SV)
                    OSD_writeString(11, 2, "               ");
            } break;
        }

        switch (AvModeOption) {
            case 0: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "Auto           ");
            } break;
            case 1: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL            ");
            } break;
            case 2: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "NTSC-M         ");
            } break;
            case 3: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL-60         ");
            } break;
            case 4: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "NTSC443        ");
            } break;
            case 5: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "NTSC-J          ");
            } break;
            case 6: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL-N w/ p      ");
            } break;
            case 7: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL-M w/o p    ");
            } break;
            case 8: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL-M          ");
            } break;
            case 9: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL Cmb -N     ");
            } break;
            case 10: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "PAL Cmb -N w/ p");
            } break;
            case 11: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "SECAM          ");
            } break;
            default: {
                if (oled_menuItem == OSD_Input_AV)
                    OSD_writeString(11, 3, "               ");
            } break;
        }
    };
    void handle_percent(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __(I, _27);
        number_stroca = stroca3;
        __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Setting");
    };
    void handle_ampersand(void)
    {

        OSD_c1(0x3E, P13, main0);
        OSD_c1(0x3E, P14, main0);
        OSD_c1(0x3E, P15, main0);
        OSD_c1(0x3E, P16, main0);
        OSD_c1(0x3E, P17, main0);
        OSD_c1(0x3E, P18, main0);

        OSD_c2(0x3E, P13, main0);
        OSD_c2(0x3E, P14, main0);
        OSD_c2(0x3E, P15, main0);
        OSD_c2(0x3E, P16, main0);
        OSD_c2(0x3E, P17, main0);
        OSD_c2(0x3E, P18, main0);


        OSD_c3(0x3E, P13, main0);
        OSD_c3(0x3E, P14, main0);
        OSD_c3(0x3E, P15, main0);
        OSD_c3(0x3E, P16, main0);
        OSD_c3(0x3E, P17, main0);
        OSD_c3(0x3E, P18, main0);

        if (LineOption) {
            OSD_c1(n2, P23, main0);
            OSD_c1(X, P24, main0);
        } else {
            OSD_c1(n1, P23, main0);
            OSD_c1(X, P24, main0);
            SmoothOption = false;
        }
        if (SmoothOption) {
            OSD_c2(O, P23, main0);
            OSD_c2(N, P24, main0);
            OSD_c2(F, P25, blue_fill);
        } else {
            OSD_c2(O, P23, main0);
            OSD_c2(F, P24, main0);
            OSD_c2(F, P25, main0);
        }

        colour1 = main0;
        number_stroca = stroca3;
        sequence_number1 = _25;
        sequence_number2 = _24;
        sequence_number3 = _23;
        Typ(Bright);


        // colour1 = main0;
        // number_stroca = stroca3;
        // sequence_number1 = _25;
        // sequence_number2 = _24;
        // sequence_number3 = _23;
        // Typ(Contrast);
    };
    void handle_asterisk(void)
    {
        if (COl_L == 1) {
            A1_yellow = yellowT;
            A2_main0 = main0;
            A3_main0 = main0;
        } else if (COl_L == 2) {
            A1_yellow = main0;
            A2_main0 = yellowT;
            A3_main0 = main0;
        } else if (COl_L == 3) {
            A1_yellow = main0;
            A2_main0 = main0;
            A3_main0 = yellowT;
        }

        colour1 = blue;
        number_stroca = stroca1;
        __(icon5, _27);
        number_stroca = stroca2;
        __('4', _27);
        // number_stroca = stroca3;
        // __(icon6, _27);

        colour1 = A1_yellow;
        number_stroca = stroca1;
        Osd_Display(1, "Matched presets");
        // colour1 = A2_main0;
        // number_stroca = stroca2;
    };
