#include "adv-link.h"
#include <string.h>

uint16_t advCrc16(const uint8_t* p, size_t n) {
    uint16_t c = 0xFFFF;
    while (n--) {
        c ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++) c = (uint16_t)((c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}

size_t advFrameEncode(uint8_t seq, uint8_t cmd, const uint8_t* payload, uint8_t len, uint8_t* out, size_t cap) {
    size_t n = (size_t)len + ADV_FRAME_OVERHEAD;
    if (len > ADV_MAX_PAYLOAD || n > cap) return 0;
    out[0] = ADV_SYNC0; out[1] = ADV_SYNC1; out[2] = len; out[3] = seq; out[4] = cmd;
    if (len) memcpy(out + 5, payload, len);
    uint16_t c = advCrc16(out + 2, (size_t)len + 3);
    out[5 + len] = (uint8_t)(c >> 8);
    out[6 + len] = (uint8_t)c;
    return n;
}

void advParserInit(AdvParser* p) { memset(p, 0, sizeof(*p)); }

/* raw holds bytes from a sync; returns 1 when raw[0..n) is a complete valid frame, -1 when it can never be,
 * 0 when more bytes are needed */
static int check(const AdvParser* p) {
    if (p->n >= 1 && p->raw[0] != ADV_SYNC0) return -1;
    if (p->n >= 2 && p->raw[1] != ADV_SYNC1) return -1;
    if (p->n >= 3 && p->raw[2] > ADV_MAX_PAYLOAD) return -1;
    if (p->n < 3) return 0;
    uint8_t total = (uint8_t)(p->raw[2] + ADV_FRAME_OVERHEAD);
    if (p->n < total) return 0;
    uint16_t c = advCrc16(p->raw + 2, (size_t)p->raw[2] + 3);
    return (p->raw[total - 2] == (uint8_t)(c >> 8) && p->raw[total - 1] == (uint8_t)c) ? 1 : -1;
}

static void deliver(AdvParser* p, AdvFrameCb cb, void* ctx) {
    p->len = p->raw[2];
    p->seq = p->raw[3];
    p->cmd = p->raw[4];
    memcpy(p->buf, p->raw + 5, p->len);
    p->n = 0;
    if (cb) cb(p, ctx);
}

void advParserFeed(AdvParser* p, uint8_t b, AdvFrameCb cb, void* ctx) {
    p->raw[p->n++] = b;
    for (;;) {
        int r = check(p);
        if (r == 0) return;
        if (r == 1) { deliver(p, cb, ctx); return; }
        /* rejected: count a real candidate (sync seen), then rescan from the byte after its first sync byte */
        if (p->n >= 3) p->bad++;
        uint8_t keep = (uint8_t)(p->n - 1);
        memmove(p->raw, p->raw + 1, keep);
        p->n = keep;
        uint8_t i = 0;
        while (i < p->n && p->raw[i] != ADV_SYNC0) i++;
        if (i) { memmove(p->raw, p->raw + i, (size_t)(p->n - i)); p->n = (uint8_t)(p->n - i); }
        if (!p->n) return;
        /* re-check what is left: it may complete (or start) another frame */
        while (p->n) {
            int q = check(p);
            if (q == 0) return;
            if (q == 1) {
                uint8_t total = (uint8_t)(p->raw[2] + ADV_FRAME_OVERHEAD);
                uint8_t rest = (uint8_t)(p->n - total);
                uint8_t tail[ADV_MAX_PAYLOAD + ADV_FRAME_OVERHEAD];
                memcpy(tail, p->raw + total, rest);
                deliver(p, cb, ctx);
                for (uint8_t k = 0; k < rest; k++) advParserFeed(p, tail[k], cb, ctx);
                return;
            }
            break;   /* rejected again: outer loop drops one byte and rescans */
        }
    }
}

/* ---- state ---- */

static uint8_t crc8(const uint8_t* p, size_t n) {
    uint8_t c = 0;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++) c = (uint8_t)((c & 0x80) ? (c << 1) ^ 0x07 : c << 1);
    }
    return c;
}

/* Packed order = struct order (byte 32 listen); bytes 33..38 reserved (0), byte 39 crc8 */
size_t advStatePack(const AdvState* s, uint8_t* out) {
    memset(out, 0, ADV_STATE_BYTES);
    memcpy(out, s, sizeof(AdvState) < ADV_STATE_BYTES - 1 ? sizeof(AdvState) : ADV_STATE_BYTES - 1);
    out[ADV_STATE_BYTES - 1] = crc8(out, ADV_STATE_BYTES - 1);
    return ADV_STATE_BYTES;
}

static uint8_t clampTo(uint8_t v, uint8_t max) { return v > max ? max : v; }

int advStateUnpack(const uint8_t* in, size_t n, AdvState* s) {
    if (n != ADV_STATE_BYTES || crc8(in, ADV_STATE_BYTES - 1) != in[ADV_STATE_BYTES - 1]) return 0;
    AdvState t;
    memcpy(&t, in, sizeof(t));
    if (t.version != ADV_STATE_VERSION || t.input < ADV_INPUT_RGBS || t.input > ADV_INPUT_AV || t.formatSv >= ADV_FORMAT_COUNT || t.formatAv >= ADV_FORMAT_COUNT) return 0;
    t.syncStripper = clampTo(t.syncStripper, 1); t.termination = clampTo(t.termination, 1);
    t.i2p = clampTo(t.i2p, 1); t.smooth = clampTo(t.smooth, 1); t.ace = clampTo(t.ace, 1);
    t.aceLumaGain = clampTo(t.aceLumaGain, 31); t.aceChromaGain = clampTo(t.aceChromaGain, 15);
    t.aceChromaMax = clampTo(t.aceChromaMax, 15); t.aceGammaGain = clampTo(t.aceGammaGain, 15);
    t.aceResponseSpeed = clampTo(t.aceResponseSpeed, 15);
    t.filterY = clampTo(t.filterY, 31); t.filterC = clampTo(t.filterC, 7); t.filterWy = clampTo(t.filterWy, 31);
    t.filterWyOverride = clampTo(t.filterWyOverride, 1); t.combNtsc = clampTo(t.combNtsc, 3);
    t.combPal = clampTo(t.combPal, 3);
    t.combLumaNtsc = clampTo(t.combLumaNtsc, 7); t.combChromaNtsc = clampTo(t.combChromaNtsc, 7);
    t.combTapsNtsc = clampTo(t.combTapsNtsc, 3); t.combLumaPal = clampTo(t.combLumaPal, 7);
    t.combChromaPal = clampTo(t.combChromaPal, 7); t.combTapsPal = clampTo(t.combTapsPal, 3);
    t.dnr = clampTo(t.dnr, 1); t.cti = clampTo(t.cti, 1); t.listen = clampTo(t.listen, 1);
    *s = t;
    return 1;
}

uint16_t advStateHash(const AdvState* s) {
    uint8_t b[ADV_STATE_BYTES];
    advStatePack(s, b);
    return advCrc16(b, ADV_STATE_BYTES);
}

/* ---- switches and plan ---- */

AdvSwitches advSwitchesFor(const AdvState* s) {
    AdvSwitches w = {0, 0, 1, 0, 0, 0};
    uint8_t strip = s->syncStripper ? 0 : 1;   /* ASW2 = 0 routes the sync through the LM1881 stripper */
    switch (s->input) {
        case ADV_INPUT_SV: case ADV_INPUT_AV: w.asw1 = 0; w.asw2 = 0; w.asw3 = 1; w.asw4 = 0; w.avConnect = 1; w.adv = 1; break;
        case ADV_INPUT_RGBS:   /* listen: BL1551 on (termination) and the decoder powered, the TV5725 path unchanged */
            w.asw1 = 0; w.asw2 = strip; w.asw3 = 0; w.asw4 = 1; w.avConnect = (s->termination || s->listen) ? 1 : 0;
            w.adv = s->listen ? 1 : 0;
            break;
        case ADV_INPUT_RGSB: w.asw1 = 0; w.asw2 = strip; w.asw3 = 1; w.asw4 = 0; break;
        case ADV_INPUT_VGA: w.asw1 = 1; w.asw2 = 1; w.asw3 = 1; w.asw4 = 1; break;
        default: w.asw1 = 0; w.asw2 = 0; w.asw3 = 1; w.asw4 = 0; break;   /* YPbPr */
    }
    return w;
}

int advInputIsDecoder(uint8_t input) { return input == ADV_INPUT_SV || input == ADV_INPUT_AV; }
int advDecoderOn(const AdvState* s) { return advInputIsDecoder(s->input) || (s->input == ADV_INPUT_RGBS && s->listen); }
uint8_t advActiveFormat(const AdvState* s) { return s->input == ADV_INPUT_SV ? s->formatSv : s->formatAv; }

uint8_t advPlan(const AdvState* cur, const AdvState* next, int haveCur) {
    const uint8_t all = ADV_STEP_FORMAT | ADV_STEP_I2P | ADV_STEP_ACE | ADV_STEP_BCSH | ADV_STEP_FILTERS;
    int on = advDecoderOn(next);
    /* a new decoder init: first state, the decoder turning on or off, S-Video <-> CVBS, or another input with the
     * decoder off; RGB SCART listening <-> Composite keeps the decoder (both CVBS): switches and changed groups only */
    if (!haveCur || !cur || advDecoderOn(cur) != on || (on && (cur->input == ADV_INPUT_SV) != (next->input == ADV_INPUT_SV)) ||
        (!on && cur->input != next->input))
        return (uint8_t)(ADV_STEP_INPUT | ADV_STEP_SWITCHES | (on ? all : 0));
    uint8_t steps = 0;
    AdvSwitches a = advSwitchesFor(cur), b = advSwitchesFor(next);
    if (memcmp(&a, &b, sizeof(a))) steps |= ADV_STEP_SWITCHES;
    if (!on) return steps;     /* decoder off: values are applied at the next decoder input */
    if (advActiveFormat(cur) != advActiveFormat(next)) steps |= ADV_STEP_FORMAT;
    if (cur->i2p != next->i2p || cur->smooth != next->smooth) steps |= ADV_STEP_I2P;
    if (cur->ace != next->ace || cur->aceLumaGain != next->aceLumaGain || cur->aceChromaGain != next->aceChromaGain ||
        cur->aceChromaMax != next->aceChromaMax || cur->aceGammaGain != next->aceGammaGain ||
        cur->aceResponseSpeed != next->aceResponseSpeed)
        steps |= ADV_STEP_ACE;
    if (cur->brightness != next->brightness || cur->contrast != next->contrast ||
        cur->saturation != next->saturation || cur->hue != next->hue)
        steps |= ADV_STEP_BCSH;
    if (memcmp(&cur->filterY, &next->filterY, (size_t)(&next->cti - &next->filterY) + 1))   /* filters .. dnr, cti */
        steps |= ADV_STEP_FILTERS;
    return steps;
}

uint8_t advFormatRegister(uint8_t index) {
    /* 12: autodetect with NTSC-M 7.5 IRE setup (UG-637 VID_SEL 0001; PAL keeps no pedestal) */
    static const uint8_t kFormats[ADV_FORMAT_COUNT] = {0x04, 0x84, 0x54, 0x64, 0x74, 0x44, 0x94, 0xA4, 0xB4, 0xC4, 0xD4, 0xE4, 0x14};
    return kFormats[index < ADV_FORMAT_COUNT ? index : 0];
}

/* ---- INFO ---- */

static void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static uint16_t get16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t* p) { return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16); }

size_t advInfoPack(const AdvInfo* i, uint8_t* out) {
    memset(out, 0, ADV_INFO_BYTES);
    out[0] = i->protocol;
    out[1] = i->stateVersion;
    memcpy(out + 2, i->fw, 12);
    put32(out + 14, i->buildId);
    out[18] = i->flags;
    out[19] = i->standard;
    put16(out + 20, i->uptimeS);
    put16(out + 22, i->rxFrames);
    put16(out + 24, i->badFrames);
    out[26] = i->appliedCount;
    put16(out + 27, i->stateHash);
    out[29] = i->caps;
    return ADV_INFO_BYTES;
}

int advInfoUnpack(const uint8_t* in, size_t n, AdvInfo* i) {
    if (n != ADV_INFO_BYTES) return 0;
    memset(i, 0, sizeof(*i));
    i->protocol = in[0];
    i->stateVersion = in[1];
    memcpy(i->fw, in + 2, 11);
    i->fw[11] = '\0';
    i->buildId = get32(in + 14);
    i->flags = in[18];
    i->standard = in[19];
    i->uptimeS = get16(in + 20);
    i->rxFrames = get16(in + 22);
    i->badFrames = get16(in + 24);
    i->appliedCount = in[26];
    i->stateHash = get16(in + 27);
    i->caps = in[29];
    return 1;
}

/* ---- signal report ---- */

size_t advSignalPack(const AdvSignal* s, uint8_t* out) {
    memset(out, 0, ADV_SIGNAL_BYTES);
    out[0] = s->gen; out[1] = s->flags; out[2] = s->flags2; out[3] = s->standard; out[4] = s->hz;
    out[5] = s->status1; out[6] = s->status2; out[7] = s->status3; out[8] = s->vidSel; out[9] = s->encoder;
    out[10] = s->input;
    return ADV_SIGNAL_BYTES;
}

int advSignalUnpack(const uint8_t* in, size_t n, AdvSignal* s) {
    if (n != ADV_SIGNAL_BYTES) return 0;
    memset(s, 0, sizeof(*s));
    s->gen = in[0]; s->flags = in[1]; s->flags2 = in[2]; s->standard = in[3]; s->hz = in[4];
    s->status1 = in[5]; s->status2 = in[6]; s->status3 = in[7]; s->vidSel = in[8]; s->encoder = in[9];
    s->input = in[10];
    return 1;
}

/* ---- update payloads ---- */

uint32_t advCrc32Update(uint32_t crc, const uint8_t* p, size_t n) {
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

uint32_t advCrc32(const uint8_t* p, size_t n) { return advCrc32Update(0, p, n); }

size_t advUpdBeginPack(uint32_t size, uint32_t crc, const char* version, uint8_t* out) {
    memset(out, 0, ADV_UPD_BEGIN_BYTES);
    put32(out, size);
    put32(out + 4, crc);
    if (version) strncpy((char*)out + 8, version, 11);
    return ADV_UPD_BEGIN_BYTES;
}

int advUpdBeginUnpack(const uint8_t* in, size_t n, uint32_t* size, uint32_t* crc, char version[12]) {
    if (n != ADV_UPD_BEGIN_BYTES) return 0;
    *size = get32(in);
    *crc = get32(in + 4);
    memcpy(version, in + 8, 11);
    version[11] = '\0';
    return 1;
}

size_t advUpdDataPack(uint32_t offset, const uint8_t* data, uint8_t n, uint8_t* out) {
    if (n > ADV_UPD_CHUNK) n = ADV_UPD_CHUNK;
    put32(out, offset);
    memcpy(out + 4, data, n);
    return (size_t)n + 4;
}

int advUpdDataUnpack(const uint8_t* in, size_t n, uint32_t* offset, const uint8_t** data, uint8_t* len) {
    if (n < 5 || n > 4 + ADV_UPD_CHUNK) return 0;
    *offset = get32(in);
    *data = in + 4;
    *len = (uint8_t)(n - 4);
    return 1;
}

size_t advUpdReplyPack(uint8_t status, uint32_t next, uint8_t* out) {
    out[0] = status;
    put32(out + 1, next);
    return ADV_UPD_REPLY_BYTES;
}

int advUpdReplyUnpack(const uint8_t* in, size_t n, uint8_t* status, uint32_t* next) {
    if (n != ADV_UPD_REPLY_BYTES) return 0;
    *status = in[0];
    *next = get32(in + 1);
    return 1;
}

size_t advRegReadReqPack(uint8_t dev, uint8_t map, uint8_t reg, uint8_t count, uint8_t* out) {
    out[0] = dev; out[1] = map; out[2] = reg; out[3] = count;
    return ADV_REG_READ_REQ_BYTES;
}

int advRegReadReqUnpack(const uint8_t* in, size_t n, uint8_t* dev, uint8_t* map, uint8_t* reg, uint8_t* count) {
    if (n != ADV_REG_READ_REQ_BYTES) return 0;
    if (in[0] != 0x42 && in[0] != 0x56 && in[0] != 0x84) return 0;
    if (in[1] && (in[0] != 0x42 || (in[1] != 0x20 && in[1] != 0x40))) return 0;
    if (!in[3] || in[3] > ADV_REG_READ_MAX || (unsigned)in[2] + in[3] > 0x100U) return 0;
    *dev = in[0]; *map = in[1]; *reg = in[2]; *count = in[3];
    return 1;
}

size_t advRegReadReplyPack(uint8_t status, const uint8_t* v, uint8_t n, uint8_t* out) {
    out[0] = status;
    if (status != ADV_REG_READ_OK) return 1;
    if (n > ADV_REG_READ_MAX) n = ADV_REG_READ_MAX;
    memcpy(out + 1, v, n);
    return (size_t)(1 + n);
}

int advRegReadReplyUnpack(const uint8_t* in, size_t n, uint8_t* status, uint8_t* v, uint8_t* count) {
    if (n < 1 || n > ADV_REG_READ_REPLY_MAX) return 0;
    *status = in[0];
    *count = (uint8_t)(n - 1);
    if (*status != ADV_REG_READ_OK && n != 1) return 0;
    memcpy(v, in + 1, n - 1);
    return 1;
}
