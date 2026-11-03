/* Host shim of the HC32 DDL for the golden I2C traces (test/test_golden.c): only what the ADV modules (adv7280,
 * adv7391, board, led), link_hal_adv.c and the headers they include use. GPIO and delays are recorded in the trace. */
#ifndef HC32_LL_SHIM_H
#define HC32_LL_SHIM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define LL_OK 0
#define LL_ERR (-1)
#define ENABLE 1
#define DISABLE 0
typedef enum { RESET = 0, SET = 1 } en_flag_status_t;
typedef enum { PIN_RESET = 0, PIN_SET = 1 } en_pin_state_t;

#define GPIO_PORT_A 0U
#define GPIO_PORT_B 1U
#define GPIO_PORT_C 2U
#define GPIO_PORT_H 7U
#define GPIO_PIN_00 0x0001U
#define GPIO_PIN_01 0x0002U
#define GPIO_PIN_02 0x0004U
#define GPIO_PIN_03 0x0008U
#define GPIO_PIN_04 0x0010U
#define GPIO_PIN_05 0x0020U
#define GPIO_PIN_06 0x0040U
#define GPIO_PIN_07 0x0080U
#define GPIO_PIN_08 0x0100U
#define GPIO_PIN_09 0x0200U
#define GPIO_PIN_10 0x0400U
#define GPIO_PIN_11 0x0800U
#define GPIO_PIN_12 0x1000U
#define GPIO_PIN_13 0x2000U
#define GPIO_PIN_14 0x4000U
#define GPIO_PIN_15 0x8000U
#define PIN_DIR_OUT 1U
#define PIN_DIR_IN 0U
#define PIN_ATTR_DIGITAL 0U
#define PIN_HIGH_DRV 2U
#define PIN_OUT_TYPE_CMOS 0U
#define PIN_PU_OFF 0U
#define PIN_PU_ON 1U
#define PIN_STAT_RST 0U
#define PIN_STAT_SET 1U
#define LED_RED (0x01U)
#define LED_GREEN (0x02U)
#define LED_BLUE (0x04U)
#define LED_ALL (LED_RED | LED_GREEN | LED_BLUE)

typedef struct { uint16_t u16PinState, u16PinDir, u16PinOutputType, u16PinDrv, u16Latch, u16PullUp, u16Invert,
                 u16ExtInt, u16PinInputType, u16PinAttr; } stc_gpio_init_t;

void GOLDEN_Gpio(const char* op, uint8_t port, uint16_t pins);
void GOLDEN_GpioInit(uint8_t port, uint16_t pins, uint16_t dir, uint16_t state, uint16_t pullUp);
void GOLDEN_Delay(uint32_t ms);
static inline int32_t GPIO_StructInit(stc_gpio_init_t* p) { memset(p, 0, sizeof(*p)); return LL_OK; }
static inline int32_t GPIO_Init(uint8_t port, uint16_t pins, const stc_gpio_init_t* p) {
    GOLDEN_GpioInit(port, pins, p->u16PinDir, p->u16PinState, p->u16PullUp); return LL_OK; }
static inline void GPIO_SetPins(uint8_t port, uint16_t pins) { GOLDEN_Gpio("set", port, pins); }
static inline void GPIO_ResetPins(uint8_t port, uint16_t pins) { GOLDEN_Gpio("reset", port, pins); }
static inline void DDL_DelayMS(uint32_t ms) { GOLDEN_Delay(ms); }
static inline void DDL_DelayUS(uint32_t us) { (void)us; }

/* I2C master DDL (only declared: test/test_i2c.c fakes it to test src/i2c.c; the golden build replaces i2c.c) */
#define LL_ERR_TIMEOUT (-8)       /* as hc32_ll_def.h */
#define LL_ERR_INVD_PARAM (-3)
typedef struct { int unit; } CM_I2C_TypeDef;
extern CM_I2C_TypeDef gFakeI2c1;
#define CM_I2C1 (&gFakeI2c1)
#define I2C_DIR_TX 0U
#define I2C_DIR_RX 1U
#define I2C_ACK 0U
#define I2C_NACK 1U
#define I2C_CLK_DIV4 2U
#define FCG1_PERIPH_I2C1 1U
#define GPIO_FUNC_48 48U
#define GPIO_FUNC_49 49U
void I2C_Cmd(CM_I2C_TypeDef* i, int en);
void I2C_SWResetCmd(CM_I2C_TypeDef* i, int en);
int32_t I2C_Start(CM_I2C_TypeDef* i, uint32_t timeout);
int32_t I2C_Restart(CM_I2C_TypeDef* i, uint32_t timeout);
int32_t I2C_Stop(CM_I2C_TypeDef* i, uint32_t timeout);
int32_t I2C_TransAddr(CM_I2C_TypeDef* i, uint16_t addr, uint8_t dir, uint32_t timeout);
int32_t I2C_TransData(CM_I2C_TypeDef* i, uint8_t const data[], uint32_t size, uint32_t timeout);
int32_t I2C_MasterReceiveDataAndStop(CM_I2C_TypeDef* i, uint8_t data[], uint32_t size, uint32_t timeout);
void I2C_AckConfig(CM_I2C_TypeDef* i, uint32_t ack);

/* printf of the apply paths: not part of the I2C behaviour */
#define printf(...) ((void)0)
#endif
