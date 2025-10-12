#include "main.h"
#include "i2c.h"

int32_t I2C_Master_Transmit(uint16_t u16DevAddr, uint8_t const au8Data[], uint32_t u32Size, uint32_t u32Timeout)
{
    int32_t i32Ret;
    I2C_Cmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, DISABLE);
    i32Ret = I2C_Start(I2C_UNIT, u32Timeout);
    if (LL_OK == i32Ret)
    {
#if (I2C_ADDR_MD == I2C_ADDR_MD_10BIT)
        i32Ret = I2C_Trans10BitAddr(I2C_UNIT, u16DevAddr, I2C_DIR_TX, u32Timeout);
#else
        i32Ret = I2C_TransAddr(I2C_UNIT, u16DevAddr >> 1, I2C_DIR_TX, u32Timeout);
#endif
        DDL_DelayUS(10);
        if (LL_OK == i32Ret)
        {
            i32Ret = I2C_TransData(I2C_UNIT, au8Data, u32Size, u32Timeout);
        }
    }

    (void)I2C_Stop(I2C_UNIT, u32Timeout);
    I2C_Cmd(I2C_UNIT, DISABLE);
    DDL_DelayUS(10);
    return i32Ret;
}

int32_t ADV_7280_Send_Buff(uint8_t const au8Data[], uint32_t u32Size, uint32_t u32Timeout)
{
    int32_t i32Count = 0;
    int32_t i32Ret;

    //    printf("i2c_buff_start\n");
    while ((i32Count) < u32Size)
    {
        I2C_Cmd(I2C_UNIT, ENABLE);
        I2C_SWResetCmd(I2C_UNIT, ENABLE);
        I2C_SWResetCmd(I2C_UNIT, DISABLE);
        i32Ret = I2C_Start(I2C_UNIT, u32Timeout);
        if (LL_OK == i32Ret)
        {
            i32Ret = I2C_TransAddr(I2C_UNIT, (uint16_t)au8Data[i32Count * 3] >> 1, I2C_DIR_TX, u32Timeout);
            DDL_DelayUS(10);
            if (LL_OK == i32Ret)
            {
                i32Ret = I2C_TransData(I2C_UNIT, &au8Data[i32Count * 3 + 1], 2, u32Timeout);
            }
        }
        (void)I2C_Stop(I2C_UNIT, u32Timeout);
        I2C_Cmd(I2C_UNIT, DISABLE);
        DDL_DelayUS(10);
        i32Count++;
    }

    //    printf("i2c_buff_end\n");
    return i32Ret;
}
/**
 * @brief  Master receive data
 *
 * @param  [in] u16DevAddr          The slave address
 * @param  [in] au8Data             The data array
 * @param  [in] u32Size             Data size
 * @param  [in] u32Timeout          Time out count
 * @retval int32_t:
 *            - LL_OK:              Success
 *            - LL_ERR_TIMEOUT:     Time out
 */
int32_t I2C_Master_Receive(uint16_t u16DevAddr, uint8_t au8Data[], uint32_t u32Size, uint32_t u32Timeout)
{
    int32_t i32Ret;

    I2C_Cmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, DISABLE);
    i32Ret = I2C_Start(I2C_UNIT, u32Timeout);
    if (LL_OK == i32Ret)
    {
        if (1UL == u32Size)
        {
            I2C_AckConfig(I2C_UNIT, I2C_NACK);
        }

#if (I2C_ADDR_MD == I2C_ADDR_MD_10BIT)
        i32Ret = I2C_Trans10BitAddr(I2C_UNIT, u16DevAddr, I2C_DIR_RX, u32Timeout);
#else
        i32Ret = I2C_TransAddr(I2C_UNIT, u16DevAddr >> 1, I2C_DIR_RX, u32Timeout);
#endif
        DDL_DelayUS(10);
        if (LL_OK == i32Ret)
        {
            i32Ret = I2C_MasterReceiveDataAndStop(I2C_UNIT, au8Data, u32Size, u32Timeout);
        }

        I2C_AckConfig(I2C_UNIT, I2C_ACK);
    }

    if (LL_OK != i32Ret)
    {
        (void)I2C_Stop(I2C_UNIT, u32Timeout);
    }
    I2C_Cmd(I2C_UNIT, DISABLE);
    return i32Ret;
}

int32_t Chip_Receive(uint16_t u16DevAddr, uint8_t au8Data_tx[], uint8_t au8Data_rx[], uint32_t u32Size, uint32_t u32Timeout)
{
    int32_t i32Ret;
    uint8_t field;
    if (au8Data_rx == NULL)
    {
        au8Data_rx = &field;
    }

    I2C_Cmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, DISABLE);
    i32Ret = I2C_Start(I2C_UNIT, u32Timeout);
    if (LL_OK == i32Ret)
    {
        i32Ret = I2C_TransAddr(I2C_UNIT, u16DevAddr >> 1, I2C_DIR_TX, u32Timeout);
        DDL_DelayUS(10);
        if (LL_OK == i32Ret)
        {
            i32Ret = I2C_TransData(I2C_UNIT, au8Data_tx, u32Size, u32Timeout);
        }
    }
    DDL_DelayUS(10);
    // Receive
    i32Ret = I2C_Restart(I2C_UNIT, u32Timeout);

    if (LL_OK == i32Ret)
    {
        if (1UL == u32Size)
        {
            I2C_AckConfig(I2C_UNIT, I2C_NACK);
        }

        i32Ret = I2C_TransAddr(I2C_UNIT, u16DevAddr >> 1, I2C_DIR_RX, u32Timeout);
        DDL_DelayUS(10);
        if (LL_OK == i32Ret)
        {
            i32Ret = I2C_MasterReceiveDataAndStop(I2C_UNIT, au8Data_rx, u32Size, u32Timeout);
        }

        I2C_AckConfig(I2C_UNIT, I2C_ACK);
    }

    if (LL_OK != i32Ret)
    {
        (void)I2C_Stop(I2C_UNIT, u32Timeout);
    }

    I2C_Cmd(I2C_UNIT, DISABLE);
    DDL_DelayUS(10);
    //    printf("i2c_tx_state :%d\n",i32Ret);
    return i32Ret;
}

/**
 * @brief  Initialize the I2C peripheral for master
 * @param  None
 * @retval int32_t:
 *            - LL_OK:                  Success
 *            - LL_ERR_INVD_PARAM:      Invalid parameter
 */
int32_t Master_Initialize(void)
{
    int32_t i32Ret;
    stc_i2c_init_t stcI2cInit;
    float32_t fErr;

    I2C_DeInit(I2C_UNIT);

    (void)I2C_StructInit(&stcI2cInit);
    stcI2cInit.u32ClockDiv = I2C_CLK_DIVx;
    stcI2cInit.u32Baudrate = I2C_BAUDRATE;
    stcI2cInit.u32SclTime = 3UL;
    i32Ret = I2C_Init(I2C_UNIT, &stcI2cInit, &fErr);

    I2C_BusWaitCmd(I2C_UNIT, ENABLE);
    printf("err_code:%d\n", i32Ret);
    return i32Ret;
}

void i2c_init(void)
{
    GPIO_SetFunc(I2C_SCL_PORT, I2C_SCL_PIN, I2C_GPIO_SCL_FUNC);
    GPIO_SetFunc(I2C_SDA_PORT, I2C_SDA_PIN, I2C_GPIO_SDA_FUNC);

    FCG_Fcg1PeriphClockCmd(I2C_FCG_USE, ENABLE);

    if (LL_OK != Master_Initialize())
    {
        BSP_LED_Off(LED_ALL);
        printf("I2C_ERR \n");
        for (;;)
        {
            BSP_LED_Toggle(LED_RED);
            DDL_DelayMS(500);
        }
    }

    //    I2C_SlaveAddrConfig(CM_I2C1,I2C_ADDR0,I2C_ADDR_7BIT,DEVICE_ADDR);   //set address
    //    I2C_SlaveAddrCmd(CM_I2C1,I2C_ADDR0,ENABLE);
    printf("I2C_init ok\n");
}
