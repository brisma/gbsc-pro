/* Console text for info / update / log events (DDL-free, host-tested) */
#ifndef CONSOLE_FMT_H
#define CONSOLE_FMT_H
#include <stddef.h>
#include <stdint.h>
#include "link.h"

typedef struct {
    const char* fw;
    uint32_t build, uptimeS;
    uint16_t rx, bad, dropped;
    uint8_t applied;
    uint16_t hash;           /* advStateHash: 16 bits */
    uint8_t input, lock;
    int usbUp;
    uint32_t irqRate, isrUs, usbDropped;
} ConInfo;

typedef struct { uint8_t state, err; uint32_t next, size; uint8_t erased, sectors; } ConUpd;

void conFmtInfo(const ConInfo* i, char* out, size_t n);
void conFmtUpdate(const ConUpd* u, char* out, size_t n);
void conFmtEvent(const LinkEvent* e, char* out, size_t n);
#endif
