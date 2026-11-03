/* Host tests of the ADV Controller link v3 dispatcher (make test) */
#include <stdio.h>
#include <string.h>
#include "link.h"
#include "stub_hal.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int logHas(const char* s) { for (int i = 0; i < gLogN; i++) if (!strcmp(gLog[i], s)) return 1; return 0; }

static AdvState base(uint8_t input) {
    AdvState s; memset(&s, 0, sizeof(s));
    s.version = ADV_STATE_VERSION; s.input = input; s.syncStripper = 1; s.formatSv = 2; s.formatAv = 1;
    s.i2p = 1; s.ace = 1; s.aceLumaGain = 13; s.aceChromaGain = 8; s.aceChromaMax = 8; s.aceGammaGain = 8;
    s.aceResponseSpeed = 15; s.brightness = 128; s.contrast = 128; s.saturation = 128; s.hue = 128;
    s.filterY = 1; s.filterWy = 19; s.filterWyOverride = 1; s.combPal = 1; s.combTapsNtsc = 2; s.combTapsPal = 3;
    return s;
}

static void sendState(const AdvState* s, int byteByByte, int noise) {
    uint8_t b[ADV_STATE_BYTES], f[128];
    advStatePack(s, b);
    size_t n = advFrameEncode(1, ADV_CMD_STATE, b, ADV_STATE_BYTES, f, sizeof(f));
    if (noise) { const uint8_t z[] = {0x00, 0xA7, 'x', 0x3C}; for (size_t i = 0; i < sizeof(z); i++) LINK_Feed(z[i]); }
    for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
    if (byteByByte) LINK_Process();
}

static void test_first_state_rgbs(void) {
    stubReset(); LINK_Init();
    AdvState s = base(1);
    sendState(&s, 1, 0);
    CHECK(logHas("switches 0001 av0"));
    CHECK(logHas("deinit"));
    CHECK(!logHas("init"));
    CHECK(gRegs.inputSignal == 1 && gRegs.adv == 0);
    CHECK(LINK_AppliedSeq() == 1);
}

static void test_first_state_sv_globals_before_init(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); s.brightness = 140; s.hue = 120;
    sendState(&s, 1, 0);
    CHECK(logHas("switches 0010 av1"));
    CHECK(logHas("init"));
    CHECK(!strcmp(gLog[0], "globals"));          /* the globals are stored before anything else */
    CHECK(!logHas("bcsh"));                        /* ADV_Init applies everything */
    CHECK(gRegs.ycInput == 1 && gRegs.adv == 1);
    CHECK(gRegs.formatReg == 0x54);                /* formatSv 2 = NTSC-M */
    CHECK(gRegs.bright == 12);                     /* 140 - 128 */
    CHECK(gRegs.hue == (uint8_t)(120 - 128));
    CHECK(gRegs.saturation == 128 && gRegs.contrast == 128);
}

static void test_composite_uses_its_format_and_cvbs(void) {
    stubReset(); LINK_Init();
    AdvState s = base(6);
    sendState(&s, 1, 0);
    CHECK(gRegs.ycInput == 0);
    CHECK(gRegs.formatReg == 0x84);                /* formatAv 1 = PAL */
}

static void test_brightness_change_on_sv(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();
    s.brightness = 150; sendState(&s, 1, 0);
    CHECK(gLogN == 2 && logHas("globals") && logHas("bcsh"));
    CHECK(gRegs.bright == 22);
}

static void test_same_state_twice_does_nothing(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();
    sendState(&s, 1, 0);
    CHECK(gLogN == 0);
    CHECK(LINK_AppliedSeq() == 2);                 /* still counted: the ESP's keep-alive was received */
}

static void test_invalid_frame_ignored(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5);
    uint8_t b[ADV_STATE_BYTES], f[128];
    advStatePack(&s, b);
    b[10] ^= 0x55;                                 /* breaks the state crc8, the frame CRC is still valid */
    size_t n = advFrameEncode(1, ADV_CMD_STATE, b, ADV_STATE_BYTES, f, sizeof(f));
    for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
    LINK_Process();
    CHECK(gLogN == 0);
    CHECK(LINK_AppliedSeq() == 0);
    CHECK(LINK_BadFrames() == 1);
}

static void test_i2c_batch(void) {
    stubReset(); LINK_Init();
    uint8_t p[7] = {2, 0x42, 0x0A, 0x10, 0x42, 0x08, 0x80}, f[128];
    size_t n = advFrameEncode(3, ADV_CMD_I2C_BATCH, p, 7, f, sizeof(f));
    for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
    LINK_Process();                                /* deferred like STATE (after replies, arrival order) */
    CHECK(gLogN == 2 && logHas("i2c 42 0A 10") && logHas("i2c 42 08 80"));
}

static void test_noise_before_frame(void) {
    stubReset(); LINK_Init();
    AdvState s = base(2);
    sendState(&s, 1, 1);
    CHECK(logHas("switches 0010 av0"));
}

static void test_smooth_forced_off_without_i2p(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();
    s.i2p = 0; s.smooth = 1; sendState(&s, 1, 0);
    CHECK(logHas("i2p 0 0"));
    CHECK(gRegs.smooth == 0);
}

static void test_every_field_reaches_the_registers(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5);
    s.i2p = 1; s.smooth = 1; s.ace = 1;
    s.aceLumaGain = 21; s.aceChromaGain = 3; s.aceChromaMax = 4; s.aceGammaGain = 5; s.aceResponseSpeed = 6;
    s.contrast = 99; s.saturation = 77;
    s.filterY = 7; s.filterC = 2; s.filterWy = 11; s.filterWyOverride = 0; s.combNtsc = 3; s.combPal = 2;
    s.combLumaNtsc = 4; s.combChromaNtsc = 5; s.combTapsNtsc = 1; s.combLumaPal = 6; s.combChromaPal = 7; s.combTapsPal = 0;
    sendState(&s, 1, 0);
    CHECK(gRegs.i2p == 1 && gRegs.smooth == 1 && gRegs.ace == 1);
    CHECK(gRegs.aceLuma == 21 && gRegs.aceChromaGain == 3 && gRegs.aceChromaMax == 4 && gRegs.aceGamma == 5 && gRegs.aceSpeed == 6);
    CHECK(gRegs.contrast == 99 && gRegs.saturation == 77);
    CHECK(gRegs.filterY == 7 && gRegs.filterC == 2 && gRegs.filterWy == 11 && gRegs.filterWyOvr == 0);
    CHECK(gRegs.combNtsc == 3 && gRegs.combPal == 2);
    CHECK(gRegs.combLumaNtsc == 4 && gRegs.combChromaNtsc == 5 && gRegs.combTapsNtsc == 1);
    CHECK(gRegs.combLumaPal == 6 && gRegs.combChromaPal == 7 && gRegs.combTapsPal == 0);
}

static void test_input_change_to_rgbs_powers_down(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();
    s.input = 1; s.syncStripper = 0; s.termination = 1; sendState(&s, 1, 0);
    CHECK(logHas("switches 0101 av1"));
    CHECK(logHas("deinit"));
}

/* Several STATE frames buffered while the loop was busy: only the newest is applied (full state) */
static void test_only_the_latest_buffered_state_is_applied(void) {
    stubReset(); LINK_Init();
    AdvState a = base(5), b = base(6), c = base(1);
    sendState(&a, 0, 0); sendState(&b, 0, 0); sendState(&c, 0, 0);
    LINK_Process();
    CHECK(!logHas("init"));
    CHECK(logHas("deinit"));
    CHECK(gRegs.inputSignal == 1);
    stubReset();
    LINK_Process();                                /* nothing pending any more */
    CHECK(gLogN == 0);
}

/* ---- replies ---- */
static void poll(uint32_t us) { gNowUs = us; LINK_PollBegin(us); }
static void request(uint8_t seq, uint8_t cmd) {
    uint8_t f[16]; size_t n = advFrameEncode(seq, cmd, 0, 0, f, sizeof(f));
    for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
}

static void test_ping_reply(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(3000);                         /* previous poll 2 ms earlier: fresh */
    request(5, ADV_CMD_PING | ADV_WANT_REPLY);
    LINK_Process();
    CHECK(gReplies == 1);
    CHECK(gReplyCmd == 0xC7 && gReplySeq == 5 && gReplyLen == 1 && gReplyPayload[0] == ADV_PROTOCOL);
    CHECK(gReplyNotBefore == 3000 + 3000 && gReplyDeadline == 3000 + 10000);
}

static void test_info_reply_reports_applied_state(void) {
    stubReset(); LINK_Init();
    AdvState s = base(1);
    poll(1000); sendState(&s, 1, 0);
    poll(2000); request(6, ADV_CMD_INFO | ADV_WANT_REPLY); LINK_Process();
    CHECK(gReplies == 1 && gReplyCmd == 0xC3 && gReplySeq == 6 && gReplyLen == ADV_INFO_BYTES);
    AdvInfo i; CHECK(advInfoUnpack(gReplyPayload, gReplyLen, &i));
    CHECK(i.protocol == ADV_PROTOCOL && i.stateVersion == ADV_STATE_VERSION);
    CHECK(!strcmp(i.fw, "3.0.0") && i.buildId == 0xC3A7B74E && i.uptimeS == 42);
    CHECK(i.stateHash == advStateHash(&s) && i.appliedCount == 1);
    CHECK(i.rxFrames == 2);
    CHECK(i.caps == (ADV_INFO_CAP_SIGNAL | ADV_INFO_CAP_REG_READ | ADV_INFO_CAP_LISTEN));   /* SIGNAL, REG_READ, listen */
}

static void test_signal_reply(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000); request(9, ADV_CMD_SIGNAL); LINK_Process();
    CHECK(gReplies == 1 && gReplyCmd == 0xC9 && gReplySeq == 9 && gReplyLen == ADV_SIGNAL_BYTES);
    AdvSignal g; CHECK(advSignalUnpack(gReplyPayload, gReplyLen, &g));
    CHECK(g.gen == 3 && g.standard == 4 && g.hz == 50 && (g.flags & ADV_SIG_PRESENT));
}

static void test_unknown_request_gets_nak(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000); request(7, 0x55); LINK_Process();
    CHECK(gReplies == 1 && gReplyCmd == ADV_CMD_NAK && gReplyLen == 1 && gReplyPayload[0] == 0x55);
}

static void test_state_with_reply_bit_never_replies(void) {
    stubReset(); LINK_Init();
    AdvState s = base(2);
    uint8_t b[ADV_STATE_BYTES], f[128];
    advStatePack(&s, b);
    size_t n = advFrameEncode(1, ADV_CMD_STATE | ADV_WANT_REPLY, b, ADV_STATE_BYTES, f, sizeof(f));
    poll(1000); poll(2000);
    for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
    LINK_Process();
    CHECK(gReplies == 0);
    CHECK(logHas("switches 0010 av0"));
}

static void test_stale_request_not_answered(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(10000);                        /* 9 ms since the previous poll: the frame may be old */
    request(8, ADV_CMD_PING | ADV_WANT_REPLY); LINK_Process();
    CHECK(gReplies == 0);
    poll(12000); request(9, ADV_CMD_PING | ADV_WANT_REPLY); LINK_Process();
    CHECK(gReplies == 1);
}

static void test_reply_time_becomes_the_previous_poll(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000);
    request(1, ADV_CMD_PING | ADV_WANT_REPLY); LINK_Process();      /* RX back at 2000 + 3000 + 1000 */
    poll(6000 + 7000);                                               /* 7 ms after RX re-enabled: fresh */
    request(2, ADV_CMD_INFO | ADV_WANT_REPLY); LINK_Process();
    CHECK(gReplies == 2);
}

static void test_dropped_reply_counts(void) {
    stubReset(); LINK_Init();
    gDropReply = 1;
    poll(1000); poll(2000); request(3, ADV_CMD_PING | ADV_WANT_REPLY); LINK_Process();
    CHECK(gReplies == 0 && LINK_DroppedReplies() == 1);
}

static void test_reply_before_pending_state(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5);
    poll(1000); poll(2000);
    sendState(&s, 0, 0);
    request(4, ADV_CMD_INFO | ADV_WANT_REPLY);
    LINK_Process();
    int r = -1, in = -1;
    for (int k = 0; k < gLogN; k++) { if (!strcmp(gLog[k], "reply") && r < 0) r = k; if (!strcmp(gLog[k], "init") && in < 0) in = k; }
    CHECK(r >= 0 && in > r);
}

static void test_state_and_batches_in_arrival_order(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5);
    uint8_t p1[4] = {1, 0x42, 0x01, 0x11}, p2[7] = {2, 0x42, 0x02, 0x22, 0x42, 0x03, 0x33}, f[128];
    poll(1000);
    sendState(&s, 0, 0);
    size_t n = advFrameEncode(2, ADV_CMD_I2C_BATCH, p1, 4, f, sizeof(f)); for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
    n = advFrameEncode(3, ADV_CMD_I2C_BATCH, p2, 7, f, sizeof(f)); for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
    LINK_Process();
    int in = -1, w1 = -1, w3 = -1;
    for (int k = 0; k < gLogN; k++) {
        if (!strcmp(gLog[k], "init")) in = k;
        if (!strcmp(gLog[k], "i2c 42 01 11")) w1 = k;
        if (!strcmp(gLog[k], "i2c 42 03 33")) w3 = k;
    }
    CHECK(in >= 0 && w1 > in && w3 > w1);
}

static void test_responses_from_the_line_are_ignored(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000); request(5, 0xC7); LINK_Process();
    CHECK(gReplies == 0 && gLogN == 0);
}

/* ---- update session ---- */
static uint8_t gImg[40960];
static uint32_t imgCrc(const uint8_t* img, uint32_t size) {        /* fw-header.py rule: 0x418..0x41B zeroed */
    uint8_t save[4]; memcpy(save, img + 0x418, 4);
    uint8_t* w = (uint8_t*)img; memset(w + 0x418, 0, 4);
    uint32_t c = advCrc32(img, size);
    memcpy(w + 0x418, save, 4);
    return c;
}
static uint32_t makeImage(uint32_t size) {
    for (uint32_t i = 0; i < size; i++) gImg[i] = (uint8_t)(i * 7 + 3);
    uint32_t sp = 0x1FFF9000, rst = 0x10501;
    memcpy(gImg, &sp, 4); memcpy(gImg + 4, &rst, 4);
    memcpy(gImg + 0x400, "ADVC", 4);   /* ADV Controller image magic */
    gImg[0x404] = 1; gImg[0x405] = 0; gImg[0x406] = 0; gImg[0x407] = 0;
    memset(gImg + 0x408, 0, 12); memcpy(gImg + 0x408, "3.1.0", 5);
    memcpy(gImg + 0x414, &size, 4);
    uint32_t c = imgCrc(gImg, size);
    memcpy(gImg + 0x418, &c, 4);
    return c;
}
static uint32_t gT = 1000;
/* 6 ms per step: after a reply the stub's RX is back 4 ms after the poll (freshness bound 8 ms) */
static void updReq(uint8_t cmd, const uint8_t* p, uint8_t n) {
    gT += 6000; poll(gT);
    uint8_t f[128]; size_t m = advFrameEncode(9, cmd, p, n, f, sizeof(f));
    for (size_t i = 0; i < m; i++) LINK_Feed(f[i]);
    LINK_Process(); LINK_Tick();
}
static void idleTick(void);
static void idleTick(void) { gT += 6000; poll(gT); LINK_Process(); LINK_Tick(); }
static uint8_t replyStatus(uint32_t* next) { uint8_t st = 0xEE; uint32_t nx = 0; advUpdReplyUnpack(gReplyPayload, gReplyLen, &st, &nx); if (next) *next = nx; return st; }
static void begin(uint32_t size, uint32_t crc) { uint8_t b[ADV_UPD_BEGIN_BYTES]; advUpdBeginPack(size, crc, "3.1.0", b); updReq(ADV_CMD_UPD_BEGIN, b, ADV_UPD_BEGIN_BYTES); }
static void data(uint32_t off, uint8_t n) { uint8_t b[72]; size_t m = advUpdDataPack(off, gImg + off, n, b); updReq(ADV_CMD_UPD_DATA, b, (uint8_t)m); }
static void transfer(uint32_t size) { for (uint32_t o = 0; o < size; o += 64) data(o, (uint8_t)(size - o < 64 ? size - o : 64)); }
static void eraseAll(void) {
    for (int k = 0; k < 30 && LINK_UpdateState() == ADV_UST_ERASING; k++) idleTick();
    idleTick();                                   /* the poll after the last erase is stale */
}

static void test_update_happy_path(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 20480, crc = makeImage(size);
    begin(size, crc);
    CHECK(gReplyCmd == (ADV_CMD_UPD_BEGIN | ADV_RESP) && replyStatus(0) == ADV_UPD_OK);
    CHECK(LINK_UpdateState() == ADV_UST_ERASING);
    eraseAll();
    CHECK(LINK_UpdateState() == ADV_UST_READY && logHas("erase 0") && logHas("erase 2") && !logHas("erase 3"));
    transfer(size);
    uint32_t next; CHECK(replyStatus(&next) == ADV_UPD_OK && next == size);
    CHECK(!memcmp(gStaging, gImg, size));
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    CHECK(replyStatus(0) == ADV_COMMIT_CRC_OK);
    idleTick();
    CHECK(logHas("copy 20480"));
}

static void test_update_resume_and_bad_length(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc); eraseAll();
    data(0, 64);
    data(256, 64);                               /* wrong offset */
    uint32_t next; CHECK(replyStatus(&next) == ADV_UPD_RESUME && next == 64);
    data(64, 32);                                /* short chunk in the middle */
    CHECK(replyStatus(&next) == ADV_UPD_RESUME && next == 64);
}

static void test_update_crc_and_header_bad(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc ^ 1); eraseAll(); transfer(size);
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    CHECK(replyStatus(0) == ADV_COMMIT_CRC_BAD);
    idleTick(); CHECK(!logHas("copy 8192"));
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    crc = makeImage(size); gImg[0x400] = 'X'; crc = imgCrc(gImg, size); memcpy(gImg + 0x418, &crc, 4);
    begin(size, crc); eraseAll(); transfer(size);
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    CHECK(replyStatus(0) == ADV_COMMIT_HEADER_BAD);
    stubReset(); LINK_Init(); idleTick();            /* an "HC32" magic is not an image */
    crc = makeImage(size); memcpy(gImg + 0x400, "HC32", 4); crc = imgCrc(gImg, size); memcpy(gImg + 0x418, &crc, 4);
    begin(size, crc); eraseAll(); transfer(size);
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    CHECK(replyStatus(0) == ADV_COMMIT_HEADER_BAD);
}

static void test_update_commit_rules(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    CHECK(replyStatus(0) == ADV_COMMIT_NO_SESSION);
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc); eraseAll(); data(0, 64);
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    CHECK(replyStatus(0) == ADV_COMMIT_NOT_COMPLETE);
}

static void test_update_too_big_and_no_session(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    begin(LINK_STAGING_SIZE + 4, 1);
    CHECK(replyStatus(0) == ADV_UPD_TOO_BIG);
    data(0, 64);
    CHECK(replyStatus(0) == ADV_UPD_NO_SESSION);
}

static void test_update_restart_mid_transfer(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc); eraseAll(); data(0, 64); data(64, 64);
    begin(size, crc);
    uint32_t next; CHECK(replyStatus(&next) == ADV_UPD_OK && next == 0);
    CHECK(LINK_UpdateState() == ADV_UST_ERASING);
}

static void test_update_program_error(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc); eraseAll();
    gProgramFail = 1; data(0, 64);
    CHECK(replyStatus(0) == ADV_UPD_NO_SESSION && LINK_UpdateState() == ADV_UST_ERROR);
}

static void test_update_status_and_stale_after_erase(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 16384, crc = makeImage(size);
    begin(size, crc);
    idleTick();                                   /* erases sector 0 */
    int before = gReplies;
    updReq(ADV_CMD_UPD_STATUS, 0, 0);            /* first poll after the erase: stale, no reply */
    CHECK(gReplies == before);
    updReq(ADV_CMD_UPD_STATUS, 0, 0);
    CHECK(gReplies == before + 1 && gReplyLen == ADV_UPD_REPLY_BYTES);
}

static void test_update_state_still_applied(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc); eraseAll(); data(0, 64);
    AdvState s = base(2); gT += 6000; poll(gT); sendState(&s, 1, 0);
    CHECK(logHas("switches 0010 av0"));
    data(64, 64);
    uint32_t next; CHECK(replyStatus(&next) == ADV_UPD_OK && next == 128);
}

static void test_update_no_copy_when_reply_dropped(void) {
    stubReset(); LINK_Init(); idleTick();            /* a first poll: freshness needs a previous one */
    uint32_t size = 8192, crc = makeImage(size);
    begin(size, crc); eraseAll(); transfer(size);
    gDropReply = 1;
    updReq(ADV_CMD_UPD_COMMIT, 0, 0);
    idleTick();
    CHECK(!logHas("copy 8192"));
}

/* Console accessors and events: counters, state hash, an event per frame and per
 * applied STATE, read after LINK_Poll (nothing formatted inside the frame callback) */
static void test_console_accessors_and_events(void) {
    stubReset(); LINK_Init();
    LinkEvent e;
    while (LINK_PopEvent(&e)) {}
    AdvState s = base(5);
    sendState(&s, 1, 0);
    CHECK(LINK_RxFrames() == 1);
    CHECK(LINK_StateHash() == advStateHash(&s));
    CHECK(LINK_Input() == 5);
    CHECK(LINK_PopEvent(&e) && e.kind == LINK_EV_FRAME && e.cmd == ADV_CMD_STATE && e.len == ADV_STATE_BYTES);
    CHECK(LINK_PopEvent(&e) && e.kind == LINK_EV_STATE && e.steps != 0);
    CHECK(!LINK_PopEvent(&e));
    uint32_t next = 1, size = 1; uint8_t erased = 1, sectors = 1;
    LINK_UpdateProgress(&next, &size, &erased, &sectors);
    CHECK(next == 0 && size == 0 && erased == 0 && sectors == 0);
    CHECK(LINK_UpdateError() == LINK_UPD_ERR_NONE);
    for (int k = 0; k < 40; k++) sendState(&s, 1, 0);       /* the ring keeps the newest, never overflows */
    int n = 0;
    while (LINK_PopEvent(&e)) n++;
    CHECK(n == LINK_EVENT_RING - 1);
}

static void regRead(uint8_t seq, uint8_t dev, uint8_t map, uint8_t reg, uint8_t count) {
    uint8_t b[ADV_REG_READ_REQ_BYTES], f[32];
    advRegReadReqPack(dev, map, reg, count, b);
    size_t n = advFrameEncode(seq, ADV_CMD_REG_READ | ADV_WANT_REPLY, b, sizeof(b), f, sizeof(f));
    for (size_t i = 0; i < n; i++) LINK_Feed(f[i]);
}

static int logIndex(const char* s) { for (int i = 0; i < gLogN; i++) if (!strcmp(gLog[i], s)) return i; return -1; }

static void test_reg_read_map_switch_and_restore(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000); regRead(11, 0x42, 0x40, 0x80, 4); LINK_Process();
    int sw = logIndex("i2c 42 0E 40"), rd = logIndex("i2c_read 42 80 4"), back = logIndex("i2c 42 0E 00");
    CHECK(sw >= 0 && rd > sw && back > rd);                      /* map, one burst read, map 0 again */
    CHECK(logIndex("reply") > back);                             /* the reply leaves after the restore */
    CHECK(gReplies == 1 && gReplyCmd == (ADV_CMD_REG_READ | ADV_RESP) && gReplySeq == 11 && gReplyLen == 5);
    uint8_t st, v[32], n;
    CHECK(advRegReadReplyUnpack(gReplyPayload, gReplyLen, &st, v, &n));
    CHECK(st == ADV_REG_READ_OK && n == 4 && v[0] == 0x80 && v[3] == 0x83);   /* stub: value = register */
    stubReset(); LINK_Init(); gI2cReadFail = 1;                  /* an I2C error still restores the map */
    poll(1000); poll(2000); regRead(12, 0x42, 0x20, 0x00, 1); LINK_Process();
    CHECK(logIndex("i2c 42 0E 20") >= 0 && logIndex("i2c 42 0E 00") > logIndex("i2c 42 0E 20"));
    CHECK(gReplies == 1 && gReplyLen == 1 && gReplyPayload[0] == ADV_REG_READ_I2C_ERROR);
    stubReset(); LINK_Init();                                    /* no map: no 0x0E write */
    poll(1000); poll(2000); regRead(13, 0x56, 0x00, 0x17, 2); LINK_Process();
    CHECK(logIndex("i2c_read 56 17 2") >= 0 && logIndex("i2c 42 0E 00") < 0 && gReplyLen == 3);
}

static void test_reg_read_bad_request_status_2(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000); regRead(14, 0x56, 0x40, 0x00, 1); LINK_Process();
    CHECK(gReplies == 1 && gReplyLen == 1 && gReplyPayload[0] == ADV_REG_READ_BAD_REQUEST);
    CHECK(logIndex("i2c_read 56 00 1") < 0);                     /* nothing read */
    stubReset(); LINK_Init();
    poll(1000); poll(2000); regRead(15, 0x42, 0x00, 0x00, 33); LINK_Process();
    CHECK(gReplies == 1 && gReplyPayload[0] == ADV_REG_READ_BAD_REQUEST);
}

static void test_info_caps_has_reg_read(void) {
    stubReset(); LINK_Init();
    poll(1000); poll(2000); request(6, ADV_CMD_INFO | ADV_WANT_REPLY); LINK_Process();
    AdvInfo i; CHECK(advInfoUnpack(gReplyPayload, gReplyLen, &i));
    CHECK(i.caps & ADV_INFO_CAP_REG_READ);
}

/* Listen (input search): RGB SCART + listen powers the decoder on CVBS with the termination on; Composite after it
 * keeps the decoder (no second init); listen off powers it down */
static void test_listen_lifecycle(void) {
    stubReset(); LINK_Init();
    AdvState s = base(1);
    sendState(&s, 1, 0);
    gLogN = 0;
    s.listen = 1; sendState(&s, 1, 0);
    CHECK(logHas("init") && !logHas("deinit"));
    CHECK(logHas("switches 0001 av1"));
    CHECK(gRegs.adv == 1 && gRegs.ycInput == 0 && gRegs.inputSignal == 1);
    gLogN = 0;
    AdvState av = base(6); sendState(&av, 1, 0);
    CHECK(!logHas("init") && !logHas("deinit"));
    CHECK(logHas("switches 0010 av1"));
    gLogN = 0;
    AdvState l = base(1); l.listen = 1; sendState(&l, 1, 0);     /* Composite -> RGB SCART listening: still no init */
    CHECK(!logHas("init") && !logHas("deinit"));
    gLogN = 0;
    l.listen = 0; sendState(&l, 1, 0);
    CHECK(logHas("deinit") && logHas("switches 0001 av0"));
}

static int logCount(const char* s) { int n = 0; for (int i = 0; i < gLogN; i++) if (!strcmp(gLog[i], s)) n++; return n; }

/* A group whose I2C writes failed is applied again with the next STATE (the ESP resends it every 5 s) */
static void test_failed_group_retried_with_the_next_state(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();
    gHalFail = ADV_STEP_BCSH;
    s.brightness = 150; sendState(&s, 1, 0);
    CHECK(logCount("bcsh") == 1);
    gHalFail = 0; gLogN = 0;
    sendState(&s, 1, 0);                           /* the keep-alive: same values */
    CHECK(logCount("bcsh") == 1 && !logHas("ace") && !logHas("filters") && !logHas("init"));
    gLogN = 0;
    sendState(&s, 1, 0);                           /* applied now: nothing more */
    CHECK(gLogN == 0);
}

static void test_failed_init_retried(void) {
    stubReset(); LINK_Init();
    gHalFail = ADV_STEP_INPUT;
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();                                   /* clears the HAL failure, the log and the stored globals */
    sendState(&s, 1, 0);
    CHECK(logCount("init") == 1);
    CHECK(gLogN >= 2 && !strcmp(gLog[0], "globals"));   /* the globals are loaded before the retried init */
    CHECK(gRegs.adv == 1 && gRegs.ycInput == 1);
}

/* A chip that keeps failing is not re-initialised forever: three retries, then the STATE stands as applied */
static void test_retry_gives_up_after_three(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    stubReset();
    gHalFail = ADV_STEP_BCSH;
    s.brightness = 150;
    for (int i = 0; i < 6; i++) sendState(&s, 1, 0);
    CHECK(logCount("bcsh") == 4);
    gLogN = 0;
    s.brightness = 160;                            /* a new change gets a new budget: 1 + 3 again */
    for (int i = 0; i < 6; i++) sendState(&s, 1, 0);
    CHECK(logCount("bcsh") == 4);
}

/* The decoder turned off: its failed groups no longer matter */
static void test_retry_dropped_when_the_decoder_turns_off(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    gHalFail = ADV_STEP_BCSH;
    s.brightness = 150; sendState(&s, 1, 0);
    gHalFail = 0; gLogN = 0;
    AdvState r = base(1); sendState(&r, 1, 0);
    sendState(&r, 1, 0);
    CHECK(!logHas("bcsh") && !logHas("init"));
}

static void test_state_event_reports_failed_groups(void) {
    stubReset(); LINK_Init();
    AdvState s = base(5); sendState(&s, 1, 0);
    LinkEvent e;
    while (LINK_PopEvent(&e)) {}
    gHalFail = ADV_STEP_BCSH | ADV_STEP_FILTERS;
    s.brightness = 150; s.filterY = 2; sendState(&s, 1, 0);
    int found = 0;
    while (LINK_PopEvent(&e)) if (e.kind == LINK_EV_STATE) found = e.failed == (ADV_STEP_BCSH | ADV_STEP_FILTERS);
    CHECK(found);
}

int main(void) {
    test_failed_group_retried_with_the_next_state();
    test_failed_init_retried();
    test_retry_gives_up_after_three();
    test_retry_dropped_when_the_decoder_turns_off();
    test_state_event_reports_failed_groups();
    test_listen_lifecycle();
    test_reg_read_map_switch_and_restore();
    test_reg_read_bad_request_status_2();
    test_info_caps_has_reg_read();
    test_first_state_rgbs();
    test_first_state_sv_globals_before_init();
    test_composite_uses_its_format_and_cvbs();
    test_brightness_change_on_sv();
    test_same_state_twice_does_nothing();
    test_invalid_frame_ignored();
    test_i2c_batch();
    test_noise_before_frame();
    test_smooth_forced_off_without_i2p();
    test_every_field_reaches_the_registers();
    test_input_change_to_rgbs_powers_down();
    test_only_the_latest_buffered_state_is_applied();
    test_ping_reply();
    test_info_reply_reports_applied_state();
    test_signal_reply();
    test_unknown_request_gets_nak();
    test_state_with_reply_bit_never_replies();
    test_stale_request_not_answered();
    test_reply_time_becomes_the_previous_poll();
    test_dropped_reply_counts();
    test_reply_before_pending_state();
    test_state_and_batches_in_arrival_order();
    test_responses_from_the_line_are_ignored();
    test_update_happy_path();
    test_update_resume_and_bad_length();
    test_update_crc_and_header_bad();
    test_update_commit_rules();
    test_update_too_big_and_no_session();
    test_update_restart_mid_transfer();
    test_update_program_error();
    test_update_status_and_stale_after_erase();
    test_update_state_still_applied();
    test_update_no_copy_when_reply_dropped();
    test_console_accessors_and_events();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
