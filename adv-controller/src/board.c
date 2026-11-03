#include "main.h"
#include "board.h"
#include "led.h"

bool asw_01, asw_02, asw_03, asw_04;
bool AVsw;

void ASW_SetSwitches(uint8_t sw1, uint8_t sw2, uint8_t sw3, uint8_t sw4)
{
    if (sw1) GPIO_SetPins(GPIO_PORT_ASW, GPIO_PIN_ASW1); else GPIO_ResetPins(GPIO_PORT_ASW, GPIO_PIN_ASW1);
    if (sw2) GPIO_SetPins(GPIO_PORT_ASW, GPIO_PIN_ASW2); else GPIO_ResetPins(GPIO_PORT_ASW, GPIO_PIN_ASW2);
    if (sw3) GPIO_SetPins(GPIO_PORT_ASW, GPIO_PIN_ASW3); else GPIO_ResetPins(GPIO_PORT_ASW, GPIO_PIN_ASW3);
    if (sw4) GPIO_SetPins(GPIO_PORT_ASW, GPIO_PIN_ASW4); else GPIO_ResetPins(GPIO_PORT_ASW, GPIO_PIN_ASW4);
}

void ASW_SetAVConnect(uint8_t sw)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    stcGpioInit.u16PinAttr = PIN_ATTR_DIGITAL;
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    if (sw) {
        stcGpioInit.u16PullUp = PIN_PU_OFF;
        stcGpioInit.u16PinState = PIN_STAT_RST;
        c_state = 1;
    } else {
        stcGpioInit.u16PullUp = PIN_PU_ON;
        stcGpioInit.u16PinState = PIN_STAT_SET;
        c_state = 2;
    }
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_08, &stcGpioInit);
}

void BOARD_VideoPowerUp(void)
{
    GPIO_SetPins(POWER_DOWN_PORT, GPIO_PIN_POWER_DOWN);
    DDL_DelayMS(5);
    GPIO_SetPins(INPUT_RESET_PORT, GPIO_PIN_INPUT_RESET);
    DDL_DelayMS(10);
    GPIO_SetPins(OUTPUT_PORT, GPIO_PIN_OUTPUT_EN);
    DDL_DelayMS(10);
}

void BOARD_VideoPowerDown(void)
{
    GPIO_ResetPins(POWER_DOWN_PORT, GPIO_PIN_POWER_DOWN);
    GPIO_ResetPins(INPUT_RESET_PORT, GPIO_PIN_INPUT_RESET);
    GPIO_ResetPins(OUTPUT_PORT, GPIO_PIN_OUTPUT_EN);
}
