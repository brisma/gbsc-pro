#ifndef GOLDEN_TRACE_H
#define GOLDEN_TRACE_H
#include <stdint.h>
#define GOLDEN_MAX 4096
extern char gTrace[GOLDEN_MAX][40];
extern int gTraceN;
extern uint8_t gReadValue[256];
extern int gReadFail;   /* reads NAK (return LL_ERR) and leave 0x40 (PAL bits) in rx: an uninitialised byte */
extern int gWriteFail;  /* batch writes NAK (return LL_ERR), still traced */
extern int gWriteFailReg;  /* >= 0: only writes to this register NAK; a batch returns the first error */
extern int gWriteFailAddr; /* >= 0: gWriteFailReg only at this device address */
void GOLDEN_Reset(void);
void GOLDEN_Note(const char* s);
#endif
