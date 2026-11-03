#include "i2c.h"

static void I2C_Reset(void)
{
    I2C_Cmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, ENABLE);
    I2C_SWResetCmd(I2C_UNIT, DISABLE);
}

static void I2C_End(uint32_t timeout)
{
    (void)I2C_Stop(I2C_UNIT, timeout);
    I2C_Cmd(I2C_UNIT, DISABLE);
    DDL_DelayUS(10);
}

int32_t I2C_Transmit(uint16_t addr, uint8_t const data[], uint32_t size, uint32_t timeout)
{
    int32_t ret;

    I2C_Reset();
    ret = I2C_Start(I2C_UNIT, timeout);

    if (ret == LL_OK)
    {
        ret = I2C_TransAddr(I2C_UNIT, addr >> 1, I2C_DIR_TX, timeout);
        DDL_DelayUS(10);
        if (ret == LL_OK)
            ret = I2C_TransData(I2C_UNIT, data, size, timeout);
    }

    I2C_End(timeout);
    return ret;
}

/* Every row is sent; the first error is returned (a later ACK must not hide an earlier NAK). A start that cannot
 * get the bus (held low: each wait runs the whole timeout, ~13 ms) ends the batch: the next rows would wait as long,
 * and a decoder init on a stuck bus would outlast the 2.68 s watchdog. The caller retries the group later. */
int32_t I2C_TransmitBatch(uint8_t const data[], uint32_t count, uint32_t timeout)
{
    int32_t ret = LL_OK, first = LL_OK;

    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t *cmd = &data[i * 3];

        I2C_Reset();
        ret = I2C_Start(I2C_UNIT, timeout);
        if (ret != LL_OK)
        {
            I2C_End(timeout);                /* as before: a half-made start still gets its stop (one more wait) */
            return first == LL_OK ? ret : first;
        }

        ret = I2C_TransAddr(I2C_UNIT, cmd[0] >> 1, I2C_DIR_TX, timeout);
        DDL_DelayUS(10);
        if (ret == LL_OK)
            ret = I2C_TransData(I2C_UNIT, &cmd[1], 2, timeout);

        I2C_End(timeout);
        if (first == LL_OK) first = ret;
    }

    return first;
}

int32_t I2C_Receive(uint16_t addr, uint8_t tx[], uint8_t rx[], uint32_t size, uint32_t timeout)
{
    int32_t ret;
    uint8_t dummy;

    if (rx == NULL)
        rx = &dummy;

    I2C_Reset();
    ret = I2C_Start(I2C_UNIT, timeout);

    if (ret == LL_OK)
    {
        ret = I2C_TransAddr(I2C_UNIT, addr >> 1, I2C_DIR_TX, timeout);
        DDL_DelayUS(10);
        if (ret == LL_OK)
            ret = I2C_TransData(I2C_UNIT, tx, size, timeout);
    }

    if (ret == LL_OK)                    /* a NAK on the address or the register stops here */
    {
        DDL_DelayUS(10);
        ret = I2C_Restart(I2C_UNIT, timeout);
    }

    if (ret == LL_OK)
    {
        if (size == 1)
            I2C_AckConfig(I2C_UNIT, I2C_NACK);

        ret = I2C_TransAddr(I2C_UNIT, addr >> 1, I2C_DIR_RX, timeout);
        DDL_DelayUS(10);

        if (ret == LL_OK)
            ret = I2C_MasterReceiveDataAndStop(I2C_UNIT, rx, size, timeout);

        I2C_AckConfig(I2C_UNIT, I2C_ACK);
    }

    if (ret != LL_OK)
        (void)I2C_Stop(I2C_UNIT, timeout);

    I2C_Cmd(I2C_UNIT, DISABLE);
    DDL_DelayUS(10);
    return ret;
}

int32_t I2C_ReadBurst(uint16_t addr, uint8_t reg, uint8_t rx[], uint32_t n, uint32_t timeout, uint8_t *stage)
{
    int32_t ret;
    uint8_t st = 1;   /* the step that failed (diagnostics in the reply): 1 start, 2 address, 3 register, 4 restart,
                         5 address for reading, 6 data */

    if (rx == NULL || n == 0U)
        return LL_ERR_INVD_PARAM;

    I2C_Reset();
    ret = I2C_Start(I2C_UNIT, timeout);

    if (ret == LL_OK)
    {
        st = 2;
        ret = I2C_TransAddr(I2C_UNIT, addr >> 1, I2C_DIR_TX, timeout);
        DDL_DelayUS(10);
        if (ret == LL_OK) { st = 3; ret = I2C_TransData(I2C_UNIT, &reg, 1U, timeout); }
    }

    if (ret == LL_OK)
    {
        st = 4;
        DDL_DelayUS(10);
        ret = I2C_Restart(I2C_UNIT, timeout);
    }

    if (ret == LL_OK)
    {
        if (n == 1U)
            I2C_AckConfig(I2C_UNIT, I2C_NACK);

        st = 5;
        ret = I2C_TransAddr(I2C_UNIT, addr >> 1, I2C_DIR_RX, timeout);
        DDL_DelayUS(10);

        if (ret == LL_OK) { st = 6; ret = I2C_MasterReceiveDataAndStop(I2C_UNIT, rx, n, timeout); }   /* NACK before the last byte */

        I2C_AckConfig(I2C_UNIT, I2C_ACK);
    }

    if (ret != LL_OK)
        (void)I2C_Stop(I2C_UNIT, timeout);

    I2C_Cmd(I2C_UNIT, DISABLE);
    DDL_DelayUS(10);
    if (stage) *stage = ret == LL_OK ? 0 : st;
    return ret;
}

#ifndef I2C_HOST_TEST   /* the peripheral set-up: device only (test/test_i2c.c fakes the transfers) */
static int32_t I2C_MasterInit(void)
{
    stc_i2c_init_t cfg;
    float32_t err;

    I2C_DeInit(I2C_UNIT);
    (void)I2C_StructInit(&cfg);
    cfg.u32ClockDiv = I2C_CLK_DIVx;
    cfg.u32Baudrate = I2C_BAUDRATE;
    cfg.u32SclTime  = 3UL;

    int32_t ret = I2C_Init(I2C_UNIT, &cfg, &err);
    I2C_BusWaitCmd(I2C_UNIT, ENABLE);

    return ret;
}

void I2C_DriverInit(void)
{
    GPIO_SetFunc(I2C_SCL_PORT, I2C_SCL_PIN, I2C_GPIO_SCL_FUNC);
    GPIO_SetFunc(I2C_SDA_PORT, I2C_SDA_PIN, I2C_GPIO_SDA_FUNC);
    FCG_Fcg1PeriphClockCmd(I2C_FCG_USE, ENABLE);

    if (I2C_MasterInit() != LL_OK)
    {
        BSP_LED_Off(LED_ALL);
        printf("I2C init failed\n");
        for (;;)
        {
            BSP_LED_Toggle(LED_RED);
            DDL_DelayMS(500);
        }
    }

    printf("I2C init OK\n");
}
#endif
