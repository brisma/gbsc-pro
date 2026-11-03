/* Host stub of link_hal.h: records every call for test_link.c */
#include <stdio.h>
#include <string.h>
#include "stub_hal.h"

char gLog[2048][48];
int gLogN;
AdvRegs gRegs;
AdvSwitches gSw;
int gHalFail;

static void logf_(const char* s) { if (gLogN < 2048) snprintf(gLog[gLogN++], sizeof(gLog[0]), "%s", s); }
void stubReset(void) { gLogN = 0; memset(&gRegs, 0, sizeof(gRegs)); memset(&gSw, 0, sizeof(gSw)); gReplies = 0; gDropReply = 0; gProgramFail = 0; gI2cReadFail = 0; gHalFail = 0; memset(gStaging, 0xFF, sizeof(gStaging)); }

void halLoadGlobals(const AdvRegs* r) { gRegs = *r; logf_("globals"); }
void halSwitches(const AdvSwitches* w) {
    char b[48]; gSw = *w;
    snprintf(b, sizeof(b), "switches %u%u%u%u av%u", w->asw1, w->asw2, w->asw3, w->asw4, w->avConnect);
    logf_(b);
}
int halAdvInit(void) { logf_("init"); return gHalFail & ADV_STEP_INPUT; }
void halAdvDeinit(void) { logf_("deinit"); }
int halFormat(uint8_t reg) { char b[48]; snprintf(b, sizeof(b), "format %02X", reg); logf_(b); return gHalFail & ADV_STEP_FORMAT; }
int halI2p(uint8_t i2p, uint8_t smooth) { char b[48]; snprintf(b, sizeof(b), "i2p %u %u", i2p, smooth); logf_(b); return gHalFail & ADV_STEP_I2P; }
int halAce(void) { logf_("ace"); return gHalFail & ADV_STEP_ACE; }
int halBcsh(void) { logf_("bcsh"); return gHalFail & ADV_STEP_BCSH; }
int halFilters(void) { logf_("filters"); return gHalFail & ADV_STEP_FILTERS; }
uint32_t gNowUs;
int gDropReply;
uint8_t gReplyCmd, gReplySeq, gReplyLen, gReplyPayload[ADV_MAX_PAYLOAD];
int gReplies;
uint32_t gReplyNotBefore, gReplyDeadline;
static void onReplyFrame(const AdvParser* p, void* c) {
    (void)c; gReplyCmd = p->cmd; gReplySeq = p->seq; gReplyLen = p->len; memcpy(gReplyPayload, p->buf, p->len);
}
uint32_t halTimeUs(void) { return gNowUs; }
uint32_t halReply(const uint8_t* frame, uint8_t n, uint32_t notBeforeUs, uint32_t deadlineUs) {
    gReplyNotBefore = notBeforeUs; gReplyDeadline = deadlineUs;
    if (gDropReply) return 0;
    AdvParser p; advParserInit(&p);
    for (uint8_t i = 0; i < n; i++) advParserFeed(&p, frame[i], onReplyFrame, 0);
    gReplies++;
    logf_("reply");
    return notBeforeUs + 1000;                     /* RX back 1 ms after the turnaround */
}
void halInfo(AdvInfo* out) {
    memset(out, 0, sizeof(*out));
    strcpy(out->fw, "3.0.0"); out->buildId = 0xC3A7B74E; out->flags = ADV_INFO_LOCK; out->standard = 2; out->uptimeS = 42;
}
void halSignal(AdvSignal* out) {
    memset(out, 0, sizeof(*out));
    out->gen = 3; out->flags = ADV_SIG_PRESENT | ADV_SIG_LOCK | ADV_SIG_POWERED; out->standard = 4; out->hz = 50;
}
uint8_t gStaging[LINK_STAGING_SIZE];
int gProgramFail;
int halStagingErase(uint8_t sector) {
    char b[48]; memset(gStaging + sector * LINK_SECTOR_SIZE, 0xFF, LINK_SECTOR_SIZE);
    snprintf(b, sizeof(b), "erase %u", sector); logf_(b); return 0;
}
int halStagingProgram(uint32_t offset, const uint8_t* data, uint8_t len) {
    if (gProgramFail) return -1;
    memcpy(gStaging + offset, data, len); return 0;
}
const uint8_t* halStagingPtr(void) { return gStaging; }
void halCopyAndReset(uint32_t size) { char b[48]; snprintf(b, sizeof(b), "copy %u", (unsigned)size); logf_(b); }
int gI2cReadFail;
int halI2cRead(uint8_t addr, uint8_t reg, uint8_t* v, uint8_t n) {   /* value = register number */
    char b[48]; snprintf(b, sizeof(b), "i2c_read %02X %02X %u", addr, reg, n); logf_(b);
    if (gI2cReadFail) return -1;
    for (uint8_t i = 0; i < n; i++) v[i] = (uint8_t)(reg + i);
    return 0;
}
void halI2cWrite(uint8_t addr, uint8_t reg, uint8_t val) { char b[48]; snprintf(b, sizeof(b), "i2c %02X %02X %02X", addr, reg, val); logf_(b); }
