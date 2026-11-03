/* Host tests of the signal report (signal.c, make test): fields derived from STATUS_1/STATUS_3, debounce on the
 * derived fields, generation counter, powered-off state */
#include <stdio.h>
#include <string.h>
#include "signal.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static AdvSignal get(void) { AdvSignal s; SIG_Get(&s, 0x04, 2, 6); return s; }

static void test_powered_off(void) {
    SIG_Reset();
    SIG_Sample(0, 0, 0, 0x00, 0);
    AdvSignal s = get();
    CHECK(s.flags == 0);
    CHECK(s.standard == ADV_SIG_NO_STANDARD && s.hz == 0);
    CHECK(s.vidSel == 0x04 && s.encoder == 2 && s.input == 6);
}

static void test_lock_needs_two_equal_samples(void) {
    SIG_Reset();
    SIG_Sample(1, 1, 0x45, 0x00, 0x44);            /* PAL locked, fsc, 50 Hz, interlaced */
    AdvSignal s = get();
    CHECK(!(s.flags & ADV_SIG_LOCK));         /* one sample: not accepted yet */
    CHECK(s.flags & ADV_SIG_POWERED);
    CHECK(s.status1 == 0x45 && s.status3 == 0x44);   /* raw bytes are always the last read */
    uint8_t g0 = s.gen;
    SIG_Sample(1, 1, 0x45, 0x00, 0x44);
    s = get();
    CHECK(s.flags & ADV_SIG_LOCK);
    CHECK(s.flags & ADV_SIG_PRESENT);
    CHECK(s.flags & ADV_SIG_FSC);
    CHECK(s.flags & ADV_SIG_INTERLACED);
    CHECK(!(s.flags & ADV_SIG_FREERUN));
    CHECK(s.flags2 & ADV_SIG2_ACK);
    CHECK(s.standard == 4 && s.hz == 50);
    CHECK(s.gen == (uint8_t)(g0 + 1));
}

static void test_latched_lost_lock_does_not_churn(void) {
    SIG_Reset();
    SIG_Sample(1, 1, 0x05, 0x00, 0x00); SIG_Sample(1, 1, 0x05, 0x00, 0x00);   /* NTSC 60 Hz */
    uint8_t g = get().gen;
    SIG_Sample(1, 1, 0x07, 0x00, 0x00);            /* LOST_LOCK latched once, still locked */
    SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    AdvSignal s = get();
    CHECK(s.gen == g);
    CHECK(s.standard == 0 && s.hz == 60);
}

static void test_free_run_is_not_present(void) {
    SIG_Reset();
    SIG_Sample(1, 1, 0x01, 0x00, 0x10); SIG_Sample(1, 1, 0x01, 0x00, 0x10);   /* lock bit with FREE_RUN_ACT */
    AdvSignal s = get();
    CHECK(s.flags & ADV_SIG_FREERUN);
    CHECK(!(s.flags & ADV_SIG_PRESENT));
}

static void test_no_lock_has_no_standard(void) {
    SIG_Reset();
    SIG_Sample(1, 1, 0x40, 0x00, 0x04); SIG_Sample(1, 1, 0x40, 0x00, 0x04);   /* AD_RESULT bits without IN_LOCK */
    AdvSignal s = get();
    CHECK(!(s.flags & ADV_SIG_LOCK));
    CHECK(s.standard == ADV_SIG_NO_STANDARD && s.hz == 0);
}

static void test_flicker_is_not_accepted(void) {
    SIG_Reset();
    SIG_Sample(1, 1, 0x05, 0x00, 0x00); SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    uint8_t g = get().gen;
    SIG_Sample(1, 1, 0x45, 0x00, 0x04);            /* one PAL sample between NTSC ones */
    SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    CHECK(get().gen == g && get().standard == 0);
}

static void test_power_off_is_accepted_at_once(void) {
    SIG_Reset();
    SIG_Sample(1, 1, 0x05, 0x00, 0x00); SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    uint8_t g = get().gen;
    SIG_Sample(0, 0, 0, 0x00, 0);
    AdvSignal s = get();
    CHECK(s.flags == 0 && s.standard == ADV_SIG_NO_STANDARD);
    CHECK(s.gen == (uint8_t)(g + 1));
    SIG_Sample(0, 0, 0, 0x00, 0);                  /* steady: no new generation */
    CHECK(get().gen == (uint8_t)(g + 1));
}

static void test_no_ack(void) {
    SIG_Reset();
    SIG_Sample(1, 0, 0x45, 0x00, 0x44); SIG_Sample(1, 0, 0x45, 0x00, 0x44);   /* I2C not acknowledged: values unknown */
    AdvSignal s = get();
    CHECK(s.flags == ADV_SIG_POWERED);
    CHECK(!(s.flags2 & ADV_SIG2_ACK));
}

static void test_status2_nonstandard(void) {   /* STATUS_2 0x12 read every tick, LL_NSTD bit 4 */
    SIG_Reset();
    SIG_Sample(1, 1, 0x05, 0x10, 0x00); SIG_Sample(1, 1, 0x05, 0x10, 0x00);   /* NTSC, non-standard line length */
    AdvSignal s = get();
    CHECK(s.flags & ADV_SIG_NONSTD);
    CHECK(s.status2 == 0x10);
    SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    s = get();
    CHECK(s.flags & ADV_SIG_NONSTD);          /* one sample: not accepted yet */
    SIG_Sample(1, 1, 0x05, 0x00, 0x00);
    s = get();
    CHECK(!(s.flags & ADV_SIG_NONSTD));
    CHECK(s.status2 == 0x00);
    SIG_Sample(0, 0, 0, 0x10, 0);             /* powered off: no flag, no raw byte */
    s = get();
    CHECK(!(s.flags & ADV_SIG_NONSTD) && s.status2 == 0);
}

int main(void) {
    test_status2_nonstandard();
    test_powered_off();
    test_lock_needs_two_equal_samples();
    test_latched_lost_lock_does_not_churn();
    test_free_run_is_not_present();
    test_no_lock_has_no_standard();
    test_flicker_is_not_accepted();
    test_power_off_is_accepted_at_once();
    test_no_ack();
    printf("signal: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
