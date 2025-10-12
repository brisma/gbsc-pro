/**
 *******************************************************************************
 * @file  ev_hc32f460_lqfp100_v2.c
 * @brief This file provides firmware functions for EV_HC32F460_LQFP100_V2 BSP
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2022-10-31       CDT             Add configuration of XTAL IO as analog function
   2023-09-30       CDT             Add API BSP_XTAL32_Init()
                                    Optimize function BSP_I2C_Init()
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
#include "ev_hc32f460_lqfp100_v2.h"
#include <string.h>

#include "main.h"

uint16_t g_u16_sys_timer;
uint16_t g_u16_key_timer;
uint16_t g_u16_mis_timer;
uint16_t g_u16_osd_timer;
uint16_t g_u16_rgbs_timer;

uint16_t g_u16_led_timer;
uint8_t led_state = 0;
uint8_t led_sw = 0;

/**
 * @defgroup EV_HC32F460_LQFP100_V2 EV_HC32F460_LQFP100_V2
 * @{
 */

/**
 * @defgroup EV_HC32F460_LQFP100_V2_BASE EV_HC32F460_LQFP100_V2 Base
 * @{
 */

#if (BSP_EV_HC32F460_LQFP100_V2 == BSP_EV_HC32F4XX)

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/**
 * @defgroup EV_HC32F460_LQFP100_V2_Local_Types EV_HC32F460_LQFP100_V2 Local Types
 * @{
 */
typedef struct
{
    uint8_t port;
    uint16_t pin;
} BSP_Port_Pin;

typedef struct
{
    uint8_t port;
    uint16_t pin;
    uint32_t ch;
    en_int_src_t int_src;
    IRQn_Type irq;
    func_ptr_t callback;
} BSP_KeyIn_Config;
/**
 * @}
 */

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
/**
 * @addtogroup EV_HC32F460_LQFP100_V2_Local_Functions
 * @{
 */

void TMR02_A_Config(void)
{
    stc_tmr0_init_t stcTmr0Init;
    stc_irq_signin_config_t stcIrqSignConfig;
    /* Enable timer0 and AOS clock */
    FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMR0_2, ENABLE);
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

    /* TIMER0 configuration */
    (void)TMR0_StructInit(&stcTmr0Init);
    stcTmr0Init.u32ClockSrc = TMR0_CLK_SRC_INTERN_CLK; // 时钟源采用内部低速振荡器
    stcTmr0Init.u32ClockDiv = TMR0_CLK_DIV1;
    stcTmr0Init.u32Func = TMR0_FUNC_CMP;
    stcTmr0Init.u16CompareValue = (uint16_t)TMR0_CMP_VALUE_10US;

    (void)TMR0_Init(CM_TMR0_2, TMR0_CH_A, &stcTmr0Init);
    DDL_DelayMS(1U);
    TMR0_HWStopCondCmd(CM_TMR0_2, TMR0_CH_A, ENABLE);
    DDL_DelayMS(1U);
    TMR0_IntCmd(CM_TMR0_2, TMR0_INT_CMP_A, ENABLE);
    DDL_DelayMS(1U);

    stcIrqSignConfig.enIntSrc = INT_SRC_TMR0_2_CMP_A;
    stcIrqSignConfig.enIRQn = INT007_IRQn;
    stcIrqSignConfig.pfnCallback = &TMR0_CHA_CompareIrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
}

static uint32_t timer0_CHB_count = 0;
void TMR0_CHA_CompareIrqCallback(void) // 10US
{
    static uint32_t u32TmrCnt = 0U;
    if (timer0_CHB_count % 100 == 0) // 1ms
    {
        u32TmrCnt++;
        //     if (u32TmrCnt >= 5U)
        //    {
        //        m_u8SpeedUpd = 1U;

        //        u32TmrCnt    = 0U;
        //    }
    }
    if (timer0_CHB_count % 1000 == 0) // 10ms
    {
        g_u16_sys_timer++;
        g_u16_key_timer++;
        g_u16_mis_timer++;
        g_u16_osd_timer++;

        /*led*/
        if ((led_sw & 0x01) && (led_sw & LED_ERR_RED)) // 错误提示灯
        {
            g_u16_led_timer++;
            BSP_LED_Sw(0x01);
            if (g_u16_led_timer >= LED_TIME)
            {
                g_u16_led_timer = 0;
                led_sw = 0;
            }
        }
        else if ((led_sw & 0x01) && (led_sw & LED_ERR_GREEN)) // 错误提示灯
        {
            g_u16_led_timer++;
            BSP_LED_Sw(0x02);
            if (g_u16_led_timer >= LED_TIME)
            {
                g_u16_led_timer = 0;
                led_sw = 0;
            }
        }
        else if ((led_sw & 0x01) && (led_sw & LED_ERR_BLUE)) // 错误提示灯
        {
            g_u16_led_timer++;
            BSP_LED_Sw(0x04);
            if (g_u16_led_timer >= LED_TIME)
            {
                g_u16_led_timer = 0;
                led_sw = 0;
            }
        }
        else if ((led_sw & 0x01) && (led_sw & LED_OK)) // OK提示灯
        {
            g_u16_led_timer++;
            BSP_LED_Sw(0x01 | 0x02 | 0x04);
            if (g_u16_led_timer >= LED_TIME)
            {
                g_u16_led_timer = 0;
                led_sw = 0;
            }
        }
        else if (led_sw & 0x01)
        {
            g_u16_led_timer++;
            BSP_LED_Sw(0x00);
            if (g_u16_led_timer >= 50)
            {
                g_u16_led_timer = 0;
                led_sw = 0;
            }
        }
        else
            BSP_LED_Sw(led_state); // led_state
    }
    timer0_CHB_count++;

    TMR0_ClearStatus(CM_TMR0_2, TMR0_FLAG_CMP_B);
}

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @defgroup EV_HC32F460_LQFP100_V2_Global_Functions EV_HC32F460_LQFP100_V2 Global Functions
 * @{
 */

#if (LL_I2C_ENABLE == DDL_ON)
/**
 * @brief  BSP I2C initialize
 * @param  [in] I2Cx                Pointer to I2C instance register base.
 *                                  This parameter can be a value of the following:
 *         @arg CM_I2Cx:            I2C instance register base.
 * @retval int32_t:
 *            - LL_OK:              Configure success
 *            - LL_ERR_INVD_PARAM:  Invalid parameter
 */
// int32_t BSP_I2C_Init(CM_I2C_TypeDef *I2Cx)
//{
//     int32_t i32Ret;
//     float32_t fErr;
//     stc_i2c_init_t stcI2cInit;
//     uint32_t I2cSrcClk;
//     uint32_t I2cClkDiv;
//     uint32_t I2cClkDivReg;

//    I2cSrcClk = I2C_SRC_CLK;
//    I2cClkDiv = I2cSrcClk / BSP_I2C_BAUDRATE / I2C_WIDTH_MAX_IMME;
//    for (I2cClkDivReg = I2C_CLK_DIV1; I2cClkDivReg <= I2C_CLK_DIV128; I2cClkDivReg++) {
//        if (I2cClkDiv < (1UL << I2cClkDivReg)) {
//            break;
//        }
//    }

//    I2C_DeInit(I2Cx);
//    (void)I2C_StructInit(&stcI2cInit);
//    stcI2cInit.u32Baudrate = BSP_I2C_BAUDRATE;
//    stcI2cInit.u32SclTime  = 250UL * I2cSrcClk / 1000000000UL;  /* SCL time is about 250nS in EVB board */
//    stcI2cInit.u32ClockDiv = I2cClkDivReg;
//    i32Ret = I2C_Init(I2Cx, &stcI2cInit, &fErr);

//    if (LL_OK == i32Ret) {
//        I2C_BusWaitCmd(I2Cx, ENABLE);
//        I2C_Cmd(I2Cx, ENABLE);
//    }

//    return i32Ret;
//}

/**
 * @brief  BSP I2C De-initialize
 * @param  [in] I2Cx                Pointer to I2C instance register base.
 *                                  This parameter can be a value of the following:
 *         @arg CM_I2Cx:            I2C instance register base.
 * @retval None
 */
// void BSP_I2C_DeInit(CM_I2C_TypeDef *I2Cx)
//{
//     I2C_DeInit(I2Cx);
// }

/**
 * @brief  BSP I2C write.
 * @param  [in] I2Cx                Pointer to I2C instance register base.
 *                                  This parameter can be a value of the following:
 *         @arg CM_I2Cx:            I2C instance register base.
 * @param  [in] u16DevAddr:         Device address.
 * @param  [in] pu8Reg:             Pointer to the register address or memory address.
 * @param  [in] u8RegLen:           Length of register address or memory address.
 * @param  [in] pu8Buf:             The pointer to the buffer contains the data to be write.
 * @param  [in] u32Len:             Buffer size in byte.
 * @retval int32_t:
 *            - LL_OK:              Success
 *            - LL_ERR:             Receive NACK
 *            - LL_ERR_TIMEOUT:     Timeout
 *            - LL_ERR_INVD_PARAM:  pu8Buf is NULL
 */
// int32_t BSP_I2C_Write(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr, const uint8_t *pu8Reg, uint8_t u8RegLen, const uint8_t *pu8Buf, uint32_t u32Len)
//{
//     int32_t i32Ret;

//    I2C_SWResetCmd(I2Cx, ENABLE);
//    I2C_SWResetCmd(I2Cx, DISABLE);
//    i32Ret = I2C_Start(I2Cx, BSP_I2C_TIMEOUT);
//    if (LL_OK == i32Ret) {
//        i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_TX, BSP_I2C_TIMEOUT);

//        if (LL_OK == i32Ret) {
//            i32Ret = I2C_TransData(I2Cx, pu8Reg, u8RegLen, BSP_I2C_TIMEOUT);
//            if (LL_OK == i32Ret) {
//                i32Ret = I2C_TransData(I2Cx, pu8Buf, u32Len, BSP_I2C_TIMEOUT);
//            }
//        }
//    }
//    (void)I2C_Stop(I2Cx, BSP_I2C_TIMEOUT);
//    return i32Ret;
//}

/**
 * @brief  BSP I2C read.
 * @param  [in] I2Cx                Pointer to I2C instance register base.
 *                                  This parameter can be a value of the following:
 *         @arg CM_I2Cx:            I2C instance register base.
 * @param  [in] u16DevAddr:         Device address.
 * @param  [in] pu8Reg:             Pointer to the register address or memory address.
 * @param  [in] u8RegLen:           Length of register address or memory address.
 * @param  [in] pu8Buf:             The pointer to the buffer contains the data to be read.
 * @param  [in] u32Len:             Buffer size in byte.
 * @retval int32_t:
 *            - LL_OK:              Success
 *            - LL_ERR:             Receive NACK
 *            - LL_ERR_TIMEOUT:     Timeout
 *            - LL_ERR_INVD_PARAM:  pu8Buf is NULL
 */
// int32_t BSP_I2C_Read(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr, const uint8_t *pu8Reg, uint8_t u8RegLen, uint8_t *pu8Buf, uint32_t u32Len)
//{
//     int32_t i32Ret;
//     I2C_SWResetCmd(I2Cx, ENABLE);
//     I2C_SWResetCmd(I2Cx, DISABLE);
//     i32Ret = I2C_Start(I2Cx, BSP_I2C_TIMEOUT);
//     if (LL_OK == i32Ret) {
//         i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_TX, BSP_I2C_TIMEOUT);

//        if (LL_OK == i32Ret) {
//            i32Ret = I2C_TransData(I2Cx, pu8Reg, u8RegLen, BSP_I2C_TIMEOUT);
//            if (LL_OK == i32Ret) {
//                i32Ret = I2C_Restart(I2Cx, BSP_I2C_TIMEOUT);
//                if (LL_OK == i32Ret) {
//                    if (1UL == u32Len) {
//                        I2C_AckConfig(I2Cx, I2C_NACK);
//                    }

//                    i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_RX, BSP_I2C_TIMEOUT);
//                    if (LL_OK == i32Ret) {
//                        i32Ret = I2C_MasterReceiveDataAndStop(I2Cx, pu8Buf, u32Len, BSP_I2C_TIMEOUT);
//                    }
//                    I2C_AckConfig(I2Cx, I2C_ACK);
//                }
//            }
//        }
//    }

//    if (LL_OK != i32Ret) {
//        (void)I2C_Stop(I2Cx, BSP_I2C_TIMEOUT);
//    }

//    return i32Ret;
//}

/**
 * @brief  BSP 24CXX status get.
 * @param  [in] I2Cx                Pointer to I2C instance register base.
 *                                  This parameter can be a value of the following:
 *         @arg CM_I2Cx:            I2C instance register base.
 * @param  [in] u16DevAddr:         Device address.
 * @retval int32_t:
 *            - LL_OK:              Idle
 *            - LL_ERR:             Receive NACK
 *            - LL_ERR_TIMEOUT:     Timeout
 *            - LL_ERR_INVD_PARAM:  pu8Buf is NULL
 */
// int32_t BSP_I2C_GetDevStatus(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr)
//{
//     int32_t i32Ret;

//    i32Ret = I2C_Start(I2Cx, BSP_I2C_TIMEOUT);
//    if (LL_OK == i32Ret) {
//        i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_TX, BSP_I2C_TIMEOUT);

//        if (LL_OK == i32Ret) {
//            if (SET == I2C_GetStatus(I2Cx, I2C_FLAG_ACKR)) {
//                i32Ret = LL_ERR;
//            }
//        }
//    }
//    (void)I2C_Stop(I2Cx, BSP_I2C_TIMEOUT);
//    return i32Ret;
//}
#endif /* LL_I2C_ENABLE */

/**
 * @brief  BSP clock initialize.
 *         Set board system clock to MPLL@200MHz
 * @param  None
 * @retval None
 */
__WEAKDEF void BSP_CLK_Init(void)
{
    stc_clock_xtal_init_t stcXtalInit;
    stc_clock_pll_init_t stcMpllInit;

    GPIO_AnalogCmd(BSP_XTAL_PORT, BSP_XTAL_IN_PIN | BSP_XTAL_OUT_PIN, ENABLE);
    (void)CLK_XtalStructInit(&stcXtalInit);
    (void)CLK_PLLStructInit(&stcMpllInit);

    /* Set bus clk div. */
    CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 |
                                      CLK_PCLK2_DIV4 | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2));

    /* Config Xtal and enable Xtal */
    stcXtalInit.u8Mode = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8State = CLK_XTAL_ON;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;
    (void)CLK_XtalInit(&stcXtalInit);

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M). */
    stcMpllInit.PLLCFGR = 0UL;
    stcMpllInit.PLLCFGR_f.PLLM = 1UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLN = 50UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLP = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLQ = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLR = 2UL - 1UL;
    stcMpllInit.u8PLLState = CLK_PLL_ON;
    stcMpllInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&stcMpllInit);
    /* Wait MPLL ready. */
    while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL))
    {
        ;
    }

    /* sram init include read/write wait cycle setting */
    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM12 | SRAM_SRAM3 | SRAM_SRAMR), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);

    /* flash read wait cycle setting */
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    /* 3 cycles for 126MHz ~ 200MHz */
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);
    /* Switch driver ability */
    (void)PWC_HighSpeedToHighPerformance();
    /* Switch system clock source to MPLL. */
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
}

/**
 * @brief  BSP Xtal32 initialize.
 * @param  None
 * @retval int32_t:
 *         - LL_OK: XTAL32 enable successfully
 *         - LL_ERR_TIMEOUT: XTAL32 enable timeout.
 */
__WEAKDEF int32_t BSP_XTAL32_Init(void)
{
    stc_clock_xtal32_init_t stcXtal32Init;
    stc_fcm_init_t stcFcmInit;
    uint32_t u32TimeOut = 0UL;
    uint32_t u32Time = HCLK_VALUE / 5UL;

    if (CLK_XTAL32_ON == READ_REG8(CM_CMU->XTAL32CR))
    {
        /* Disable xtal32 */
        (void)CLK_Xtal32Cmd(DISABLE);
        /* Wait 5 * xtal32 cycle */
        DDL_DelayUS(160U);
    }

    /* Xtal32 config */
    (void)CLK_Xtal32StructInit(&stcXtal32Init);
    stcXtal32Init.u8State = CLK_XTAL32_ON;
    stcXtal32Init.u8Drv = CLK_XTAL32_DRV_MID;
    stcXtal32Init.u8Filter = CLK_XTAL32_FILTER_ALL_MD;
    GPIO_AnalogCmd(BSP_XTAL32_PORT, BSP_XTAL32_IN_PIN | BSP_XTAL32_OUT_PIN, ENABLE);
    (void)CLK_Xtal32Init(&stcXtal32Init);

    /* FCM config */
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_FCM, ENABLE);
    (void)FCM_StructInit(&stcFcmInit);
    stcFcmInit.u32RefClock = FCM_REF_CLK_MRC;
    stcFcmInit.u32RefClockDiv = FCM_REF_CLK_DIV8192;
    stcFcmInit.u32RefClockEdge = FCM_REF_CLK_RISING;
    stcFcmInit.u32TargetClock = FCM_TARGET_CLK_XTAL32;
    stcFcmInit.u32TargetClockDiv = FCM_TARGET_CLK_DIV1;
    stcFcmInit.u16LowerLimit = (uint16_t)((XTAL32_VALUE / (MRC_VALUE / 8192U)) * 96UL / 100UL);
    stcFcmInit.u16UpperLimit = (uint16_t)((XTAL32_VALUE / (MRC_VALUE / 8192U)) * 104UL / 100UL);
    (void)FCM_Init(&stcFcmInit);
    /* Enable FCM, to ensure xtal32 stable */
    FCM_Cmd(ENABLE);
    for (;;)
    {
        if (SET == FCM_GetStatus(FCM_FLAG_END))
        {
            FCM_ClearStatus(FCM_FLAG_END);
            if ((SET == FCM_GetStatus(FCM_FLAG_ERR)) || (SET == FCM_GetStatus(FCM_FLAG_OVF)))
            {
                FCM_ClearStatus(FCM_FLAG_ERR | FCM_FLAG_OVF);
            }
            else
            {
                (void)FCM_DeInit();
                FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_FCM, DISABLE);
                return LL_OK;
            }
        }
        u32TimeOut++;
        if (u32TimeOut > u32Time)
        {
            (void)FCM_DeInit();
            FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_FCM, DISABLE);
            return LL_ERR_TIMEOUT;
        }
    }
}

/**
 * @brief  BSP key initialize
 * @param  None
 * @retval None
 */

/**
 * @brief  Get BSP key status
 * @param  [in] u32Key chose one macro from below
 *   @arg  BSP_KEY_1
 *   @arg  BSP_KEY_2
 *   @arg  BSP_KEY_3
 *   @arg  BSP_KEY_4
 *   @arg  BSP_KEY_5
 *   @arg  BSP_KEY_6
 *   @arg  BSP_KEY_7
 *   @arg  BSP_KEY_8
 *   @arg  BSP_KEY_9
 *   @arg  BSP_KEY_10
 * @retval An @ref en_flag_status_t enumeration type value.
 */

/**
 * @brief  LED initialize.
 * @param  None
 * @retval None
 */
void BSP_LED_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    /* configuration structure initialization */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    /* Initialize LED pin */

    (void)GPIO_Init(BSP_LED_RED_PORT, BSP_LED_RED_PIN, &stcGpioInit);
    (void)GPIO_Init(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN, &stcGpioInit);
    (void)GPIO_Init(BSP_LED_BLUE_PORT, BSP_LED_BLUE_PIN, &stcGpioInit);
}

void BSP_LED_Sw(uint8_t u8Led) // 0x01->red 0x02->green 0x04->blue
{
    if (u8Led & 0x01)
        GPIO_ResetPins(BSP_LED_RED_PORT, BSP_LED_RED_PIN);
    else
        GPIO_SetPins(BSP_LED_RED_PORT, BSP_LED_RED_PIN);

    if (u8Led & 0x02)
        GPIO_ResetPins(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);
    else
        GPIO_SetPins(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);

    if (u8Led & 0x04)
        GPIO_ResetPins(BSP_LED_BLUE_PORT, BSP_LED_BLUE_PIN);
    else
        GPIO_SetPins(BSP_LED_BLUE_PORT, BSP_LED_BLUE_PIN);
}

void BSP_LED_Off(uint8_t u8Led)
{
    if (u8Led & 0x01)
        GPIO_SetPins(BSP_LED_RED_PORT, BSP_LED_RED_PIN);
    if (u8Led & 0x02)
        GPIO_SetPins(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);
    if (u8Led & 0x04)
        GPIO_SetPins(BSP_LED_BLUE_PORT, BSP_LED_BLUE_PIN);
}

void BSP_LED_On(uint8_t u8Led)
{
    if (u8Led & 0x01)
        GPIO_ResetPins(BSP_LED_RED_PORT, BSP_LED_RED_PIN);
    if (u8Led & 0x02)
        GPIO_ResetPins(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);
    if (u8Led & 0x04)
        GPIO_ResetPins(BSP_LED_BLUE_PORT, BSP_LED_BLUE_PIN);
}

void BSP_LED_Toggle(uint8_t u8Led)
{
    if (u8Led & 0x01)
        GPIO_TogglePins(BSP_LED_RED_PORT, BSP_LED_RED_PIN);
    if (u8Led & 0x02)
        GPIO_TogglePins(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);
    if (u8Led & 0x04)
        GPIO_TogglePins(BSP_LED_BLUE_PORT, BSP_LED_BLUE_PIN);
}

void C_LED_OK(void)
{
    led_sw = 0X01;
    led_sw |= LED_OK;
}
void C_LED_ERR_RED(void)
{
    led_sw = 0X01;
    led_sw |= LED_ERR_RED;
}

void C_LED_ERR_GREEN(void)
{
    led_sw = 0X01;
    led_sw |= LED_ERR_GREEN;
}

void C_LED_ERR_BLUE(void)
{
    led_sw = 0X01;
    led_sw |= LED_ERR_BLUE;
}

#if (LL_PRINT_ENABLE == DDL_ON)
/**
 * @brief  BSP printf device, clock and port pre-initialize.
 * @param  [in] vpDevice                Pointer to print device
 * @param  [in] u32Baudrate             Print device communication baudrate
 * @retval int32_t:
 *           - LL_OK:                   Initialize successfully.
 *           - LL_ERR:                  Initialize unsuccessfully.
 *           - LL_ERR_INVD_PARAM:       The u32Baudrate value is 0.
 */
int32_t BSP_PRINTF_Preinit(void *vpDevice, uint32_t u32Baudrate)
{
    uint32_t u32Div;
    float32_t f32Error;
    stc_usart_uart_init_t stcUartInit;
    int32_t i32Ret = LL_ERR_INVD_PARAM;

    (void)vpDevice;

    if (0UL != u32Baudrate)
    {
        /* Set TX port function */
        GPIO_SetFunc(BSP_PRINTF_PORT, BSP_PRINTF_PIN, BSP_PRINTF_PORT_FUNC);

        /* Enable clock  */
        FCG_Fcg1PeriphClockCmd(BSP_PRINTF_DEVICE_FCG, ENABLE);

        /* Configure UART */
        (void)USART_UART_StructInit(&stcUartInit);
        stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
        (void)USART_UART_Init(BSP_PRINTF_DEVICE, &stcUartInit, NULL);

        for (u32Div = 0UL; u32Div <= USART_CLK_DIV64; u32Div++)
        {
            USART_SetClockDiv(BSP_PRINTF_DEVICE, u32Div);
            i32Ret = USART_SetBaudrate(BSP_PRINTF_DEVICE, u32Baudrate, &f32Error);
            if ((LL_OK == i32Ret) &&
                ((-BSP_PRINTF_BAUDRATE_ERR_MAX <= f32Error) && (f32Error <= BSP_PRINTF_BAUDRATE_ERR_MAX)))
            {
                USART_FuncCmd(BSP_PRINTF_DEVICE, USART_TX, ENABLE);
                break;
            }
            else
            {
                i32Ret = LL_ERR;
            }
        }
    }

    return i32Ret;
}
#endif

/**
 * @}
 */

#endif /* BSP_EV_HC32F460_LQFP100_V2 */
