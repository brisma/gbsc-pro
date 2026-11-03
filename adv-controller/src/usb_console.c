#include "usb_console.h"
#include "usb_bsp.h"
#include "usb_cdc.h"
#include "usb_dev_desc.h"
#include "usb_dev_core.h"
#include "usb_guard.h"
#include "hc32_ll.h"

static volatile uint8_t sSuspended;
static int sUp;
static UsbGuard sGuard;
static uint32_t sWindowMs, sIrqAt, sRate;

/* User callbacks: VBUS forced valid (no sensing pin), DTR cleared on bus reset, suspend pauses output */
static void userInit(void) { CM_USBFS->GVBUSCFG |= USBFS_GVBUSCFG_VBUSOVEN | USBFS_GVBUSCFG_VBUSVAL; }
static void userReset(void) { usbCdcBusReset(); sSuspended = 0; }
static void userNone(void) {}
static void userSuspend(void) { sSuspended = 1; }
static void userResume(void) { sSuspended = 0; }
static usb_dev_user_func sUser = {userInit, userReset, userNone, userSuspend, userResume, userNone, userNone};

static int usbConnected(void) { return sUp && usbCdcDtr() && !sSuspended; }
static void usbWrite(const char *s, size_t n) { if (usbConnected()) (void)cdcRingWrite(&g_cdcTx, s, n); }
static size_t usbSpace(void) { return usbConnected() ? cdcRingFree(&g_cdcTx) : 4096U; }
static const ConsolePort kUsb = {usbWrite, usbConnected, usbSpace};

static int hookAhbIdle(void) { return (CM_USBFS->GRSTCTL & USBFS_GRSTCTL_AHBIDL) != 0UL; }
static void hookDevInit(void) { usb_dev_init(&g_usbDev, &g_usbConsoleDesc, &g_usbCdcClass, &sUser); }
static void hookGate(void)
{
    NVIC_DisableIRQ(INT030_IRQn);
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USBFS, DISABLE);
}
static const UsbStartHooks kStart = {usb_bsp_clock_start, hookAhbIdle, DDL_DelayUS, hookDevInit, hookGate};

const ConsolePort *USBCON_Init(void)
{
    usbCdcInitRings();
    /* the clock and the idle core are checked before usb_dev_init (its soft reset never times out) */
    sUp = usbStart(&kStart) && (CM_USBFS->GUSBCFG & USBFS_GUSBCFG_FDMOD);
    if (!sUp) {
        hookGate();
        return NULL;
    }
    return &kUsb;
}

void USBCON_Poll(uint32_t nowMs)
{
    if (!sUp) return;
    uint8_t buf[16];
    size_t n = cdcRingRead(&g_cdcRx, buf, sizeof(buf));
    for (size_t i = 0; i < n; i++) CONSOLE_Feed(1, (char)buf[i]);
    if (nowMs - sWindowMs >= 1000U) {                  /* interrupt rate shown by "info" */
        uint32_t irqs = usb_bsp_irq_count();
        sRate = irqs - sIrqAt;
        sIrqAt = irqs;
        sWindowMs = nowMs;
    }
    switch (usbGuardPoll(&sGuard, nowMs, usb_bsp_irq_non_sof(), usbCdcConfigured(), sSuspended)) {
        case USB_GUARD_DISCONNECT: CM_USBFS->DCTL |= USBFS_DCTL_SDIS; break;
        case USB_GUARD_RECONNECT: CM_USBFS->DCTL &= ~USBFS_DCTL_SDIS; break;
        default: break;
    }
}

int USBCON_Up(void) { return sUp; }
uint32_t USBCON_IrqRate(void) { return sRate; }
uint32_t USBCON_IrqMaxUs(void) { return usb_bsp_irq_max_cycles() / 200U; }
uint32_t USBCON_Dropped(void) { return g_cdcTx.dropped; }
