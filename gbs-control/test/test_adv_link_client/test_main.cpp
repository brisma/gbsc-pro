#include <unity.h>
#include <string.h>
#include "adv-link.h"
#include "adv-link-client.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t PING = ADV_CMD_PING | ADV_WANT_REPLY, INFO = ADV_CMD_INFO | ADV_WANT_REPLY;

static uint8_t gCaps = 0;   // INFO caps the fake ADV Controller reports
static const uint8_t SIGNAL = ADV_CMD_SIGNAL;

static void reply(AdvLinkClient& c, uint8_t req, bool ok, uint32_t now, uint16_t hash = 0, uint8_t stateVersion = ADV_STATE_VERSION) {
    if (!ok) { c.result(req, false, 0, nullptr, 0, now); return; }
    if ((req & 0x3F) == (ADV_CMD_PING & 0x3F)) { uint8_t p = ADV_PROTOCOL; c.result(req, true, ADV_CMD_PING | ADV_RESP, &p, 1, now); return; }
    AdvInfo i; memset(&i, 0, sizeof(i));
    i.protocol = ADV_PROTOCOL; i.stateVersion = stateVersion; strcpy(i.fw, "3.0.0"); i.stateHash = hash; i.caps = gCaps;
    uint8_t b[ADV_INFO_BYTES]; advInfoPack(&i, b);
    c.result(req, true, ADV_CMD_INFO | ADV_RESP, b, ADV_INFO_BYTES, now);
}

// Runs the boot sequence to OK; returns the time after it
static uint32_t bootOk(AdvLinkClient& c, uint32_t t = 1000) {
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(t, true, true)); reply(c, PING, true, t);
    t += 60;
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t, true, true)); reply(c, INFO, true, t);
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_OK, c.advController().status);
    return t;
}

void test_accept_reply_uses_real_constants(void) {
    TEST_ASSERT_TRUE(AdvLinkClient::acceptReply(PING, 9, 0xC7, 9));
    TEST_ASSERT_TRUE(AdvLinkClient::acceptReply(INFO, 9, 0xC3, 9));
    TEST_ASSERT_FALSE(AdvLinkClient::acceptReply(PING, 9, 0xC7, 8));     // other seq
    TEST_ASSERT_FALSE(AdvLinkClient::acceptReply(PING, 9, 0xC3, 9));     // other command
    TEST_ASSERT_FALSE(AdvLinkClient::acceptReply(PING, 9, ADV_CMD_NAK, 9));
    TEST_ASSERT_FALSE(AdvLinkClient::acceptReply(PING, 9, 0x47, 9));     // not a response
}

void test_boot_pings_then_no_reply(void) {
    AdvLinkClient c;
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_CHECKING, c.advController().status);
    uint32_t t = 5000;                       // the window starts at the first allowed due()
    int pings = 0;
    for (; t < 5000 + 2600; t += 10) {
        uint8_t r = c.due(t, true, true);
        if (r) { TEST_ASSERT_EQUAL_HEX8(PING, r); pings++; reply(c, r, false, t); }
        if (c.advController().status == ADV_CONTROLLER_NO_REPLY) break;
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_NO_REPLY, c.advController().status);
    TEST_ASSERT_TRUE(pings >= 8);
    TEST_ASSERT_TRUE(t - 5000 >= 2000);
    TEST_ASSERT_TRUE(c.noReplyNotice());
    TEST_ASSERT_FALSE(c.noReplyNotice());
}

void test_boot_window_waits_for_allowed_due(void) {
    AdvLinkClient c;
    for (uint32_t t = 0; t < 5000; t += 10) TEST_ASSERT_EQUAL_HEX8(0, c.due(t, false, true));   // quiet window
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_CHECKING, c.advController().status);
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(5000, true, true));
}

void test_ping_info_ok(void) {
    AdvLinkClient c;
    bootOk(c);
    TEST_ASSERT_EQUAL_STRING("3.0.0", c.advController().info.fw);
    TEST_ASSERT_TRUE(c.advController().haveInfo);
}

void test_silence_after_any_frame(void) {
    AdvLinkClient c;
    uint32_t t = bootOk(c);
    t += 20000;
    c.frameSent(t);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 10, true, true));
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t + 50, true, true));   // the periodic INFO, after 50 ms of silence
}

void test_info_after_state_and_hash_check(void) {
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 100;
    c.stateSent(0x1111, t);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 20, true, true));
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 60, false, true));     // quiet window still closed
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t + 400, true, true));
    reply(c, INFO, true, t + 400, 0x2222);
    TEST_ASSERT_TRUE(c.wantStateResend());
    TEST_ASSERT_FALSE(c.wantStateResend());
}

void test_resend_capped_then_not_applied(void) {
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 100;
    for (int k = 0; k < 4; k++) {
        c.stateSent(0x1111, t);
        t += 60;
        TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t, true, true));
        reply(c, INFO, true, t, 0x2222);
        bool w = c.wantStateResend();
        if (k < 3) TEST_ASSERT_TRUE(w); else TEST_ASSERT_FALSE(w);
        t += 60;
    }
    TEST_ASSERT_TRUE(c.advController().notApplied);
    c.stateSent(0x3333, t);                                     // a new state: counting restarts
    TEST_ASSERT_FALSE(c.advController().notApplied);
}

void test_failed_info_retries_and_cycles_to_no_reply(void) {
    AdvLinkClient c;
    uint32_t t = bootOk(c);
    for (int cycle = 0; cycle < 3; cycle++) {
        t += 10001;
        for (int tr = 0; tr < 3; tr++) {
            TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t, true, true));
            reply(c, INFO, false, t);
            TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 50, true, true));   // tries spaced >= 100 ms
            t += 100;
        }
        if (cycle < 2) TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_OK, c.advController().status);
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_NO_REPLY, c.advController().status);
    TEST_ASSERT_FALSE(c.noReplyNotice());                       // the notice is for the boot check only
    // slow ping, then recovery through PING -> INFO
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 1000, true, true));
    t += 15000;
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(t, true, true)); reply(c, PING, true, t);
    t += 60;
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t, true, true)); reply(c, INFO, true, t);
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_OK, c.advController().status);
}

void test_nak_counts_as_failure(void) {
    AdvLinkClient c;
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(1000, true, true));
    uint8_t p = PING;
    c.result(PING, true, ADV_CMD_NAK, &p, 1, 1000);
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_CHECKING, c.advController().status);
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(1200, true, true));      // boot cadence continues
}

void test_incompatible_state_version(void) {
    AdvLinkClient c;
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(1000, true, true)); reply(c, PING, true, 1000);
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(1060, true, true)); reply(c, INFO, true, 1060, 0x1234, 2);
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_INCOMPATIBLE, c.advController().status);
    TEST_ASSERT_TRUE(c.stateBlocked());
    c.stateSent(0x9999, 2000);
    TEST_ASSERT_FALSE(c.wantStateResend());
}

void test_periodic_info(void) {
    AdvLinkClient c;
    uint32_t t = bootOk(c);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 9999, true, true));
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t + 10000, true, true));
}

void test_link_busy_blocks_requests(void) {
    AdvLinkClient c;
    TEST_ASSERT_EQUAL_HEX8(0, c.due(1000, true, false));
}

void test_ping_ok_but_info_always_failing_ends_in_no_reply(void) {
    AdvLinkClient c;
    uint32_t t = 1000;
    for (int k = 0; k < 400 && c.advController().status == ADV_CONTROLLER_CHECKING; k++, t += 10) {
        uint8_t r = c.due(t, true, true);
        if (r == PING) reply(c, PING, true, t);
        else if (r == INFO) reply(c, INFO, false, t);
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_NO_REPLY, c.advController().status);
    TEST_ASSERT_TRUE(c.noReplyNotice());
}

void test_no_reply_notice_only_once(void) {
    AdvLinkClient c;
    uint32_t t = 1000;
    for (; c.advController().status != ADV_CONTROLLER_NO_REPLY; t += 10) { uint8_t r = c.due(t, true, true); if (r) reply(c, r, false, t); }
    TEST_ASSERT_TRUE(c.noReplyNotice());
    // slow ping answered, INFO keeps failing -> NO_REPLY again: no second notice
    for (int k = 0; k < 6000 && !(c.advController().status == ADV_CONTROLLER_CHECKING); k++, t += 10) {
        uint8_t r = c.due(t, true, true); if (r == PING) reply(c, PING, true, t); else if (r) reply(c, r, false, t);
    }
    for (int k = 0; k < 400 && c.advController().status == ADV_CONTROLLER_CHECKING; k++, t += 10) {
        uint8_t r = c.due(t, true, true); if (r == PING) reply(c, PING, true, t); else if (r) reply(c, r, false, t);
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_NO_REPLY, c.advController().status);
    TEST_ASSERT_FALSE(c.noReplyNotice());
}

void test_keep_alive_state_does_not_trigger_info(void) {
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 100;
    c.stateSent(0x1111, t);                                     // new hash: INFO to check it
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t + 60, true, true)); reply(c, INFO, true, t + 60, 0x1111);
    c.stateSent(0x1111, t + 5000);                              // keep-alive, same hash
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 5100, true, true));     // no INFO until the 10 s period
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t + 60 + 10000, true, true));
}

// An ADV Controller answering PING with another protocol number is an old/new firmware, not a silent one
void test_ping_other_protocol_is_incompatible(void) {
    AdvLinkClient c;
    TEST_ASSERT_EQUAL_HEX8(PING, c.due(1000, true, true));
    uint8_t p = (uint8_t)(ADV_PROTOCOL + 1);
    c.result(PING, true, ADV_CMD_PING | ADV_RESP, &p, 1, 1000);
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_INCOMPATIBLE, c.advController().status);
    TEST_ASSERT_TRUE(c.stateBlocked());
}


// v4 signal report
static void signalReply(AdvLinkClient& c, uint32_t now, uint8_t flags, uint8_t standard) {
    AdvSignal g; memset(&g, 0, sizeof(g));
    g.gen = 1; g.flags = flags; g.standard = standard; g.hz = 50;
    uint8_t b[ADV_SIGNAL_BYTES]; advSignalPack(&g, b);
    c.result(SIGNAL, true, ADV_CMD_SIGNAL | ADV_RESP, b, ADV_SIGNAL_BYTES, now);
}

void test_no_signal_requests_without_caps(void) {
    gCaps = 0;
    AdvLinkClient c;
    uint32_t t = bootOk(c);
    for (uint32_t e = t + 60; e < t + 9000; e += 10) TEST_ASSERT_EQUAL_HEX8(0, c.due(e, true, true));
    TEST_ASSERT_NULL(c.signal());
}

void test_signal_polled_every_500_ms_while_powered(void) {
    gCaps = ADV_INFO_CAP_SIGNAL;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t, true, true));        // first one at once
    signalReply(c, t, ADV_SIG_POWERED | ADV_SIG_LOCK | ADV_SIG_PRESENT, 4);
    TEST_ASSERT_NOT_NULL(c.signal());
    TEST_ASSERT_EQUAL_UINT8(4, c.signal()->standard);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 400, true, true));
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t + 500, true, true));
    gCaps = 0;
}

void test_signal_slow_when_decoder_off(void) {
    gCaps = ADV_INFO_CAP_SIGNAL;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t, true, true));
    signalReply(c, t, 0, ADV_SIG_NO_STANDARD);                    // RGB input: decoder powered down
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 1900, true, true));
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t + 2000, true, true));
    gCaps = 0;
}

// Auto input scan: while probing S-Video/Composite the report is polled every 500 ms even when the
// last one said the decoder was off, so a lock is seen within the probe window
void test_signal_fast_while_scanning(void) {
    gCaps = ADV_INFO_CAP_SIGNAL;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t, true, true));
    signalReply(c, t, 0, ADV_SIG_NO_STANDARD);
    c.setSignalFast(true);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 400, true, true));
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t + 500, true, true));
    signalReply(c, t + 500, 0, ADV_SIG_NO_STANDARD);
    c.setSignalFast(false);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 1500, true, true));
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t + 2500, true, true));
    gCaps = 0;
}

void test_signal_failure_is_one_try_and_not_a_no_reply(void) {
    gCaps = ADV_INFO_CAP_SIGNAL;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    for (int k = 0; k < 4; k++) {                                 // 4 lost SIGNAL replies (> kFailedCycles), within the INFO period
        TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t, true, true));
        c.result(SIGNAL, false, 0, nullptr, 0, t);
        TEST_ASSERT_EQUAL_HEX8(0, c.due(t + 100, true, true));    // no retry
        t += 2000;
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_OK, c.advController().status);
    gCaps = 0;
}

void test_info_after_state_comes_before_signal(void) {
    gCaps = ADV_INFO_CAP_SIGNAL;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.stateSent(0x1234, t);
    TEST_ASSERT_EQUAL_HEX8(INFO, c.due(t + 50, true, true));
    gCaps = 0;
}

void test_restart_forgets_the_signal(void) {   // after an ADV Controller update: no report from the old firmware
    gCaps = ADV_INFO_CAP_SIGNAL;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    TEST_ASSERT_EQUAL_HEX8(SIGNAL, c.due(t, true, true));
    signalReply(c, t, ADV_SIG_POWERED | ADV_SIG_LOCK, 4);
    TEST_ASSERT_NOT_NULL(c.signal());
    c.restart();
    TEST_ASSERT_NULL(c.signal());
    gCaps = 0;
}

// ---- Register read (ADV_CMD_REG_READ): one request at a time, own result, never NO_REPLY ----
static const uint8_t REGREAD = ADV_CMD_REG_READ;   // 0x4A has the reply bit

static void regReply(AdvLinkClient& c, uint32_t now, uint8_t status, uint8_t n) {
    uint8_t v[32], b[33];
    for (uint8_t i = 0; i < n; i++) v[i] = (uint8_t)(0x10 + i);
    size_t len = advRegReadReplyPack(status, v, n, b);
    c.result(REGREAD, true, REGREAD | ADV_RESP, b, (uint8_t)len, now);
}

void test_reg_read_waits_for_quiet_window(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x42, 0x40, 0x80, 4, 7);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_PENDING, c.regRead().state);
    TEST_ASSERT_EQUAL_UINT8(7, c.regRead().gen);
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t, false, true));            // not in the quiet window
    TEST_ASSERT_EQUAL_HEX8(0, c.due(t, true, false));            // an update owns the link
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));
    uint8_t p[8], n = 0;
    TEST_ASSERT_TRUE(c.requestPayload(p, &n));
    TEST_ASSERT_EQUAL_UINT8(4, n);
    TEST_ASSERT_EQUAL_HEX8(0x42, p[0]); TEST_ASSERT_EQUAL_HEX8(0x40, p[1]);
    TEST_ASSERT_EQUAL_HEX8(0x80, p[2]); TEST_ASSERT_EQUAL_UINT8(4, p[3]);
    gCaps = 0;
}

void test_reg_read_reply_fills_values(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x56, 0, 0x17, 3, 1);
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));
    regReply(c, t, ADV_REG_READ_OK, 3);
    const AdvRegRead& r = c.regRead();
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_DONE, r.state);
    TEST_ASSERT_EQUAL_UINT8(3, r.count);
    TEST_ASSERT_EQUAL_HEX8(0x10, r.v[0]); TEST_ASSERT_EQUAL_HEX8(0x12, r.v[2]);
    uint8_t p[8], n;
    TEST_ASSERT_FALSE(c.requestPayload(p, &n));                  // nothing pending any more
    c.requestRegRead(0x42, 0, 0x00, 2, 2);                       // an I2C error on the ADV Controller: error
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t + 200, true, true));
    regReply(c, t + 200, ADV_REG_READ_I2C_ERROR, 0);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_ERROR, c.regRead().state);
    gCaps = 0;
}

void test_reg_read_three_tries_then_error(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x42, 0, 0x10, 1, 3);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));
        c.result(REGREAD, false, 0, nullptr, 0, t);
        t += AdvLinkClient::kTrySpacingMs;
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_ERROR, c.regRead().state);
    TEST_ASSERT_NOT_EQUAL(REGREAD, c.due(t, true, true));
    gCaps = 0;
}

// Why a read ended in error, for the log (every read used to say only "error")
void test_reg_read_error_says_why(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x42, 0, 0x10, 2, 1);
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));
    regReply(c, t, ADV_REG_READ_I2C_ERROR, 0);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_WHY_STATUS, c.regRead().why);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_READ_I2C_ERROR, c.regRead().replyStatus);
    c.requestRegRead(0x42, 0, 0x10, 2, 2);
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t + 200, true, true));
    regReply(c, t + 200, ADV_REG_READ_OK, 1);                   // fewer values than asked
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_WHY_COUNT, c.regRead().why);
    c.requestRegRead(0x42, 0, 0x10, 2, 3);
    t += 400;
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));
        uint8_t junk[1] = {0};
        if (i == 0) c.result(REGREAD, true, REGREAD | ADV_RESP, junk, 0, t);   // a reply that does not unpack
        else c.result(REGREAD, false, 0, nullptr, 0, t);
        t += AdvLinkClient::kTrySpacingMs;
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_ERROR, c.regRead().state);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_WHY_NO_REPLY, c.regRead().why);
    TEST_ASSERT_EQUAL_UINT8(1, c.regRead().badReplies);           // the undecodable answer was counted
    gCaps = 0;
}

void test_reg_read_failures_keep_status_ok(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    for (uint8_t k = 0; k < 5; k++) {                            // 5 failed reads (15 lost replies)
        c.requestRegRead(0x42, 0, 0x10, 1, k);
        for (int i = 0; i < 3; i++) {
            TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));
            c.result(REGREAD, false, 0, nullptr, 0, t);
            t += AdvLinkClient::kTrySpacingMs;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_OK, c.advController().status);
    gCaps = 0;
}

void test_reg_read_unsupported_without_cap(void) {
    gCaps = ADV_INFO_CAP_SIGNAL;                                 // an image before the register read
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x42, 0, 0x10, 1, 4);
    TEST_ASSERT_NOT_EQUAL(REGREAD, c.due(t, true, true));
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_UNSUPPORTED, c.regRead().state);
    gCaps = 0;
}

void test_reg_read_second_request_waits_for_the_first(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x42, 0, 0x10, 1, 1);
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t, true, true));       // on the wire
    c.requestRegRead(0x56, 0, 0x20, 2, 2);                       // the payload of the retry must not change
    uint8_t p[8], n;
    TEST_ASSERT_TRUE(c.requestPayload(p, &n));
    TEST_ASSERT_EQUAL_HEX8(0x42, p[0]);
    regReply(c, t, ADV_REG_READ_OK, 1);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_PENDING, c.regRead().state);  // the second one now
    TEST_ASSERT_EQUAL_UINT8(2, c.regRead().gen);
    TEST_ASSERT_EQUAL_HEX8(REGREAD, c.due(t + 200, true, true));
    TEST_ASSERT_TRUE(c.requestPayload(p, &n));
    TEST_ASSERT_EQUAL_HEX8(0x56, p[0]); TEST_ASSERT_EQUAL_HEX8(0x20, p[2]);
    gCaps = 0;
}

void test_restart_ends_a_pending_read(void) {   // after an ADV Controller update the read is an error, not pending forever
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    bootOk(c);
    c.requestRegRead(0x42, 0, 0x10, 1, 9);
    c.restart();
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_ERROR, c.regRead().state);
    TEST_ASSERT_EQUAL_UINT8(9, c.regRead().gen);
    gCaps = 0;
}

// A read while the ADV Controller does not answer (wire off, 2.x image) ends as error at once, never pending forever
void test_reg_read_error_while_no_reply(void) {
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = 1000;
    for (int i = 0; i < 12 && c.advController().status != ADV_CONTROLLER_NO_REPLY; i++) {   // boot pings unanswered
        uint8_t r = c.due(t, true, true);
        if (r) reply(c, r, false, t);
        t += AdvLinkClient::kBootPingEveryMs;
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_NO_REPLY, c.advController().status);
    c.requestRegRead(0x42, 0, 0x10, 1, 5);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_ERROR, c.regRead().state);
    TEST_ASSERT_EQUAL_UINT8(5, c.regRead().gen);
    gCaps = 0;
}

void test_reg_read_pending_becomes_error_on_no_reply(void) {   // asked while OK, the controller then goes silent
    gCaps = ADV_INFO_CAP_REG_READ;
    AdvLinkClient c;
    uint32_t t = bootOk(c) + 60;
    c.requestRegRead(0x42, 0, 0x10, 1, 6);
    for (int k = 0; k < 3; k++) {                                 // 3 failed INFO cycles first (period 10 s)
        t += AdvLinkClient::kInfoPeriodMs;
        for (int i = 0; i < 3; i++) {
            uint8_t r = c.due(t, true, true);
            TEST_ASSERT_EQUAL_HEX8(INFO, r);
            reply(c, r, false, t);
            t += AdvLinkClient::kTrySpacingMs;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(ADV_CONTROLLER_NO_REPLY, c.advController().status);
    c.due(t, true, true);
    TEST_ASSERT_EQUAL_UINT8(ADV_REG_ERROR, c.regRead().state);
    gCaps = 0;
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ping_other_protocol_is_incompatible);
    RUN_TEST(test_accept_reply_uses_real_constants);
    RUN_TEST(test_boot_pings_then_no_reply);
    RUN_TEST(test_boot_window_waits_for_allowed_due);
    RUN_TEST(test_ping_info_ok);
    RUN_TEST(test_silence_after_any_frame);
    RUN_TEST(test_info_after_state_and_hash_check);
    RUN_TEST(test_resend_capped_then_not_applied);
    RUN_TEST(test_failed_info_retries_and_cycles_to_no_reply);
    RUN_TEST(test_nak_counts_as_failure);
    RUN_TEST(test_incompatible_state_version);
    RUN_TEST(test_periodic_info);
    RUN_TEST(test_link_busy_blocks_requests);
    RUN_TEST(test_ping_ok_but_info_always_failing_ends_in_no_reply);
    RUN_TEST(test_no_reply_notice_only_once);
    RUN_TEST(test_keep_alive_state_does_not_trigger_info);
    RUN_TEST(test_no_signal_requests_without_caps);
    RUN_TEST(test_signal_polled_every_500_ms_while_powered);
    RUN_TEST(test_signal_slow_when_decoder_off);
    RUN_TEST(test_signal_fast_while_scanning);
    RUN_TEST(test_signal_failure_is_one_try_and_not_a_no_reply);
    RUN_TEST(test_info_after_state_comes_before_signal);
    RUN_TEST(test_restart_forgets_the_signal);
    RUN_TEST(test_reg_read_waits_for_quiet_window);
    RUN_TEST(test_reg_read_reply_fills_values);
    RUN_TEST(test_reg_read_three_tries_then_error);
    RUN_TEST(test_reg_read_error_says_why);
    RUN_TEST(test_reg_read_failures_keep_status_ok);
    RUN_TEST(test_reg_read_unsupported_without_cap);
    RUN_TEST(test_reg_read_second_request_waits_for_the_first);
    RUN_TEST(test_restart_ends_a_pending_read);
    RUN_TEST(test_reg_read_error_while_no_reply);
    RUN_TEST(test_reg_read_pending_becomes_error_on_no_reply);
    return UNITY_END();
}
