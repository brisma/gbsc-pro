/* Board wiring: analog input switches (ASW1-4), AV connect (PA8), decoder/encoder power, reset and output
 * enable pins. GPIO only, no I2C. */
#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stdbool.h>

/* Analog switches */
#define GPIO_PORT_ASW GPIO_PORT_B
#define GPIO_PIN_ASW1 GPIO_PIN_15
#define GPIO_PIN_ASW2 GPIO_PIN_14
#define GPIO_PIN_ASW3 GPIO_PIN_13
#define GPIO_PIN_ASW4 GPIO_PIN_12

/* Video chips: ADV power down (PB00), ADV7280 reset (PA03), ADV7391 output enable (PB01) */
#define POWER_DOWN_PORT      GPIO_PORT_B
#define GPIO_PIN_POWER_DOWN  GPIO_PIN_00
#define INPUT_RESET_PORT     GPIO_PORT_A
#define GPIO_PIN_INPUT_RESET GPIO_PIN_03
#define OUTPUT_PORT          GPIO_PORT_B
#define GPIO_PIN_OUTPUT_EN   GPIO_PIN_01

/* Last switch values (boot default and every STATE) */
extern bool asw_01, asw_02, asw_03, asw_04;
extern bool AVsw;

void ASW_SetSwitches(uint8_t sw1, uint8_t sw2, uint8_t sw3, uint8_t sw4);
void ASW_SetAVConnect(uint8_t sw);       /* 1: AV connector on (PA8 low, no pull-up) */
void BOARD_VideoPowerUp(void);           /* power, reset release, output enable (5/10/10 ms) */
void BOARD_VideoPowerDown(void);

#endif
