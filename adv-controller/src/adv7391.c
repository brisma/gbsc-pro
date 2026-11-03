#include "main.h"
#include "adv7391.h"

/* ED base (54 MHz input mode, YPrPb out), loaded by I2C_AUTO_ENCODER after the ADV7391 reset and kept for both I2P
 * states, as the 2.4.x firmware did: the SD mode (with I2P off) lost the sync at 240p and shook at 288p on the TV */
static const uint8_t ENC_RESET[] = {
    0x56, 0x17, 0x02,   // ADV7391 - Software reset
};
static const uint8_t ENC_ED_BASE[] = {
    0x56, 0x00, 0x9C,   // ADV7391 - Power up DACs and PLL (as I2C_AUTO_ENCODER)
    0x56, 0x01, 0x70,   // ADV7391 - Mode select: ED at 54 MHz input
};

static const uint8_t ENC_525P[] = {
    0x56, 0x30, 0x04,   // ADV7391 - ED/HD mode 1: 525p, EAV/SAV
    0x56, 0x31, 0x11,   // ADV7391 - ED/HD mode 2: pixel data valid, VBI open
};
static const uint8_t ENC_625P[] = {
    0x56, 0x30, 0x1C,   // ADV7391 - ED/HD mode 1: 625p, EAV/SAV
    0x56, 0x31, 0x11,
};

#define BATCH(t) I2C_TransmitBatch(t, sizeof(t) / 3, TIMEOUT)

static uint8_t sMode;            /* ENC_MODE_*: as last written, 0 = not configured since the last reset */
static uint8_t sBase;            /* 1: the ED base is loaded (I2C_AUTO_ENCODER or a reload), 0 unknown (a failed write) */
static uint8_t sReloadWait;      /* follows to skip after a failed reload: its 10 ms pause on every 50 ms detection pass
                                    left the link polls waiting (too old to answer); one try a second instead */
#define ENC_RELOAD_PASSES 20

void ENC_Forget(void) { sMode = ENC_MODE_NONE; sBase = 1; }
void ENC_Lost(void) { sMode = ENC_MODE_NONE; sBase = 0; sReloadWait = 0; }

void ENC_FollowOutput(uint8_t status3)
{
    uint8_t want = (status3 & 0x04) ? ENC_MODE_625P : ENC_MODE_525P;   /* SD_OP_50Hz */
    if (!sBase) {                                         /* a failed init write: reset and reload the base */
        if (sReloadWait) { sReloadWait--; return; }
        int32_t r = BATCH(ENC_RESET);
        DDL_DelayMS(10);
        if (r == LL_OK) r = BATCH(ENC_ED_BASE);
        if (r != LL_OK) { sReloadWait = ENC_RELOAD_PASSES - 1; return; }   /* tried again about a second later */
        sBase = 1;
        sMode = ENC_MODE_NONE;
    }
    if (sMode != want) {
        int32_t r = want == ENC_MODE_625P ? BATCH(ENC_625P) : BATCH(ENC_525P);
        sMode = (r == LL_OK) ? want : ENC_MODE_NONE;      /* a failed write is retried on the next follow */
    }
}

uint8_t ENC_Mode(void) { return sMode; }
