/**
 *******************************************************************************
 * @file  iap/iap_ymodem_boot/source/flash.c
 * @brief This file provides firmware functions to manage the Flash driver.
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

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "flash.h"
#include "main.h"
/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
__ALIGN_BEGIN  uint8_t u8_buf[256];
/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  检查地址是否对齐。
 * @param  u32Addr                      Flash address
 * @retval An en_result_t enumeration value:
 *           - LL_OK: Address aligned
 *           - LL_ERR: Address unaligned
 */
int32_t FLASH_CheckAddrAlign(uint32_t u32Addr)
{
    uint32_t u32Step = FLASH_SECTOR_SIZE;

    if (VECT_TAB_STEP > FLASH_SECTOR_SIZE) {
        u32Step = VECT_TAB_STEP;
    }
    if ((u32Addr % u32Step) != 0UL) {
        return LL_ERR;
    }

    return LL_OK;
}

/**
 * @brief  擦除闪存扇区.
 * @param  u32Addr                      闪存地址
 * @param  u32Size                      固件大小（0：当前地址扇区）
 * @retval An en_result_t enumeration value:
 *           - LL_OK: Erase succeeded
 *           - LL_ERR: Erase timeout
 *           - LL_ERR_INVD_PARAM: The parameters is invalid.
 */
int32_t FLASH_EraseSector(uint32_t u32Addr, uint32_t u32Size)
{
    uint32_t i;
    uint32_t u32PageNum;

    if (u32Addr >= (FLASH_BASE + FLASH_SIZE)) //判断是否越界
    {
        return LL_ERR_INVD_PARAM;
    }

    if (u32Size == 0U)   //长度为0 则擦除该地址所在扇区
    {   
        //进入这里
        return EFM_SectorErase(u32Addr);
    } 
    else 
    {
        u32PageNum = u32Size / FLASH_SECTOR_SIZE;
        if ((u32Size % FLASH_SECTOR_SIZE) != 0UL) 
        {
            u32PageNum += 1U;
        }
        for (i = 0; i < u32PageNum; i++) 
        {
            if (LL_OK != EFM_SectorErase(u32Addr + (i * FLASH_SECTOR_SIZE))) 
            {
                return LL_ERR;
            }
        }
    }

    return LL_OK;
}

/**
 * @brief  Write data to flash.
 * @param  u32Addr                      Flash address
 * @param  pu8Buff                      Pointer to the buffer to be written
 * @param  u32Len                       Buffer length
 * @retval int32_t:
 *           - LL_OK: Program successful.
 *           - LL_ERR_INVD_PARAM: The parameters is invalid.
 *           - LL_ERR_NOT_RDY: EFM if not ready.
 *           - LL_ERR_ADDR_ALIGN: Address alignment error
 */
int32_t FLASH_WriteData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len)
{   
    // __disable_irq;
    if ((pu8Buff == NULL) || (u32Len == 0U) || ((u32Addr + u32Len) > (FLASH_BASE + FLASH_SIZE))) 
    {
        return LL_ERR_INVD_PARAM;
    }
    if (0UL != (u32Addr % 4U)) 
    {
        return LL_ERR_ADDR_ALIGN;
    }
    // __enable_irq;
    return EFM_Program(u32Addr, pu8Buff, u32Len);
    
}

/**
 * @brief  Read data from flash.
 * @param  u32Addr                      Flash address
 * @param  pu8Buff                      Pointer to the buffer to be reading
 * @param  u32Len                       Buffer length
 * @retval int32_t:
 *           - LL_OK: Read data succeeded
 *           - LL_ERR_INVD_PARAM: The parameters is invalid
 *           - LL_ERR_ADDR_ALIGN: Address alignment error
 */
int32_t FLASH_ReadData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len)
{   
    // __disable_irq;
    uint32_t i;
    uint32_t u32WordLength, u8ByteRemain;
    uint32_t *pu32ReadBuff;
    __IO uint32_t *pu32FlashAddr;
    uint8_t  *pu8Byte;
    __IO uint8_t  *pu8FlashAddr;

    if ((pu8Buff == NULL) || (u32Len == 0U) || ((u32Addr + u32Len) > (FLASH_BASE + FLASH_SIZE))) 
    {
        return LL_ERR_INVD_PARAM;
    }
    if (0UL != (u32Addr % 4U)) 
    {
        return LL_ERR_ADDR_ALIGN;
    }

    pu32ReadBuff  = (uint32_t *)(uint32_t)pu8Buff;
    pu32FlashAddr = (uint32_t *)u32Addr;
    u32WordLength = u32Len / 4U;
    u8ByteRemain  = u32Len % 4U;
    /* Read data */
    for (i = 0UL; i < u32WordLength; i++) 
    {
        *(pu32ReadBuff++) = *(pu32FlashAddr++);
    }
    if (0UL != u8ByteRemain) 
    {
        pu8Byte      = (uint8_t *)pu32ReadBuff;
        pu8FlashAddr = (uint8_t *)pu32FlashAddr;
        for (i = 0UL; i < u8ByteRemain; i++) 
        {
            *(pu8Byte++) = *(pu8FlashAddr++);
        }
    }
    // __enable_irq;
    return LL_OK;
}

//void video_read(uint8_t state )
//{   
//    
//    FLASH_ReadData(EFM_BASE+FLASH_LEAF_ADDR(USER), (uint8_t *)u8_buf, 64);
//    
//    if((u8_buf[0]) > (0xF0)     ||   u8_buf[0] != 0xbc)    //读取失败 ，Flash返回0XFF   
//    {   
//        return;
//    }
//    else 
//    {
//        adv_input  =    u8_buf[1];
//        adv_double =    u8_buf[2];
//        adv_smooth =    u8_buf[3];
//        adv_ace    =    u8_buf[4];
//        adv_sw     =    u8_buf[5];
//        adv_tv     =    u8_buf[6];
//        
//        asw_01     =    u8_buf[7]   ;
//        asw_02     =    u8_buf[8]   ;
//        asw_03     =    u8_buf[9]   ;
//        asw_04     =    u8_buf[10]  ;
//        
//        AVsw       =    u8_buf[11]  ;
//        Input_signal =  u8_buf[48]  ;
//        ASC(asw_01,asw_02,asw_03,asw_04,state);
//    }
//}   
void Video_ReadNot2(uint8_t state )
{   
    
    FLASH_ReadData(EFM_BASE+FLASH_LEAF_ADDR(USER), (uint8_t *)u8_buf, 64);
    
    if((u8_buf[0]) > (0xF0)     ||   u8_buf[0] != 0xbc)    //读取失败 ，Flash返回0XFF   
    {   
        return;
    }
    else 
    {   
        if(u8_buf[1] >= 2)
            adv_input  = false;
        else
            adv_input  =    u8_buf[1];
        
        if(u8_buf[2] >= 2)
            adv_double = false;
        else
            adv_double =    u8_buf[2];
        
//        adv_double = true;
        
        if(u8_buf[3] >= 2)
            adv_smooth = false;
        else
            adv_smooth =    u8_buf[3];

        if(u8_buf[4] >= 2)
            adv_ace = false;
        else
            adv_ace    =    u8_buf[4];
        
        if(u8_buf[5] >= 2)
            adv_sw = false;
        else
            adv_sw     =    u8_buf[5];
        
        if(u8_buf[6] >= 0xf5)
            adv_tv = false;
        else
            adv_tv     =    u8_buf[6];
        
        if(u8_buf[7] >= 2)
            asw_01 = false;
        else
            asw_01     =    u8_buf[7]   ;
        
//        asw_02     =    u8_buf[8] ;
        
        if(u8_buf[9] >= 2)
            asw_03 = false;
        else
            asw_03     =    u8_buf[9]   ;
        
        if(u8_buf[10] >= 2)
            asw_04 = false;
        else
            asw_04     =    u8_buf[10]  ;
        

        Bright = (u8_buf[12] == 0xff) ? 0x00 : u8_buf[12]; 
        Contrast = (u8_buf[13] == 0xff) ? 0x80 : u8_buf[13];
        Saturation = (u8_buf[14] == 0xff) ? 0x80 : u8_buf[14];


        Input_signal =  u8_buf[48]  ;
        ASC_Not02(asw_01,asw_02,asw_03,asw_04,state);
    }
}  
uint8_t Read_ASW2(void)
{   
    u8_buf[8] = 0;
    FLASH_ReadData(EFM_BASE+FLASH_LEAF_ADDR(USER), (uint8_t *)u8_buf, 64);
    if((u8_buf[0]) > (0xF0)     ||   u8_buf[0] != 0xbc)    //读取失败 ，Flash返回0XFF   
    {   
        return 0;
    }
    else 
    {
        asw_02     =    u8_buf[8];
        return asw_02;
    }
}

uint8_t Read_AVSW(void)
{   
    u8_buf[11] = 0;
    FLASH_ReadData(EFM_BASE+FLASH_LEAF_ADDR(USER), (uint8_t *)u8_buf, 64);
    if((u8_buf[0]) > (0xF0)     ||   u8_buf[0] != 0xbc)    //读取失败 ，Flash返回0XFF   
    {   
        return 0;
    }
    else 
    {
        AVsw     =    u8_buf[11]   ;
        return AVsw;
    }
}

void mem_settings(void)
{   
   FLASH_EraseSector(EFM_BASE+FLASH_LEAF_ADDR(USER),FLASH_SECTOR_SIZE);  //清除扇区
    
   u8_buf[0] =     0xbc;
   u8_buf[1] =     adv_input  ;
   u8_buf[2] =     adv_double ;
   u8_buf[3] =     adv_smooth ;
   u8_buf[4] =     adv_ace    ;
   u8_buf[5] =     adv_sw     ;
   u8_buf[6] =     adv_tv     ;
    
   u8_buf[7] =     asw_01     ;
   u8_buf[8] =     asw_02     ;
   u8_buf[9] =     asw_03     ;
   u8_buf[10] =    asw_04     ;
   u8_buf[11] =    AVsw       ;
   u8_buf[12] =    Bright     ;
   u8_buf[13] =    Contrast   ;
   u8_buf[14] =    Saturation ;

   u8_buf[48] =   Input_signal;
   FLASH_WriteData(EFM_BASE+FLASH_LEAF_ADDR(USER), (uint8_t *)u8_buf, 64);
   printf(" save adv_tv: 0x%02x \n",adv_tv);
   printf("Bright    : 0x%02x\n", (unsigned int)Bright);
    printf("Contrast  : 0x%02x\n", (unsigned int)Contrast);
    printf("Saturation: 0x%02x\n", (unsigned int)Saturation);
}












/******************************************************************************
 * EOF (not truncated)
 *****************************************************************************/
