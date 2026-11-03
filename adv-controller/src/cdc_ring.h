/* Single-producer / single-consumer byte ring for the USB CDC console (DDL-free, host-tested).
 * TX: the main loop writes; the USB ISR peeks a contiguous block, sends it and consumes it only after the IN
 * transfer completed (the FIFO is filled later from the TXFE interrupt). RX: the ISR writes, the loop reads.
 * Full: the newest bytes are dropped (and counted), never the ones the reader may be using. */
#ifndef CDC_RING_H
#define CDC_RING_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t* buf;
    size_t size;
    volatile size_t head;       /* writer */
    volatile size_t tail;       /* reader */
    volatile uint32_t dropped;
} CdcRing;

void cdcRingInit(CdcRing* r, uint8_t* mem, size_t size);
size_t cdcRingFree(const CdcRing* r);
size_t cdcRingUsed(const CdcRing* r);
size_t cdcRingWrite(CdcRing* r, const void* data, size_t n);         /* bytes stored */
size_t cdcRingPeek(const CdcRing* r, const uint8_t** p, size_t max); /* contiguous bytes at the tail */
void cdcRingConsume(CdcRing* r, size_t n);
size_t cdcRingRead(CdcRing* r, uint8_t* out, size_t max);            /* peek + consume, for RX */
#endif
