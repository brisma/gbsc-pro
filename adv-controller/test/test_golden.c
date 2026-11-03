/* Golden I2C traces (make test): the I2C/GPIO/delay sequence of every apply path, recorded from the firmware's
 * own ADV modules (adv7280, adv7391, board, led) + link_hal_adv.c + link.c built on the host. A refactor must
 * keep every trace identical. GOLDEN_UPDATE=1 rewrites them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link.h"
#include "adv-link.h"
#include "golden_trace.h"

void ADV_DetectLoop(void);
extern uint8_t err_flag;

static int fails, files;

static AdvState base(uint8_t input) {
    AdvState s; memset(&s, 0, sizeof(s));
    s.version = ADV_STATE_VERSION; s.input = input; s.syncStripper = 1; s.termination = 0; s.formatSv = 0; s.formatAv = 0;
    s.i2p = 1; s.smooth = 0; s.ace = 0; s.aceLumaGain = 13; s.aceChromaGain = 8; s.aceChromaMax = 8; s.aceGammaGain = 8;
    s.aceResponseSpeed = 15; s.brightness = 128; s.contrast = 128; s.saturation = 128; s.hue = 128;
    s.filterY = 1; s.filterC = 0; s.filterWy = 19; s.filterWyOverride = 1; s.combNtsc = 0; s.combPal = 1;
    s.combTapsNtsc = 2; s.combTapsPal = 3;
    return s;
}

static void feed(uint8_t cmd, const uint8_t* p, uint8_t n) {
    static uint8_t seq;
    uint8_t f[160];
    size_t m = advFrameEncode(seq++, cmd, p, n, f, sizeof(f));
    for (size_t i = 0; i < m; i++) LINK_Feed(f[i]);
    LINK_Process();
}

static void state(const AdvState* s, const char* note) {
    uint8_t b[ADV_STATE_BYTES];
    advStatePack(s, b);
    GOLDEN_Note(note);
    feed(ADV_CMD_STATE, b, ADV_STATE_BYTES);
}

static void detect(uint8_t status1, uint8_t status3, int ticks, const char* note) {
    gReadValue[0x10] = status1; gReadValue[0x13] = status3;
    GOLDEN_Note(note);
    for (int i = 0; i < ticks; i++) ADV_DetectLoop();
}

extern uint8_t c_state, led_state, Input_signal;
uint8_t ADV_Powered(void);
uint8_t ENC_Mode(void);
static int updateRequested(void) {   /* only GOLDEN_UPDATE=1: GOLDEN_UPDATE=0 used to rewrite them too */
    const char* v = getenv("GOLDEN_UPDATE");
    return v && strcmp(v, "1") == 0;
}

static void check(const char* name) {
    char st[40]; snprintf(st, sizeof(st), "end c_state %u led %u input %u adv %u", c_state, led_state, Input_signal, ADV_Powered());
    GOLDEN_Note(st);
    char path[160]; snprintf(path, sizeof(path), "test/golden/%s.txt", name);
    files++;
    if (updateRequested()) {
        FILE* f = fopen(path, "wb");
        for (int i = 0; i < gTraceN; i++) fprintf(f, "%s\n", gTrace[i]);
        fclose(f);
        return;
    }
    FILE* f = fopen(path, "rb");
    if (!f) { printf("FAIL %s: no golden file (run with GOLDEN_UPDATE=1)\n", name); fails++; return; }
    char line[64]; int i = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (i >= gTraceN || strcmp(line, gTrace[i])) {
            printf("FAIL %s line %d: golden \"%s\", got \"%s\"\n", name, i + 1, line, i < gTraceN ? gTrace[i] : "(end)");
            fails++; fclose(f); return;
        }
        i++;
    }
    fclose(f);
    if (i != gTraceN) { printf("FAIL %s: %d extra lines, first \"%s\"\n", name, gTraceN - i, gTrace[i]); fails++; }
}

static void boot(uint8_t input, const char* name) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(input);
    state(&s, "first state");
    check(name);
}

/* Value of the last write to ADV7391 register reg in the trace, -1 if none */
static int lastEncoderWrite(uint8_t reg) {
    int v = -1; char pre[16]; snprintf(pre, sizeof(pre), "w 56 %02X ", reg);
    for (int i = 0; i < gTraceN; i++) if (!strncmp(gTrace[i], pre, 8)) v = (int)strtol(gTrace[i] + 8, NULL, 16);
    return v;
}
static void expect(int ok, const char* what) { if (!ok) { printf("FAIL %s\n", what); fails++; } }

static int traceHas(const char* line) {
    for (int i = 0; i < gTraceN; i++) if (!strcmp(gTrace[i], line)) return 1;
    return 0;
}
static int tracePrefix(const char* pre) {
    for (int i = 0; i < gTraceN; i++) if (!strncmp(gTrace[i], pre, strlen(pre))) return 1;
    return 0;
}

static int traceCountFrom(int from, const char* line) {
    int c = 0;
    for (int i = from; i < gTraceN; i++) c += !strcmp(gTrace[i], line);
    return c;
}

/* With I2P off the encoder stays in its ED base, as the 2.4.x firmware did: the SD mode (525i/625i with the
 * non-interlaced 240p/288p mode) lost the sync every ~1.6 s at 240p and shook at 288p on the TV */
uint8_t ENC_Mode(void);
static void i2pOffChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); s.i2p = 0; state(&s, "composite i2p off");
    int n = gTraceN;
    detect(0x05, 0x40, 2, "ntsc 480i");
    expect(traceCountFrom(n, "w 56 17 02") == 0, "I2P off: no encoder reset");
    expect(traceCountFrom(n, "w 56 01 00") == 0, "I2P off: no SD input mode");
    expect(lastEncoderWrite(0x30) == 0x04, "I2P off, 60 Hz: ED 525p");
    expect(ENC_Mode() == 1, "encoder mode 525p");
    n = gTraceN;
    detect(0x45, 0x04, 2, "pal 288p");
    expect(lastEncoderWrite(0x30) == 0x1C, "I2P off, 50 Hz: ED 625p");
    expect(traceCountFrom(n, "w 56 88 02") == 0 && traceCountFrom(n, "w 56 88 00") == 0, "no SD non-interlaced mode");
    expect(ENC_Mode() == 2, "encoder mode 625p");
    n = gTraceN;
    { AdvState t = s; t.i2p = 1; state(&t, "i2p on"); }
    detect(0x45, 0x04, 2, "pal 576p");
    expect(traceCountFrom(n, "w 56 17 02") == 0, "I2P on again: the ED base stays (no reset)");
    n = gTraceN;
    detect(0x45, 0x04, 2, "still 576p");
    int writes = 0;
    for (int i = n; i < gTraceN; i++) writes += !strncmp(gTrace[i], "w 56 ", 5);
    expect(writes == 0, "no encoder writes while nothing changes");
}

/* A batch whose first row NAKs while a later one ACKs must not be recorded as written */
static void encoderRobustnessChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); state(&s, "composite");
    detect(0x05, 0x00, 2, "ntsc 525p");
    gWriteFailReg = 0x30; detect(0x45, 0x04, 1, "pal, 0x30 NAKs, 0x31 ACKs"); gWriteFailReg = -1;
    int n = gTraceN;
    detect(0x45, 0x04, 1, "pal again");
    expect(traceCountFrom(n, "w 56 30 1C") == 1, "625p retried after a partial batch failure");
}

/* The encoder follows the decoder's output rate (STATUS_3 SD_OP_50Hz), not AD_RESULT:
 * they differ with a forced format and in free-run */
static void outputRateChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); state(&s, "composite");
    detect(0x45, 0x00, 2, "pal detected, 60 Hz output");
    expect(lastEncoderWrite(0x30) == 0x04, "525p when the output is 60 Hz, whatever AD_RESULT says");
    detect(0x05, 0x04, 2, "ntsc detected, 50 Hz output");
    expect(lastEncoderWrite(0x30) == 0x1C, "625p when the output is 50 Hz, whatever AD_RESULT says");
}

/* One status read per tick, STATUS_2 included */
static void detectTickChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); state(&s, "composite");
    int n = gTraceN, reads = 0, s1 = 0;
    for (int i = 0; i < 4; i++) ADV_DetectLoop();
    for (int i = n; i < gTraceN; i++) { reads += !strcmp(gTrace[i], "r 42 12 n1"); s1 += !strcmp(gTrace[i], "r 42 10 n1"); }
    expect(reads == 4 && s1 == 4, "STATUS_1 and STATUS_2 read on every detect tick");
}

/* Decoder init sequence */
static void initChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); state(&s, "composite");
    expect(!tracePrefix("w FF "), "no I2C transaction to the reserved address 0x7F");
    int resets = 0, delayed = 0;
    for (int i = 0; i + 1 < gTraceN; i++)
        if (!strcmp(gTrace[i], "w 56 17 02")) { resets++; delayed += !strcmp(gTrace[i + 1], "delay 10"); }
    expect(resets == 2 && delayed == 2, "a real 10 ms delay right after each soft reset");
    expect(traceHas("w 42 07 7F"), "SECAM-525 autodetect off (0x07=0x7F)");
    expect(!traceHas("w 42 07 FF"), "no 0x07=0xFF");
    expect(!tracePrefix("r 42 E0"), "no read of undocumented 0xE0");
    expect(traceHas("w 42 14 11"), "free-run pattern: 100 % colour bars");
    expect(traceHas("w 42 4D CC"), "DNR and CTI off by default (the chip resets 0x4D to 0xEF)");
    { AdvState t = s; t.dnr = 1; state(&t, "dnr on"); }
    expect(traceHas("w 42 4D EC"), "DNR on: 0x4D bit 5");
    { AdvState t = s; t.cti = 1; state(&t, "cti on"); }
    expect(traceHas("w 42 4D CF"), "CTI on: 0x4D bits 0-1");
    { AdvState t = s; t.formatAv = 12; state(&t, "auto 7.5 IRE"); }
    expect(traceHas("w 42 02 14"), "format 12: autodetect with NTSC-M setup");
    expect(!traceHas("w 42 14 15"), "no boundary box pattern");
}

/* Encoder output standard (not a golden: these pin the behaviour) */
static void encoderChecks(void) {
    /* NTSC source, re-initialised on NTSC (input change): ADV_Init resets the ADV7391 to 625p, it must go back to 525p */
    GOLDEN_Reset(); LINK_Init();
    gReadValue[0x10] = 0x05;
    AdvState s = base(6); state(&s, "composite ntsc");
    expect(lastEncoderWrite(0x30) == 0x04, "encoder 525p after the first init on NTSC");
    { AdvState t = base(5); state(&t, "to svideo, still ntsc"); }
    expect(lastEncoderWrite(0x30) == 0x04, "encoder 525p after a re-init on NTSC");
    /* A failed AD_RESULT read must not switch the encoder */
    detect(0x05, 0x00, 2, "ntsc locked");
    int n = gTraceN;
    gReadFail = 1; GOLDEN_Note("reads fail"); for (int i = 0; i < 3; i++) ADV_DetectLoop(); gReadFail = 0;
    int switched = 0;
    for (int i = n; i < gTraceN; i++) if (!strncmp(gTrace[i], "w 56 30 ", 8)) switched = 1;
    expect(!switched, "no encoder write while the STATUS_3 read fails");
    expect(lastEncoderWrite(0x30) == 0x04, "encoder still 525p after failed reads");
    /* A failed encoder write must be retried: the cache only records what reached the chip */
    gWriteFail = 1; detect(0x45, 0x04, 1, "pal, encoder write fails"); gWriteFail = 0;
    n = gTraceN;
    detect(0x45, 0x04, 2, "pal, writes work again");
    int retried = 0;
    for (int i = n; i < gTraceN; i++) if (!strcmp(gTrace[i], "w 56 30 1C")) retried = 1;
    expect(retried, "encoder 625p retried after a failed write");
    /* Powered down (RGBs): the encoder mode is unknown, not the last standard */
    { AdvState t = base(1); state(&t, "to rgbs"); }
    expect(ENC_Mode() == 0, "encoder mode 0 after ADV_Deinit");
}

/* A decoder group whose writes NAK is written again with the next STATE (the ESP's keep-alive), the encoder is not
 * counted (it retries on its own): link.c retries what the ADV modules report */
static void retryOne(AdvState* s, AdvState t, int addr, int reg, const char* line, const char* what) {
    gWriteFailAddr = addr; gWriteFailReg = reg; state(&t, what); gWriteFailAddr = -1; gWriteFailReg = -1;
    int n = gTraceN;
    state(&t, "keep-alive");
    char msg[80]; snprintf(msg, sizeof(msg), "%s written again after a NAK", what);
    expect(traceCountFrom(n, line) == 1, msg);
    n = gTraceN;
    state(&t, "keep-alive 2");
    snprintf(msg, sizeof(msg), "%s: nothing after the good retry", what);
    expect(gTraceN == n + 1, msg);
    state(s, "back");
}

static void retryChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); state(&s, "composite");
    { AdvState t = s; t.brightness = 140; retryOne(&s, t, 0x42, 0x0A, "w 42 0A 0C", "brightness"); }
    { AdvState t = s; t.formatAv = 2; retryOne(&s, t, 0x42, 0x02, "w 42 02 54", "format"); }
    { AdvState t = s; t.i2p = 0; retryOne(&s, t, 0x84, 0x55, "w 84 55 00", "i2p off"); }
    { AdvState t = s; t.ace = 1; retryOne(&s, t, 0x42, 0x80, "w 42 80 80", "ace on"); }
    { AdvState t = s; t.filterY = 5; retryOne(&s, t, 0x42, 0x17, "w 42 17 05", "filters"); }

    GOLDEN_Reset(); LINK_Init();
    gWriteFailAddr = 0x42; gWriteFailReg = 0x00; state(&s, "init, input control NAKs"); gWriteFailAddr = -1; gWriteFailReg = -1;
    int n = gTraceN;
    state(&s, "keep-alive");
    expect(traceCountFrom(n, "delay 300") == 1, "failed init done again");

    GOLDEN_Reset(); LINK_Init();
    gWriteFailAddr = 0x56; gWriteFailReg = 0x00; state(&s, "init, encoder NAKs"); gWriteFailAddr = -1; gWriteFailReg = -1;
    n = gTraceN;
    state(&s, "keep-alive");
    expect(traceCountFrom(n, "delay 300") == 0, "an encoder NAK alone does not repeat the decoder init");

    GOLDEN_Reset(); LINK_Init();
    gWriteFailAddr = 0x56; gWriteFailReg = 0x17; state(&s, "init, encoder reset NAKs"); gWriteFailAddr = -1; gWriteFailReg = -1;
    n = gTraceN;
    state(&s, "keep-alive");
    expect(traceCountFrom(n, "delay 300") == 0, "an encoder reset NAK does not repeat the decoder init");
}

/* An encoder that stays failed: its reset and base reload (10 ms each time) at most once a second (20 detection
 * passes), not on every pass, so the link polls are not left waiting; it recovers within a second */
static void encoderReloadChecks(void) {
    GOLDEN_Reset(); LINK_Init();
    gWriteFailAddr = 0x56; gWriteFailReg = 0x00;
    AdvState s = base(6); state(&s, "composite, encoder base NAKs");
    gWriteFailReg = 0x17;
    int n = gTraceN;
    detect(0x05, 0x00, 40, "encoder reset keeps failing");
    expect(traceCountFrom(n, "w 56 17 02") <= 2, "encoder reset at most once a second while it fails");
    expect(traceCountFrom(n, "delay 10") <= 2, "no 10 ms pause on every pass");
    gWriteFailAddr = -1; gWriteFailReg = -1;
    n = gTraceN;
    detect(0x05, 0x00, 20, "encoder back");
    expect(traceCountFrom(n, "w 56 17 02") == 1, "encoder reloaded within a second");
    expect(ENC_Mode() == 1, "encoder 525p after the reload");
}

int main(void) {
    boot(1, "boot_rgbs"); boot(2, "boot_rgsb"); boot(3, "boot_vga"); boot(4, "boot_ypbpr");
    boot(5, "boot_svideo"); boot(6, "boot_composite");

    /* Composite running: every group change, one at a time, back to the base in between */
    GOLDEN_Reset(); LINK_Init();
    AdvState s = base(6); state(&s, "base composite");
    for (uint8_t f = 1; f < 12; f++) { AdvState t = s; t.formatAv = f; char n[24]; snprintf(n, sizeof(n), "format %u", f); state(&t, n); }
    { AdvState t = s; t.i2p = 0; state(&t, "i2p off"); }
    state(&s, "i2p on");
    { AdvState t = s; t.smooth = 1; state(&t, "smooth on"); }
    state(&s, "smooth off");
    { AdvState t = s; t.ace = 1; state(&t, "ace on"); t.aceLumaGain = 20; t.aceGammaGain = 3; state(&t, "ace params"); }
    state(&s, "ace off");
    { AdvState t = s; t.brightness = 140; t.contrast = 100; t.saturation = 150; t.hue = 120; state(&t, "bcsh"); }
    state(&s, "bcsh back");
    { AdvState t = s; t.filterY = 5; t.filterC = 3; t.filterWy = 7; t.filterWyOverride = 0; state(&t, "filters"); }
    { AdvState t = s; t.combNtsc = 2; t.combPal = 3; t.combLumaNtsc = 4; t.combChromaPal = 5; t.combTapsPal = 1; state(&t, "comb"); }
    state(&s, "filters back");
    check("composite_groups");

    /* Listen on RGB SCART (input search): decoder on CVBS + termination, then off */
    GOLDEN_Reset(); LINK_Init();
    s = base(1); state(&s, "rgbs");
    s.listen = 1; state(&s, "listen on");
    s.listen = 0; state(&s, "listen off");
    check("rgbs_listen");

    /* Listen, then the identification moves to Composite: the decoder stays initialised */
    GOLDEN_Reset(); LINK_Init();
    s = base(1); state(&s, "rgbs");
    s.listen = 1; state(&s, "listen on");
    { AdvState t = base(6); state(&t, "to composite"); }
    check("rgbs_listen_to_av");

    /* Input changes */
    GOLDEN_Reset(); LINK_Init();
    s = base(6); state(&s, "composite");
    { AdvState t = base(5); state(&t, "to svideo"); }
    { AdvState t = base(1); state(&t, "to rgbs"); t.syncStripper = 0; state(&t, "stripper off"); t.termination = 1; state(&t, "termination"); }
    { AdvState t = base(6); t.i2p = 0; state(&t, "to composite i2p off"); }
    check("input_changes");

    /* Detection ticks: NTSC lock, PAL, lost lock (free-run) with err_flag, lock back */
    GOLDEN_Reset(); LINK_Init();
    s = base(6); state(&s, "composite");
    detect(0x05, 0x00, 4, "ntsc locked");
    detect(0x45, 0x04, 4, "pal locked");
    err_flag = 1; detect(0x47, 0x14, 2, "lost lock");
    err_flag = 1; detect(0x45, 0x04, 2, "lock back");
    detect(0x05, 0x00, 2, "ntsc again");
    check("detect");

    /* I2C passthrough batch */
    GOLDEN_Reset(); LINK_Init();
    { uint8_t p[] = {2, 0x42, 0x0A, 0x10, 0x56, 0x01, 0x70}; GOLDEN_Note("i2c batch"); feed(ADV_CMD_I2C_BATCH, p, sizeof(p)); }
    check("i2c_batch");

    initChecks();
    outputRateChecks();
    detectTickChecks();
    i2pOffChecks();
    encoderRobustnessChecks();
    retryChecks();
    encoderReloadChecks();
    encoderChecks();   /* last: it leaves the encoder cache in its own state */

    printf("golden: %d files, %d failures%s\n", files, fails, updateRequested() ? " (updated)" : "");
    return fails ? 1 : 0;
}
