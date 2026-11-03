/* USART4 receive path from gbs-control (ESP8266), link v3 (PB07 RX @ 115200) */

#ifndef UART_DMA_H
#define UART_DMA_H

#include "main.h"

#define RX_DMA_UNIT         (CM_DMA1)
#define RX_DMA_CH           (DMA_CH0)
#define RX_DMA_FCG_ENABLE() (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA1, ENABLE))
#define RX_DMA_TRIG_SEL     (AOS_DMA1_0)
#define RX_DMA_TRIG_EVT_SRC (EVT_SRC_USART4_RI)

#define USART_RX_PORT      (GPIO_PORT_B)
#define USART_RX_PIN       (GPIO_PIN_07)
#define USART_RX_GPIO_FUNC (GPIO_FUNC_37)

#define USART_UNIT         (CM_USART4)
#define USART_FCG_ENABLE() (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART4, ENABLE))
#define USART_BAUDRATE     (115200UL)

#define USART_RX_ERR_IRQn    (INT003_IRQn)
#define USART_RX_ERR_INT_SRC (INT_SRC_USART4_EI)

#define APP_FRAME_LEN_MAX (4096U)

extern uint8_t dma_au8RxBuf[APP_FRAME_LEN_MAX];

void     UART_DMA_Init(void);
uint16_t UART_RxWriteIndex(void);   /* next ring position the DMA will write */

#endif
