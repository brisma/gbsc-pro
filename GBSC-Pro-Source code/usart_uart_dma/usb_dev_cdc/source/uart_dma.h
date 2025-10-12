#ifndef UART_DMA_H
#define UART_DMA_H

#include "main.h"


/* DMA definition */
#define RX_DMA_UNIT                     (CM_DMA1)
#define RX_DMA_CH                       (DMA_CH0)
#define RX_DMA_FCG_ENABLE()             (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA1, ENABLE))
#define RX_DMA_TRIG_SEL                 (AOS_DMA1_0)
#define RX_DMA_TRIG_EVT_SRC             (EVT_SRC_USART4_RI)
#define RX_DMA_RECONF_TRIG_SEL          (AOS_DMA_RC)
#define RX_DMA_RECONF_TRIG_EVT_SRC      (EVT_SRC_AOS_STRG)
#define RX_DMA_TC_INT                   (DMA_INT_TC_CH0)
#define RX_DMA_TC_FLAG                  (DMA_FLAG_TC_CH0)
#define RX_DMA_TC_IRQn                  (INT000_IRQn)
#define RX_DMA_TC_INT_SRC               (INT_SRC_DMA1_TC0)

#define TX_DMA_UNIT                     (CM_DMA2)
#define TX_DMA_CH                       (DMA_CH0)
#define TX_DMA_FCG_ENABLE()             (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA2, ENABLE))
#define TX_DMA_TRIG_SEL                 (AOS_DMA2_0)
#define TX_DMA_TRIG_EVT_SRC             (EVT_SRC_USART4_TI)
#define TX_DMA_TC_INT                   (DMA_INT_TC_CH0)
#define TX_DMA_TC_FLAG                  (DMA_FLAG_TC_CH0)
#define TX_DMA_TC_IRQn                  (INT001_IRQn)
#define TX_DMA_TC_INT_SRC               (INT_SRC_DMA2_TC0)

/* USART RX/TX pin definition */
#define USART_RX_PORT                   (GPIO_PORT_B)   /* PB9: USART4_RX */
#define USART_RX_PIN                    (GPIO_PIN_07)
#define USART_RX_GPIO_FUNC              (GPIO_FUNC_37)

#define USART_TX_PORT                   (GPIO_PORT_B)   /* PE6: USART4_TX */
#define USART_TX_PIN                    (GPIO_PIN_06)
#define USART_TX_GPIO_FUNC              (GPIO_FUNC_36)

/* USART unit definition */
#define USART_UNIT                      (CM_USART4)
#define USART_FCG_ENABLE()              (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART4, ENABLE))

/* USART baudrate definition */
#define USART_BAUDRATE                  (115200UL)

/* USART timeout bits definition */
#define USART_TIMEOUT_BITS              (2000U)

/* USART interrupt definition */
#define USART_TX_CPLT_IRQn              (INT002_IRQn)
#define USART_TX_CPLT_INT_SRC           (INT_SRC_USART4_TCI)

#define USART_RX_ERR_IRQn               (INT003_IRQn)
#define USART_RX_ERR_INT_SRC            (INT_SRC_USART4_EI)

#define USART_RX_TIMEOUT_IRQn           (INT004_IRQn)
#define USART_RX_TIMEOUT_INT_SRC        (INT_SRC_USART4_RTO)




/* Timer0 unit & channel definition */
#define DMATMR0_UNIT                       (CM_TMR0_2)
#define DMATMR0_CH                         (TMR0_CH_B)
#define DMATMR0_FCG_ENABLE()               (FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMR0_2, ENABLE))


typedef unsigned   char uint8_t;
/* Application frame length max definition */
#define APP_FRAME_LEN_MAX               (500U)
static __IO en_flag_status_t m_enRxFrameEnd;
static __IO uint16_t m_u16RxLen;

void TMR0_Config(uint16_t u16TimeoutBits);
int32_t DMA_Config(void);
void USART_TxComplete_IrqCallback(void);
void USART_RxError_IrqCallback(void);
void USART_RxTimeout_IrqCallback(void);
void uart_dma_init(void);
void uart_dma_send(uint8_t *buff,uint8_t len);
void signal_turn(void);

#endif
