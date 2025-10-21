#include "hc32_ll.h"

void DebugUart_Init(void)
{
    stc_usart_uart_init_t stcUartInit;
	(void)USART_UART_StructInit(&stcUartInit);
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART1, ENABLE);

    GPIO_SetFunc(GPIO_PORT_H, GPIO_PIN_02, GPIO_FUNC_33); // PH2 = TX
    GPIO_SetFunc(GPIO_PORT_C, GPIO_PIN_13, GPIO_FUNC_32); // PC13 = RX

    stcUartInit.u32Baudrate = 115200UL;
    stcUartInit.u32Parity = USART_PARITY_NONE;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;

    (void)USART_UART_Init(CM_USART1, &stcUartInit, NULL);
    USART_FuncCmd(CM_USART1, USART_RX | USART_TX, ENABLE);
}

__attribute__((weak)) int fputc(int ch, FILE *f)
{
    USART_WriteData(CM_USART1, (uint16_t)ch);
    while (RESET == USART_GetStatus(CM_USART1, USART_FLAG_TX_EMPTY)) {}
    return ch;
}