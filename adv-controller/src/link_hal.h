/* Hardware interface of the link v3 dispatcher (link.c): the HC32 implementation is link_hal_hc32.c, the host
 * tests use a stub. No DDL headers here, so link.c builds on the host. */
#ifndef LINK_HAL_H
#define LINK_HAL_H
#include <stdint.h>
#include "adv-link.h"

/* ADV7280/ADV7391 values derived from an AdvState */
typedef struct {
    uint8_t inputSignal;       /* 1..6, for the LED */
    uint8_t adv;               /* 1: the decoder path is used (S-Video/Composite) */
    uint8_t ycInput;           /* 1: S-Video (Y/C), 0: Composite */
    uint8_t formatReg;         /* register 0x02 */
    uint8_t bright, contrast, saturation, hue;   /* registers 0x0A, 0x08, 0xE3/0xE4, 0x0B */
    uint8_t i2p, smooth, ace;  /* smooth is 0 when i2p is off */
    uint8_t aceLuma, aceChromaGain, aceChromaMax, aceGamma, aceSpeed;
    uint8_t filterY, filterC, filterWy, filterWyOvr, combNtsc, combPal;
    uint8_t combLumaNtsc, combChromaNtsc, combTapsNtsc, combLumaPal, combChromaPal, combTapsPal;
    uint8_t dnr, cti;          /* register 0x4D DNR_EN / CTI_EN */
} AdvRegs;

void halLoadGlobals(const AdvRegs* r);    /* stores every value; no I2C */
void halSwitches(const AdvSwitches* w);   /* ASW1-4 and AV connect */
/* The decoder groups return 0 when every I2C write was acknowledged, else nonzero (link.c applies the group again
 * with the next STATE). The encoder is not counted: it retries on its own (ENC_FollowOutput). */
int halAdvInit(void);                     /* power up + full ADV init from the globals (+ settle delay) */
void halAdvDeinit(void);                  /* power down the decoder path */
int halFormat(uint8_t reg);
int halI2p(uint8_t i2p, uint8_t smooth);
int halAce(void);
int halBcsh(void);
int halFilters(void);
void halI2cWrite(uint8_t addr, uint8_t reg, uint8_t val);
int halI2cRead(uint8_t addr, uint8_t reg, uint8_t* v, uint8_t n);   /* one burst read; 0 ok, 1..6 the failed I2C step, <0 other */

/* Replies */
uint32_t halTimeUs(void);                 /* free-running, 10 us resolution, wraps */
/* Sends a reply frame on the shared wire: not before notBeforeUs (turnaround), dropped when deadlineUs has
 * passed right before the line is taken. Returns the time RX was re-enabled, 0 when dropped. */
uint32_t halReply(const uint8_t* frame, uint8_t n, uint32_t notBeforeUs, uint32_t deadlineUs);
void halInfo(AdvInfo* out);               /* fw, build id, status flags, standard, uptime */
void halSignal(AdvSignal* out);           /* v4 signal report (signal.c) */

/* Firmware update: staging area 0x40000-0x6FFFF, 24 sectors of 8 KB */
#define LINK_STAGING_SIZE   0x30000UL
#define LINK_SECTOR_SIZE    0x2000UL
#define LINK_APP_BASE       0x10000UL
int halStagingErase(uint8_t sector);                                     /* 0 = ok */
int halStagingProgram(uint32_t offset, const uint8_t* data, uint8_t len); /* 0 = ok; len <= 64, offset word aligned */
const uint8_t* halStagingPtr(void);
void halCopyAndReset(uint32_t size);                                     /* never returns on the HC32 */

#endif
