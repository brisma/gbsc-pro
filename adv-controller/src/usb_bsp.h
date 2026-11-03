/* Board support for the XHSC 2022 USB device core (names fixed by the library) */
#ifndef USB_BSP_H
#define USB_BSP_H
#include "usb_lib.h"

int  usb_bsp_clock_start(void);               /* UPLL 48 MHz + USBFS clock, before usb_dev_init; 0 = failed */
void usb_bsp_init(usb_core_instance *pdev);   /* PA11/PA12, ISR timing (called by usb_dev_init) */
void usb_bsp_nvicconfig(void);                /* INT030, priority 15 */
void usb_udelay(const uint32_t usec);
void usb_mdelay(const uint32_t msec);

uint32_t usb_bsp_irq_count(void);             /* USB interrupts since boot */
uint32_t usb_bsp_irq_non_sof(void);           /* of which with a cause other than SOF (storm guard) */
uint32_t usb_bsp_irq_max_cycles(void);        /* worst-case ISR duration (DWT cycles, 200 MHz) */
extern usb_core_instance g_usbDev;
#endif
