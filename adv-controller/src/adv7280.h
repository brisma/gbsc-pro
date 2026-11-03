/* ADV7280 decoder (+ the ADV7391 rows of the shared init sequence): register tables, init, input, format,
 * I2P/smooth, ACE, BCSH, filters and the periodic status read. The values come from the applied STATE
 * (ADV_LoadRegs); the I2C traffic is pinned by the golden traces (test/test_golden.c). */
#ifndef ADV7280_H
#define ADV7280_H
#include <stdint.h>
#include "link_hal.h"   /* AdvRegs */

#define VID_SEL_REG 0x02   /* video selection (format) */

void ADV_LoadRegs(const AdvRegs* r);   /* no I2C */
const AdvRegs* ADV_Regs(void);
int32_t ADV_Init(void);                /* power up + full init from the loaded values; LL_OK = every decoder write ACKed */
void ADV_Deinit(void);
int32_t ADV_SetInput(uint8_t yc);         /* 1 S-Video, 0 Composite; reapplies ACE and filters */
int32_t ADV_SetI2P(uint8_t enable);
int32_t ADV_SetSmooth(uint8_t smooth);
int32_t ADV_SetBCSH(void);
int32_t ADV_SetACE(uint8_t ace);
int32_t ADV_SetACEParams(void);
int32_t ADV_SetVideoFilters(void);
void ADV_DetectLoop(void);             /* every 50 ms: reads STATUS_3, STATUS_1, STATUS_2 on each call */

uint8_t ADV_Powered(void);             /* 1 after ADV_Init, 0 after ADV_Deinit */
uint8_t ADV_LastStatus1(void);         /* STATUS_1 (0x10) of the last read, 0 when off or not acked */
uint8_t ADV_LastAck(void);             /* the last status read was acknowledged */

#endif
