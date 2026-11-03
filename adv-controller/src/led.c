#include "main.h"
#include "led.h"

uint8_t c_state = 0;
uint8_t err_flag = 0;
uint8_t Input_signal = 0;
static uint8_t sFreeRun = 0;

void LED_SetSignal(uint8_t signal)
{
    switch (signal) {
    case 1: led_state = LED_RED | LED_GREEN | LED_BLUE; break;   /* RGBs */
    case 2: led_state = LED_GREEN; break;                        /* RGsB */
    case 3: led_state = LED_BLUE; break;                         /* VGA */
    case 4: led_state = LED_RED | LED_GREEN; break;              /* YPbPr */
    case 5: led_state = LED_RED | LED_BLUE; break;               /* S-Video */
    case 6: led_state = LED_GREEN | LED_BLUE; break;             /* Composite */
    }
}

void LED_FreeRunCheck(uint8_t status1)
{
    if (((uint8_t)(status1 & 0x02) == 0x02) && !sFreeRun && err_flag) {           /* lost lock: free run */
        sFreeRun = 1;
        c_state = 2;
        err_flag = 0;
        led_state = LED_RED;
    } else if (sFreeRun && ((uint8_t)(status1 & 0x05) == 0x05) && err_flag) {     /* lock + fsc lock back */
        sFreeRun = 0;
        err_flag = 0;
        LED_SetSignal(Input_signal);
    }
}

void LED_ResetFreeRun(void) { sFreeRun = 0; }

#ifndef GOLDEN_HOST   /* the BSP colour helpers: device only */
void LED_Tick(void)
{
    static uint8_t last_signal = 0;

    switch (c_state) {
    case 1: C_LED_OK(); break;
    case 2: C_LED_ERR_RED(); break;
    case 3: C_LED_ERR_GREEN(); break;
    case 4: C_LED_ERR_BLUE(); break;
    }
    c_state = 0;
    if (Input_signal != last_signal) {
        LED_SetSignal(Input_signal);
        last_signal = Input_signal;
    }
}
#endif
