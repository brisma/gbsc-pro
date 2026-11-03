#ifndef __HC32F4XX_CONF_H__
#define __HC32F4XX_CONF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define LL_ICG_ENABLE     (DDL_OFF)
#define LL_UTILITY_ENABLE (DDL_ON)
#define LL_PRINT_ENABLE   (DDL_ON)

#define LL_ADC_ENABLE              (DDL_OFF)
#define LL_AES_ENABLE              (DDL_OFF)
#define LL_AOS_ENABLE              (DDL_ON)
#define LL_CAN_ENABLE              (DDL_OFF)
#define LL_CLK_ENABLE              (DDL_ON)
#define LL_CMP_ENABLE              (DDL_OFF)
#define LL_CRC_ENABLE              (DDL_OFF)
#define LL_DBGC_ENABLE             (DDL_OFF)
#define LL_DCU_ENABLE              (DDL_OFF)
#define LL_DMA_ENABLE              (DDL_ON)
#define LL_EFM_ENABLE              (DDL_ON)
#define LL_EMB_ENABLE              (DDL_OFF)
#define LL_EVENT_PORT_ENABLE       (DDL_OFF)
#define LL_FCG_ENABLE              (DDL_ON)
#define LL_FCM_ENABLE              (DDL_ON)
#define LL_GPIO_ENABLE             (DDL_ON)
#define LL_HASH_ENABLE             (DDL_OFF)
#define LL_I2C_ENABLE              (DDL_ON)
#define LL_I2S_ENABLE              (DDL_OFF)
#define LL_INTERRUPTS_ENABLE       (DDL_ON)
#define LL_INTERRUPTS_SHARE_ENABLE (DDL_OFF)
#define LL_KEYSCAN_ENABLE          (DDL_ON)
#define LL_MPU_ENABLE              (DDL_OFF)
#define LL_OTS_ENABLE              (DDL_OFF)
#define LL_PWC_ENABLE              (DDL_ON)
#define LL_QSPI_ENABLE             (DDL_OFF)
#define LL_RMU_ENABLE              (DDL_OFF)
#define LL_RTC_ENABLE              (DDL_OFF)
#define LL_SDIOC_ENABLE            (DDL_OFF)
#define LL_SPI_ENABLE              (DDL_OFF)
#define LL_SRAM_ENABLE             (DDL_ON)
#define LL_SWDT_ENABLE             (DDL_OFF)
#define LL_TMR0_ENABLE             (DDL_ON)
#define LL_TMR4_ENABLE             (DDL_OFF)
#define LL_TMR6_ENABLE             (DDL_OFF)
#define LL_TMRA_ENABLE             (DDL_OFF)
#define LL_TRNG_ENABLE             (DDL_OFF)
#define LL_USART_ENABLE            (DDL_ON)
#define LL_USB_ENABLE              (DDL_ON)
#define LL_WDT_ENABLE              (DDL_ON)

#define BSP_EV_HC32F460_LQFP100_V2 (4U)
#define BSP_EV_HC32F4XX            (BSP_EV_HC32F460_LQFP100_V2)

#define BSP_24CXX_ENABLE  (DDL_OFF)
#define BSP_W25QXX_ENABLE (DDL_OFF)
#define BSP_WM8731_ENABLE (DDL_OFF)

#define IAP_BOOT_SIZE   (0x10000UL)
#define IAP_APP_ADDR    (EFM_BASE + IAP_BOOT_SIZE)
#define VECT_TAB_OFFSET (EFM_BASE + IAP_APP_ADDR)

#ifdef __cplusplus
}
#endif

#include "usb_app_conf.h"   /* USB console (hc32_ll_usb.c reads it through this header) */

#endif
