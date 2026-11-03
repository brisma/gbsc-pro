#include <string.h>
#include "signal.h"

/* Derived fields: debounced as a whole (raw bytes carry the latched LOST_LOCK bit) */
typedef struct { uint8_t flags, standard, hz; } Fields;

static Fields sAccepted, sCandidate;
static uint8_t sCandidateCount, sGen, sAck, sStatus1, sStatus2, sStatus3, sPowered;

static Fields derive(uint8_t powered, uint8_t ack, uint8_t s1, uint8_t s2, uint8_t s3) {
    Fields f = {0, ADV_SIG_NO_STANDARD, 0};
    if (!powered || !ack) return f;   /* POWERED is not debounced: added by SIG_Get */
    uint8_t lock = s1 & 0x01;
    uint8_t freeRun = (s3 >> 4) & 0x01;
    if (lock) f.flags |= ADV_SIG_LOCK;
    if (s1 & 0x04) f.flags |= ADV_SIG_FSC;
    if (freeRun) f.flags |= ADV_SIG_FREERUN;
    if (s3 & 0x40) f.flags |= ADV_SIG_INTERLACED;
    if (s1 & 0x80) f.flags |= ADV_SIG_COLORKILL;
    if (s2 & 0x10) f.flags |= ADV_SIG_NONSTD;      /* LL_NSTD: non-standard line length */
    if (lock && !freeRun) f.flags |= ADV_SIG_PRESENT;
    if (lock) {
        f.standard = (uint8_t)((s1 >> 4) & 0x07);
        f.hz = (s3 & 0x04) ? 50 : 60;    /* SD_OP_50Hz */
    }
    return f;
}

static int same(const Fields* a, const Fields* b) {
    return a->flags == b->flags && a->standard == b->standard && a->hz == b->hz;
}

void SIG_Reset(void) {
    Fields off = {0, ADV_SIG_NO_STANDARD, 0};
    sAccepted = off; sCandidate = off;
    sCandidateCount = 0; sGen = 0; sAck = 0; sStatus1 = 0; sStatus2 = 0; sStatus3 = 0; sPowered = 0;
}

void SIG_Sample(uint8_t powered, uint8_t ack, uint8_t status1, uint8_t status2, uint8_t status3) {
    Fields f = derive(powered, ack, status1, status2, status3);
    sPowered = powered;
    if (powered) { sAck = ack; sStatus1 = status1; sStatus2 = status2; sStatus3 = status3; }
    else { sAck = 0; sStatus1 = 0; sStatus2 = 0; sStatus3 = 0; }
    if (same(&f, &sCandidate)) {
        if (sCandidateCount < 2) sCandidateCount++;
    } else {
        sCandidate = f;
        sCandidateCount = 1;
    }
    /* two equal samples in a row; powering off is final at once */
    if ((sCandidateCount >= 2 || !powered) && !same(&sCandidate, &sAccepted)) {
        sAccepted = sCandidate;
        sGen++;
    }
}

void SIG_Get(AdvSignal* out, uint8_t vidSel, uint8_t encoder, uint8_t input) {
    memset(out, 0, sizeof(*out));
    out->gen = sGen;
    out->flags = (uint8_t)(sAccepted.flags | (sPowered ? ADV_SIG_POWERED : 0));
    out->flags2 = sAck ? ADV_SIG2_ACK : 0;
    out->standard = sAccepted.standard;
    out->hz = sAccepted.hz;
    out->status1 = sStatus1;
    out->status2 = sStatus2;
    out->status3 = sStatus3;
    out->vidSel = vidSel;
    out->encoder = encoder;
    out->input = input;
}
