/* link_hal.h on the HC32: time, INFO, replies on the shared wire, update staging, RX ring polling (the ADV and
 * switch half is link_hal_adv.c) */
#include "main.h"
#include "uart_dma.h"
#include "link.h"
#include "link_hal.h"
#include "fault_handler.h"
#include "fw_header.h"
#include "fw_copy.h"

extern volatile uint32_t g_u32_tick10us, g_u32_ms;

uint32_t halTimeUs(void) { return g_u32_tick10us * 10U; }

void halInfo(AdvInfo* out)
{
    memset(out, 0, sizeof(*out));
    strncpy(out->fw, ADV_FW_VERSION, sizeof(out->fw) - 1);
    out->buildId = fwBuildId();
    if (ADV_LastStatus1() & 0x01) out->flags |= ADV_INFO_LOCK;
    if (ADV_LastStatus1() & 0x04) out->flags |= ADV_INFO_FSC_LOCK;
    if (ADV_LastAck()) out->flags |= ADV_INFO_ADV_PRESENT;
    if (ADV_Powered()) out->flags |= ADV_INFO_ADV_POWERED;
    out->standard = (uint8_t)((ADV_LastStatus1() >> 4) & 0x07);
    out->uptimeS = (uint16_t)(g_u32_ms / 1000U);
}

/* Reply on the shared wire: PB7 becomes USART4 TX at 38400 for the frame, then RX again */
uint32_t halReply(const uint8_t* frame, uint8_t n, uint32_t notBeforeUs, uint32_t deadlineUs)
{
    while ((int32_t)(halTimeUs() - notBeforeUs) < 0) {}            /* turnaround: the ESP releases the line */
    if ((int32_t)(halTimeUs() - deadlineUs) > 0) return 0;          /* too late: the ESP stopped listening */
    USART_FuncCmd(USART_UNIT, (USART_RX | USART_INT_RX), DISABLE);
    (void)USART_SetBaudrate(USART_UNIT, 38400UL, NULL);
    USART_FuncCmd(USART_UNIT, USART_TX, ENABLE);                    /* TX idles high before it reaches the pin */
    GPIO_SetFunc(USART_RX_PORT, USART_RX_PIN, GPIO_FUNC_36);
    for (int16_t i = -1; i < n; i++) {
        while (USART_GetStatus(USART_UNIT, USART_FLAG_TX_EMPTY) == RESET) {}
        USART_WriteData(USART_UNIT, i < 0 ? 0xFFU : frame[i]);      /* leading 0xFF: start-bit resync */
    }
    while (USART_GetStatus(USART_UNIT, USART_FLAG_TX_CPLT) == RESET) {}
    GPIO_SetFunc(USART_RX_PORT, USART_RX_PIN, USART_RX_GPIO_FUNC);
    USART_FuncCmd(USART_UNIT, USART_TX, DISABLE);
    (void)USART_SetBaudrate(USART_UNIT, USART_BAUDRATE, NULL);
    USART_ClearStatus(USART_UNIT, (USART_FLAG_OVERRUN | USART_FLAG_FRAME_ERR | USART_FLAG_PARITY_ERR));
    USART_FuncCmd(USART_UNIT, (USART_RX | USART_INT_RX), ENABLE);
    return halTimeUs() | 1U;                                        /* never 0 (0 = dropped) */
}

/* ---- firmware update staging ---- */
#define STAGING_BASE 0x00040000UL

/* Flash program/erase stays locked outside these calls: EFM register write protection and the FWMC program
 * enable are opened around each operation only */
static void flashUnlock(void)
{
    LL_PERIPH_WE(LL_PERIPH_EFM);
    EFM_FWMC_Cmd(ENABLE);
}

static void flashLock(void)
{
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(LL_PERIPH_EFM);
}

int halStagingErase(uint8_t sector)
{
    WDT_Refresh();
    flashUnlock();
    int32_t r = EFM_SectorErase(STAGING_BASE + (uint32_t)sector * LINK_SECTOR_SIZE);
    flashLock();
    return r == LL_OK ? 0 : -1;
}

int halStagingProgram(uint32_t offset, const uint8_t* data, uint8_t len)
{
    static uint32_t buf[ADV_UPD_CHUNK / 4];       /* word aligned copy (the frame payload is not) */
    uint32_t words = (len + 3U) / 4U;
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, data, len);
    flashUnlock();
    __disable_irq();                              /* no ISR fetch from flash during the sequence program */
    int32_t r = EFM_SequenceProgram(STAGING_BASE + offset, (uint8_t*)buf, words * 4U);
    __enable_irq();
    flashLock();
    return r == LL_OK ? 0 : -1;
}

const uint8_t* halStagingPtr(void) { return (const uint8_t*)STAGING_BASE; }

void halCopyAndReset(uint32_t size) { hc32CopyAndReset(size); }

void LINK_Poll(void)
{
    static uint16_t rd = 0;
    LINK_PollBegin(halTimeUs());                                    /* taken before the write index */
    uint16_t wr = UART_RxWriteIndex();
    while (rd != wr) {
        LINK_Feed(dma_au8RxBuf[rd]);
        rd = (uint16_t)((rd + 1U) % APP_FRAME_LEN_MAX);
    }
    LINK_Process();   /* only the newest STATE: a burst of input changes costs one ADV_Init */
    LINK_Tick();      /* staging erase (one sector per pass), the copy after a confirmed COMMIT */
}
