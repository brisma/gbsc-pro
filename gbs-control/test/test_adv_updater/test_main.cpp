#include <unity.h>
#include <string.h>
#include "adv-link.h"
#include "adv-updater.h"
#include "adv-link-client.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t gImg[1000];
static void readImg(uint32_t off, uint8_t* out, uint8_t n, void*) { memcpy(out, gImg + off, n); }

struct Step { bool have; uint8_t cmd, len; uint8_t p[ADV_MAX_PAYLOAD]; };
static Step nextReq(AdvUpdater& u, uint32_t now) { Step s = {}; s.have = u.next(now, s.cmd, s.p, s.len, readImg, nullptr); return s; }
static void rep(AdvUpdater& u, uint8_t cmd, uint8_t status, uint32_t next, uint32_t now) {
    uint8_t b[ADV_UPD_REPLY_BYTES]; advUpdReplyPack(status, next, b);
    u.result(true, (uint8_t)(cmd | ADV_RESP), b, ADV_UPD_REPLY_BYTES, now);
}
static void noRep(AdvUpdater& u, uint32_t now) { u.result(false, 0, nullptr, 0, now); }
static void infoRep(AdvUpdater& u, uint32_t build, uint32_t now) {
    AdvInfo i; memset(&i, 0, sizeof(i)); i.protocol = ADV_PROTOCOL; i.stateVersion = ADV_STATE_VERSION; i.buildId = build;
    uint8_t b[ADV_INFO_BYTES]; advInfoPack(&i, b);
    u.result(true, ADV_CMD_INFO | ADV_RESP, b, ADV_INFO_BYTES, now);
}
static void pingRep(AdvUpdater& u, uint32_t now) { uint8_t v = ADV_PROTOCOL; u.result(true, ADV_CMD_PING | ADV_RESP, &v, 1, now); }

static uint32_t t;
// BEGIN -> STATUS READY; returns with the updater in the SEND state
static void toSend(AdvUpdater& u, uint32_t size) {
    u.start(size, 0xABCD1234, "3.1.0", t);
    Step s = nextReq(u, t);
    TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_BEGIN, s.cmd);
    uint32_t sz, crc; char v[12];
    TEST_ASSERT_TRUE(advUpdBeginUnpack(s.p, s.len, &sz, &crc, v));
    TEST_ASSERT_EQUAL_UINT32(size, sz); TEST_ASSERT_EQUAL_HEX32(0xABCD1234, crc);
    rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_OK, 0, t);
    t += 50; s = nextReq(u, t); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_STATUS, s.cmd);
    rep(u, ADV_CMD_UPD_STATUS, ADV_UST_ERASING, 0, t);
    t += 50; s = nextReq(u, t); noRep(u, t);                      // a missing STATUS reply while erasing is normal
    t += 50; s = nextReq(u, t); rep(u, ADV_CMD_UPD_STATUS, ADV_UST_READY, 0, t);
    TEST_ASSERT_EQUAL_UINT8(AU_SEND, u.state());
}
static void sendAll(AdvUpdater& u, uint32_t size) {
    while (u.state() == AU_SEND) {
        Step s = nextReq(u, ++t);
        TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_DATA, s.cmd);
        uint32_t off; const uint8_t* d; uint8_t n;
        TEST_ASSERT_TRUE(advUpdDataUnpack(s.p, s.len, &off, &d, &n));
        TEST_ASSERT_EQUAL_MEMORY(gImg + off, d, n);
        rep(u, ADV_CMD_UPD_DATA, ADV_UPD_OK, off + n, t);
    }
    (void)size;
}

void test_happy_path(void) {
    AdvUpdater u; t = 1000;
    for (int i = 0; i < 1000; i++) gImg[i] = (uint8_t)i;
    toSend(u, 1000);
    sendAll(u, 1000);
    TEST_ASSERT_EQUAL_UINT8(AU_COMMIT, u.state());
    TEST_ASSERT_EQUAL_UINT8(100, u.percent());
    Step s = nextReq(u, ++t); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_COMMIT, s.cmd);
    rep(u, ADV_CMD_UPD_COMMIT, ADV_COMMIT_CRC_OK, 1000, t);
    TEST_ASSERT_EQUAL_UINT8(AU_REBOOT_WAIT, u.state());
    TEST_ASSERT_FALSE(nextReq(u, t + 500).have);                 // 1 s for the copy and the reset
    s = nextReq(u, t + 1000); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_PING, s.cmd); noRep(u, t + 1000);
    s = nextReq(u, t + 1200); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_PING, s.cmd); pingRep(u, t + 1200);
    s = nextReq(u, t + 1300); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_INFO, s.cmd); infoRep(u, 0xABCD1234, t + 1300);
    TEST_ASSERT_EQUAL_UINT8(AU_DONE, u.state());
    TEST_ASSERT_FALSE(u.active());
}

void test_resume_after_lost_reply(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 200);
    Step s = nextReq(u, ++t); noRep(u, t);                       // chunk 0 programmed, reply lost
    s = nextReq(u, ++t); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_DATA, s.cmd);
    rep(u, ADV_CMD_UPD_DATA, ADV_UPD_RESUME, 64, t);             // the ADV Controller already has it
    s = nextReq(u, ++t);
    uint32_t off; const uint8_t* d; uint8_t n; advUpdDataUnpack(s.p, s.len, &off, &d, &n);
    TEST_ASSERT_EQUAL_UINT32(64, off);
    TEST_ASSERT_EQUAL_UINT8(32, (uint8_t)((uint32_t)u.percent()));
}

void test_chunk_fails_three_times(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 200);
    for (int k = 0; k < 3; k++) { nextReq(u, ++t); noRep(u, t); }
    TEST_ASSERT_EQUAL_UINT8(AU_SEND, u.state());                  // retries cover at least 1 s
    t += 1000; nextReq(u, t); noRep(u, t);
    TEST_ASSERT_EQUAL_UINT8(AU_FAILED, u.state());
    TEST_ASSERT_EQUAL_STRING("transfer", u.error());
}

void test_too_big(void) {
    AdvUpdater u; t = 1000;
    u.start(999999, 1, "x", t);
    nextReq(u, t); rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_TOO_BIG, 0, t);
    TEST_ASSERT_EQUAL_UINT8(AU_FAILED, u.state());
    TEST_ASSERT_EQUAL_STRING("begin", u.error());
}

void test_no_session_restarts_once(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 200);
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_DATA, ADV_UPD_NO_SESSION, 0, t);
    Step s = nextReq(u, ++t); TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_BEGIN, s.cmd);   // restart
    rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_OK, 0, t);
    t += 50; nextReq(u, t); rep(u, ADV_CMD_UPD_STATUS, ADV_UST_READY, 0, t);
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_DATA, ADV_UPD_NO_SESSION, 0, t);
    TEST_ASSERT_EQUAL_UINT8(AU_FAILED, u.state());
}

void test_erase_timeout(void) {
    AdvUpdater u; t = 1000;
    u.start(200, 1, "x", t);
    nextReq(u, t); rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_OK, 0, t);
    for (int k = 0; k < 200 && u.state() == AU_WAIT_READY; k++) { t += 50; if (nextReq(u, t).have) noRep(u, t); }
    TEST_ASSERT_EQUAL_UINT8(AU_FAILED, u.state());
    TEST_ASSERT_EQUAL_STRING("erase", u.error());
}

void test_crc_bad(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 64); sendAll(u, 64);
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_COMMIT, ADV_COMMIT_CRC_BAD, 64, t);
    TEST_ASSERT_EQUAL_STRING("crc", u.error());
}

void test_commit_not_complete_resumes(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 128); sendAll(u, 128);
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_COMMIT, ADV_COMMIT_NOT_COMPLETE, 64, t);
    TEST_ASSERT_EQUAL_UINT8(AU_SEND, u.state());
    Step s = nextReq(u, ++t);
    uint32_t off; const uint8_t* d; uint8_t n; advUpdDataUnpack(s.p, s.len, &off, &d, &n);
    TEST_ASSERT_EQUAL_UINT32(64, off);
}

void test_commit_reply_lost_then_build_ok(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 64); sendAll(u, 64);
    for (int k = 0; k < 3; k++) { nextReq(u, ++t); noRep(u, t); }
    t += 1000; nextReq(u, t); noRep(u, t);
    TEST_ASSERT_EQUAL_UINT8(AU_REBOOT_WAIT, u.state());          // the build id decides
    t += 1000; nextReq(u, t); pingRep(u, t);
    t += 100; nextReq(u, t); infoRep(u, 0xABCD1234, t);
    TEST_ASSERT_EQUAL_UINT8(AU_DONE, u.state());
}

void test_reboot_no_reply_and_wrong_build(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 64); sendAll(u, 64);
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_COMMIT, ADV_COMMIT_CRC_OK, 64, t);
    for (int k = 0; k < 100 && u.state() == AU_REBOOT_WAIT; k++) { t += 200; if (nextReq(u, t).have) noRep(u, t); }
    TEST_ASSERT_EQUAL_STRING("reboot", u.error());
    AdvUpdater w; t = 1000;
    toSend(w, 64); sendAll(w, 64);
    nextReq(w, ++t); rep(w, ADV_CMD_UPD_COMMIT, ADV_COMMIT_CRC_OK, 64, t);
    t += 1000; nextReq(w, t); pingRep(w, t);
    t += 100; nextReq(w, t); infoRep(w, 0x1111, t);
    TEST_ASSERT_EQUAL_STRING("build", w.error());
}

void test_client_restart(void) {
    AdvLinkClient c;
    TEST_ASSERT_EQUAL_HEX8(ADV_CMD_PING | ADV_WANT_REPLY, c.due(1000, true, true));
    uint8_t v = ADV_PROTOCOL; c.result(ADV_CMD_PING | ADV_WANT_REPLY, true, ADV_CMD_PING | ADV_RESP, &v, 1, 1000);
    c.restart();
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_CHECKING, c.advController().status);
    TEST_ASSERT_FALSE(c.advController().haveInfo);
    TEST_ASSERT_EQUAL_HEX8(ADV_CMD_PING | ADV_WANT_REPLY, c.due(5000, true, true));   // boot sequence again
}

// A loop pass blocked for seconds (e.g. a TLS check) must not turn a working update into a failure
void test_blocked_pass_in_reboot_wait(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 64); sendAll(u, 64);
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_COMMIT, ADV_COMMIT_CRC_OK, 64, t);
    t += 8000;
    Step s = nextReq(u, t);
    TEST_ASSERT_TRUE(s.have);
    TEST_ASSERT_EQUAL_HEX8(ADV_CMD_PING | ADV_WANT_REPLY, s.cmd);
    pingRep(u, t);
    t += 10; nextReq(u, t); infoRep(u, 0xABCD1234, t);
    TEST_ASSERT_EQUAL_UINT8(AU_DONE, u.state());
}

void test_blocked_pass_in_wait_ready(void) {
    AdvUpdater u; t = 1000;
    u.start(200, 1, "x", t);
    nextReq(u, t); rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_OK, 0, t);
    t += 6000;
    Step s = nextReq(u, t);
    TEST_ASSERT_TRUE(s.have);
    TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_STATUS, s.cmd);
    rep(u, ADV_CMD_UPD_STATUS, ADV_UST_READY, 0, t);
    TEST_ASSERT_EQUAL_UINT8(AU_SEND, u.state());
}

// An ADV Controller stall of a few hundred ms (ADV_Init delay) is covered by the retry window
void test_begin_retries_cover_a_stall(void) {
    AdvUpdater u; t = 1000;
    u.start(200, 1, "x", t);
    for (int k = 0; k < 6; k++) { TEST_ASSERT_EQUAL_HEX8(ADV_CMD_UPD_BEGIN, nextReq(u, t).cmd); noRep(u, t); t += 50; }
    TEST_ASSERT_EQUAL_UINT8(AU_BEGIN, u.state());
    nextReq(u, t); rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_OK, 0, t);
    TEST_ASSERT_EQUAL_UINT8(AU_WAIT_READY, u.state());
}

void test_resume_without_progress_ends(void) {
    AdvUpdater u; t = 1000;
    toSend(u, 200);
    for (int k = 0; k < 200 && u.state() == AU_SEND; k++) { nextReq(u, ++t); rep(u, ADV_CMD_UPD_DATA, ADV_UPD_RESUME, 0, t); }
    TEST_ASSERT_EQUAL_UINT8(AU_BEGIN, u.state());                 // restart once
    nextReq(u, ++t); rep(u, ADV_CMD_UPD_BEGIN, ADV_UPD_OK, 0, t);
    t += 50; nextReq(u, t); rep(u, ADV_CMD_UPD_STATUS, ADV_UST_READY, 0, t);
    for (int k = 0; k < 200 && u.state() == AU_SEND; k++) { nextReq(u, ++t); rep(u, ADV_CMD_UPD_DATA, ADV_UPD_RESUME, 0, t); }
    TEST_ASSERT_EQUAL_UINT8(AU_FAILED, u.state());
    TEST_ASSERT_EQUAL_STRING("transfer", u.error());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path);
    RUN_TEST(test_resume_after_lost_reply);
    RUN_TEST(test_chunk_fails_three_times);
    RUN_TEST(test_too_big);
    RUN_TEST(test_no_session_restarts_once);
    RUN_TEST(test_erase_timeout);
    RUN_TEST(test_crc_bad);
    RUN_TEST(test_commit_not_complete_resumes);
    RUN_TEST(test_commit_reply_lost_then_build_ok);
    RUN_TEST(test_reboot_no_reply_and_wrong_build);
    RUN_TEST(test_client_restart);
    RUN_TEST(test_blocked_pass_in_reboot_wait);
    RUN_TEST(test_blocked_pass_in_wait_ready);
    RUN_TEST(test_begin_retries_cover_a_stall);
    RUN_TEST(test_resume_without_progress_ends);
    return UNITY_END();
}
