#ifndef MAIN_H
#define MAIN_H

#include "hc32_ll.h"
#include "ev_hc32f460_lqfp100_v2_bsp.h"
#include "uart_dma.h"
#include "i2c.h"
#include "board.h"
#include "adv7280.h"
#include "adv7391.h"
#include "led.h"
#include "string.h"
#include "stdbool.h"

// Flash record of the previous firmware's settings: only used to erase it (main.c EraseLegacySettings)
#define USER                1
#define FLASH_LEAF_ADDR(x)  (uint32_t)(0x7a000U + (0x2000U * (x)))

extern uint8_t led_state;   /* BSP LED colour (ev_hc32f460_lqfp100_v2.c) */

#endif