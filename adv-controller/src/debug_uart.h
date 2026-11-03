/* Debug UART on PH2 (TX) / PC13 (RX): CM_USART3 @ 115200 baud for printf output and console input */

#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#include <stddef.h>
#include <stdint.h>

#define DEBUG_UART              CM_USART3
#define DEBUG_UART_BAUDRATE     115200UL
#define DEBUG_UART_FCG_ENABLE() FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART3, ENABLE)

void    DebugUart_Init(void);
void    DebugUart_ClearErrors(void);
int32_t DebugUart_SendChar(char ch);
int32_t DebugUart_ReceiveChar(char *ch);
size_t  DebugUart_TxSpace(void);
void    DebugUart_Write(const char *s, size_t n);   /* console output ring, never blocks */
void    DebugUart_Poll(void);                       /* main loop: at most 16 bytes per pass */

#endif /* DEBUG_UART_H */
