/* HC32 console glue: CONSOLE_HC32_Init after the peripherals (usb = NULL when USB is not up),
 * CONSOLE_HC32_Poll from the main loop after LINK_Poll */
#ifndef CONSOLE_HC32_H
#define CONSOLE_HC32_H
#include <stdint.h>
#include "console.h"
void CONSOLE_HC32_Init(const ConsolePort *usb);
void CONSOLE_HC32_Poll(uint32_t nowMs);
#endif
