/* Host tests of src/i2c.c against a fake DDL I2C master (make test) */
#include <stdio.h>
#include <string.h>
#include "i2c.h"
#undef printf   /* the shim silences the firmware printf; the test reports with it */

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

CM_I2C_TypeDef gFakeI2c1;
static int sStarts, sStops, sAddrs;
static int sBusStuck;          /* SDA held low: every start times out (the DDL waits for BUSY to clear) */
static int sStuckFromStart = -1;   /* the bus gets stuck from this start (0-based) on */
static int sNakAddrRow = -1;   /* this start (0-based) gets a NAK on the address */

static void fakeReset(void) { sStarts = sStops = sAddrs = 0; sBusStuck = 0; sStuckFromStart = -1; sNakAddrRow = -1; }
void I2C_Cmd(CM_I2C_TypeDef* i, int en) { (void)i; (void)en; }
void I2C_SWResetCmd(CM_I2C_TypeDef* i, int en) { (void)i; (void)en; }
int32_t I2C_Start(CM_I2C_TypeDef* i, uint32_t t) {
    (void)i; (void)t;
    if (sStuckFromStart >= 0 && sStarts >= sStuckFromStart) sBusStuck = 1;
    sStarts++;
    return sBusStuck ? LL_ERR_TIMEOUT : LL_OK;
}
int32_t I2C_Restart(CM_I2C_TypeDef* i, uint32_t t) { (void)i; (void)t; return LL_OK; }
int32_t I2C_Stop(CM_I2C_TypeDef* i, uint32_t t) { (void)i; (void)t; sStops++; return sBusStuck ? LL_ERR_TIMEOUT : LL_OK; }
int32_t I2C_TransAddr(CM_I2C_TypeDef* i, uint16_t a, uint8_t d, uint32_t t) {
    (void)i; (void)a; (void)d; (void)t; sAddrs++;
    return sStarts - 1 == sNakAddrRow ? LL_ERR : LL_OK;
}
int32_t I2C_TransData(CM_I2C_TypeDef* i, uint8_t const data[], uint32_t n, uint32_t t) { (void)i; (void)data; (void)n; (void)t; return LL_OK; }
int32_t I2C_MasterReceiveDataAndStop(CM_I2C_TypeDef* i, uint8_t data[], uint32_t n, uint32_t t) {
    (void)i; (void)t; memset(data, 0, n); return LL_OK;
}
void I2C_AckConfig(CM_I2C_TypeDef* i, uint32_t a) { (void)i; (void)a; }

static const uint8_t kBatch[] = {0x42, 0x0E, 0x00, 0x42, 0x0A, 0x10, 0x42, 0x08, 0x80, 0x42, 0x0B, 0x00};

/* A stuck bus costs two timeouts per row (~26 ms): the batch stops at the first start that cannot get the bus, so
 * a decoder init on a stuck bus stays far below the 2.68 s watchdog */
static void test_stuck_bus_stops_the_batch(void) {
    fakeReset(); sBusStuck = 1;
    int32_t r = I2C_TransmitBatch(kBatch, 4, TIMEOUT);
    CHECK(r != LL_OK);
    CHECK(sStarts == 1);
    CHECK(sStops == 1);                            /* the half-made start still gets its stop */
}

/* An earlier NAK is the error reported, not the timeout of the stuck bus after it */
static void test_nak_then_stuck_bus_reports_the_nak(void) {
    fakeReset(); sNakAddrRow = 0; sStuckFromStart = 2;
    int32_t r = I2C_TransmitBatch(kBatch, 4, TIMEOUT);
    CHECK(r == LL_ERR);
    CHECK(sStarts == 3);
}

/* A NAK is not a bus problem: the other rows are still sent and the first error is returned */
static void test_nak_keeps_sending_the_other_rows(void) {
    fakeReset(); sNakAddrRow = 1;
    int32_t r = I2C_TransmitBatch(kBatch, 4, TIMEOUT);
    CHECK(r == LL_ERR);
    CHECK(sStarts == 4 && sAddrs == 4);
}

static void test_good_batch(void) {
    fakeReset();
    CHECK(I2C_TransmitBatch(kBatch, 4, TIMEOUT) == LL_OK);
    CHECK(sStarts == 4 && sStops == 4);
}

int main(void) {
    test_stuck_bus_stops_the_batch();
    test_nak_keeps_sending_the_other_rows();
    test_nak_then_stuck_bus_reports_the_nak();
    test_good_batch();
    printf("i2c: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
