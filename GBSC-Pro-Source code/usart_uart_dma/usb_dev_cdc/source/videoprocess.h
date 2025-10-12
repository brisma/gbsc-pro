#ifndef _VIDEOPRCESS_H
#define _VIDEOPRCESS_H
#include "i2c.h"
#include "main.h"

#define OUTPUT_SWITCH_PORT GPIO_PORT_A //
#define OUTPUT_SWITCH_PIN GPIO_PIN_07  //

#define POWER_DOWN_PORT GPIO_PORT_B
#define INPUT_RESET_PORT GPIO_PORT_A
#define OUTPUT_PORT GPIO_PORT_B

#define GPIO_PIN_POWER_DOWN GPIO_PIN_00
#define GPIO_PIN_INPUT_RESET GPIO_PIN_03
#define GPIO_PIN_OUTPUT_EN GPIO_PIN_01

void video_init(void);
void video_Deinit(void);
void Video_Sw(uint8_t sw);

void set_input(uint8_t input);
void set_double_line(uint8_t doubleline);
void set_smooth(uint8_t smooth);
void set_bcsh(void);
void set_output(uint8_t output);
void set_ace(uint8_t ace);
void Read_Video_key_change(void);

void detect_loop(void);
void ASC(uint8_t sw1, uint8_t sw2, uint8_t sw3, uint8_t sw4, uint8_t state);
void ASC_Not02(uint8_t sw1, uint8_t sw2, uint8_t sw3, uint8_t sw4, uint8_t state);

void info(void);
void AV_Connecte_Set(uint8_t sw);

void Signal_led(uint8_t signal);

void Reser_Status(void);

#endif //_VIDEO_PRCESS_H
