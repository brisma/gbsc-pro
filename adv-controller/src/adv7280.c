#include "main.h"
#include "adv7280.h"
#include "adv7391.h"
#include "board.h"
#include "led.h"
#include "signal.h"

/* Values of the applied STATE (defaults until the first one) */
static AdvRegs sRegs = {
    .contrast = 0x80, .saturation = 0x80, .formatReg = 0xFF, .i2p = 1,
    .aceLuma = 13, .aceChromaGain = 8, .aceChromaMax = 8, .aceGamma = 8, .aceSpeed = 15,
    .filterY = 1, .filterWy = 19, .filterWyOvr = 1, .combPal = 1, .combTapsNtsc = 2, .combTapsPal = 3,
};
static uint8_t sPowered, sStatus1, sAck;

/* I2C command tables: [I2C_ADDR, REG, VALUE] triplets */

/* I2P (Interlace-to-Progressive) Enable */
static const uint8_t I2C_COMMANDS_I2P_ON[] = {
    0x84, 0x55, 0x80,   // ADV7280 - I2C_DEINT_ENABLE: Enable I2P Converter
    0x84, 0x5A, 0x02,   // ADV7280 - Not documented on page 99 - Configure I2P Parameters Smooth 1A (?)
};

/* I2P Disable for progressive input */
static const uint8_t I2C_COMMANDS_I2P_OFF_p[] = {
    0x84, 0x55, 0x00,   // ADV7280 - I2C_DEINT_ENABLE: Disable I2P Converter
    0x84, 0x5A, 0x02,   // ADV7280 - Not documented on page 99 - Configure I2P Parameters Smooth 1A (?)
};

/* Smooth filter OFF */
static const uint8_t I2C_COMMANDS_SMOOTH_OFF[] = {
    0x84, 0x55, 0x80,   // ADV7280 - I2C_DEINT_ENABLE: Enable I2P Converter
    0x84, 0x5A, 0x02,   // ADV7280 - Disable I2P Smooth 1A
    0x42, 0x0E, 0x00    // ADV7280 - ADI Control 1: main register
};

/* Smooth filter ON */
static const uint8_t I2C_COMMANDS_SMOOTH_ON[] = {
    0x84, 0x55, 0x80,   // ADV7280 - I2C_DEINT_ENABLE: Enable I2P Converter
    0x84, 0x5A, 0x1A,   // ADV7280 - Enable I2P Smooth 1A
    0x42, 0x0E, 0x00    // ADV7280 - ADI Control 1: main register
};

/* S-Video (Y/C) input configuration */
static const uint8_t I2C_COMMANDS_YC_INPUT[] = {
    0x42, 0x0E, 0x00,   // ADV7280 - ADI Control 1: main register
    0x42, 0x00, 0x09,   // ADV7280 - Input control: Y input on A3, C input on A4
};

/* CVBS (Composite) input configuration */
static const uint8_t I2C_COMMANDS_CVBS_INPUT[] = {
    0x42, 0x0E, 0x00,   // ADV7280 - ADI Control 1: main register
    0x42, 0x00, 0x00,   // ADV7280 - Input control: CVBS input on A1
};

/* Full initialization sequence for ADV7280 + ADV7391, in three parts with a real delay after each soft reset
 * (UG-637 p.26: wait 5 ms after a reset; a dummy I2C write is no delay: an absent address NAKs at once) */
static const uint8_t I2C_AUTO_RESET[] = {
    0x42, 0x0F, 0x80,   // ADV7280 - Power management: Start reset sequence
    0x56, 0x17, 0x02,   // ADV7391 - Software reset: Software reset
};

static const uint8_t I2C_AUTO_COMMANDS[] = {
    0x42, 0x0F, 0x00,   // ADV7280 - Power management: Normal operation

    /* Configure input */
    0x42, 0x05, 0x00,   // ADV7280 - Not documented on page 71
    0x42, 0x02, 0x04,   // ADV7280 - Video Selection 2: Autodetect, set to default
    0x42, 0x07, 0x7F,   // ADV7280 - Autodetect enable: all but SECAM-525 (reset value)
    0x42, 0x14, 0x11,   // ADV7280 - Analog clamp control: current sources on, free-run 100 % colour bars
    0x42, 0x00, 0x00,   // ADV7280 - Input control: CVBS input on A1

    /* ADI Required write */
    0x42, 0x0E, 0x80,   // ADV7280 - ADI Required Write; Reset Current Clamp Circuitry (step1)
    0x42, 0x9C, 0x00,   // ADV7280 - ADI Required Write; Reset Current Clamp Circuitry (step2)
    0x42, 0x9C, 0xFF,   // ADV7280 - ADI Required Write; Reset Current Clamp Circuitry (step3)
    0x42, 0x0E, 0x00,   // ADV7280 - ADI Required Write; Reset Current Clamp Circuitry (step4)

    /* Power Up Digital Output Pads */
    0x42, 0x03, 0x0C,   // ADV7280 - Enable Pixel & Sync output drivers
    0x42, 0x04, 0x07,   // ADV7280 - Power-up INTRQ, HS & VS pads
    0x42, 0x13, 0x00,   // ADV7280 - Enable ADV7182 for 28_63636MHz crystal

    /* Configuration */
    0x42, 0x17, 0x40,   // ADV7280 - Shaping Filter Control 1: SH1 (autowide notch / wideband + comb)
    0x42, 0x1D, 0x40,   // ADV7280 - Enable LLC output driver

    /* ADI Required Write for Fast Switch */
    0x42, 0x52, 0xCD,   // ADV7280 - Single Ended CVBS - Set optimized IBIAS for the AFE
    0x42, 0x80, 0x51,   // ADV7280 - ADI Required Write
    0x42, 0x81, 0x51,   // ADV7280 - ADI Required Write
    0x42, 0x82, 0x00,   // ADV7280 - ADI Required Write
    0x42, 0x0E, 0x80,   // ADV7280 - ADI Required Write
    0x42, 0xD9, 0x44,   // ADV7280 - ADI Required Write
    0x42, 0x0E, 0x00,   // ADV7280 - ADI Control 1: main register

    /* Enable I2P */
    0x42, 0xFD, 0x84,   // ADV7280 - set the VPP address to 0x84
    0x84, 0xA3, 0x00,   // ADV7280 - ADI Required Write - VPP writes begin
    0x84, 0x5B, 0x00,   // ADV7280 - ADV_TIMING_MODE_EN: Enable advanced timing mode
    0x84, 0x55, 0x80,   // ADV7280 - I2C_DEINT_ENABLE - Enable I2P Converter
    0x84, 0x5A, 0x02,   // ADV7280 - Disable I2P Smooth 1A
    0x42, 0x6B, 0x11,   // ADV7280 - Output Sync Select 2: VSYNC
};

/* Right after I2C_AUTO_COMMANDS (same transfers, its own table: an encoder NAK is not a decoder failure) */
static const uint8_t I2C_AUTO_ENC_RESET[] = {
    /* Reset ADV7391 */
    0x56, 0x17, 0x02,   // ADV7391 - Software reset: Software reset
};

static const uint8_t I2C_AUTO_ENCODER[] = {
    /* Encoder configuration */
    0x56, 0x00, 0x9C,   // ADV7391 - Power up DACs and PLL [Encoder writes begin]
    0x56, 0x01, 0x70,   // ADV7391 - Mode select: ED (at 54MHz) input, Chrome rising, luma falling
    0x56, 0x30, 0x1C,   // ADV7391 - ED/HD Mode Register 1: 625p, EAV/SAV (ENC_FollowOutput then sets the rate)
    0x56, 0x31, 0x01,   // ADV7391 - ED/HD Mode Register 2: Pixel data valid on
    0x42, 0x0E, 0x00,   // ADV7280 - ADI Control 1: main register
};

/* ACE (Adaptive Contrast Enhancement) ON */
static const uint8_t Ace_Code_ON[] = {
    0x42, 0x0E, 0x40,   // ADV7280 - ADI Control 1: User Sub Map 2
    0x42, 0x80, 0x80,   // ADV7280 - ACE Control 1: Enable ACE
    0x42, 0x0E, 0x00    // ADV7280 - ADI Control 1: main register
};

/* ACE (Adaptive Contrast Enhancement) OFF */
static const uint8_t Ace_Code_OFF[] = {
    0x42, 0x0E, 0x40,   // ADV7280 - ADI Control 1: User Sub Map 2
    0x42, 0x80, 0x00,   // ADV7280 - ACE Control 1: Disable ACE
    0x42, 0x0E, 0x00    // ADV7280 - ADI Control 1: main register
};

void ADV_LoadRegs(const AdvRegs* r) { sRegs = *r; }
const AdvRegs* ADV_Regs(void) { return &sRegs; }

/* Returns LL_OK when every decoder write was acknowledged (the encoder rows are not counted: ENC_* retries them) */
int32_t ADV_Init(void)
{
    uint8_t buff[2];
    int32_t err = LL_OK;

    BOARD_VideoPowerUp();
    DDL_DelayMS(15);

    (void)I2C_TransmitBatch(I2C_AUTO_RESET, sizeof(I2C_AUTO_RESET) / 3, TIMEOUT);   /* resets: a reset may NAK */
    DDL_DelayMS(10);
    err |= I2C_TransmitBatch(I2C_AUTO_COMMANDS, sizeof(I2C_AUTO_COMMANDS) / 3, TIMEOUT);
    (void)I2C_TransmitBatch(I2C_AUTO_ENC_RESET, sizeof(I2C_AUTO_ENC_RESET) / 3, TIMEOUT);
    DDL_DelayMS(10);
    int32_t encOk = I2C_TransmitBatch(I2C_AUTO_ENCODER, sizeof(I2C_AUTO_ENCODER) / 3, TIMEOUT);
    err |= ADV_SetInput(sRegs.ycInput);

    buff[0] = VID_SEL_REG;
    buff[1] = sRegs.formatReg;
    err |= I2C_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);

    if (encOk == LL_OK) ENC_Forget();   /* I2C_AUTO_ENCODER left the ADV7391 at the ED base, 625p */
    else ENC_Lost();
    {
        uint8_t reg = 0x13, status3;   /* not locked yet: status1 0 (the SD field mode waits for a lock) */
        if (I2C_Receive(DEVICE_ADDR, &reg, &status3, 1, TIMEOUT) == LL_OK) ENC_FollowOutput(status3);
    }
    err |= ADV_SetBCSH();
    err |= ADV_SetACE(sRegs.ace);
    err |= ADV_SetACEParams();
    err |= ADV_SetVideoFilters();

    if (sRegs.i2p) {
        err |= ADV_SetI2P(1);
        if (sRegs.smooth) err |= ADV_SetSmooth(1);
    } else {
        err |= ADV_SetI2P(1);    /* through I2P + smooth on before turning both off (the known-good sequence) */
        err |= ADV_SetSmooth(1);
        sRegs.smooth = 0;
        err |= ADV_SetSmooth(0);
        err |= ADV_SetI2P(0);
    }
    sPowered = 1;
    return err;
}

void ADV_Deinit(void)
{
    BOARD_VideoPowerDown();
    ENC_Forget();               /* powered down: the encoder mode is unknown until the next ADV_Init */
    sPowered = 0;
}

int32_t ADV_SetInput(uint8_t yc)
{
    int32_t err;
    if (yc) err = I2C_TransmitBatch(I2C_COMMANDS_YC_INPUT, sizeof(I2C_COMMANDS_YC_INPUT) / 3, TIMEOUT);
    else err = I2C_TransmitBatch(I2C_COMMANDS_CVBS_INPUT, sizeof(I2C_COMMANDS_CVBS_INPUT) / 3, TIMEOUT);
    err |= ADV_SetACE(sRegs.ace);       /* reapplied after an input switch */
    err |= ADV_SetACEParams();
    err |= ADV_SetVideoFilters();
    return err;
}

int32_t ADV_SetI2P(uint8_t enable)
{
    if (enable) return I2C_TransmitBatch(I2C_COMMANDS_I2P_ON, sizeof(I2C_COMMANDS_I2P_ON) / 3, TIMEOUT);
    return I2C_TransmitBatch(I2C_COMMANDS_I2P_OFF_p, sizeof(I2C_COMMANDS_I2P_OFF_p) / 3, TIMEOUT);
}

int32_t ADV_SetSmooth(uint8_t smooth)
{
    if (smooth) return I2C_TransmitBatch(I2C_COMMANDS_SMOOTH_ON, sizeof(I2C_COMMANDS_SMOOTH_ON) / 3, TIMEOUT);
    return I2C_TransmitBatch(I2C_COMMANDS_SMOOTH_OFF, sizeof(I2C_COMMANDS_SMOOTH_OFF) / 3, TIMEOUT);
}

int32_t ADV_SetBCSH(void)
{
    const uint8_t batch[] = {
        0x42, 0x0E, 0x00,                 /* main register map */
        0x42, 0x0A, sRegs.bright,         /* brightness */
        0x42, 0x08, sRegs.contrast,       /* contrast (0x80 = gain 1) */
        0x42, 0xE3, sRegs.saturation,     /* SD saturation Cb (0x80 = 0 dB) */
        0x42, 0xE4, sRegs.saturation,     /* SD saturation Cr */
        0x42, 0x0B, sRegs.hue,            /* hue */
    };
    return I2C_TransmitBatch(batch, sizeof(batch) / 3, TIMEOUT);
}

int32_t ADV_SetACE(uint8_t ace)
{
    if (ace) return I2C_TransmitBatch(Ace_Code_ON, sizeof(Ace_Code_ON) / 3, TIMEOUT);
    return I2C_TransmitBatch(Ace_Code_OFF, sizeof(Ace_Code_OFF) / 3, TIMEOUT);
}

/* User Sub Map 2 (0x0E = 0x40), registers 0x83-0x85 */
int32_t ADV_SetACEParams(void)
{
    const uint8_t batch[] = {
        DEVICE_ADDR, 0x0E, 0x40,                                                    /* User Sub Map 2 */
        DEVICE_ADDR, 0x83, sRegs.aceLuma & 0x1F,                                    /* luma gain [4:0] */
        DEVICE_ADDR, 0x84, (uint8_t)((sRegs.aceChromaMax << 4) | (sRegs.aceChromaGain & 0x0F)),
        DEVICE_ADDR, 0x85, (uint8_t)((sRegs.aceSpeed << 4) | (sRegs.aceGamma & 0x0F)),
        DEVICE_ADDR, 0x0E, 0x00                                                     /* main map */
    };
    return I2C_TransmitBatch(batch, 5, TIMEOUT);
}

/* Shaping filters (0x17-0x19) and comb control (0x38-0x39) */
int32_t ADV_SetVideoFilters(void)
{
    uint8_t reg17 = (uint8_t)(((sRegs.filterC & 0x07) << 5) | (sRegs.filterY & 0x1F));
    uint8_t reg18 = (uint8_t)(((sRegs.filterWyOvr & 0x01) << 7) | (sRegs.filterWy & 0x1F));   /* bit 7 WYSFMOVR */
    uint8_t reg19 = (uint8_t)(0xF0 | ((sRegs.combNtsc & 0x03) << 2) | (sRegs.combPal & 0x03));
    uint8_t reg38 = (uint8_t)((sRegs.combTapsNtsc << 6) | (sRegs.combChromaNtsc << 3) | sRegs.combLumaNtsc);
    uint8_t reg39 = (uint8_t)((sRegs.combTapsPal << 6) | (sRegs.combChromaPal << 3) | sRegs.combLumaPal);
    /* 0x4D: reset 0xEF = DNR and CTI on; bits 7:6 and CTI_AB kept at their reset values, CTI = CTI_EN + CTI_AB_EN */
    uint8_t reg4d = (uint8_t)(0xCC | (sRegs.dnr ? 0x20 : 0) | (sRegs.cti ? 0x03 : 0));

    const uint8_t batch[] = {
        DEVICE_ADDR, 0x0E, 0x00,
        DEVICE_ADDR, 0x17, reg17,
        DEVICE_ADDR, 0x18, reg18,
        DEVICE_ADDR, 0x19, reg19,
        DEVICE_ADDR, 0x38, reg38,
        DEVICE_ADDR, 0x39, reg39,
        DEVICE_ADDR, 0x4D, reg4d
    };
    return I2C_TransmitBatch(batch, sizeof(batch) / 3, TIMEOUT);
}

/* One pass every 50 ms: main map, then STATUS_3, STATUS_1, STATUS_2. The signal report and the encoder use these
 * reads only. */
void ADV_DetectLoop(void)
{
    if (!sPowered) {
        sStatus1 = 0;           /* decoder powered down: no lock, no standard */
        SIG_Sample(0, 0, 0, 0, 0);
        return;
    }

    uint8_t status1 = 0, status2 = 0, status3 = 0;
    uint8_t buff[2] = { 0x0E, 0x00 };
    (void)I2C_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);

    uint8_t r3 = 0x13, r1 = 0x10, r2 = 0x12;
    uint8_t ack3 = I2C_Receive(DEVICE_ADDR, &r3, &status3, 1, TIMEOUT) == LL_OK;
    sAck = (I2C_Receive(DEVICE_ADDR, &r1, &status1, 1, TIMEOUT) == LL_OK) ? 1 : 0;
    if (I2C_Receive(DEVICE_ADDR, &r2, &status2, 1, TIMEOUT) != LL_OK) status2 = 0;
    if (!ack3) status3 = 0;
    if (!sAck) status1 = 0;
    sStatus1 = status1;
    SIG_Sample(1, sAck, status1, status2, status3);

    if (ack3) ENC_FollowOutput(status3);   /* NTSC <-> PAL output */
    LED_FreeRunCheck(status1);
}

uint8_t ADV_Powered(void) { return sPowered; }
uint8_t ADV_LastStatus1(void) { return sStatus1; }
uint8_t ADV_LastAck(void) { return sAck; }
