/**
 *******************************************************************************
 * @file  iap/iap_ymodem_boot/source/flash.h
 * @brief This file contains all the functions prototypes of the Flash driver.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2023, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#ifndef __FLASH_H__
#define __FLASH_H__

/* C binding of definitions if building with C++ compiler */
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ll_efm.h"
#include "hc32_ll_fcg.h"
#include "hc32_ll_gpio.h"
#include "main.h"
/*******************************************************************************
 * Global type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Global pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* Flash definitions */
#define FLASH_BASE                      (EFM_START_ADDR)
#define FLASH_SIZE                      (EFM_END_ADDR + 1U)
#define FLASH_SECTOR_SIZE               (SECTOR_SIZE)
#define FLASH_SECTOR_NUM                (64U)

/* SRAM definitions */
#define SRAM_SIZE                       (0x020000UL)
/* Vector table */
#define VECT_TAB_STEP                   (0x400UL)

/*******************************************************************************
 * Global variable definitions ('extern')
 ******************************************************************************/

/*******************************************************************************
  Global function prototypes (definition in C source)
 ******************************************************************************/
int32_t FLASH_CheckAddrAlign(uint32_t u32Addr);
int32_t FLASH_EraseSector(uint32_t u32Addr, uint32_t u32Size);
int32_t FLASH_WriteData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len);
int32_t FLASH_ReadData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len);



void mem_settings(void);

void video_read(uint8_t state );
uint8_t Read_ASW2(void);
uint8_t Read_AVSW(void);
void Video_ReadNot2(uint8_t state );
#ifdef __cplusplus
}
#endif

#endif /* __FLASH_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
