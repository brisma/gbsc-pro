/* link_hal.h, ADV half: decoder, encoder, switches and LED modules (host-buildable with the test shims: the
 * golden I2C traces run this file) */
#include "main.h"
#include "link_hal.h"
#include "signal.h"
#include "fault_handler.h"   /* WDT_Refresh */

void halLoadGlobals(const AdvRegs* r)
{
    Input_signal = r->inputSignal;
    ADV_LoadRegs(r);
}

void halSwitches(const AdvSwitches* w)
{
    asw_01 = w->asw1;
    asw_02 = w->asw2;
    asw_03 = w->asw3;
    asw_04 = w->asw4;
    AVsw = w->avConnect;
    ASW_SetSwitches(asw_01, asw_02, asw_03, asw_04);
    ASW_SetAVConnect(AVsw);
    c_state = 1;        /* status LED "ok" (ASW_SetAVConnect(0) leaves the error code: cleared here) */
}

int halAdvInit(void)
{
    WDT_Refresh();      /* ADV_Init + settle ≈ 370 ms */
    int32_t err = ADV_Init();
    DDL_DelayMS(300);   /* the ADV7280 settles; the ESP sends no request for 1 s after an input change to the decoder */
    return err != LL_OK;
}

void halAdvDeinit(void) { ADV_Deinit(); }

int halFormat(uint8_t reg)
{
    uint8_t b[2] = {VID_SEL_REG, reg};
    return I2C_Transmit(DEVICE_ADDR, b, 2, TIMEOUT) != LL_OK;
}

int halI2p(uint8_t i2p, uint8_t smooth)
{
    int32_t err;
    if (i2p) {
        err = ADV_SetI2P(1);
        err |= ADV_SetSmooth(smooth);
    } else {
        err = ADV_SetSmooth(0);   /* smooth off first */
        err |= ADV_SetI2P(0);
    }
    LED_ResetFreeRun();
    return err != LL_OK;
}

int halAce(void)
{
    int32_t err = ADV_SetACE(ADV_Regs()->ace);
    err |= ADV_SetACEParams();
    return err != LL_OK;
}

int halBcsh(void) { return ADV_SetBCSH() != LL_OK; }
int halFilters(void) { return ADV_SetVideoFilters() != LL_OK; }

void halI2cWrite(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t t[3] = {addr, reg, val};
    (void)I2C_TransmitBatch(t, 1, TIMEOUT);
}

int halI2cRead(uint8_t addr, uint8_t reg, uint8_t* v, uint8_t n)
{
    /* The DDL timeout counts polling tries, not time: 1000 tries ended before one byte at 200 kHz (~45 us), so every
       read failed on the device. TIMEOUT as every other transfer: a good read takes <2 ms for 30 bytes */
    uint8_t stage = 0;
    return I2C_ReadBurst(addr, reg, v, n, TIMEOUT, &stage) == LL_OK ? 0 : (stage ? (int)stage : -1);
}


void halSignal(AdvSignal* out) { SIG_Get(out, ADV_Regs()->formatReg, ENC_Mode(), Input_signal); }
