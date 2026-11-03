/* USB device configuration for the console (XHSC 2022 device core). Included from hc32f4xx_conf.h so that
 * hc32_ll_usb.c sees it. Device mode only (never USE_HOST_MODE); VBUS sensing off (GVBUSCFG forced valid). */
#ifndef USB_APP_CONF_H
#define USB_APP_CONF_H

#define USB_FS_MODE
#define USE_DEVICE_MODE

#define CDC_IN_EP               (0x82U)
#define CDC_OUT_EP              (0x01U)
#define CDC_CMD_EP              (0x85U)

#define RX_FIFO_FS_SIZE         (128U)
#define TX0_FIFO_FS_SIZE        (64U)
#define TX1_FIFO_FS_SIZE        (0U)
#define TX2_FIFO_FS_SIZE        (32U)
#define TX3_FIFO_FS_SIZE        (0U)
#define TX4_FIFO_FS_SIZE        (0U)
#define TX5_FIFO_FS_SIZE        (32U)

#define DEV_MAX_CFG_NUM         (1U)
#define USBD_ITF_MAX_NUM        (1U)
#define USB_MAX_STR_DESC_SIZ    (128U)

#define MAX_CDC_PACKET_SIZE     (64U)
#define CDC_CMD_PACKET_SIZE     (8U)
#define CDC_IN_FRAME_INTERVAL   (5U)
#define APP_RX_DATA_SIZE        (1024U)

#endif
