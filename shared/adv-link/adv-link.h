/* GBSC PRO link v3 between the ESP8266 and the ADV Controller.
 * Pure C99, shared by both firmwares: frame encoder, byte-wise parser, ADV state and the diff planner. */
#ifndef ADV_LINK_H
#define ADV_LINK_H
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Frame: 0xA7 0x3C | len | seq | cmd | payload[len] | crc16 (CCITT-FALSE, big-endian, over len..payload) */
#define ADV_SYNC0 0xA7
#define ADV_SYNC1 0x3C
#define ADV_MAX_PAYLOAD 96
#define ADV_FRAME_OVERHEAD 7

enum {
    ADV_CMD_STATE = 0x01, ADV_CMD_I2C_BATCH = 0x02,
    ADV_CMD_INFO = 0x43, ADV_CMD_UPD_BEGIN = 0x44, ADV_CMD_UPD_DATA = 0x45, ADV_CMD_UPD_COMMIT = 0x46,
    ADV_CMD_PING = 0x47, ADV_CMD_UPD_STATUS = 0x48, ADV_CMD_SIGNAL = 0x49, ADV_CMD_REG_READ = 0x4A, ADV_CMD_NAK = 0xFF,
    ADV_RESP = 0x80,         /* response bit (ADV Controller -> ESP) */
    ADV_WANT_REPLY = 0x40    /* request bit: a reply is expected */
};

uint16_t advCrc16(const uint8_t* p, size_t n);
/* Returns the frame length, 0 when it does not fit cap or len > ADV_MAX_PAYLOAD */
size_t advFrameEncode(uint8_t seq, uint8_t cmd, const uint8_t* payload, uint8_t len, uint8_t* out, size_t cap);

typedef struct AdvParser {
    uint8_t raw[ADV_MAX_PAYLOAD + ADV_FRAME_OVERHEAD];   /* bytes of the frame being received, from its sync */
    uint8_t n;                                          /* bytes in raw */
    uint8_t seq, cmd, len;                              /* the last frame delivered */
    uint8_t buf[ADV_MAX_PAYLOAD];
    uint16_t bad;                                       /* rejected frames (CRC or length) */
} AdvParser;
typedef void (*AdvFrameCb)(const AdvParser* p, void* ctx);

void advParserInit(AdvParser* p);
/* Feeds one byte; every valid frame is delivered once through cb (a frame hidden inside rejected bytes too) */
void advParserFeed(AdvParser* p, uint8_t b, AdvFrameCb cb, void* ctx);

#define ADV_STATE_VERSION 1
#define ADV_STATE_BYTES 40
enum { ADV_INPUT_RGBS = 1, ADV_INPUT_RGSB = 2, ADV_INPUT_VGA = 3, ADV_INPUT_YPBPR = 4, ADV_INPUT_SV = 5, ADV_INPUT_AV = 6 };
#define ADV_FORMAT_COUNT 13       /* advFormatRegister indexes 0..12 */
#define ADV_I2C_BATCH_MAX 30      /* (addr, reg, val) triplets in one ADV_CMD_I2C_BATCH frame */
#define ADV_CMD_BASE(c) ((uint8_t)((c) & 0x3F))   /* command without the reply/flag bits */
typedef struct {
    uint8_t version;
    uint8_t input;                 /* 1 RGBs, 2 RGsB, 3 VGA, 4 YPbPr, 5 S-Video, 6 Composite */
    uint8_t syncStripper, termination;
    uint8_t formatSv, formatAv;    /* 0..12 ADV video format index (advFormatRegister) */
    uint8_t i2p, smooth, ace;
    uint8_t aceLumaGain, aceChromaGain, aceChromaMax, aceGammaGain, aceResponseSpeed;
    uint8_t brightness, contrast, saturation, hue;   /* 0..254 levels, 128 = neutral (adv-state.cpp maps the settings) */
    uint8_t filterY, filterC, filterWy, filterWyOverride, combNtsc, combPal;
    uint8_t combLumaNtsc, combChromaNtsc, combTapsNtsc, combLumaPal, combChromaPal, combTapsPal;
    uint8_t dnr, cti;              /* bytes 30/31, 0 = off: ADV7280 0x4D DNR_EN / CTI_EN */
    uint8_t listen;                /* byte 32, RGB SCART only: the decoder on CVBS and the pin-20 termination while the
                                    * TV5725 stays on RGB SCART (the input search identifies the cable; older images ignore it) */
} AdvState;

size_t advStatePack(const AdvState* s, uint8_t* out);   /* ADV_STATE_BYTES; the last byte is a crc8 */
/* 1 when valid: version, crc8, input 1..6 and formats <= 12 are checked; other fields are clamped to their
 * register widths */
int advStateUnpack(const uint8_t* in, size_t n, AdvState* s);
uint16_t advStateHash(const AdvState* s);

/* Analog switches and AV connect for an input; adv = the ADV7280 decoder is used */
typedef struct { uint8_t asw1, asw2, asw3, asw4, avConnect, adv; } AdvSwitches;
AdvSwitches advSwitchesFor(const AdvState* s);

enum { ADV_STEP_INPUT = 1, ADV_STEP_FORMAT = 2, ADV_STEP_SWITCHES = 4, ADV_STEP_I2P = 8, ADV_STEP_ACE = 16,
       ADV_STEP_BCSH = 32, ADV_STEP_FILTERS = 64 };
/* Groups to apply to go from cur to next (haveCur = 0: nothing applied yet) */
uint8_t advPlan(const AdvState* cur, const AdvState* next, int haveCur);
uint8_t advFormatRegister(uint8_t index);   /* ADV7280 register 0x02 value; out of range -> Auto */
int advInputIsDecoder(uint8_t input);        /* S-Video or Composite: the ADV7280 is used */
int advDecoderOn(const AdvState* s);         /* the ADV7280 is powered: a decoder input, or RGB SCART listening */
uint8_t advActiveFormat(const AdvState* s);  /* formatSv on S-Video, else formatAv */

/* ---- replies (frozen forever: PING, INFO, NAK) ---- */
#define ADV_PROTOCOL 3
typedef struct {
    uint8_t protocol, stateVersion;
    char fw[12];                 /* NUL-terminated after unpack */
    uint32_t buildId;            /* CRC32 of the ADV Controller image */
    uint8_t flags, standard;     /* ADV_INFO_*; ADV7280 reg 0x10 bits 6:4 */
    uint16_t uptimeS, rxFrames, badFrames;
    uint8_t appliedCount;        /* STATE frames applied (wraps) */
    uint16_t stateHash;          /* advStateHash of the applied state, 0 before any */
    uint8_t caps;                /* ADV_INFO_CAP_* (byte 29; older images send 0) */
} AdvInfo;
enum { ADV_INFO_LOCK = 1, ADV_INFO_FSC_LOCK = 2, ADV_INFO_ADV_PRESENT = 4, ADV_INFO_ADV_POWERED = 8 };
#define ADV_INFO_BYTES 36        /* 30 used + 6 reserved, little-endian, field by field */
enum { ADV_INFO_CAP_SIGNAL = 1,     /* the ADV Controller answers ADV_CMD_SIGNAL */
       ADV_INFO_CAP_REG_READ = 2,    /* ... and ADV_CMD_REG_READ */
       ADV_INFO_CAP_LISTEN = 4 };    /* ... and AdvState.listen */
size_t advInfoPack(const AdvInfo* i, uint8_t* out);
int advInfoUnpack(const uint8_t* in, size_t n, AdvInfo* i);

/* ---- signal report: reply to ADV_CMD_SIGNAL ---- */
#define ADV_SIGNAL_BYTES 12
enum {
    ADV_SIG_PRESENT = 0x01,      /* IN_LOCK && !FREE_RUN_ACT */
    ADV_SIG_LOCK = 0x02,         /* STATUS_1 bit 0 */
    ADV_SIG_FSC = 0x04,          /* STATUS_1 bit 2 */
    ADV_SIG_FREERUN = 0x08,      /* STATUS_3 bit 4 */
    ADV_SIG_INTERLACED = 0x10,   /* STATUS_3 bit 6 */
    ADV_SIG_NONSTD = 0x20,       /* STATUS_2 bit 4 LL_NSTD (0 from images before 3.2.0) */
    ADV_SIG_COLORKILL = 0x40,    /* STATUS_1 bit 7 */
    ADV_SIG_POWERED = 0x80,      /* decoder powered (S-Video / Composite) */
};
enum { ADV_SIG2_ACK = 0x01 };    /* the last status read was acknowledged */
#define ADV_SIG_NO_STANDARD 0xFF
typedef struct {
    uint8_t gen;                 /* +1 on every accepted (debounced) change, wraps */
    uint8_t flags, flags2;       /* ADV_SIG_*, ADV_SIG2_* */
    uint8_t standard;            /* AD_RESULT 0..7 while locked, else ADV_SIG_NO_STANDARD */
    uint8_t hz;                  /* 50 / 60 while locked (STATUS_3 bit 2), else 0 */
    uint8_t status1, status2, status3;   /* raw 0x10, 0x12, 0x13 of the last read */
    uint8_t vidSel;              /* register 0x02 as applied */
    uint8_t encoder;             /* 0 not configured, 1 525p, 2 625p */
    uint8_t input;               /* 1..6, 0 before the first STATE */
} AdvSignal;
size_t advSignalPack(const AdvSignal* s, uint8_t* out);
int advSignalUnpack(const uint8_t* in, size_t n, AdvSignal* s);

/* ---- register read: ADV_CMD_REG_READ request and reply ----
 * Request: dev (8-bit write address: 0x42 ADV7280, 0x56 ADV7391, 0x84 ADV7280 VPP), map (0 none; 0x20 interrupt/VDP
 * or 0x40 user sub map 2, only for 0x42: written to 0x0E around the read, then 0x0E = 0), reg, count 1..32 within
 * 0x00..0xFF. Reply: status (ADV_REG_READ_*) + count values when ok. */
#define ADV_REG_READ_REQ_BYTES 4
#define ADV_REG_READ_MAX 32
#define ADV_REG_READ_REPLY_MAX (1 + ADV_REG_READ_MAX)
enum { ADV_REG_READ_OK = 0, ADV_REG_READ_I2C_ERROR = 1, ADV_REG_READ_BAD_REQUEST = 2 };
size_t advRegReadReqPack(uint8_t dev, uint8_t map, uint8_t reg, uint8_t count, uint8_t* out);
int advRegReadReqUnpack(const uint8_t* in, size_t n, uint8_t* dev, uint8_t* map, uint8_t* reg, uint8_t* count);  /* 0: bad */
size_t advRegReadReplyPack(uint8_t status, const uint8_t* v, uint8_t n, uint8_t* out);   /* values only when ok */
int advRegReadReplyUnpack(const uint8_t* in, size_t n, uint8_t* status, uint8_t* v, uint8_t* count);

/* ---- ADV Controller update (frozen forever: UPD_BEGIN, UPD_DATA, UPD_COMMIT, UPD_STATUS) ---- */
uint32_t advCrc32Update(uint32_t crc, const uint8_t* p, size_t n);   /* zlib CRC32; start with 0 */
uint32_t advCrc32(const uint8_t* p, size_t n);
#define ADV_UPD_CHUNK 64
#define ADV_UPD_BEGIN_BYTES 20       /* u32 size, u32 crc32 (= build id), char version[12] */
#define ADV_UPD_REPLY_BYTES 5        /* u8 status (or state), u32 next offset: every UPD_* reply */
enum { ADV_UPD_OK = 0, ADV_UPD_RESUME = 1, ADV_UPD_TOO_BIG = 2, ADV_UPD_NO_SESSION = 2 };            /* BEGIN, DATA */
enum { ADV_COMMIT_CRC_OK = 0, ADV_COMMIT_CRC_BAD = 1, ADV_COMMIT_HEADER_BAD = 2, ADV_COMMIT_NOT_COMPLETE = 3,
       ADV_COMMIT_NO_SESSION = 4 };
enum { ADV_UST_IDLE = 0, ADV_UST_ERASING = 1, ADV_UST_READY = 2, ADV_UST_RECEIVING = 3, ADV_UST_VERIFYING = 4,
       ADV_UST_ERROR = 5 };                                                                         /* STATUS */
size_t advUpdBeginPack(uint32_t size, uint32_t crc, const char* version, uint8_t* out);
int advUpdBeginUnpack(const uint8_t* in, size_t n, uint32_t* size, uint32_t* crc, char version[12]);
size_t advUpdDataPack(uint32_t offset, const uint8_t* data, uint8_t n, uint8_t* out);
int advUpdDataUnpack(const uint8_t* in, size_t n, uint32_t* offset, const uint8_t** data, uint8_t* len);
size_t advUpdReplyPack(uint8_t status, uint32_t next, uint8_t* out);
int advUpdReplyUnpack(const uint8_t* in, size_t n, uint8_t* status, uint32_t* next);

#ifdef __cplusplus
}
#endif
#endif
