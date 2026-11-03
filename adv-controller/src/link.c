#include <string.h>
#include "link.h"
#include "link_hal.h"

static AdvParser sParser;
static AdvState sCur;
static int sHave;
static uint8_t sApplied;
static uint16_t sBadState, sRxFrames, sDropped;
static uint32_t sPrevPollUs, sCurPollUs;

/* STATE and I2C_BATCH frames run after any reply of the same poll, in arrival order; consecutive STATEs merge
 * (a full state supersedes the older one) */
#define LINK_FIFO 4
typedef struct { uint8_t cmd, len; uint8_t buf[1 + ADV_I2C_BATCH_MAX * 3]; } Deferred;
static Deferred sFifo[LINK_FIFO];
static uint8_t sFifoN;

static const uint32_t kFreshUs = 8000, kTurnaroundUs = 3000, kDeadlineUs = 10000;

/* Firmware update session */
static struct {
    uint8_t state;            /* ADV_UST_* */
    uint32_t size, crc, runCrc, next;
    uint8_t sectors, erased;
    uint8_t copyPending;
} sUpd;
static uint8_t sUpdErr;        /* LINK_UPD_ERR_* */
static LinkEvent sEv[LINK_EVENT_RING];
static uint8_t sEvHead, sEvTail;
static void pushEvent(uint8_t kind, uint8_t cmd, uint8_t seq, uint8_t len, uint8_t steps, uint8_t failed) {
    uint8_t h = (uint8_t)((sEvHead + 1U) % LINK_EVENT_RING);
    if (h == sEvTail) sEvTail = (uint8_t)((sEvTail + 1U) % LINK_EVENT_RING);   /* full: drop the oldest */
    LinkEvent* e = &sEv[sEvHead];
    e->kind = kind; e->cmd = cmd; e->seq = seq; e->len = len; e->steps = steps; e->failed = failed;
    sEvHead = h;
}
static uint8_t sStaleNext;     /* an erase stalled the clock: the next poll cannot judge freshness */
static uint8_t sFramesThisPoll;

#define HDR_OFF 0x400U
#define HDR_CRC_OFF (HDR_OFF + 24U)  /* imageCrc32: zeroed for the image CRC (tools/fw-header.py) */

/* AdvState BCSH bytes are 0..254 with 128 = neutral; the brightness and hue registers are signed: subtract 128 */
static void toRegs(const AdvState* s, AdvRegs* r) {
    memset(r, 0, sizeof(*r));
    r->inputSignal = s->input;
    r->adv = advDecoderOn(s) ? 1 : 0;              /* a decoder input, or RGB SCART listening on pin 20 (CVBS) */
    r->ycInput = s->input == ADV_INPUT_SV ? 1 : 0;
    r->formatReg = advFormatRegister(advActiveFormat(s));
    r->bright = (uint8_t)(s->brightness - 128);
    r->contrast = s->contrast;
    r->saturation = s->saturation;
    r->hue = (uint8_t)(s->hue - 128);
    r->i2p = s->i2p;
    r->smooth = s->i2p ? s->smooth : 0;          /* smooth needs I2P */
    r->ace = s->ace;
    r->aceLuma = s->aceLumaGain; r->aceChromaGain = s->aceChromaGain; r->aceChromaMax = s->aceChromaMax;
    r->aceGamma = s->aceGammaGain; r->aceSpeed = s->aceResponseSpeed;
    r->filterY = s->filterY; r->filterC = s->filterC; r->filterWy = s->filterWy; r->filterWyOvr = s->filterWyOverride;
    r->combNtsc = s->combNtsc; r->combPal = s->combPal;
    r->combLumaNtsc = s->combLumaNtsc; r->combChromaNtsc = s->combChromaNtsc; r->combTapsNtsc = s->combTapsNtsc;
    r->combLumaPal = s->combLumaPal; r->combChromaPal = s->combChromaPal; r->combTapsPal = s->combTapsPal;
    r->dnr = s->dnr; r->cti = s->cti;
}

/* Groups whose I2C writes failed are planned again with the next STATE (the ESP's 5 s keep-alive), at most
 * kApplyRetries times per change: a chip that keeps failing is not re-initialised forever */
static const uint8_t kApplyRetries = 3;
static uint8_t sRetry, sRetryLeft;

static void applyState(const AdvState* next) {
    uint8_t planned = advPlan(&sCur, next, sHave);
    if (planned) sRetryLeft = kApplyRetries;      /* a new change: a new budget */
    uint8_t steps = planned | (advDecoderOn(next) ? sRetry : 0);
    uint8_t failed = 0;
    sApplied++;
    if (steps) {
        AdvRegs r;
        toRegs(next, &r);
        halLoadGlobals(&r);
        /* the switches (the pin-20 termination) go before the decoder power-down: the ESP releases the RGB SCART
           preset on the first SIGNAL without POWERED after a listen-off STATE (input-auto-device.cpp) */
        if (steps & ADV_STEP_SWITCHES) { AdvSwitches w = advSwitchesFor(next); halSwitches(&w); }
        if (steps & ADV_STEP_INPUT) {
            if (r.adv) { if (halAdvInit()) failed |= ADV_STEP_INPUT; }   /* applies every ADV group from the globals */
            else halAdvDeinit();
        } else {
            if ((steps & ADV_STEP_FORMAT) && halFormat(r.formatReg)) failed |= ADV_STEP_FORMAT;
            if ((steps & ADV_STEP_I2P) && halI2p(r.i2p, r.smooth)) failed |= ADV_STEP_I2P;
            if ((steps & ADV_STEP_ACE) && halAce()) failed |= ADV_STEP_ACE;
            if ((steps & ADV_STEP_BCSH) && halBcsh()) failed |= ADV_STEP_BCSH;
            if ((steps & ADV_STEP_FILTERS) && halFilters()) failed |= ADV_STEP_FILTERS;
        }
    }
    sRetry = 0;
    if (failed && sRetryLeft) { sRetryLeft--; sRetry = failed; }
    pushEvent(LINK_EV_STATE, ADV_CMD_STATE, 0, 0, steps, failed);
    sCur = *next;
    sHave = 1;
}

static void defer(uint8_t cmd, const uint8_t* buf, uint8_t len) {
    if (len > sizeof(sFifo[0].buf)) { sBadState++; return; }
    Deferred* d;
    if (cmd == ADV_CMD_STATE && sFifoN && sFifo[sFifoN - 1].cmd == ADV_CMD_STATE) d = &sFifo[sFifoN - 1];
    else if (sFifoN < LINK_FIFO) d = &sFifo[sFifoN++];
    else { sBadState++; return; }
    d->cmd = cmd;
    d->len = len;
    memcpy(d->buf, buf, len);
}

/* 1 when the reply went out */
static int reply(uint8_t seq, uint8_t cmd, const uint8_t* payload, uint8_t len) {
    if (sCurPollUs - sPrevPollUs > kFreshUs) { sDropped++; return 0; }   /* the frame may be too old to answer */
    uint8_t f[ADV_INFO_BYTES + ADV_FRAME_OVERHEAD];
    size_t n = advFrameEncode(seq, cmd, payload, len, f, sizeof(f));
    uint32_t back = halReply(f, (uint8_t)n, sCurPollUs + kTurnaroundUs, sCurPollUs + kDeadlineUs);
    if (!back) { sDropped++; return 0; }
    sCurPollUs = back;                       /* RX was off until now: nothing older can be waiting */
    return 1;
}

static int updReply(const AdvParser* p, uint8_t status, uint32_t next) {
    uint8_t b[ADV_UPD_REPLY_BYTES];
    advUpdReplyPack(status, next, b);
    return reply(p->seq, (uint8_t)(p->cmd | ADV_RESP), b, ADV_UPD_REPLY_BYTES);
}

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

/* Staged image header, initial SP and reset vector */
static int stagedHeaderOk(void) {
    const uint8_t* img = halStagingPtr();
    uint32_t sp = rd32(img), rst = rd32(img + 4) & ~1U;
    if (memcmp(img + HDR_OFF, "ADVC", 4) != 0) return 0;
    if (rd32(img + HDR_OFF + 20) != sUpd.size || rd32(img + HDR_CRC_OFF) != sUpd.crc) return 0;
    if (sp < 0x1FFF8000U || sp > 0x20027000U || (sp & 7U)) return 0;
    return rst >= LINK_APP_BASE && rst < LINK_APP_BASE + sUpd.size;
}

static void updBegin(const AdvParser* p) {
    uint32_t size, crc;
    char ver[12];
    if (sUpd.copyPending) return;                                /* the copy wins */
    if (!advUpdBeginUnpack(p->buf, p->len, &size, &crc, ver)) { sBadState++; return; }
    if (!size || size > LINK_STAGING_SIZE) { updReply(p, ADV_UPD_TOO_BIG, 0); return; }
    memset(&sUpd, 0, sizeof(sUpd));
    sUpdErr = LINK_UPD_ERR_NONE;
    sUpd.state = ADV_UST_ERASING;
    sUpd.size = size;
    sUpd.crc = crc;
    sUpd.sectors = (uint8_t)((size + LINK_SECTOR_SIZE - 1U) / LINK_SECTOR_SIZE);
    updReply(p, ADV_UPD_OK, 0);
}

static void updData(const AdvParser* p) {
    uint32_t off;
    const uint8_t* d;
    uint8_t n;
    if (!advUpdDataUnpack(p->buf, p->len, &off, &d, &n)) { sBadState++; return; }
    if (sUpd.state != ADV_UST_READY && sUpd.state != ADV_UST_RECEIVING) { updReply(p, ADV_UPD_NO_SESSION, sUpd.next); return; }
    uint32_t want = sUpd.size - sUpd.next < ADV_UPD_CHUNK ? sUpd.size - sUpd.next : ADV_UPD_CHUNK;
    if (off != sUpd.next || n != want) { updReply(p, ADV_UPD_RESUME, sUpd.next); return; }
    if (halStagingProgram(off, d, n) != 0) { sUpd.state = ADV_UST_ERROR; sUpdErr = LINK_UPD_ERR_PROGRAM; updReply(p, ADV_UPD_NO_SESSION, sUpd.next); return; }
    uint8_t b[ADV_UPD_CHUNK];                                    /* CRC of what the flash holds, header CRC field zeroed */
    memcpy(b, halStagingPtr() + off, n);
    for (uint8_t i = 0; i < n; i++)
        if (off + i >= HDR_CRC_OFF && off + i < HDR_CRC_OFF + 4U) b[i] = 0;
    sUpd.runCrc = advCrc32Update(sUpd.runCrc, b, n);
    sUpd.next += n;
    sUpd.state = ADV_UST_RECEIVING;
    updReply(p, ADV_UPD_OK, sUpd.next);
}

static void updCommit(const AdvParser* p) {
    uint8_t st;
    if (sUpd.state != ADV_UST_READY && sUpd.state != ADV_UST_RECEIVING) st = ADV_COMMIT_NO_SESSION;
    else if (sUpd.next != sUpd.size) st = ADV_COMMIT_NOT_COMPLETE;
    else if (sUpd.runCrc != sUpd.crc) st = ADV_COMMIT_CRC_BAD;
    else if (!stagedHeaderOk()) st = ADV_COMMIT_HEADER_BAD;
    else st = ADV_COMMIT_CRC_OK;
    if (st == ADV_COMMIT_CRC_BAD) sUpdErr = LINK_UPD_ERR_CRC;
    if (st == ADV_COMMIT_HEADER_BAD) sUpdErr = LINK_UPD_ERR_HEADER;
    if (updReply(p, st, sUpd.next) && st == ADV_COMMIT_CRC_OK) {
        sUpd.state = ADV_UST_VERIFYING;
        sUpd.copyPending = 1;                                    /* only once the ESP got the answer */
    }
}

/* ADV_CMD_REG_READ: map switch around one burst read (0x0E back to 0 also after an I2C error), then the reply */
_Static_assert(ADV_REG_READ_REPLY_MAX <= ADV_INFO_BYTES, "the reply buffer holds ADV_INFO_BYTES");
static void regRead(const AdvParser* p) {
    uint8_t dev, map, reg, count = 0, v[ADV_REG_READ_MAX], b[ADV_REG_READ_REPLY_MAX], st;
    if (!advRegReadReqUnpack(p->buf, p->len, &dev, &map, &reg, &count)) st = ADV_REG_READ_BAD_REQUEST;
    else {
        if (map) halI2cWrite(dev, 0x0E, map);
        int r = halI2cRead(dev, reg, v, count);   /* an I2C error carries the failed step in the high nibble */
        st = r == 0 ? ADV_REG_READ_OK : (uint8_t)(ADV_REG_READ_I2C_ERROR | (r > 0 ? (r & 0x0F) << 4 : 0));
        if (map) halI2cWrite(dev, 0x0E, 0x00);
    }
    size_t n = advRegReadReplyPack(st, v, count, b);
    reply(p->seq, (uint8_t)(p->cmd | ADV_RESP), b, (uint8_t)n);
}

static void answer(const AdvParser* p) {
    uint8_t base = ADV_CMD_BASE(p->cmd);
    if (base == ADV_CMD_BASE(ADV_CMD_UPD_BEGIN)) { updBegin(p); return; }
    if (base == ADV_CMD_BASE(ADV_CMD_UPD_DATA)) { updData(p); return; }
    if (base == ADV_CMD_BASE(ADV_CMD_UPD_COMMIT)) { updCommit(p); return; }
    if (base == ADV_CMD_BASE(ADV_CMD_UPD_STATUS)) { updReply(p, sUpd.state, sUpd.next); return; }
    if (base == ADV_CMD_BASE(ADV_CMD_REG_READ)) { regRead(p); return; }
    if (base == ADV_CMD_BASE(ADV_CMD_PING)) {
        uint8_t v = ADV_PROTOCOL;
        reply(p->seq, (uint8_t)(p->cmd | ADV_RESP), &v, 1);
    } else if (base == ADV_CMD_BASE(ADV_CMD_SIGNAL)) {
        AdvSignal g;
        uint8_t b[ADV_SIGNAL_BYTES];
        halSignal(&g);
        advSignalPack(&g, b);
        reply(p->seq, (uint8_t)(p->cmd | ADV_RESP), b, ADV_SIGNAL_BYTES);
    } else if (base == ADV_CMD_BASE(ADV_CMD_INFO)) {
        AdvInfo i;
        uint8_t b[ADV_INFO_BYTES];
        halInfo(&i);
        i.protocol = ADV_PROTOCOL;
        i.stateVersion = ADV_STATE_VERSION;
        i.caps = ADV_INFO_CAP_SIGNAL | ADV_INFO_CAP_REG_READ | ADV_INFO_CAP_LISTEN;
        i.rxFrames = sRxFrames;
        i.badFrames = LINK_BadFrames();
        i.appliedCount = sApplied;
        i.stateHash = sHave ? advStateHash(&sCur) : 0;
        advInfoPack(&i, b);
        reply(p->seq, (uint8_t)(p->cmd | ADV_RESP), b, ADV_INFO_BYTES);
    } else {
        uint8_t c = p->cmd;
        reply(p->seq, ADV_CMD_NAK, &c, 1);
    }
}

static void onFrame(const AdvParser* p, void* ctx) {
    (void)ctx;
    sRxFrames++;
    sFramesThisPoll++;
    pushEvent(LINK_EV_FRAME, p->cmd, p->seq, p->len, 0, 0);
    if (p->cmd & ADV_RESP) return;                          /* responses never come from the ESP */
    uint8_t base = ADV_CMD_BASE(p->cmd);
    if (base == ADV_CMD_STATE || base == ADV_CMD_I2C_BATCH) { defer(base, p->buf, p->len); return; }   /* never a reply */
    if (p->cmd & ADV_WANT_REPLY) answer(p);
}

static void runDeferred(const Deferred* d) {
    if (d->cmd == ADV_CMD_STATE) {
        AdvState s;
        if (advStateUnpack(d->buf, d->len, &s)) applyState(&s);
        else sBadState++;
        return;
    }
    uint8_t n = d->len ? d->buf[0] : 0;
    if (n > ADV_I2C_BATCH_MAX || d->len != (uint8_t)(1 + n * 3)) { sBadState++; return; }
    for (uint8_t i = 0; i < n; i++) halI2cWrite(d->buf[1 + i * 3], d->buf[2 + i * 3], d->buf[3 + i * 3]);
}

void LINK_Init(void) {
    advParserInit(&sParser);
    memset(&sCur, 0, sizeof(sCur));
    sHave = 0;
    sRetry = 0; sRetryLeft = 0;
    sApplied = 0;
    sBadState = 0;
    sRxFrames = 0;
    sDropped = 0;
    sUpdErr = 0;
    sEvHead = sEvTail = 0;
    sFifoN = 0;
    sPrevPollUs = sCurPollUs = 0;
    memset(&sUpd, 0, sizeof(sUpd));
    sStaleNext = 0;
    sFramesThisPoll = 0;
}

void LINK_Tick(void) {
    if (sUpd.copyPending) { halCopyAndReset(sUpd.size); sUpd.copyPending = 0; return; }   /* returns only on the host */
    if (sUpd.state != ADV_UST_ERASING) return;
    if (sParser.n || sFramesThisPoll) return;                    /* never erase while a frame is arriving */
    if (halStagingErase(sUpd.erased) != 0) { sUpd.state = ADV_UST_ERROR; sUpdErr = LINK_UPD_ERR_ERASE; return; }
    sStaleNext = 1;                                              /* the erase stalled the clock */
    if (++sUpd.erased >= sUpd.sectors) sUpd.state = ADV_UST_READY;
}

uint8_t LINK_UpdateState(void) { return sUpd.state; }

void LINK_Process(void) {
    for (uint8_t i = 0; i < sFifoN; i++) runDeferred(&sFifo[i]);
    sFifoN = 0;
}

void LINK_PollBegin(uint32_t nowUs) {
    sPrevPollUs = sStaleNext ? nowUs - 1000000U : sCurPollUs;
    sStaleNext = 0;
    sCurPollUs = nowUs;
    sFramesThisPoll = 0;
}

uint16_t LINK_DroppedReplies(void) { return sDropped; }

void LINK_Feed(uint8_t b) { advParserFeed(&sParser, b, onFrame, 0); }
uint8_t LINK_AppliedSeq(void) { return sApplied; }
uint16_t LINK_BadFrames(void) { return (uint16_t)(sParser.bad + sBadState); }

uint16_t LINK_RxFrames(void) { return sRxFrames; }
uint8_t LINK_Input(void) { return sHave ? sCur.input : 0U; }
uint16_t LINK_StateHash(void) { return sHave ? advStateHash(&sCur) : 0U; }
uint8_t LINK_UpdateError(void) { return sUpdErr; }
void LINK_UpdateProgress(uint32_t* next, uint32_t* size, uint8_t* erased, uint8_t* sectors) {
    *next = sUpd.next; *size = sUpd.size; *erased = sUpd.erased; *sectors = sUpd.sectors;
}
int LINK_PopEvent(LinkEvent* out) {
    if (sEvTail == sEvHead) return 0;
    *out = sEv[sEvTail];
    sEvTail = (uint8_t)((sEvTail + 1U) % LINK_EVENT_RING);
    return 1;
}
