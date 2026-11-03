/* Status LED: colour per input, red while the decoder runs free (lost lock), the c_state codes of the switch
 * and console paths */
#ifndef LED_H
#define LED_H
#include <stdint.h>

/* Blink codes of the BSP LED driver (ev_hc32f460_lqfp100_v2.c: led_sw, C_LED_OK/ERR_*) */
#define LED_ERR_RED   0x80
#define LED_ERR_GREEN 0x40
#define LED_ERR_BLUE  0x20
#define LED_OK        0x10
#define LED_TIME      35
extern uint8_t led_sw;

extern uint8_t c_state;        /* 1 ok, 2..4 error colours; consumed by LED_Tick */
extern uint8_t err_flag;       /* set every 500 ms by main: the free-run check runs at most that often */
extern uint8_t Input_signal;   /* 1..6 from the applied STATE */

void LED_SetSignal(uint8_t signal);       /* 1 RGBs, 2 RGsB, 3 VGA, 4 YPbPr, 5 S-Video, 6 Composite */
void LED_FreeRunCheck(uint8_t status1);   /* after each decoder status read (STATUS_1, reg 0x10) */
void LED_ResetFreeRun(void);
void LED_Tick(void);                      /* every 100 ms from main */

#endif
