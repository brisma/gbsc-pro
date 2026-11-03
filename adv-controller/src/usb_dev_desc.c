/* Device and string descriptors (adapted from the XHSC usb_dev_cdc example, 2022 API) */
#include "usb_dev_desc.h"
#include "usb_dev_stdreq.h"
#include "hc32_ll.h"

static uint8_t *devDesc(uint16_t *length);
static uint8_t *langIdDesc(uint16_t *length);
static uint8_t *manufacturerStr(uint16_t *length);
static uint8_t *productStr(uint16_t *length);
static uint8_t *serialStr(uint16_t *length);
static uint8_t *configStr(uint16_t *length);
static uint8_t *interfaceStr(uint16_t *length);

usb_dev_desc_func g_usbConsoleDesc = {
    &devDesc, &langIdDesc, &manufacturerStr, &productStr, &serialStr, &configStr, &interfaceStr,
};

__USB_ALIGN_BEGIN static uint8_t sDevice[USB_SIZ_DEVICE_DESC] = {
    0x12, USB_DEVICE_DESCRIPTOR_TYPE, 0x00, 0x02,
    0x02, 0x00, 0x00,                          /* class CDC (communications), interfaces describe the rest */
    USB_MAX_EP0_SIZE,
    LOBYTE(USBCON_VID), HIBYTE(USBCON_VID), LOBYTE(USBCON_PID), HIBYTE(USBCON_PID),
    0x00, 0x03,                                /* bcdDevice 3.00 */
    MFC_STR_IDX, PRODUCT_STR_IDX, SERIAL_STR_IDX, DEV_MAX_CFG_NUM,
};

__USB_ALIGN_BEGIN static uint8_t sLangId[USB_SIZ_STRING_LANGID] = {
    USB_SIZ_STRING_LANGID, USB_DESC_TYPE_STRING, 0x09, 0x04,
};

__USB_ALIGN_BEGIN static uint8_t sStr[USB_MAX_STR_DESC_SIZ];

static uint8_t *devDesc(uint16_t *length) { *length = (uint16_t)sizeof(sDevice); return sDevice; }
static uint8_t *langIdDesc(uint16_t *length) { *length = (uint16_t)sizeof(sLangId); return sLangId; }
static uint8_t *strOf(const char *s, uint16_t *length) { usb_getstring((uint8_t *)s, sStr, length); return sStr; }
static uint8_t *manufacturerStr(uint16_t *length) { return strOf(USBCON_MANUFACTURER, length); }
static uint8_t *productStr(uint16_t *length) { return strOf(USBCON_PRODUCT, length); }
static uint8_t *configStr(uint16_t *length) { return strOf("Console", length); }
static uint8_t *interfaceStr(uint16_t *length) { return strOf("Console", length); }

static uint8_t *serialStr(uint16_t *length)
{
    static char hex[25];
    static const char d[] = "0123456789ABCDEF";
    uint32_t id[3] = {CM_EFM->UQID0, CM_EFM->UQID1, CM_EFM->UQID2};
    for (uint8_t i = 0; i < 12; i++) {
        uint8_t b = (uint8_t)(id[i / 4] >> (8U * (i % 4)));
        hex[2 * i] = d[b >> 4];
        hex[2 * i + 1] = d[b & 0x0F];
    }
    hex[24] = 0;
    return strOf(hex, length);
}
