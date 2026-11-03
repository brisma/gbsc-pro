#include "usb_guard.h"

int usbStart(const UsbStartHooks *h)
{
    if (h->clockStart()) {
        for (uint32_t i = 0; i < USB_IDLE_POLLS; i++) {
            if (h->ahbIdle()) { h->devInit(); return 1; }
            h->delayUs(USB_IDLE_STEP_US);
        }
    }
    h->gate();
    return 0;
}

int usbGuardPoll(UsbGuard *g, uint32_t nowMs, uint32_t nonSofIrqs, int configured, int suspended)
{
    if (nowMs - g->windowMs >= 1000U) {
        g->rate = nonSofIrqs - g->irqAt;
        g->irqAt = nonSofIrqs;
        g->windowMs = nowMs;
        if (g->rate > USB_STORM_RATE && (!configured || suspended) && !g->disconnected) {
            g->disconnected = 1;
            g->reconnectAt = nowMs + 1000U;
            return USB_GUARD_DISCONNECT;
        }
    }
    if (g->disconnected && (int32_t)(nowMs - g->reconnectAt) >= 0) {
        g->disconnected = 0;
        return USB_GUARD_RECONNECT;
    }
    return USB_GUARD_NONE;
}
