/* I2C master for the ADV7280/ADV7391 (I2C1 on PA06 SCL / PA07 SDA, 200 kHz); addresses are 8-bit (write) */

#ifndef I2C_H
#define I2C_H

#include "main.h"

#define I2C_UNIT          (CM_I2C1)
#define I2C_FCG_USE       (FCG1_PERIPH_I2C1)
#define I2C_BAUDRATE      (200000UL)
#define I2C_CLK_DIVx      (I2C_CLK_DIV4)

#define I2C_SCL_PORT      (GPIO_PORT_A)
#define I2C_SCL_PIN       (GPIO_PIN_06)
#define I2C_SDA_PORT      (GPIO_PORT_A)
#define I2C_SDA_PIN       (GPIO_PIN_07)
#define I2C_GPIO_SCL_FUNC (GPIO_FUNC_49)
#define I2C_GPIO_SDA_FUNC (GPIO_FUNC_48)

#define DEVICE_ADDR       (0x42)   /* ADV7280 I2C address */
#define TIMEOUT           (0x40000UL)

void    I2C_DriverInit(void);
int32_t I2C_Transmit(uint16_t addr, uint8_t const data[], uint32_t size, uint32_t timeout);
int32_t I2C_TransmitBatch(uint8_t const data[], uint32_t count, uint32_t timeout);
int32_t I2C_Receive(uint16_t addr, uint8_t tx[], uint8_t rx[], uint32_t size, uint32_t timeout);
/* One register address, then n bytes (I2C_Receive sends as many address bytes as it reads) */
int32_t I2C_ReadBurst(uint16_t addr, uint8_t reg, uint8_t rx[], uint32_t n, uint32_t timeout, uint8_t *stage);   /* stage: failed step 1..6, 0 ok */

#endif /* I2C_H */
