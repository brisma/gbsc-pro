/* Fault handlers and watchdog for crash recovery */
#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include "main.h"

void WDT_Config(void);    /* timeout about 2.7 s (PCLK3 50 MHz / 8192, 16384 counts) */
void WDT_Refresh(void);

#endif /* FAULT_HANDLER_H */
