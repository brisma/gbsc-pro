#include "debug_uart.h"
#include "main.h"
#include <stdio.h>

void DebugUart_Init(void)
{
    stc_usart_uart_init_t cfg;

    DEBUG_UART_FCG_ENABLE();

    // HC32F460JEUA-QFN48TR Datasheet - Pages 33 and 38
    GPIO_SetFunc(GPIO_PORT_H, GPIO_PIN_02, GPIO_FUNC_32); // PH2 = USART3_TX
    GPIO_SetFunc(GPIO_PORT_C, GPIO_PIN_13, GPIO_FUNC_33); // PC13 = USART3_RX

    (void)USART_UART_StructInit(&cfg);
    cfg.u32Baudrate      = DEBUG_UART_BAUDRATE;
    cfg.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;

    (void)USART_UART_Init(DEBUG_UART, &cfg, NULL);
    USART_FuncCmd(DEBUG_UART, USART_TX | USART_RX, ENABLE);
}

void DebugUart_ClearErrors(void)
{
    USART_ClearStatus(DEBUG_UART, USART_FLAG_OVERRUN | USART_FLAG_FRAME_ERR | USART_FLAG_PARITY_ERR);
}

/* Boot printf (blocking, before the link starts) */
int32_t DebugUart_SendChar(char ch)
{
    while (USART_GetStatus(DEBUG_UART, USART_FLAG_TX_EMPTY) == RESET)
        ;
    USART_WriteData(DEBUG_UART, (uint16_t)ch);
    return ch;
}

/* Console output: a 512 B ring drained by DebugUart_Poll(), never waits (drops when full) */
#define TX_RING 512U
static char sTx[TX_RING];
static volatile uint16_t sTxHead, sTxTail;

static size_t txUsed(void) { return (size_t)((sTxHead - sTxTail) & (TX_RING - 1U)); }

size_t DebugUart_TxSpace(void) { return TX_RING - 1U - txUsed(); }

void DebugUart_Write(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (DebugUart_TxSpace() == 0U) return;
        sTx[sTxHead] = s[i];
        sTxHead = (uint16_t)((sTxHead + 1U) & (TX_RING - 1U));
    }
}

void DebugUart_Poll(void)
{
    for (uint8_t k = 0; k < 16U && sTxTail != sTxHead; k++) {
        if (USART_GetStatus(DEBUG_UART, USART_FLAG_TX_EMPTY) == RESET) return;
        USART_WriteData(DEBUG_UART, (uint16_t)sTx[sTxTail]);
        sTxTail = (uint16_t)((sTxTail + 1U) & (TX_RING - 1U));
    }
}

/* Non-blocking: 0 when a character was read, -1 when none is waiting */
int32_t DebugUart_ReceiveChar(char *ch)
{
    if (USART_GetStatus(DEBUG_UART, USART_FLAG_RX_FULL) == SET)
    {
        *ch = (char)USART_ReadData(DEBUG_UART);
        return 0;
    }
    return -1;
}

/* printf goes to the debug UART (blocking) */
__attribute__((weak)) int fputc(int ch, FILE *f)
{
    (void)f;
    return DebugUart_SendChar((char)ch);
}
