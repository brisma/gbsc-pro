/* USB CDC port of the console: USBCON_Init returns the console port when the USB core came up, NULL
 * otherwise (the console stays on USART3). USBCON_Poll feeds received bytes to the console and runs the
 * IRQ-storm guard. */
#ifndef USB_CONSOLE_H
#define USB_CONSOLE_H
#include <stdint.h>
#include "console.h"

const ConsolePort *USBCON_Init(void);
void USBCON_Poll(uint32_t nowMs);
int USBCON_Up(void);
uint32_t USBCON_IrqRate(void);        /* interrupts in the last second */
uint32_t USBCON_IrqMaxUs(void);       /* worst-case ISR time, µs */
uint32_t USBCON_Dropped(void);        /* TX bytes dropped (ring full) */
#endif
