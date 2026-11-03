/* Device glue of the console (HC32 DDL): USART3 port (non-blocking ring), USB CDC port (usb_console.c), I2C access */
#include "console_hc32.h"
#include "console.h"
#include "debug_uart.h"
#include "main.h"
#include "link.h"
#include "link_hal.h"
#include "console_fmt.h"
#include "usb_console.h"

#define I2C_TIMEOUT TIMEOUT   /* DDL polling tries: 1000 was shorter than one byte at 200 kHz (3be1653) */

static int uartConnected(void) { return 1; }
static const ConsolePort kUart = {DebugUart_Write, uartConnected, DebugUart_TxSpace};

static int i2cRead(uint8_t dev, uint8_t reg, uint8_t *val) { return I2C_Receive(dev, &reg, val, 1, I2C_TIMEOUT) == 0 ? 0 : -1; }
static int i2cWrite(uint8_t dev, uint8_t reg, uint8_t val)
{
    uint8_t cmd[2] = {reg, val};
    return I2C_Transmit(dev, cmd, 2, I2C_TIMEOUT) == 0 ? 0 : -1;
}

static void cmdInfo(uint8_t port, const char *args)
{
    (void)args;
    AdvInfo a;
    halInfo(&a);
    ConInfo i = {a.fw, a.buildId, a.uptimeS, LINK_RxFrames(), LINK_BadFrames(), LINK_DroppedReplies(),
                 LINK_AppliedSeq(), LINK_StateHash(), LINK_Input(), (a.flags & ADV_INFO_LOCK) ? 1U : 0U,
                 USBCON_Up(), USBCON_IrqRate(), USBCON_IrqMaxUs(), USBCON_Dropped()};
    char b[320];
    conFmtInfo(&i, b, sizeof(b));
    CONSOLE_Write(port, b);
}

static void cmdUpdate(uint8_t port, const char *args)
{
    (void)args;
    ConUpd u;
    u.state = LINK_UpdateState();
    u.err = LINK_UpdateError();
    LINK_UpdateProgress(&u.next, &u.size, &u.erased, &u.sectors);
    char b[96];
    conFmtUpdate(&u, b, sizeof(b));
    CONSOLE_Write(port, b);
}

void CONSOLE_HC32_Init(const ConsolePort *usb)
{
    CONSOLE_Init(&kUart, usb, i2cRead, i2cWrite);
    CONSOLE_AddCommand("info", cmdInfo);
    CONSOLE_AddCommand("update", cmdUpdate);
    CONSOLE_Banner(0);
    CONSOLE_Write(0, ">");
}

void CONSOLE_HC32_Poll(uint32_t nowMs)
{
    DebugUart_ClearErrors();
    char ch;
    for (uint8_t k = 0; k < 16U && DebugUart_ReceiveChar(&ch) == 0; k++) CONSOLE_Feed(0, ch);
    LinkEvent e;                             /* link events for "log on" ports, a few per pass */
    for (uint8_t k = 0; k < 4U && LINK_PopEvent(&e); k++) {
        char b[48];
        conFmtEvent(&e, b, sizeof(b));
        CONSOLE_Log("%s", b);
    }
    CONSOLE_Poll(nowMs);
    DebugUart_Poll();
}
