/* ADV Controller side of the link v3: frames
 * from the ESP are parsed byte by byte; STATE is applied as a diff against the current state. DDL-free (host
 * tests). */
#ifndef LINK_H
#define LINK_H
#include <stdint.h>

void LINK_Init(void);
void LINK_Feed(uint8_t b);           /* one received byte (a STATE is kept, applied by LINK_Process) */
void LINK_Process(void);             /* applies the newest STATE received since the last call */
uint8_t LINK_AppliedSeq(void);       /* STATE frames received (wraps) */
uint16_t LINK_BadFrames(void);       /* frames dropped: CRC, length or invalid state */
void LINK_Poll(void);                /* HC32 only (link_hal_hc32.c): feeds the bytes the RX DMA received */
/* Start of a poll, with the time taken right before the DMA write index is read: a request is answered only
 * if the previous poll began <= 8 ms earlier (the frame is fresh enough to answer inside the ESP's window) */
void LINK_PollBegin(uint32_t nowUs);
uint16_t LINK_DroppedReplies(void);  /* replies not sent: stale frame or deadline passed */
void LINK_Tick(void);                /* every loop pass after LINK_Process: staging erase, copy after COMMIT */
uint8_t LINK_UpdateState(void);      /* ADV_UST_* (tests, diagnostics) */

/* Console diagnostics (USB console): counters, update progress, and an event ring read after LINK_Poll
 * (the frame callback only stores the event: nothing is formatted before the reply goes out) */
enum { LINK_UPD_ERR_NONE, LINK_UPD_ERR_PROGRAM, LINK_UPD_ERR_ERASE, LINK_UPD_ERR_CRC, LINK_UPD_ERR_HEADER };
enum { LINK_EV_FRAME = 1, LINK_EV_STATE = 2 };
#define LINK_EVENT_RING 16
typedef struct { uint8_t kind, cmd, seq, len, steps, failed; } LinkEvent;   /* failed: STATE groups whose I2C failed */
uint16_t LINK_RxFrames(void);
uint8_t LINK_Input(void);            /* input of the applied STATE, 0 before the first */
uint16_t LINK_StateHash(void);       /* 0 before the first STATE */
void LINK_UpdateProgress(uint32_t* next, uint32_t* size, uint8_t* erased, uint8_t* sectors);
uint8_t LINK_UpdateError(void);      /* LINK_UPD_ERR_*: why the last session failed */
int LINK_PopEvent(LinkEvent* out);   /* 1 when an event was returned */

#endif
