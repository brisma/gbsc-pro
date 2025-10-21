/**
 *******************************************************************************
 * @file  usart/usart_uart_dma/source/main.c
 * @brief This example demonstrates UART data receive and transfer by DMA.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2022-10-31       CDT             Delete the redundant code
                                    Read USART_DR.RDR when USART overrun error occur.
   2023-01-15       CDT             Update UART timeout function calculating formula for Timer0 CMP value
   2023-09-30       CDT             Split register USART_DR to USART_RDR and USART_TDR
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
#include "main.h"

/**
 * @addtogroup HC32F460_DDL_Examples
 * @{
 */

/**
 * @addtogroup USART_UART_DMA
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* Peripheral register WE/WP selection */
#define LL_PERIPH_SEL (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                       LL_PERIPH_EFM | LL_PERIPH_SRAM)

extern uint16_t g_u16_sys_timer;
extern uint16_t g_u16_key_timer;
extern uint16_t g_u16_mis_timer;
extern uint16_t g_u16_osd_timer;

__IO uint8_t m_u8SpeedUpd = 0U;

// const uint8_t usFlashInitVal[4] __attribute__((at(0x00007FFC))) = {0x23, 0x01, 0x89, 0x67}; // ��λ��flash��
// const uint8_t usFlashInitVal[4] __attribute__((at(IAP_BOOT_SIZE - 4))) = {0x23, 0x01, 0x89, 0x67}; // ��λ��flash��

const uint8_t bright_atr[3] = {0x00, 0x7f, 0x80};
const uint8_t contrast_atr[3] = {0x00, 0x80, 0xff};
static uint8_t BrightCount = 0x00;
static uint8_t ContrastCount = 0xb0;
/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

static void vd_Button_Init(void)
{
    /* configuration structure initialization */ // ������ƶ˿�
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PullUp = PIN_PU_ON;         // ����
    stcGpioInit.u16PinDir = PIN_DIR_OUT;       // �������
    stcGpioInit.u16PinAttr = PIN_ATTR_DIGITAL; // ����
    //    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;   //comsǿ���
    //    stcGpioInit.u16ExtInt = PIN_EXTINT_OFF;   //��ʹ���ⲿ�ж�
    //    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;   //����

    stcGpioInit.u16PinState = PIN_STAT_RST; // ����

    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_01, &stcGpioInit); // OUTPUT_EN
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_00, &stcGpioInit); // POWER_DOWN_N
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_03, &stcGpioInit); // INPUT_RESET_N

    //////////////////Line
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PullUp = PIN_PU_ON;         // ����
    stcGpioInit.u16PinDir = PIN_DIR_IN;        // ���뷽��
    stcGpioInit.u16PinAttr = PIN_ATTR_DIGITAL; // ����
    stcGpioInit.u16PinState = PIN_STAT_SET;    // ����

    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_05, &stcGpioInit);

    ///////////////ASW/////////////////////
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PullUp = PIN_PU_OFF;        // ����
    stcGpioInit.u16PinDir = PIN_DIR_OUT;       // �������
    stcGpioInit.u16PinAttr = PIN_ATTR_DIGITAL; // ����
    stcGpioInit.u16PinState = PIN_STAT_RST;    // ����
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;

    (void)GPIO_Init(GPIO_PORT_ASW, GPIO_PIN_ASW1, &stcGpioInit); // ASW01
    (void)GPIO_Init(GPIO_PORT_ASW, GPIO_PIN_ASW3, &stcGpioInit); // ASW03
    (void)GPIO_Init(GPIO_PORT_ASW, GPIO_PIN_ASW4, &stcGpioInit); // ASW04

    asw_02 = Read_ASW2();
    //    stcGpioInit.u16PullUp = PIN_PU_OFF;         // ����
    if (asw_02)
    {
        stcGpioInit.u16PinState = PIN_STAT_SET; // ����
    }
    else
    {
        stcGpioInit.u16PinState = PIN_STAT_RST; // ����
    }
    (void)GPIO_Init(GPIO_PORT_ASW, GPIO_PIN_ASW2, &stcGpioInit); // ASW02

    Video_ReadNot2(1);

    AVsw = Read_AVSW();
    if (AVsw)
    {
        stcGpioInit.u16PullUp = PIN_PU_OFF;     // ������
        stcGpioInit.u16PinState = PIN_STAT_RST; // ����
                                                // led_state = LED_ALL;
    }
    else
    {
        stcGpioInit.u16PullUp = PIN_PU_ON;      // ����
        stcGpioInit.u16PinState = PIN_STAT_SET; // ����
                                                // led_state = LED_RED;
    }
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_08, &stcGpioInit);
}

static void Key_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    /* configuration structure initialization */

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PullUp = PIN_PU_ON; // KeyMode
    stcGpioInit.u16PinDir = PIN_DIR_IN;
    stcGpioInit.u16PinAttr = PIN_ATTR_DIGITAL;
    stcGpioInit.u16ExtInt = PIN_EXTINT_OFF;

    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_06, &stcGpioInit);
}

static uint8_t Key_Read(uint8_t mode)
{
    static uint8_t key_up = 1; // �����ɿ���־
    if (mode == 1)
        key_up = 1; // ֧������

    if (key_up && (GPIO_ReadInputPins(GPIO_PORT_B, GPIO_PIN_06) == 0))
    {
        DDL_DelayMS(10);
        key_up = 0;
        if (GPIO_ReadInputPins(GPIO_PORT_B, GPIO_PIN_06) == 0)
            return 1;
    }
    else if (GPIO_ReadInputPins(GPIO_PORT_B, GPIO_PIN_06) == 1)
        key_up = 1;
    return 0; // �ް�������
}

// �����Զ����ĳЩ��ʽ
void enable_auto_detection(uint8_t enable)
{
    static uint8_t art[2];
    if (enable)
    {
        // ����PAL��NTSC��SECAM���Զ����
        art[0] = AUTO_DETECT_REG;
        art[1] = AD_PAL_EN | AD_NTSC_EN | AD_SECAM_EN | 0x80;
        (void)I2C_Master_Transmit(DEVICE_ADDR, art, 2, TIMEOUT);
    }
    else
    {
        // �����Զ����
        art[0] = AUTO_DETECT_REG;
        art[1] = 0x00;
        (void)I2C_Master_Transmit(DEVICE_ADDR, art, 2, TIMEOUT);
    }
}


void printBinary(unsigned char num)
{

    for (int i = sizeof(num) * 8 - 1; i >= 0; i--)
    {

        putchar((num & (1U << i)) ? '1' : '0');
    }
    putchar('\n'); 
}
/**
 * @brief  Main function of UART DMA project
 * @param  None
 * @retval int32_t return value, if needed
 */

int32_t main(void)
{
    uint8_t buff_main[2];
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    /* MCU Peripheral registers write unprotected */
    LL_PERIPH_WE(LL_PERIPH_SEL);
    __enable_irq();
    EFM_FWMC_Cmd(ENABLE);
    BSP_CLK_Init();
    BSP_LED_Init();

    // Add support for debug UART
    DebugUart_Init();
    printf("\r\n[DEBUG UART] USART1 activated (PH2=TX, PC13=RX)\r\n");

    vd_Button_Init();
    Key_Init();
#if (LL_TMR0_ENABLE == DDL_ON)
    TMR02_A_Config();
#endif

#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
#endif
    i2c_init();
    TMR0_Start(CM_TMR0_2, TMR0_CHA);
    uart_dma_init();
    Video_Sw(adv_sw);
    Signal_led(Input_signal);
    for (;;)
    {

        if (Key_Read(0))
        {
//            static uint8_t i;
//            static uint8_t art[] =
//            {
//                0x42, 0x0E, 0x00, // Re-enter map
//                0x42, 0x0b, 0x00, // new ɫ��
//            };
//            i+=10;
//            art[5] = i;
//            printf(" Hue %d",art[5]);
//            (void)ADV_7280_Send_Buff(art, sizeof(art) / 3, TIMEOUT);
            
//            btn_flag = ! btn_flag;
//            c_state = 1;
        }

        if (g_u16_sys_timer >= SYS_TIMEOUT_100MS) // 100ms
        {
            detect_loop();
            g_u16_sys_timer = 0;
        }
        if (g_u16_key_timer >= SYS_TIMEOUT_50MS) // 50MS
        {
            signal_turn();
            g_u16_key_timer = 0;
        }
        if (g_u16_mis_timer >= SYS_TIMEOUT_100MS) // 100ms
        {
            static uint8_t Input_signal_last = 0;
            if (c_state == 1)
                C_LED_OK();
            else if (c_state == 2)
                C_LED_ERR_RED();
            else if (c_state == 3)
                C_LED_ERR_GREEN();
            else if (c_state == 4)
                C_LED_ERR_BLUE();
            c_state = 0;
            if (Input_signal != Input_signal_last)
            {
                Signal_led(Input_signal);
                Input_signal_last = Input_signal;
            }
            g_u16_mis_timer = 0;
        }
        if (g_u16_osd_timer >= SYS_TIMEOUT_500MS) // 500MS   OSD
        {
            static uint8_t count;
            err_flag = 1;

            //            count++;
            //            if (count >= 3)
            //            {
            //                uint8_t buff = 0x10 ,buff_Re;
            //                count = 0;
            //                Chip_Receive(DEVICE_ADDR, &buff, &buff_Re, 1, TIMEOUT);
            //                printf("buff_Re 0x%02x \n",buff_Re);
            //
            ////                printf("ASW %d %d %d %d \n",
            ////                       GPIO_ReadInputPins(GPIO_PORT_ASW, GPIO_PIN_ASW1), GPIO_ReadInputPins(GPIO_PORT_ASW, GPIO_PIN_ASW2), GPIO_ReadInputPins(GPIO_PORT_ASW, GPIO_PIN_ASW3), GPIO_ReadInputPins(GPIO_PORT_ASW, GPIO_PIN_ASW4));
            ////                printf(" AVsw %d\n" ,AVsw);
            //            }

            g_u16_osd_timer = 0;
        }
    }
}

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
