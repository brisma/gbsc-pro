/* Host tests of the CDC TX/RX rings (make test): the main loop writes, the USB ISR sends a contiguous
 * block and consumes it only after the IN transfer completed; drop-newest when full */
#include <stdio.h>
#include <string.h>
#include "cdc_ring.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_write_peek_consume(void) {
    static uint8_t mem[16];
    CdcRing r; cdcRingInit(&r, mem, sizeof(mem));
    CHECK(cdcRingFree(&r) == 15);
    CHECK(cdcRingWrite(&r, "hello", 5) == 5);
    const uint8_t* p; size_t n = cdcRingPeek(&r, &p, 63);
    CHECK(n == 5 && !memcmp(p, "hello", 5));
    CHECK(cdcRingFree(&r) == 10);                 /* peeked bytes are still owned by the ring */
    cdcRingConsume(&r, n);
    CHECK(cdcRingFree(&r) == 15 && cdcRingPeek(&r, &p, 63) == 0);
}

static void test_wrap_gives_contiguous_blocks(void) {
    static uint8_t mem[16];
    CdcRing r; cdcRingInit(&r, mem, sizeof(mem));
    cdcRingWrite(&r, "0123456789AB", 12);
    const uint8_t* p; size_t n = cdcRingPeek(&r, &p, 63);
    cdcRingConsume(&r, n);
    cdcRingWrite(&r, "abcdefgh", 8);              /* wraps after 4 bytes */
    n = cdcRingPeek(&r, &p, 63);
    CHECK(n == 4 && !memcmp(p, "abcd", 4));
    cdcRingConsume(&r, n);
    n = cdcRingPeek(&r, &p, 63);
    CHECK(n == 4 && !memcmp(p, "efgh", 4));
}

static void test_peek_limit(void) {
    static uint8_t mem[256];
    CdcRing r; cdcRingInit(&r, mem, sizeof(mem));
    uint8_t big[200]; memset(big, 'x', sizeof(big));
    cdcRingWrite(&r, big, sizeof(big));
    const uint8_t* p;
    CHECK(cdcRingPeek(&r, &p, 63) == 63);         /* never a full 64-byte packet: no zero-length packet needed */
}

static void test_drop_newest_counts(void) {
    static uint8_t mem[8];
    CdcRing r; cdcRingInit(&r, mem, sizeof(mem));
    CHECK(cdcRingWrite(&r, "abcdefghij", 10) == 7);
    CHECK(r.dropped == 3);
    const uint8_t* p; size_t n = cdcRingPeek(&r, &p, 63);
    CHECK(n == 7 && !memcmp(p, "abcdefg", 7));    /* the oldest bytes stay (the ISR may be sending them) */
}

int main(void) {
    test_write_peek_consume();
    test_wrap_gives_contiguous_blocks();
    test_peek_limit();
    test_drop_newest_counts();
    printf("cdc_ring: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
