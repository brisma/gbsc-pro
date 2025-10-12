#include "uart_dma.h"
#include "main.h"

uint8_t dma_au8RxBuf[APP_FRAME_LEN_MAX];

/**        DMA 传输完成 IRQ 回调功能
 * @brief  DMA transfer complete IRQ callback function.
 * @param  None
 * @retval None
 */
static void RX_DMA_TC_IrqCallback(void)
{
    m_enRxFrameEnd = SET; // 接收完成
    m_u16RxLen = APP_FRAME_LEN_MAX;

    USART_FuncCmd(USART_UNIT, USART_RX_TIMEOUT, DISABLE); // 超时功能

    DMA_ClearTransCompleteStatus(RX_DMA_UNIT, RX_DMA_TC_FLAG);
}
/**        DMA 传输完成 IRQ 回调功能
 * @brief  DMA transfer complete IRQ callback function.
 * @param  None
 * @retval None
 */
static void TX_DMA_TC_IrqCallback(void)
{
    USART_FuncCmd(USART_UNIT, USART_INT_TX_CPLT, ENABLE);

    DMA_ClearTransCompleteStatus(TX_DMA_UNIT, TX_DMA_TC_FLAG);
}

/**
 * @brief  Initialize DMA.
 * @param  None
 * @retval int32_t:
 *           - LL_OK:                   Initialize successfully.
 *           - LL_ERR_INVD_PARAM:       Initialization parameters is invalid.
 */
int32_t DMA_Config(void)
{
    int32_t i32Ret;
    stc_dma_init_t stcDmaInit;
    stc_dma_llp_init_t stcDmaLlpInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    static stc_dma_llp_descriptor_t stcLlpDesc;

    /* DMA&AOS FCG enable */
    RX_DMA_FCG_ENABLE();
    TX_DMA_FCG_ENABLE();
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

    /* USART_RX_DMA */
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize = 1UL;
    stcDmaInit.u32TransCount = ARRAY_SZ(dma_au8RxBuf);
    stcDmaInit.u32DataWidth = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr = (uint32_t)dma_au8RxBuf;
    stcDmaInit.u32SrcAddr = (uint32_t)(&USART_UNIT->RDR); // RDR
    //    stcDmaInit.u32SrcAddr = (uint32_t)(0X40021406);  //RDR
    stcDmaInit.u32SrcAddrInc = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    i32Ret = DMA_Init(RX_DMA_UNIT, RX_DMA_CH, &stcDmaInit);
    if (LL_OK == i32Ret)
    {
        (void)DMA_LlpStructInit(&stcDmaLlpInit);
        stcDmaLlpInit.u32State = DMA_LLP_ENABLE;
        stcDmaLlpInit.u32Mode = DMA_LLP_WAIT;
        stcDmaLlpInit.u32Addr = (uint32_t)&stcLlpDesc;
        (void)DMA_LlpInit(RX_DMA_UNIT, RX_DMA_CH, &stcDmaLlpInit);

        stcLlpDesc.SARx = stcDmaInit.u32SrcAddr;
        stcLlpDesc.DARx = stcDmaInit.u32DestAddr;
        stcLlpDesc.DTCTLx = (stcDmaInit.u32TransCount << DMA_DTCTL_CNT_POS) | (stcDmaInit.u32BlockSize << DMA_DTCTL_BLKSIZE_POS);
        ;
        stcLlpDesc.LLPx = (uint32_t)&stcLlpDesc;
        stcLlpDesc.CHCTLx = stcDmaInit.u32SrcAddrInc | stcDmaInit.u32DestAddrInc | stcDmaInit.u32DataWidth |
                            stcDmaInit.u32IntEn | stcDmaLlpInit.u32State | stcDmaLlpInit.u32Mode;

        DMA_ReconfigLlpCmd(RX_DMA_UNIT, RX_DMA_CH, ENABLE);
        DMA_ReconfigCmd(RX_DMA_UNIT, ENABLE);
        AOS_SetTriggerEventSrc(RX_DMA_RECONF_TRIG_SEL, RX_DMA_RECONF_TRIG_EVT_SRC);

        stcIrqSignConfig.enIntSrc = RX_DMA_TC_INT_SRC;
        stcIrqSignConfig.enIRQn = RX_DMA_TC_IRQn;
        stcIrqSignConfig.pfnCallback = &RX_DMA_TC_IrqCallback;
        (void)INTC_IrqSignIn(&stcIrqSignConfig);
        NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
        NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
        NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

        AOS_SetTriggerEventSrc(RX_DMA_TRIG_SEL, RX_DMA_TRIG_EVT_SRC);

        DMA_Cmd(RX_DMA_UNIT, ENABLE);
        DMA_TransCompleteIntCmd(RX_DMA_UNIT, RX_DMA_TC_INT, ENABLE);
        (void)DMA_ChCmd(RX_DMA_UNIT, RX_DMA_CH, ENABLE);
    }

    /* USART_TX_DMA */
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize = 1UL;
    stcDmaInit.u32TransCount = ARRAY_SZ(dma_au8RxBuf);
    stcDmaInit.u32DataWidth = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr = (uint32_t)(&USART_UNIT->TDR);
    //    stcDmaInit.u32DestAddr = (uint32_t)(0X40021404);
    stcDmaInit.u32SrcAddr = (uint32_t)dma_au8RxBuf;
    stcDmaInit.u32SrcAddrInc = DMA_SRC_ADDR_INC;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_FIX;
    i32Ret = DMA_Init(TX_DMA_UNIT, TX_DMA_CH, &stcDmaInit);
    if (LL_OK == i32Ret)
    {
        stcIrqSignConfig.enIntSrc = TX_DMA_TC_INT_SRC;
        stcIrqSignConfig.enIRQn = TX_DMA_TC_IRQn;
        stcIrqSignConfig.pfnCallback = &TX_DMA_TC_IrqCallback;
        (void)INTC_IrqSignIn(&stcIrqSignConfig);
        NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
        NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
        NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

        AOS_SetTriggerEventSrc(TX_DMA_TRIG_SEL, TX_DMA_TRIG_EVT_SRC);

        DMA_Cmd(TX_DMA_UNIT, ENABLE);
        DMA_TransCompleteIntCmd(TX_DMA_UNIT, TX_DMA_TC_INT, ENABLE);
    }

    return i32Ret;
}

/**
 * @brief  Configure TMR0.
 * @param  [in] u16TimeoutBits:         Timeout bits
 * @retval None
 */
void TMR0_Config(uint16_t u16TimeoutBits)
{
    uint16_t u16Div;
    uint16_t u16Delay;
    uint16_t u16CompareValue;
    stc_tmr0_init_t stcTmr0Init;

    DMATMR0_FCG_ENABLE();

    /* Initialize TMR0 base function. */
    stcTmr0Init.u32ClockSrc = TMR0_CLK_SRC_XTAL32;
    stcTmr0Init.u32ClockDiv = TMR0_CLK_DIV8;
    stcTmr0Init.u32Func = TMR0_FUNC_CMP;
    if (TMR0_CLK_DIV1 == stcTmr0Init.u32ClockDiv)
    {
        u16Delay = 7U;
    }
    else if (TMR0_CLK_DIV2 == stcTmr0Init.u32ClockDiv)
    {
        u16Delay = 5U;
    }
    else if ((TMR0_CLK_DIV4 == stcTmr0Init.u32ClockDiv) ||
             (TMR0_CLK_DIV8 == stcTmr0Init.u32ClockDiv) ||
             (TMR0_CLK_DIV16 == stcTmr0Init.u32ClockDiv))
    {
        u16Delay = 3U;
    }
    else
    {
        u16Delay = 2U;
    }

    u16Div = (uint16_t)1U << (stcTmr0Init.u32ClockDiv >> TMR0_BCONR_CKDIVA_POS);
    u16CompareValue = ((u16TimeoutBits + u16Div - 1U) / u16Div) - u16Delay;
    stcTmr0Init.u16CompareValue = u16CompareValue;
    (void)TMR0_Init(DMATMR0_UNIT, DMATMR0_CH, &stcTmr0Init);

    TMR0_HWStartCondCmd(DMATMR0_UNIT, DMATMR0_CH, ENABLE);
    TMR0_HWClearCondCmd(DMATMR0_UNIT, DMATMR0_CH, ENABLE);
}
/**         超时回调函数
 * @brief  USART RX timeout IRQ callback.
 * @param  None
 * @retval None
 */
void USART_RxTimeout_IrqCallback(void)
{
    if (m_enRxFrameEnd != SET)
    {
        m_enRxFrameEnd = SET;
        m_u16RxLen = APP_FRAME_LEN_MAX - (uint16_t)DMA_GetTransCount(RX_DMA_UNIT, RX_DMA_CH);

        /* Trigger for re-config USART RX DMA */
        AOS_SW_Trigger();
    }

    TMR0_Stop(DMATMR0_UNIT, DMATMR0_CH);

    USART_ClearStatus(USART_UNIT, USART_FLAG_RX_TIMEOUT);
}

/**
 * @brief  USART TX complete IRQ callback function.
 * @param  None
 * @retval None
 */
void USART_TxComplete_IrqCallback(void)
{
    USART_FuncCmd(USART_UNIT, (USART_TX | USART_INT_TX_CPLT), DISABLE);

    TMR0_Stop(DMATMR0_UNIT, DMATMR0_CH);

    USART_ClearStatus(USART_UNIT, USART_FLAG_RX_TIMEOUT);

    USART_FuncCmd(USART_UNIT, USART_RX_TIMEOUT, ENABLE);

    USART_ClearStatus(USART_UNIT, USART_FLAG_TX_CPLT);
}

/**
 * @brief  USART RX error IRQ callback.
 * @param  None
 * @retval None
 */
void USART_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART_UNIT);

    USART_ClearStatus(USART_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}

void uart_dma_init(void)
{
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    /* Initialize DMA. */
    (void)DMA_Config();
    /* Configure USART RX/TX pin. */
    GPIO_SetFunc(USART_RX_PORT, USART_RX_PIN, USART_RX_GPIO_FUNC);
    GPIO_SetFunc(USART_TX_PORT, USART_TX_PIN, USART_TX_GPIO_FUNC);

    /* Enable peripheral clock */
    USART_FCG_ENABLE();
    /* Initialize UART. */
    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv = USART_CLK_DIV64;
    stcUartInit.u32CKOutput = USART_CK_OUTPUT_ENABLE;
    stcUartInit.u32Baudrate = USART_BAUDRATE;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
    if (LL_OK != USART_UART_Init(USART_UNIT, &stcUartInit, NULL)) // UART init
    {
        BSP_LED_On(LED_RED);
        for (;;)
        {
        }
    }

    /* Register TX complete IRQ handler. */
    stcIrqSigninConfig.enIRQn = USART_TX_CPLT_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_TX_CPLT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_TxComplete_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* Register RX error IRQ handler. */
    stcIrqSigninConfig.enIRQn = USART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxError_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* Register RX timeout IRQ handler. */
    stcIrqSigninConfig.enIRQn = USART_RX_TIMEOUT_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RX_TIMEOUT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxTimeout_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* MCU Peripheral registers write protected */

    TMR0_Config(USART_TIMEOUT_BITS);
    /* Enable TX && RX && RX interrupt function */
    USART_FuncCmd(USART_UNIT, (USART_RX | USART_INT_RX | USART_RX_TIMEOUT | USART_INT_RX_TIMEOUT), ENABLE);
}

void uart_dma_send(uint8_t *buff, uint8_t len)
{
    DMA_SetSrcAddr(TX_DMA_UNIT, TX_DMA_CH, (uint32_t)dma_au8RxBuf);
    DMA_SetTransCount(TX_DMA_UNIT, TX_DMA_CH, m_u16RxLen); // m_u16RxLen
    (void)DMA_ChCmd(TX_DMA_UNIT, TX_DMA_CH, ENABLE);
    USART_FuncCmd(USART_UNIT, USART_TX, ENABLE);
}

static void get_mode(const uint8_t atr)
{
    uint8_t buff[2];
    //    if(!(atr&0x0f))
    //    {
    //        return;
    //    }
    //    else
    if ((atr & 0x0f))
    {
        switch ((atr & 0x0f))
        {
        case 0x01:   //auto
        {
            adv_tv = 0x04;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("auto\n");
            c_state = 1;
        }
        break;
        case 0x02:  //PAL
        {
            adv_tv = 0x84;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("pal\n");
            c_state = 1;
        }
        break;
        case 0x03:  //NTSC-M
        {
            adv_tv = 0x54;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("NTSC-M\n");
            c_state = 1;
        }
        break;
        case 0x04:  //PAL-60
        {
            adv_tv = 0x64;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("PAL-60\n");
            c_state = 1;
        }
        break;
        case 0x05:  //NTSC443
        {
            adv_tv = 0x74;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("NTSC443\n");
            c_state = 1;
        }
        break;
        case 0x06:  //NTSC-J
        {
            adv_tv = 0x44;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("NTSC-J\n");
            c_state = 1;
        }
        break;
        case 0x07:  //PAL-N w/ p
        {
            adv_tv = 0x94;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("PAL-N w p\n");
            c_state = 1;
        }
        break;
        case 0x08:   //PAL-M w/o p
        {
            adv_tv = 0xA4;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("PAL-M wo p\n");
        }
        break;
        case 0x09:  //PAL-M
        {
            adv_tv = 0xB4;
            buff[0] = VID_SEL_REG; 
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("PAL-M\n");
            c_state = 1;
        }
        break;
        case 0x0A:  //PAL Cmb -N
        {
            adv_tv = 0xC4;
            buff[0] = VID_SEL_REG;
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("PAL Cmb -N\n");
            c_state = 1;
        }
        break;
        case 0x0B:  //PAL Cmb -N w/ p
        {
            adv_tv = 0xD4;
            buff[0] = VID_SEL_REG;
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("PAL Cmb -N w p\n");
            c_state = 1;
        }
        break;
        case 0x0C:  //SECAM
        {
            adv_tv = 0xE4;
            buff[0] = VID_SEL_REG;
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("SECAM\n");
            c_state = 1;
        }
        break;
        default:
        {
            adv_tv = 0x04;
            buff[0] = VID_SEL_REG;
            buff[1] = adv_tv;
            (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
            printf("Err Default Auto\n");
            c_state = 2;
        }
        break;
        }
    }
}

void signal_turn(void)
{
    uint8_t buff[2];

    if (SET == m_enRxFrameEnd)
    {

        //        printf("0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",dma_au8RxBuf[0],dma_au8RxBuf[1],dma_au8RxBuf[2],dma_au8RxBuf[3],dma_au8RxBuf[4]);
        buff[1] = dma_au8RxBuf[5] + dma_au8RxBuf[4] + dma_au8RxBuf[3] + dma_au8RxBuf[2] + dma_au8RxBuf[1] + dma_au8RxBuf[0];
        if (dma_au8RxBuf[5] != 0xFE || dma_au8RxBuf[6] != buff[1] || (dma_au8RxBuf[0] != 0x41 || dma_au8RxBuf[1] != 0x44))
        {   
            m_enRxFrameEnd = RESET;
            memset(dma_au8RxBuf, 0, sizeof(dma_au8RxBuf));
            return;
        }
        else if (dma_au8RxBuf[5] == 0xFE && dma_au8RxBuf[6] == buff[1] && (dma_au8RxBuf[0] == 0x41 && dma_au8RxBuf[1] == 0x44))
        {
            
//            else if (dma_au8RxBuf[2] == 'N') // detected signal
//            {
//                if(dma_au8RxBuf[3] == 0x10)  //No Signal
//                {
//                    led_state = LED_RED;
//                }
//                else if(dma_au8RxBuf[3] == 0x11)   //signalized
//                {
//                    Signal_led(Input_signal);
//                }
//            }
//            if (dma_au8RxBuf[2] == 'I'&& dma_au8RxBuf[3] == 0x80) // INFO
//            {
//                asw_01 = 0;
//                asw_02 = 0;
//                asw_03 = 1;
//                asw_04 = 0;
//                ASC(asw_01, asw_02, asw_03, asw_04,0);
//                adv_sw = true;
//                info();
//                c_state = 1;
//            }
//            else 
            if (dma_au8RxBuf[2] == 'T') // TvMode
            {
                adv_tv = dma_au8RxBuf[3];
                mem_settings();
                buff[0] = VID_SEL_REG; // addr   0x07
                buff[1] = adv_tv;
                (void)I2C_Master_Transmit(DEVICE_ADDR, buff, 2, TIMEOUT);
                printf("Tv 0x%02x\n", adv_tv);
                c_state = 1;
            }
            else if (dma_au8RxBuf[2] == 'N' && \
                    (dma_au8RxBuf[3] == 0x0a ||\
                     dma_au8RxBuf[3] == 0x08 ||\
                     dma_au8RxBuf[3] == 0xe3 ||\
                     dma_au8RxBuf[3] == 'D'                    //default
                    )) // BCSH
            {
                 
                if(dma_au8RxBuf[3] == 'D' && dma_au8RxBuf[4] == 'E')
                {   
                    uint8_t I2C_DEFAULT_BCSH[] =
                    {
                        0x42, 0x0E, 0x00, // Re-enter map
                        0x42, 0x0a, 0x00, // new 亮度   00(00)  7F(+30)  80(-30)    e0
                        0x42, 0x08, 0x80, // new 对比度 00(00)  80(01)   FF(02)     58
                        0x42, 0xe3, 0x80, // new 饱和度 00(00)  80(01)   FF(02)     80
                        0x42, 0x0b, 0x00, // new 色度
                    };
                    Bright = 0x00;
                    Contrast = 0x80;
                    Saturation = 0x80;
                    
                    (void)ADV_7280_Send_Buff(I2C_DEFAULT_BCSH, sizeof(I2C_DEFAULT_BCSH) / 3, TIMEOUT); 
                    printf("bcsh: default \n");
                }
                else
                {
                    (void)I2C_Master_Transmit(DEVICE_ADDR, &dma_au8RxBuf[3], 2, TIMEOUT);
                    printf("bcsh: 0x%02x \n",dma_au8RxBuf[4]);
                    if(dma_au8RxBuf[3] == 0x0a)
                        Bright = dma_au8RxBuf[4];
                    else if(dma_au8RxBuf[3] == 0x08)
                        Contrast = dma_au8RxBuf[4];
                    else if(dma_au8RxBuf[3] == 0xe3)
                        Saturation = dma_au8RxBuf[4];
                }
                mem_settings();
                c_state = 1;
            }
            else if (dma_au8RxBuf[2] == 'S') // Sw
            {
                // Off or On
                if (dma_au8RxBuf[3] == 0xfd) // on
                {
                    adv_sw = true;
                    mem_settings();
                    video_init();
                    c_state = 1;
                }
                else if (dma_au8RxBuf[3] == 0x01) // off
                {
                    adv_sw = false;
                    mem_settings();
                    video_Deinit();
                    c_state = 1;
                }

                // SV or AV
                else if ((dma_au8RxBuf[3] & 0xf0) == 0x10) // SV
                {   
                    AVsw= 1;
                    asw_01 = 0;
                    asw_02 = 0;
                    asw_03 = 1;
                    asw_04 = 0;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    AV_Connecte_Set(AVsw);
                    adv_sw = true;
                    adv_input = SV_INPUT;
                    get_mode(dma_au8RxBuf[3] & 0x0f);
                    video_init();
                    Input_signal = 5;
                    mem_settings();
                    printf("mode 0x%02x ", dma_au8RxBuf[3]);
                    c_state = 1;
//                    led_state = LED_RED|LED_BLUE;
                }
                else if ((dma_au8RxBuf[3] & 0xf0) == 0x20) // AV   0x20
                {   
                    AVsw= 1;
                    asw_01 = 0;
                    asw_02 = 0;
                    asw_03 = 1;
                    asw_04 = 0;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    AV_Connecte_Set(AVsw);
                    adv_sw = true;
                    adv_input = AV_INPUT;
                    get_mode(dma_au8RxBuf[3] & 0x0f);
                    video_init();
                    Input_signal = 6;
                    mem_settings();
                    printf("mode 0x%02x ", dma_au8RxBuf[3]);
                    c_state = 1;
//                    led_state = LED_GREEN | LED_BLUE;
                }

                // Smooth
                else if (dma_au8RxBuf[3] == 0x90) // on
                {   
                    if(adv_double == true)
                    {
                        adv_smooth = true;
                        mem_settings();
//                        set_smooth(adv_smooth);
                        set_smooth(adv_smooth);
//                        set_double_line(adv_double);
                    }
                    c_state = 1;
                }
                else if (dma_au8RxBuf[3] == 0x91) // off
                {
                    if(adv_double == true)
                    {
                        adv_smooth = false;
                        mem_settings();
//                        set_smooth(adv_smooth);
                        set_smooth(adv_smooth);
//                        set_double_line(adv_double); 
                    }
                    c_state = 1;
                }

                // Double Line
                else if (dma_au8RxBuf[3] == 0x30) // on
                {
                    adv_double = true;
                    mem_settings();
//                    set_double_line(adv_double);
//                    set_smooth(adv_smooth);
                    set_double_line(adv_double); 
                    c_state = 1;
                    Reser_Status();
                    
                }
                else if (dma_au8RxBuf[3] == 0x31) // off
                {
                    adv_double = false;
                    adv_smooth = false;
                    mem_settings();
                    set_smooth(adv_smooth);
                    DDL_DelayMS(50);
                    set_double_line(adv_double);
//                    set_double_line(adv_double); 
                    c_state = 1;
                }
                // Compatibility
                else if (dma_au8RxBuf[3] == 0xa0) // on
                {
                    asw_02 = 1;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    mem_settings();
                    //                    printf("ASW %d %d %d %d \n",asw_01,asw_02,asw_03,asw_04);
                    printf("Open Compatibility \n");
                    c_state = 1;
                }
                else if (dma_au8RxBuf[3] == 0xa1) // off
                {
                    asw_02 = 0;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    mem_settings();
                    //                    printf("ASW %d %d %d %d \n",asw_01,asw_02,asw_03,asw_04);
                    printf("Close Compatibility \n");
                    c_state = 1;
                }
                // RGBs
                else if ((dma_au8RxBuf[3] & 0xf0) == 0x40)
                {
                    AVsw= 0;
                    asw_01 = 0;
                    asw_02 = dma_au8RxBuf[3] & 0x01; // 0
                    asw_03 = 0;
                    asw_04 = 1;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    AV_Connecte_Set(AVsw);
                    adv_sw = false;
                    Input_signal = 1;
                    mem_settings();
                    video_Deinit();
                    printf("Compatibility %d\n", (dma_au8RxBuf[3] & 0x0f));
                    printf("RGBs\n");
                    c_state = 1;
//                    led_state = LED_RED|LED_GREEN|LED_BLUE;
                    
                }
                // RGsB
                else if ((dma_au8RxBuf[3] & 0xf0) == 0x50)
                {
                    AVsw= 0;
                    asw_01 = 0;
                    asw_02 = dma_au8RxBuf[3] & 0x01; // 0
                    asw_03 = 1;
                    asw_04 = 0;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    AV_Connecte_Set(AVsw);
                    adv_sw = false;
                    Input_signal = 2;
                    mem_settings();
                    video_Deinit();
                    printf("RGsB\n");
                    c_state = 1;
//                    led_state = LED_GREEN;
                    
                }
                // VGA
                else if ((dma_au8RxBuf[3] & 0xf0) == 0x60)
                {
                    asw_01 = 1;
                    asw_02 = dma_au8RxBuf[3] & 0x0f; // 1
                    asw_03 = 1;
                    asw_04 = 1;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    adv_sw = false;
                    Input_signal = 3;
                    mem_settings();
                    video_Deinit();
                    printf("VGA\n");
                    c_state = 1;
//                    led_state = LED_BLUE;
                }
                // ypbpr
                else if (dma_au8RxBuf[3] == 0x70)
                {
                    asw_01 = 0;
                    asw_02 = 0;
                    asw_03 = 1;
                    asw_04 = 0;
                    ASC(asw_01, asw_02, asw_03, asw_04,0);
                    adv_sw = false;
                    Input_signal = 4;
                    mem_settings();
                    video_Deinit();
                    printf("Ypbpr\n");
                    c_state = 1;
//                    led_state = LED_RED|LED_GREEN;
                }

                //                //ACE
                else if (dma_au8RxBuf[3] == 0x80) // on
                {
                    adv_ace = true;
                    mem_settings();
                    set_ace(adv_ace);
                    printf("AceOn\n");
                    c_state = 1;
                }
                else if (dma_au8RxBuf[3] == 0x81) // off
                {
                    adv_ace = false;
                    mem_settings();
                    set_ace(adv_ace);
                    printf("AceOff\n");
                    c_state = 1;
                }
            }
        }
        m_enRxFrameEnd = RESET;
    }
}
