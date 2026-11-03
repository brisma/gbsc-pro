/* CDC-ACM class for the console (fork of the XHSC 2022 single_cdc class): DTR from SET_CONTROL_LINE_STATE,
 * line coding stored and echoed only, one TX ring consumed after each IN transfer, an RX ring, no-op
 * isochronous callbacks, class requests with more than 8 data bytes stalled. */
#ifndef USB_CDC_H
#define USB_CDC_H
#include <stddef.h>
#include <stdint.h>
#include "usb_lib.h"
#include "cdc_ring.h"

extern usb_dev_class_func g_usbCdcClass;
extern CdcRing g_cdcTx, g_cdcRx;

void usbCdcInitRings(void);
int usbCdcDtr(void);
int usbCdcConfigured(void);
void usbCdcBusReset(void);    /* DTR and configured cleared */
#endif
