/* USB start sequence and storm guard (DDL-free, host-tested).
 * usbStart: the vendor core's soft reset spins forever when the USBFS clock is off or the AHB never goes idle,
 * so the clock and the idle bit are checked here first, with a bounded wait, before usb_dev_init runs.
 * usbGuardPoll: once a second; a burst of non-SOF interrupts while no host uses the port (not configured, or
 * suspended after an unplug) soft-disconnects the device for 1 s. SOF interrupts are ignored: a host that keeps
 * the device addressed but not configured (driver install, BIOS) sends 1000 SOF per second legitimately. */
#ifndef USB_GUARD_H
#define USB_GUARD_H
#include <stdint.h>

#define USB_IDLE_POLLS   1000U
#define USB_IDLE_STEP_US 10U                 /* 10 ms at most for the AHB idle wait */
#define USB_STORM_RATE   1000U               /* non-SOF interrupts per second */

typedef struct {
    int (*clockStart)(void);                 /* UPLL 48 MHz + USBFS clock; 0 = failed */
    int (*ahbIdle)(void);                    /* GRSTCTL.AHBIDL */
    void (*delayUs)(uint32_t us);
    void (*devInit)(void);                   /* usb_dev_init */
    void (*gate)(void);                      /* USBFS clock off, IRQ disabled */
} UsbStartHooks;

int usbStart(const UsbStartHooks *h);        /* 1 = the core is up */

enum { USB_GUARD_NONE, USB_GUARD_DISCONNECT, USB_GUARD_RECONNECT };
typedef struct {
    uint32_t windowMs, irqAt, rate, reconnectAt;
    uint8_t disconnected;
} UsbGuard;

int usbGuardPoll(UsbGuard *g, uint32_t nowMs, uint32_t nonSofIrqs, int configured, int suspended);
#endif
