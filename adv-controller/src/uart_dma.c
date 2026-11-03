/* PB07 = USART4 RX @ 115200 baud. DMA1 CH0 runs free on a self-looping descriptor into a 4 KB ring; the main
 * loop reads it up to the DMA write index (link_hal_hc32.c, LINK_Poll). */

#include "main.h"
#include "uart_dma.h"

uint8_t dma_au8RxBuf[APP_FRAME_LEN_MAX];

static void DMA_Config(void)
{
    stc_dma_init_t                  stcDmaInit;
    stc_dma_llp_init_t              stcDmaLlpInit;
    static stc_dma_llp_descriptor_t stcLlpDesc;

    RX_DMA_FCG_ENABLE();
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn       = DMA_INT_DISABLE;
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = ARRAY_SZ(dma_au8RxBuf);
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr    = (uint32_t)dma_au8RxBuf;
    stcDmaInit.u32SrcAddr     = (uint32_t)(&USART_UNIT->RDR);
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    if (LL_OK != DMA_Init(RX_DMA_UNIT, RX_DMA_CH, &stcDmaInit))
    {
        return;
    }

    /* The descriptor points to itself: after 4096 bytes the channel reloads the same ring (no SW trigger) */
    (void)DMA_LlpStructInit(&stcDmaLlpInit);
    stcDmaLlpInit.u32State = DMA_LLP_ENABLE;
    stcDmaLlpInit.u32Mode  = DMA_LLP_WAIT;
    stcDmaLlpInit.u32Addr  = (uint32_t)&stcLlpDesc;
    (void)DMA_LlpInit(RX_DMA_UNIT, RX_DMA_CH, &stcDmaLlpInit);

    stcLlpDesc.SARx   = stcDmaInit.u32SrcAddr;
    stcLlpDesc.DARx   = stcDmaInit.u32DestAddr;
    stcLlpDesc.DTCTLx = (stcDmaInit.u32TransCount << DMA_DTCTL_CNT_POS) | (stcDmaInit.u32BlockSize << DMA_DTCTL_BLKSIZE_POS);
    stcLlpDesc.LLPx   = (uint32_t)&stcLlpDesc;
    stcLlpDesc.CHCTLx = stcDmaInit.u32SrcAddrInc | stcDmaInit.u32DestAddrInc | stcDmaInit.u32DataWidth |
                        stcDmaInit.u32IntEn | stcDmaLlpInit.u32State | stcDmaLlpInit.u32Mode;

    AOS_SetTriggerEventSrc(RX_DMA_TRIG_SEL, RX_DMA_TRIG_EVT_SRC);   /* one transfer per received byte (RI) */
    DMA_Cmd(RX_DMA_UNIT, ENABLE);
    (void)DMA_ChCmd(RX_DMA_UNIT, RX_DMA_CH, ENABLE);
}

static void USART_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART_UNIT);
    USART_ClearStatus(USART_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}

uint16_t UART_RxWriteIndex(void)
{
    uint32_t remaining = DMA_GetTransCount(RX_DMA_UNIT, RX_DMA_CH);
    return (uint16_t)((APP_FRAME_LEN_MAX - remaining) % APP_FRAME_LEN_MAX);
}

void UART_DMA_Init(void)
{
    stc_usart_uart_init_t   stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_gpio_init_t         stcGpio;

    DMA_Config();
    /* RX pull-up: the line idles high while the ESP is in reset (no error-interrupt storm on a floating pin) */
    (void)GPIO_StructInit(&stcGpio);
    stcGpio.u16PullUp = PIN_PU_ON;
    (void)GPIO_Init(USART_RX_PORT, USART_RX_PIN, &stcGpio);
    GPIO_SetFunc(USART_RX_PORT, USART_RX_PIN, USART_RX_GPIO_FUNC);

    /* PB06 is wired to SW2 -> GND, not to the ESP: plain input, never the USART4 TX function */
    (void)GPIO_StructInit(&stcGpio);
    stcGpio.u16PinDir = PIN_DIR_IN;
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_06, &stcGpio);

    USART_FCG_ENABLE();
    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv      = USART_CLK_DIV64;
    stcUartInit.u32CKOutput      = USART_CK_OUTPUT_ENABLE;
    stcUartInit.u32Baudrate      = USART_BAUDRATE;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
    if (LL_OK != USART_UART_Init(USART_UNIT, &stcUartInit, NULL))
    {
        BSP_LED_On(LED_RED);
        for (;;)
        {
        }
    }

    stcIrqSigninConfig.enIRQn      = USART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc    = USART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxError_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* The ESP ROM boot log (74880 baud) is not a valid frame: the parser drops it, no wait needed */
    USART_ClearStatus(USART_UNIT, USART_FLAG_OVERRUN | USART_FLAG_FRAME_ERR | USART_FLAG_PARITY_ERR);
    USART_FuncCmd(USART_UNIT, (USART_RX | USART_INT_RX), ENABLE);   /* INT_RX raises the RI event for the DMA */
}
