/* Golden I2C trace recorder: stub I2C (scripted reads), GPIO/delay events, and the non-ADV half of link_hal.h */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "link_hal.h"
#include "golden_trace.h"

char gTrace[GOLDEN_MAX][40];
int gTraceN;
uint8_t gReadValue[256];          /* value returned for a read of ADV7280 register r */
int gReadFail, gWriteFail, gWriteFailReg = -1, gWriteFailAddr = -1;

static int writeFails(uint8_t addr, uint8_t reg) {
    return gWriteFail || (gWriteFailReg == reg && (gWriteFailAddr < 0 || gWriteFailAddr == addr));
}
static void add(const char* s) { if (gTraceN < GOLDEN_MAX) { strncpy(gTrace[gTraceN], s, 39); gTrace[gTraceN][39] = 0; gTraceN++; } }

void GOLDEN_Reset(void) { gTraceN = 0; gReadFail = 0; gWriteFail = 0; gWriteFailReg = -1; gWriteFailAddr = -1; memset(gReadValue, 0, sizeof(gReadValue)); }
void GOLDEN_Gpio(const char* op, uint8_t port, uint16_t pins) {
    char b[40]; snprintf(b, sizeof(b), "gpio %s %c %04X", op, 'A' + port, pins); add(b);
}
void GOLDEN_GpioInit(uint8_t port, uint16_t pins, uint16_t dir, uint16_t state, uint16_t pullUp) {
    char b[40]; snprintf(b, sizeof(b), "gpio init %c %04X dir%u st%u pu%u", 'A' + port, pins, dir, state, pullUp); add(b);
}
void GOLDEN_Delay(uint32_t ms) { char b[40]; snprintf(b, sizeof(b), "delay %u", (unsigned)ms); add(b); }
void GOLDEN_Note(const char* s) { char b[40]; snprintf(b, sizeof(b), "# %s", s); add(b); }

int32_t I2C_Transmit(uint16_t addr, uint8_t const data[], uint32_t size, uint32_t timeout) {
    (void)timeout;
    char b[40]; int n = snprintf(b, sizeof(b), "w %02X", addr);
    for (uint32_t i = 0; i < size && n < 36; i++) n += snprintf(b + n, sizeof(b) - n, " %02X", data[i]);
    add(b);
    return size && writeFails((uint8_t)addr, data[0]) ? LL_ERR : LL_OK;
}
int32_t I2C_TransmitBatch(uint8_t const data[], uint32_t count, uint32_t timeout) {
    (void)timeout;
    int32_t first = LL_OK;   /* as i2c.c: every row is sent, the first error is returned */
    for (uint32_t i = 0; i < count; i++) {
        char b[40]; snprintf(b, sizeof(b), "w %02X %02X %02X", data[3 * i], data[3 * i + 1], data[3 * i + 2]);
        add(b);
        if (first == LL_OK && writeFails(data[3 * i], data[3 * i + 1])) first = LL_ERR;
    }
    return first;
}
int32_t I2C_Receive(uint16_t addr, uint8_t tx[], uint8_t rx[], uint32_t size, uint32_t timeout) {
    (void)timeout;
    char b[40]; snprintf(b, sizeof(b), "r %02X %02X n%u", addr, tx ? tx[0] : 0, (unsigned)size); add(b);
    if (gReadFail) { if (rx) for (uint32_t i = 0; i < size; i++) rx[i] = 0x40; return LL_ERR; }
    if (rx) for (uint32_t i = 0; i < size; i++) rx[i] = tx ? gReadValue[tx[0] + i] : 0;
    return LL_OK;
}

int32_t I2C_ReadBurst(uint16_t addr, uint8_t reg, uint8_t rx[], uint32_t n, uint32_t timeout, uint8_t *stage) {
    (void)timeout;
    char b[40]; snprintf(b, sizeof(b), "r %02X %02X n%u", addr, reg, (unsigned)n); add(b);
    if (stage) *stage = gReadFail ? 5 : 0;
    if (gReadFail) return LL_ERR;
    for (uint32_t i = 0; i < n; i++) rx[i] = gReadValue[(uint8_t)(reg + i)];
    return LL_OK;
}

/* BSP / main.c symbols used by the ADV modules */
uint8_t led_state;
void WDT_Refresh(void) {}

/* link.c's non-ADV HAL: no replies, no update in these scenarios */
uint32_t halTimeUs(void) { return 0; }
uint32_t halReply(const uint8_t* f, uint8_t n, uint32_t a, uint32_t b) { (void)f; (void)n; (void)a; (void)b; return 0; }
void halInfo(AdvInfo* out) { memset(out, 0, sizeof(*out)); }
int halStagingErase(uint8_t s) { (void)s; return -1; }
int halStagingProgram(uint32_t o, const uint8_t* d, uint8_t l) { (void)o; (void)d; (void)l; return -1; }
const uint8_t* halStagingPtr(void) { static uint8_t z[1]; return z; }
void halCopyAndReset(uint32_t size) { (void)size; }
