#include "usb_cdc.h"
#include "usb_dev_driver.h"
#include "usb_dev_ctrleptrans.h"
#include "usb_dev_stdreq.h"
#include "usb_dev_desc.h"
#include <string.h>

#define CDC_CONFIG_SIZE   (75U)
#define CDC_DESC_TYPE     (0x21U)
#define SET_LINE_CODING   (0x20U)
#define GET_LINE_CODING   (0x21U)
#define SET_LINE_STATE    (0x22U)
#define NO_CMD            (0xFFU)
#define TX_MAX            (MAX_CDC_PACKET_SIZE - 1U)   /* never a full packet: no zero-length packet needed */

static uint8_t sTxMem[APP_RX_DATA_SIZE], sRxMem[256];
CdcRing g_cdcTx, g_cdcRx;

__USB_ALIGN_BEGIN static uint8_t sOut[MAX_CDC_PACKET_SIZE];
__USB_ALIGN_BEGIN static uint8_t sCmd[CDC_CMD_PACKET_SIZE];
__USB_ALIGN_BEGIN static uint8_t sLineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};   /* 115200 8N1 */
__USB_ALIGN_BEGIN static uint32_t sAlt;
static volatile uint8_t sDtr, sConfigured, sTxBusy;
static volatile size_t sInFlight;
static uint8_t sPendingCmd = NO_CMD;
static uint32_t sFrames;

static void cdcInit(void *pdev);
static void cdcDeinit(void *pdev);
static uint8_t cdcSetup(void *pdev, USB_SETUP_REQ *req);
static void cdcEp0RxReady(void *pdev);
static uint8_t *cdcConfigDesc(uint16_t *length);
static uint8_t cdcSof(void *pdev);
static void cdcDataIn(void *pdev, uint8_t epnum);
static void cdcDataOut(void *pdev, uint8_t epnum);
static void cdcIsoNone(void *pdev) { (void)pdev; }

usb_dev_class_func g_usbCdcClass = {
    &cdcInit, &cdcDeinit, &cdcSetup, NULL, &cdcEp0RxReady, &cdcConfigDesc, &cdcSof, &cdcDataIn, &cdcDataOut,
    &cdcIsoNone, &cdcIsoNone,
};

__USB_ALIGN_BEGIN static uint8_t sConfig[CDC_CONFIG_SIZE] = {
    0x09, USB_CFG_DESCRIPTOR_TYPE, CDC_CONFIG_SIZE, 0x00, 0x02, 0x01, 0x00, 0xC0, 0x32,
    /* interface association: CDC (2 interfaces) */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x04,
    /* interface 0: communication */
    0x09, USB_INTERFACE_DESCRIPTOR_TYPE, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,              /* header */
    0x05, 0x24, 0x01, 0x00, 0x01,              /* call management */
    0x04, 0x24, 0x02, 0x02,                    /* ACM: line coding + line state */
    0x05, 0x24, 0x06, 0x00, 0x01,              /* union */
    0x07, USB_ENDPOINT_DESCRIPTOR_TYPE, CDC_CMD_EP, 0x03, LOBYTE(CDC_CMD_PACKET_SIZE), HIBYTE(CDC_CMD_PACKET_SIZE), 0xFF,
    /* interface 1: data */
    0x09, USB_INTERFACE_DESCRIPTOR_TYPE, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, USB_ENDPOINT_DESCRIPTOR_TYPE, CDC_OUT_EP, 0x02, LOBYTE(MAX_CDC_PACKET_SIZE), HIBYTE(MAX_CDC_PACKET_SIZE), 0x00,
    0x07, USB_ENDPOINT_DESCRIPTOR_TYPE, CDC_IN_EP, 0x02, LOBYTE(MAX_CDC_PACKET_SIZE), HIBYTE(MAX_CDC_PACKET_SIZE), 0x00,
};

void usbCdcInitRings(void)
{
    cdcRingInit(&g_cdcTx, sTxMem, sizeof(sTxMem));
    cdcRingInit(&g_cdcRx, sRxMem, sizeof(sRxMem));
}

int usbCdcDtr(void) { return sDtr; }
int usbCdcConfigured(void) { return sConfigured; }
void usbCdcBusReset(void) { sDtr = 0; sConfigured = 0; sTxBusy = 0; sInFlight = 0; }

/* Sends the next contiguous block (ISR context); the bytes stay in the ring until the IN transfer ends */
static void startTx(void *pdev)
{
    const uint8_t *p;
    size_t n = cdcRingPeek(&g_cdcTx, &p, TX_MAX);
    if (!n) { sTxBusy = 0; return; }
    sTxBusy = 1;
    sInFlight = n;
    usb_deveptx(pdev, CDC_IN_EP, (uint8_t *)p, (uint32_t)n);
}

static void cdcInit(void *pdev)
{
    usb_opendevep(pdev, CDC_IN_EP, MAX_CDC_PACKET_SIZE, EP_TYPE_BULK);
    usb_opendevep(pdev, CDC_OUT_EP, MAX_CDC_PACKET_SIZE, EP_TYPE_BULK);
    usb_opendevep(pdev, CDC_CMD_EP, CDC_CMD_PACKET_SIZE, EP_TYPE_INTR);
    sTxBusy = 0;
    sInFlight = 0;
    sConfigured = 1;
    usb_readytorx(pdev, CDC_OUT_EP, sOut, MAX_CDC_PACKET_SIZE);
}

static void cdcDeinit(void *pdev)
{
    usb_shutdevep(pdev, CDC_IN_EP);
    usb_shutdevep(pdev, CDC_OUT_EP);
    usb_shutdevep(pdev, CDC_CMD_EP);
    usbCdcBusReset();
}

static uint8_t cdcSetup(void *pdev, USB_SETUP_REQ *req)
{
    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS:
            if (req->wLength > CDC_CMD_PACKET_SIZE) { usb_ctrlerr(pdev); return USB_DEV_FAIL; }
            if (req->wLength != 0U) {
                if ((req->bmRequest & 0x80U) != 0U) {             /* device to host */
                    if (req->bRequest == GET_LINE_CODING) memcpy(sCmd, sLineCoding, sizeof(sLineCoding));
                    else memset(sCmd, 0, sizeof(sCmd));
                    usb_ctrldatatx(pdev, sCmd, req->wLength);
                } else {
                    sPendingCmd = req->bRequest;
                    usb_ctrldatarx(pdev, sCmd, req->wLength);
                }
            } else if (req->bRequest == SET_LINE_STATE) {
                sDtr = (uint8_t)(req->wValue & 1U);
            }
            return USB_DEV_OK;
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest) {
                case USB_REQ_GET_DESCRIPTOR:
                    if ((req->wValue >> 8) == CDC_DESC_TYPE) {
                        const uint16_t off = 9U + (9U * USBD_ITF_MAX_NUM);   /* never past sConfig (vendor template) */
                        uint16_t len = (uint16_t)LL_MIN(CDC_CONFIG_SIZE - off, req->wLength);
                        usb_ctrldatatx(pdev, sConfig + off, len);
                    }
                    break;
                case USB_REQ_GET_INTERFACE:
                    usb_ctrldatatx(pdev, (uint8_t *)&sAlt, 1U);
                    break;
                case USB_REQ_SET_INTERFACE:
                    if ((uint8_t)req->wValue < USBD_ITF_MAX_NUM) sAlt = (uint8_t)req->wValue; else usb_ctrlerr(pdev);
                    break;
                default:
                    break;
            }
            return USB_DEV_OK;
        default:
            usb_ctrlerr(pdev);
            return USB_DEV_FAIL;
    }
}

static void cdcEp0RxReady(void *pdev)
{
    (void)pdev;
    if (sPendingCmd == SET_LINE_CODING) memcpy(sLineCoding, sCmd, sizeof(sLineCoding));   /* echoed back only */
    sPendingCmd = NO_CMD;
}

static uint8_t *cdcConfigDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(sConfig);
    return sConfig;
}

static uint8_t cdcSof(void *pdev)
{
    if (++sFrames >= CDC_IN_FRAME_INTERVAL) {
        sFrames = 0;
        if (!sTxBusy && sConfigured) startTx(pdev);
    }
    return USB_DEV_OK;
}

static void cdcDataIn(void *pdev, uint8_t epnum)
{
    (void)epnum;
    if (sInFlight) { cdcRingConsume(&g_cdcTx, sInFlight); sInFlight = 0; }
    startTx(pdev);
}

static void cdcDataOut(void *pdev, uint8_t epnum)
{
    uint32_t n = ((usb_core_instance *)pdev)->dev.out_ep[epnum].xfer_count;
    if (n > MAX_CDC_PACKET_SIZE) n = MAX_CDC_PACKET_SIZE;
    (void)cdcRingWrite(&g_cdcRx, sOut, n);
    usb_readytorx(pdev, CDC_OUT_EP, sOut, MAX_CDC_PACKET_SIZE);
}
