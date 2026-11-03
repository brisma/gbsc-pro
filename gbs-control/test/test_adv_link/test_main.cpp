#include <unity.h>
#include <string.h>
#include <vector>
#include "adv-link.h"

void setUp(void) {}
void tearDown(void) {}

struct Got { uint8_t seq, cmd, len; std::vector<uint8_t> payload; };
static std::vector<Got> gGot;
static void onFrame(const AdvParser* p, void*) {
    Got g; g.seq = p->seq; g.cmd = p->cmd; g.len = p->len; g.payload.assign(p->buf, p->buf + p->len);
    gGot.push_back(g);
}
static void feedAll(AdvParser& p, const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) advParserFeed(&p, b[i], onFrame, nullptr); }

static size_t frame(uint8_t seq, uint8_t cmd, const uint8_t* pl, uint8_t len, uint8_t* out) {
    return advFrameEncode(seq, cmd, pl, len, out, 128);
}

void test_crc16_vector(void) {
    TEST_ASSERT_EQUAL_HEX16(0x29B1, advCrc16((const uint8_t*)"123456789", 9));
}

void test_round_trip_byte_by_byte(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    uint8_t pl[3] = {1, 2, 3}, f[128];
    size_t n = frame(7, ADV_CMD_STATE, pl, 3, f);
    TEST_ASSERT_EQUAL_UINT32(3 + 5 + 2, n);
    feedAll(p, f, n);
    TEST_ASSERT_EQUAL_UINT32(1, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(7, gGot[0].seq);
    TEST_ASSERT_EQUAL_UINT8(ADV_CMD_STATE, gGot[0].cmd);
    TEST_ASSERT_EQUAL_UINT8(3, gGot[0].len);
    TEST_ASSERT_EQUAL_UINT8(3, gGot[0].payload[2]);
}

void test_noise_split_and_back_to_back(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    const uint8_t noise[] = {0x00, 0xA7, 0x00, 'h', 'e', 'l', 'l', 'o', 0xA7};
    feedAll(p, noise, sizeof(noise));
    uint8_t f1[128], f2[128], pl[2] = {9, 8};
    size_t n1 = frame(1, ADV_CMD_STATE, pl, 2, f1), n2 = frame(2, ADV_CMD_I2C_BATCH, pl, 1, f2);
    feedAll(p, f1, 4); feedAll(p, f1 + 4, n1 - 4);
    feedAll(p, f2, n2);
    TEST_ASSERT_EQUAL_UINT32(2, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(1, gGot[0].seq);
    TEST_ASSERT_EQUAL_UINT8(2, gGot[1].seq);
}

void test_false_sync_inside_payload(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    uint8_t pl[6] = {0xA7, 0x3C, 0x02, 0x00, 0x01, 0x55}, f[128];
    size_t n = frame(3, ADV_CMD_STATE, pl, 6, f);
    feedAll(p, f, n);
    TEST_ASSERT_EQUAL_UINT32(1, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(6, gGot[0].len);
}

void test_bad_crc_then_valid(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    uint8_t pl[2] = {1, 2}, f[128], g[128];
    size_t n = frame(4, ADV_CMD_STATE, pl, 2, f);
    f[n - 1] ^= 0xFF;
    size_t m = frame(5, ADV_CMD_STATE, pl, 2, g);
    feedAll(p, f, n); feedAll(p, g, m);
    TEST_ASSERT_EQUAL_UINT32(1, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(5, gGot[0].seq);
    TEST_ASSERT_EQUAL_UINT16(1, p.bad);
}

void test_oversized_length_rejected(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    const uint8_t bogus[] = {0xA7, 0x3C, 200};
    uint8_t pl[1] = {1}, f[128];
    size_t n = frame(6, ADV_CMD_STATE, pl, 1, f);
    feedAll(p, bogus, 3); feedAll(p, f, n);
    TEST_ASSERT_EQUAL_UINT32(1, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(6, gGot[0].seq);
}

// A bogus header with a plausible length swallows the next real frame; it is found once the CRC fails
void test_bogus_header_swallowing_a_frame(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    uint8_t pl[4] = {1, 2, 3, 4}, f[128];
    size_t n = frame(8, ADV_CMD_STATE, pl, 4, f);
    std::vector<uint8_t> s = {0xA7, 0x3C, 0x0A};           // len 10: covers the real frame (11 bytes) partly
    s.insert(s.end(), f, f + n);
    for (int i = 0; i < 6; i++) s.push_back(0x00);         // enough bytes for the bogus frame to end
    feedAll(p, s.data(), s.size());
    TEST_ASSERT_EQUAL_UINT32(1, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(8, gGot[0].seq);
}

void test_frame_inside_failed_bytes_decoded_once(void) {
    gGot.clear(); AdvParser p; advParserInit(&p);
    uint8_t pl[2] = {5, 6}, f[128];
    size_t n = frame(9, ADV_CMD_STATE, pl, 2, f);         // 9 bytes
    std::vector<uint8_t> s = {0xA7, 0x3C, 20, 0x00, 0x01};  // bogus: len 20 → needs 25 more bytes
    s.insert(s.end(), f, f + n);
    while (s.size() < 3 + 2 + 20 + 2 + 3) s.push_back(0x11);
    feedAll(p, s.data(), s.size());
    TEST_ASSERT_EQUAL_UINT32(1, gGot.size());
    TEST_ASSERT_EQUAL_UINT8(9, gGot[0].seq);
}

static AdvState sample() {
    AdvState s; memset(&s, 0, sizeof(s));
    s.version = ADV_STATE_VERSION; s.input = 5; s.syncStripper = 1; s.formatSv = 2; s.formatAv = 3;
    s.i2p = 1; s.ace = 1; s.aceLumaGain = 13; s.aceChromaGain = 8; s.aceChromaMax = 8; s.aceGammaGain = 8;
    s.aceResponseSpeed = 15; s.brightness = 128; s.contrast = 128; s.saturation = 128; s.hue = 128;
    s.filterY = 1; s.filterWy = 19; s.filterWyOverride = 1; s.combPal = 1; s.combTapsNtsc = 2; s.combTapsPal = 3;
    return s;
}

void test_state_pack_round_trip(void) {
    AdvState s = sample(), r;
    uint8_t b[ADV_STATE_BYTES];
    TEST_ASSERT_EQUAL_UINT32(ADV_STATE_BYTES, advStatePack(&s, b));
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));
    TEST_ASSERT_EQUAL_MEMORY(&s, &r, sizeof(s));
    b[5] ^= 1;
    TEST_ASSERT_FALSE(advStateUnpack(b, sizeof(b), &r));            // crc8
}

void test_state_structural_errors_rejected(void) {
    AdvState s = sample(), r;
    uint8_t b[ADV_STATE_BYTES];
    s.input = 7; advStatePack(&s, b);
    TEST_ASSERT_FALSE(advStateUnpack(b, sizeof(b), &r));
    s = sample(); s.formatSv = 12; advStatePack(&s, b);
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));               // Auto 7.5 IRE
    s = sample(); s.formatSv = 13; advStatePack(&s, b);
    TEST_ASSERT_FALSE(advStateUnpack(b, sizeof(b), &r));
    s = sample(); s.version = 2; advStatePack(&s, b);
    TEST_ASSERT_FALSE(advStateUnpack(b, sizeof(b), &r));
    TEST_ASSERT_FALSE(advStateUnpack(b, sizeof(b) - 1, &r));
}

void test_state_fields_clamped_to_register_widths(void) {
    AdvState s = sample(), r;
    uint8_t b[ADV_STATE_BYTES];
    s.filterY = 40; s.filterC = 9; s.aceLumaGain = 40; s.aceGammaGain = 20; s.combTapsPal = 5; s.i2p = 3;
    advStatePack(&s, b);
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));
    TEST_ASSERT_EQUAL_UINT8(31, r.filterY);
    TEST_ASSERT_EQUAL_UINT8(7, r.filterC);
    TEST_ASSERT_EQUAL_UINT8(31, r.aceLumaGain);
    TEST_ASSERT_EQUAL_UINT8(15, r.aceGammaGain);
    TEST_ASSERT_EQUAL_UINT8(3, r.combTapsPal);
    TEST_ASSERT_EQUAL_UINT8(1, r.i2p);
}

// Issue #56: decoder DNR / CTI, default off; bytes 30/31 of the reserved tail (version stays 1:
// an older ESP sends 0 = off)
void test_state_dnr_cti_bytes_and_plan(void) {
    AdvState s = sample(), r;
    uint8_t b[ADV_STATE_BYTES];
    s.dnr = 1; s.cti = 1;
    advStatePack(&s, b);
    TEST_ASSERT_EQUAL_UINT8(1, b[30]);
    TEST_ASSERT_EQUAL_UINT8(1, b[31]);
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));
    TEST_ASSERT_EQUAL_UINT8(1, r.dnr); TEST_ASSERT_EQUAL_UINT8(1, r.cti);
    s.dnr = 4; advStatePack(&s, b); advStateUnpack(b, sizeof(b), &r);
    TEST_ASSERT_EQUAL_UINT8(1, r.dnr);                                // clamped
    AdvState a = sample(), c = a; c.dnr = 1;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_FILTERS, advPlan(&a, &c, 1));
    c = a; c.cti = 1;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_FILTERS, advPlan(&a, &c, 1));
}

static void sw(const AdvState& s, int a1, int a2, int a3, int a4, int av, int adv) {
    AdvSwitches w = advSwitchesFor(&s);
    TEST_ASSERT_EQUAL_UINT8(a1, w.asw1); TEST_ASSERT_EQUAL_UINT8(a2, w.asw2);
    TEST_ASSERT_EQUAL_UINT8(a3, w.asw3); TEST_ASSERT_EQUAL_UINT8(a4, w.asw4);
    TEST_ASSERT_EQUAL_UINT8(av, w.avConnect); TEST_ASSERT_EQUAL_UINT8(adv, w.adv);
}

void test_truth_table(void) {
    AdvState s = sample();
    s.input = 5; sw(s, 0, 0, 1, 0, 1, 1);
    s.input = 6; sw(s, 0, 0, 1, 0, 1, 1);
    s.input = 1; s.syncStripper = 1; s.termination = 0; sw(s, 0, 0, 0, 1, 0, 0);
    s.syncStripper = 0; s.termination = 1; sw(s, 0, 1, 0, 1, 1, 0);
    s.input = 2; s.syncStripper = 1; sw(s, 0, 0, 1, 0, 0, 0);
    s.syncStripper = 0; sw(s, 0, 1, 1, 0, 0, 0);
    s.input = 3; sw(s, 1, 1, 1, 1, 0, 0);
    s.input = 4; sw(s, 0, 0, 1, 0, 0, 0);
}

static const uint8_t ADV_ALL = ADV_STEP_FORMAT | ADV_STEP_I2P | ADV_STEP_ACE | ADV_STEP_BCSH | ADV_STEP_FILTERS;

void test_plan(void) {
    AdvState a = sample(), b;
    a.input = 1;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_INPUT | ADV_STEP_SWITCHES, advPlan(nullptr, &a, 0));
    a.input = 5;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_INPUT | ADV_STEP_SWITCHES | ADV_ALL, advPlan(nullptr, &a, 0));
    b = a;
    TEST_ASSERT_EQUAL_HEX8(0, advPlan(&a, &b, 1));
    b.brightness = 140;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_BCSH, advPlan(&a, &b, 1));
    AdvState r1 = a; r1.input = 1; AdvState r2 = r1; r2.brightness = 140;
    TEST_ASSERT_EQUAL_HEX8(0, advPlan(&r1, &r2, 1));
    r2 = r1; r2.syncStripper = !r1.syncStripper;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_SWITCHES, advPlan(&r1, &r2, 1));
    b = a; b.syncStripper = !a.syncStripper;
    TEST_ASSERT_EQUAL_HEX8(0, advPlan(&a, &b, 1));
    b = a; b.formatSv = 4;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_FORMAT, advPlan(&a, &b, 1));
    b = a; b.formatAv = 4;
    TEST_ASSERT_EQUAL_HEX8(0, advPlan(&a, &b, 1));
    b = a; b.input = 6;
    TEST_ASSERT_EQUAL_HEX8(ADV_STEP_INPUT | ADV_STEP_SWITCHES | ADV_ALL, advPlan(&a, &b, 1));
    AdvState g1 = a; g1.input = 2; AdvState g2 = g1; g2.termination = 1;
    TEST_ASSERT_EQUAL_HEX8(0, advPlan(&g1, &g2, 1));
    b = a; b.i2p = 0; TEST_ASSERT_EQUAL_HEX8(ADV_STEP_I2P, advPlan(&a, &b, 1));
    b = a; b.aceGammaGain = 3; TEST_ASSERT_EQUAL_HEX8(ADV_STEP_ACE, advPlan(&a, &b, 1));
    b = a; b.combLumaPal = 5; TEST_ASSERT_EQUAL_HEX8(ADV_STEP_FILTERS, advPlan(&a, &b, 1));
}

void test_format_registers(void) {
    TEST_ASSERT_EQUAL_HEX8(0x04, advFormatRegister(0));
    TEST_ASSERT_EQUAL_HEX8(0x54, advFormatRegister(2));
    TEST_ASSERT_EQUAL_HEX8(0xE4, advFormatRegister(11));
    TEST_ASSERT_EQUAL_HEX8(0x14, advFormatRegister(12));   // issue #46: autodetect with NTSC-M 7.5 IRE setup
    TEST_ASSERT_EQUAL_HEX8(0x04, advFormatRegister(13));
}

void test_state_hash_changes(void) {
    AdvState a = sample(), b = sample();
    TEST_ASSERT_EQUAL_HEX16(advStateHash(&a), advStateHash(&b));
    b.hue = 1;
    TEST_ASSERT_NOT_EQUAL(advStateHash(&a), advStateHash(&b));
}

void test_info_round_trip(void) {
    TEST_ASSERT_EQUAL_UINT32(36, ADV_INFO_BYTES);
    AdvInfo a; memset(&a, 0, sizeof(a));
    a.protocol = ADV_PROTOCOL; a.stateVersion = 1; strcpy(a.fw, "3.0.0-rc1");
    a.buildId = 0xC3A7B74E; a.flags = ADV_INFO_LOCK | ADV_INFO_ADV_POWERED; a.standard = 5;
    a.uptimeS = 0x1234; a.rxFrames = 0x5678; a.badFrames = 0x9ABC; a.appliedCount = 77; a.stateHash = 0xDEF0;
    uint8_t b[ADV_INFO_BYTES + 4];
    memset(b, 0xEE, sizeof(b));
    TEST_ASSERT_EQUAL_UINT32(ADV_INFO_BYTES, advInfoPack(&a, b));
    TEST_ASSERT_EQUAL_HEX8(0x4E, b[14]);                          // buildId little-endian right after fw[12]
    TEST_ASSERT_EQUAL_HEX8(0xC3, b[17]);
    for (int k = 29; k < 36; k++) TEST_ASSERT_EQUAL_HEX8(0, b[k]); // reserved
    TEST_ASSERT_EQUAL_HEX8(0xEE, b[36]);                           // nothing written past the payload
    AdvInfo r;
    TEST_ASSERT_TRUE(advInfoUnpack(b, ADV_INFO_BYTES, &r));
    TEST_ASSERT_EQUAL_UINT8(3, r.protocol);
    TEST_ASSERT_EQUAL_STRING("3.0.0-rc1", r.fw);
    TEST_ASSERT_EQUAL_HEX32(0xC3A7B74E, r.buildId);
    TEST_ASSERT_EQUAL_HEX16(0x1234, r.uptimeS); TEST_ASSERT_EQUAL_HEX16(0x5678, r.rxFrames);
    TEST_ASSERT_EQUAL_HEX16(0x9ABC, r.badFrames); TEST_ASSERT_EQUAL_UINT8(77, r.appliedCount);
    TEST_ASSERT_EQUAL_HEX16(0xDEF0, r.stateHash);
    TEST_ASSERT_FALSE(advInfoUnpack(b, ADV_INFO_BYTES - 1, &r));
}

// v4: INFO byte 29 = caps; an old ADV Controller zeroes it
void test_info_caps_byte(void) {
    AdvInfo a; memset(&a, 0, sizeof(a));
    a.caps = ADV_INFO_CAP_SIGNAL;
    uint8_t b[ADV_INFO_BYTES];
    advInfoPack(&a, b);
    TEST_ASSERT_EQUAL_HEX8(ADV_INFO_CAP_SIGNAL, b[29]);
    AdvInfo c; TEST_ASSERT_TRUE(advInfoUnpack(b, sizeof(b), &c));
    TEST_ASSERT_EQUAL_HEX8(ADV_INFO_CAP_SIGNAL, c.caps);
    b[29] = 0;
    TEST_ASSERT_TRUE(advInfoUnpack(b, sizeof(b), &c));
    TEST_ASSERT_EQUAL_HEX8(0, c.caps);
}

void test_signal_round_trip(void) {
    TEST_ASSERT_EQUAL_HEX8(0x49, ADV_CMD_SIGNAL);
    TEST_ASSERT_EQUAL_UINT32(12, ADV_SIGNAL_BYTES);
    AdvSignal a; memset(&a, 0, sizeof(a));
    a.gen = 7; a.flags = ADV_SIG_PRESENT | ADV_SIG_LOCK | ADV_SIG_POWERED; a.flags2 = ADV_SIG2_ACK;
    a.standard = 4; a.hz = 50; a.status1 = 0x45; a.status2 = 0x00; a.status3 = 0x44; a.vidSel = 0x04;
    a.encoder = 2; a.input = 6;
    uint8_t b[ADV_SIGNAL_BYTES];
    TEST_ASSERT_EQUAL_UINT32(ADV_SIGNAL_BYTES, advSignalPack(&a, b));
    const uint8_t want[ADV_SIGNAL_BYTES] = {7, 0x83, 1, 4, 50, 0x45, 0x00, 0x44, 0x04, 2, 6, 0};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, b, ADV_SIGNAL_BYTES);
    AdvSignal c; TEST_ASSERT_TRUE(advSignalUnpack(b, sizeof(b), &c));
    TEST_ASSERT_EQUAL_MEMORY(&a, &c, sizeof(a));
    TEST_ASSERT_FALSE(advSignalUnpack(b, sizeof(b) - 1, &c));   // wrong length
}

void test_info_version_always_terminated(void) {
    AdvInfo a; memset(&a, 0, sizeof(a));
    memset(a.fw, 'X', sizeof(a.fw));                             // no NUL
    uint8_t b[ADV_INFO_BYTES];
    advInfoPack(&a, b);
    AdvInfo r;
    TEST_ASSERT_TRUE(advInfoUnpack(b, sizeof(b), &r));
    TEST_ASSERT_EQUAL_UINT32(11, strlen(r.fw));
}

void test_crc32(void) {
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, advCrc32((const uint8_t*)"123456789", 9));
    uint32_t c = advCrc32Update(0, (const uint8_t*)"1234", 4);
    c = advCrc32Update(c, (const uint8_t*)"56789", 5);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, c);
    TEST_ASSERT_EQUAL_HEX32(0, advCrc32((const uint8_t*)"", 0));
}

void test_update_payloads(void) {
    uint8_t b[ADV_MAX_PAYLOAD];
    TEST_ASSERT_EQUAL_UINT32(ADV_UPD_BEGIN_BYTES, advUpdBeginPack(29000, 0xA7E063D3, "3.0.0", b));
    uint32_t size, crc; char ver[12];
    TEST_ASSERT_TRUE(advUpdBeginUnpack(b, ADV_UPD_BEGIN_BYTES, &size, &crc, ver));
    TEST_ASSERT_EQUAL_UINT32(29000, size); TEST_ASSERT_EQUAL_HEX32(0xA7E063D3, crc); TEST_ASSERT_EQUAL_STRING("3.0.0", ver);
    TEST_ASSERT_FALSE(advUpdBeginUnpack(b, ADV_UPD_BEGIN_BYTES - 1, &size, &crc, ver));

    uint8_t data[64]; for (int k = 0; k < 64; k++) data[k] = (uint8_t)k;
    TEST_ASSERT_EQUAL_UINT32(68, advUpdDataPack(0x1234, data, 64, b));
    uint32_t off; const uint8_t* dp; uint8_t n;
    TEST_ASSERT_TRUE(advUpdDataUnpack(b, 68, &off, &dp, &n));
    TEST_ASSERT_EQUAL_HEX32(0x1234, off); TEST_ASSERT_EQUAL_UINT8(64, n); TEST_ASSERT_EQUAL_UINT8(63, dp[63]);
    TEST_ASSERT_EQUAL_UINT32(4 + 8, advUpdDataPack(64, data, 8, b));   // short last chunk
    TEST_ASSERT_TRUE(advUpdDataUnpack(b, 12, &off, &dp, &n));
    TEST_ASSERT_EQUAL_UINT8(8, n);
    TEST_ASSERT_FALSE(advUpdDataUnpack(b, 4, &off, &dp, &n));        // no data
    TEST_ASSERT_FALSE(advUpdDataUnpack(b, 4 + 65, &off, &dp, &n));   // too long

    TEST_ASSERT_EQUAL_UINT32(ADV_UPD_REPLY_BYTES, advUpdReplyPack(1, 0x40, b));
    uint8_t st; uint32_t next;
    TEST_ASSERT_TRUE(advUpdReplyUnpack(b, ADV_UPD_REPLY_BYTES, &st, &next));
    TEST_ASSERT_EQUAL_UINT8(1, st); TEST_ASSERT_EQUAL_HEX32(0x40, next);
    TEST_ASSERT_FALSE(advUpdReplyUnpack(b, 4, &st, &next));
}

// ADV_CMD_REG_READ (0x4A): request dev, map, reg, count; reply status + count values
void test_reg_read_pack_roundtrip(void) {
    uint8_t b[ADV_REG_READ_REPLY_MAX];
    TEST_ASSERT_EQUAL_UINT32(ADV_REG_READ_REQ_BYTES, advRegReadReqPack(0x42, 0x40, 0x80, 4, b));
    uint8_t dev, map, reg, count;
    TEST_ASSERT_TRUE(advRegReadReqUnpack(b, ADV_REG_READ_REQ_BYTES, &dev, &map, &reg, &count));
    TEST_ASSERT_EQUAL_HEX8(0x42, dev); TEST_ASSERT_EQUAL_HEX8(0x40, map);
    TEST_ASSERT_EQUAL_HEX8(0x80, reg); TEST_ASSERT_EQUAL_UINT8(4, count);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 3, &dev, &map, &reg, &count));        // short
    uint8_t v[32], back[32], st, n;
    for (int i = 0; i < 32; i++) v[i] = (uint8_t)(0xA0 + i);
    TEST_ASSERT_EQUAL_UINT32(33, advRegReadReplyPack(ADV_REG_READ_OK, v, 32, b));
    TEST_ASSERT_TRUE(advRegReadReplyUnpack(b, 33, &st, back, &n));
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_READ_OK, st); TEST_ASSERT_EQUAL_UINT8(32, n);
    TEST_ASSERT_EQUAL_MEMORY(v, back, 32);
    TEST_ASSERT_EQUAL_UINT32(1, advRegReadReplyPack(ADV_REG_READ_I2C_ERROR, v, 0, b));
    TEST_ASSERT_TRUE(advRegReadReplyUnpack(b, 1, &st, back, &n));
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_READ_I2C_ERROR, st); TEST_ASSERT_EQUAL_UINT8(0, n);
    TEST_ASSERT_FALSE(advRegReadReplyUnpack(b, 0, &st, back, &n));
    TEST_ASSERT_EQUAL_HEX8(0x4A, ADV_CMD_REG_READ);
    TEST_ASSERT_EQUAL_UINT8(2, ADV_INFO_CAP_REG_READ);
}

void test_reg_read_rejects_count_0_and_33(void) {
    uint8_t b[4], dev, map, reg, count;
    advRegReadReqPack(0x56, 0, 0x00, 0, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));
    advRegReadReqPack(0x56, 0, 0x00, 33, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));
    advRegReadReqPack(0x56, 0, 0xF0, 32, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));       // past register 0xFF
    advRegReadReqPack(0x56, 0, 0xE0, 32, b);
    TEST_ASSERT_TRUE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));
    advRegReadReqPack(0x50, 0, 0x00, 1, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));       // not an ADV chip address
}

void test_reg_read_map_only_for_decoder(void) {
    uint8_t b[4], dev, map, reg, count;
    advRegReadReqPack(0x42, 0x20, 0x00, 1, b);
    TEST_ASSERT_TRUE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));        // interrupt/VDP map
    advRegReadReqPack(0x42, 0x40, 0x00, 1, b);
    TEST_ASSERT_TRUE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));        // user sub map 2
    advRegReadReqPack(0x42, 0x10, 0x00, 1, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));       // not a map of 0x0E
    advRegReadReqPack(0x56, 0x40, 0x00, 1, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));       // the encoder has no maps
    advRegReadReqPack(0x84, 0x20, 0x00, 1, b);
    TEST_ASSERT_FALSE(advRegReadReqUnpack(b, 4, &dev, &map, &reg, &count));
}

// Listen: on RGB SCART the decoder powers up on CVBS and the pin-20 termination goes on while the TV5725 stays on
// RGB SCART; nothing changes off RGB SCART
static AdvState onInput(uint8_t input) { AdvState s = sample(); s.input = input; return s; }

void test_listen_switches_rgbs(void) {
    AdvState s = onInput(ADV_INPUT_RGBS); s.listen = 1;
    AdvSwitches w = advSwitchesFor(&s), plain;
    AdvState p = onInput(ADV_INPUT_RGBS);
    plain = advSwitchesFor(&p);
    TEST_ASSERT_EQUAL_UINT8(plain.asw1, w.asw1); TEST_ASSERT_EQUAL_UINT8(plain.asw2, w.asw2);
    TEST_ASSERT_EQUAL_UINT8(0, w.asw3); TEST_ASSERT_EQUAL_UINT8(1, w.asw4);
    TEST_ASSERT_EQUAL_UINT8(1, w.avConnect); TEST_ASSERT_EQUAL_UINT8(1, w.adv);
    TEST_ASSERT_TRUE(advDecoderOn(&s));
}

void test_listen_ignored_off_rgbs(void) {
    AdvState s = onInput(ADV_INPUT_YPBPR); s.listen = 1;
    TEST_ASSERT_EQUAL_UINT8(0, advSwitchesFor(&s).adv);
    TEST_ASSERT_EQUAL_UINT8(0, advSwitchesFor(&s).avConnect);
    TEST_ASSERT_FALSE(advDecoderOn(&s));
    AdvState g = onInput(ADV_INPUT_RGSB); g.listen = 1;     // never on RGB SoG: no false sync on green
    TEST_ASSERT_EQUAL_UINT8(0, advSwitchesFor(&g).avConnect);
    TEST_ASSERT_FALSE(advDecoderOn(&g));
}

void test_listen_on_plans_decoder_init(void) {
    AdvState a = onInput(ADV_INPUT_RGBS), b = a; b.listen = 1;
    uint8_t steps = advPlan(&a, &b, 1);
    TEST_ASSERT_TRUE(steps & ADV_STEP_INPUT); TEST_ASSERT_TRUE(steps & ADV_STEP_SWITCHES); TEST_ASSERT_TRUE(steps & ADV_STEP_FORMAT);
    steps = advPlan(&b, &a, 1);                               // listen off: the decoder powers down
    TEST_ASSERT_TRUE(steps & ADV_STEP_INPUT); TEST_ASSERT_TRUE(steps & ADV_STEP_SWITCHES);
}

void test_listen_to_av_keeps_decoder(void) {
    AdvState a = onInput(ADV_INPUT_RGBS); a.listen = 1;
    AdvState b = onInput(ADV_INPUT_AV);
    uint8_t steps = advPlan(&a, &b, 1);
    TEST_ASSERT_FALSE(steps & ADV_STEP_INPUT);
    TEST_ASSERT_TRUE(steps & ADV_STEP_SWITCHES);
    TEST_ASSERT_FALSE(steps & ADV_STEP_FORMAT);               // same formatAv, same decoder groups
}

void test_av_to_rgbs_listen_keeps_decoder(void) {
    AdvState a = onInput(ADV_INPUT_AV), b = onInput(ADV_INPUT_RGBS); b.listen = 1;
    uint8_t steps = advPlan(&a, &b, 1);
    TEST_ASSERT_FALSE(steps & ADV_STEP_INPUT);
    TEST_ASSERT_TRUE(steps & ADV_STEP_SWITCHES);
}

void test_decoder_off_input_change_unchanged(void) {
    AdvState a = onInput(ADV_INPUT_RGBS), b = onInput(ADV_INPUT_YPBPR);
    TEST_ASSERT_EQUAL_UINT8(ADV_STEP_INPUT | ADV_STEP_SWITCHES, advPlan(&a, &b, 1));
}

void test_decoder_type_change_reinits(void) {
    AdvState sv = onInput(ADV_INPUT_SV), av = onInput(ADV_INPUT_AV), l = onInput(ADV_INPUT_RGBS); l.listen = 1;
    TEST_ASSERT_TRUE(advPlan(&sv, &l, 1) & ADV_STEP_INPUT);
    TEST_ASSERT_TRUE(advPlan(&l, &sv, 1) & ADV_STEP_INPUT);
    TEST_ASSERT_TRUE(advPlan(&sv, &av, 1) & ADV_STEP_INPUT);
    TEST_ASSERT_TRUE(advPlan(&sv, &av, 1) & ADV_STEP_FORMAT);
}

void test_listen_round_trips_and_clamps(void) {
    AdvState s = onInput(ADV_INPUT_RGBS), r;
    uint8_t b[ADV_STATE_BYTES];
    s.listen = 1;
    advStatePack(&s, b);
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));
    TEST_ASSERT_EQUAL_UINT8(1, r.listen);
    b[32] = 7; b[ADV_STATE_BYTES - 1] = 0;                    // recompute the crc after the edit
    s.listen = 7; advStatePack(&s, b);
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));
    TEST_ASSERT_EQUAL_UINT8(1, r.listen);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc16_vector);
    RUN_TEST(test_round_trip_byte_by_byte);
    RUN_TEST(test_noise_split_and_back_to_back);
    RUN_TEST(test_false_sync_inside_payload);
    RUN_TEST(test_bad_crc_then_valid);
    RUN_TEST(test_oversized_length_rejected);
    RUN_TEST(test_bogus_header_swallowing_a_frame);
    RUN_TEST(test_frame_inside_failed_bytes_decoded_once);
    RUN_TEST(test_state_pack_round_trip);
    RUN_TEST(test_state_structural_errors_rejected);
    RUN_TEST(test_state_fields_clamped_to_register_widths);
    RUN_TEST(test_state_dnr_cti_bytes_and_plan);
    RUN_TEST(test_truth_table);
    RUN_TEST(test_plan);
    RUN_TEST(test_format_registers);
    RUN_TEST(test_state_hash_changes);
    RUN_TEST(test_info_round_trip);
    RUN_TEST(test_info_version_always_terminated);
    RUN_TEST(test_info_caps_byte);
    RUN_TEST(test_signal_round_trip);
    RUN_TEST(test_crc32);
    RUN_TEST(test_update_payloads);
    RUN_TEST(test_reg_read_pack_roundtrip);
    RUN_TEST(test_reg_read_rejects_count_0_and_33);
    RUN_TEST(test_reg_read_map_only_for_decoder);
    RUN_TEST(test_listen_switches_rgbs);
    RUN_TEST(test_listen_ignored_off_rgbs);
    RUN_TEST(test_listen_on_plans_decoder_init);
    RUN_TEST(test_listen_to_av_keeps_decoder);
    RUN_TEST(test_av_to_rgbs_listen_keeps_decoder);
    RUN_TEST(test_decoder_off_input_change_unchanged);
    RUN_TEST(test_decoder_type_change_reinits);
    RUN_TEST(test_listen_round_trips_and_clamps);
    return UNITY_END();
}
