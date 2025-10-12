#ifndef I2C_H
#define I2C_H

#include "main.h"

/* Define I2C unit used for the example */
#define I2C_UNIT                        (CM_I2C1)
#define I2C_FCG_USE                     (FCG1_PERIPH_I2C1)
/* Define slave device address for example */
#define DEVICE_ADDR                     (0x42)
/* I2C address mode */
#define I2C_ADDR_MD_7BIT                (0U)
#define I2C_ADDR_MD_10BIT               (1U)
/* Config I2C address mode: I2C_ADDR_MD_7BIT or I2C_ADDR_MD_10BIT */
#define I2C_ADDR_MD                     (I2C_ADDR_MD_7BIT)

/* Define port and pin for SDA and SCL */
#define I2C_SCL_PORT                    (GPIO_PORT_A)
#define I2C_SCL_PIN                     (GPIO_PIN_06)
#define I2C_SDA_PORT                    (GPIO_PORT_A)
#define I2C_SDA_PIN                     (GPIO_PIN_07)
#define I2C_GPIO_SCL_FUNC               (GPIO_FUNC_49)
#define I2C_GPIO_SDA_FUNC               (GPIO_FUNC_48)

#define TIMEOUT                         (0x40000UL)

/* Define Write and read data length for the example */
#define TEST_DATA_LEN                   (256U)
/* Define i2c baudrate */
#define I2C_BAUDRATE                    (200000UL)   //200k
#define I2C_CLK_DIVx                    (I2C_CLK_DIV4)


int32_t I2C_Master_Transmit(uint16_t u16DevAddr, uint8_t const au8Data[], uint32_t u32Size, uint32_t u32Timeout);
int32_t I2C_Master_Receive(uint16_t u16DevAddr, uint8_t au8Data[], uint32_t u32Size, uint32_t u32Timeout);
int32_t Master_Initialize(void);
int32_t Chip_Receive(uint16_t u16DevAddr, uint8_t au8Data_tx[],uint8_t au8Data_rx[], uint32_t u32Size, uint32_t u32Timeout);
void i2c_init(void);

int32_t ADV_7280_Send_Buff(uint8_t const au8Data[], uint32_t u32Size, uint32_t u32Timeout);


#endif
