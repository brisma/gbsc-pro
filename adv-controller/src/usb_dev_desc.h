/* USB descriptors of the console: 1209:0001 (pid.codes test PID, development), class 0x02, serial = UID */
#ifndef USB_DEV_DESC_H
#define USB_DEV_DESC_H
#include "usb_lib.h"

#define USB_DEVICE_DESCRIPTOR_TYPE      (0x01U)
#define USB_CFG_DESCRIPTOR_TYPE         (0x02U)
#define USB_STRING_DESCRIPTOR_TYPE      (0x03U)
#define USB_INTERFACE_DESCRIPTOR_TYPE   (0x04U)
#define USB_ENDPOINT_DESCRIPTOR_TYPE    (0x05U)
#define USB_SIZ_DEVICE_DESC             (18U)
#define USB_SIZ_STRING_LANGID           (4U)

#define USBCON_VID                      (0x1209U)
#define USBCON_PID                      (0x0001U)
#define USBCON_PRODUCT                  "ADV Controller console"
#define USBCON_MANUFACTURER             "GBSC PRO"

extern usb_dev_desc_func g_usbConsoleDesc;
#endif
