/* Host tests of the USB start sequence and storm guard (make test): the core is never initialised when the
 * clock or the AHB idle check fails (bounded wait, clock gated), and the guard ignores SOF interrupts */
#include <stdio.h>
#include <string.h>
#include "usb_guard.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int gClockOk, gIdleAfter, gIdlePolls, gDevInit, gGated, gDelays;
static int clockStart(void) { return gClockOk; }
static int ahbIdle(void) { return ++gIdlePolls > gIdleAfter; }
static void delayUs(uint32_t us) { (void)us; gDelays++; }
static void devInit(void) { gDevInit++; }
static void gate(void) { gGated++; }
static const UsbStartHooks kHooks = {clockStart, ahbIdle, delayUs, devInit, gate};

static void reset(int clockOk, int idleAfter) {
    gClockOk = clockOk; gIdleAfter = idleAfter; gIdlePolls = gDevInit = gGated = gDelays = 0;
}

static void test_start_ok(void) {
    reset(1, 3);
    CHECK(usbStart(&kHooks) == 1);
    CHECK(gDevInit == 1 && gGated == 0);
}

static void test_start_dead_clock_never_touches_the_core(void) {
    reset(0, 0);
    CHECK(usbStart(&kHooks) == 0);
    CHECK(gDevInit == 0 && gGated == 1 && gIdlePolls == 0);
}

static void test_start_core_never_idle_is_bounded(void) {
    reset(1, 1000000);
    CHECK(usbStart(&kHooks) == 0);
    CHECK(gDevInit == 0 && gGated == 1);
    CHECK((uint32_t)gIdlePolls <= USB_IDLE_POLLS);
    CHECK((uint32_t)gDelays * USB_IDLE_STEP_US <= 10000U);   /* boot keeps going within 10 ms */
}

/* Guard: one Poll per second with the interrupt counters since boot */
static void test_sof_only_traffic_never_disconnects(void) {
    UsbGuard g; memset(&g, 0, sizeof(g));
    uint32_t other = 0;
    for (uint32_t t = 1000; t <= 10000; t += 1000) {
        other += 20;                                      /* addressed, not configured: 1000 SOF (not counted) + a few setups */
        CHECK(usbGuardPoll(&g, t, other, 0, 0) == USB_GUARD_NONE);
    }
}

static void test_non_sof_storm_disconnects_then_reconnects(void) {
    UsbGuard g; memset(&g, 0, sizeof(g));
    CHECK(usbGuardPoll(&g, 1000, 5000, 0, 0) == USB_GUARD_DISCONNECT);
    CHECK(usbGuardPoll(&g, 1500, 9000, 0, 0) == USB_GUARD_NONE);       /* waiting */
    CHECK(usbGuardPoll(&g, 2000, 9000, 0, 0) == USB_GUARD_RECONNECT);
}

static void test_storm_while_configured_and_suspended(void) {
    UsbGuard g; memset(&g, 0, sizeof(g));
    CHECK(usbGuardPoll(&g, 1000, 5000, 1, 0) == USB_GUARD_NONE);       /* a host using the port */
    CHECK(usbGuardPoll(&g, 2000, 10000, 1, 1) == USB_GUARD_DISCONNECT); /* unplugged: suspended, still configured */
}

int main(void) {
    test_start_ok();
    test_start_dead_clock_never_touches_the_core();
    test_start_core_never_idle_is_bounded();
    test_sof_only_traffic_never_disconnects();
    test_non_sof_storm_disconnects_then_reconnects();
    test_storm_while_configured_and_suspended();
    printf("usb_guard: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
