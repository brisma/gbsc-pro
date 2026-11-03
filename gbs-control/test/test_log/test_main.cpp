// Console log ring: records, seq wrap, dropped counts, line format and JSON
#include <unity.h>
#include <string.h>
#include "log-ring.h"

void setUp(void) {}
void tearDown(void) {}

static LogRing gRing;   // 2 KB: static, like on the device

static void fresh() { gRing = LogRing(); }

static uint16_t oldestSeq() {
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(-1, r, &d));
    return r.seq;
}

void test_append_and_read_in_order(void) {
    fresh();
    TEST_ASSERT_EQUAL_UINT16(1, gRing.append(10, LC_GBS, LL_INFO, "a"));
    gRing.append(20, LC_SCAN, LL_DEBUG, "b");
    gRing.append(30, LC_ADV, LL_INFO, "c");
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(-1, r, &d));
    TEST_ASSERT_EQUAL_UINT16(1, r.seq);
    TEST_ASSERT_EQUAL_STRING("a", r.text);
    TEST_ASSERT_EQUAL_UINT32(10, r.ms);
    TEST_ASSERT_TRUE(gRing.readAfter(1, r, &d));
    TEST_ASSERT_EQUAL_UINT16(2, r.seq);
    TEST_ASSERT_EQUAL_STRING("b", r.text);
    TEST_ASSERT_EQUAL_UINT8(LC_SCAN, r.cat);
    TEST_ASSERT_EQUAL_UINT8(LL_DEBUG, r.level);
    TEST_ASSERT_FALSE(gRing.readAfter(3, r, &d));
    TEST_ASSERT_EQUAL_UINT16(0, d);
    TEST_ASSERT_EQUAL_UINT16(3, gRing.lastSeq());
    TEST_ASSERT_EQUAL_UINT16(3, gRing.count());
}

void test_wrap_evicts_oldest_and_counts_dropped(void) {
    fresh();
    char t[41];
    memset(t, 'y', 40); t[40] = '\0';
    for (int i = 0; i < 100; i++) gRing.append((uint32_t)i, LC_GBS, LL_INFO, t);
    TEST_ASSERT_TRUE(gRing.count() < 100);
    uint16_t oldest = oldestSeq();
    TEST_ASSERT_TRUE(oldest > 1);
    TEST_ASSERT_EQUAL_UINT16(100 - gRing.count() + 1, oldest);
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(1, r, &d));
    TEST_ASSERT_EQUAL_UINT16(oldest, r.seq);
    TEST_ASSERT_EQUAL_UINT16(oldest - 2, d);
    TEST_ASSERT_EQUAL_STRING(t, r.text);
}

void test_since_ahead_of_last_returns_all(void) {
    fresh();
    for (int i = 0; i < 5; i++) gRing.append(0, LC_GBS, LL_INFO, "x");
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(500, r, &d));
    TEST_ASSERT_EQUAL_UINT16(1, r.seq);
    TEST_ASSERT_EQUAL_UINT16(0, d);
}

// Seq 0 is never used: since=0 always means "everything"
void test_seq_wraps_past_65535_skipping_0(void) {
    fresh();
    for (uint32_t i = 0; i < 65537; i++) gRing.append(i, LC_GBS, LL_INFO, "s");
    TEST_ASSERT_EQUAL_UINT16(2, gRing.lastSeq());          // 1..65535, then 1, 2
    TEST_ASSERT_TRUE(logSeqAfter(1, 65535));
    TEST_ASSERT_FALSE(logSeqAfter(65535, 1));
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(65535, r, &d));
    TEST_ASSERT_EQUAL_UINT16(1, r.seq);
    TEST_ASSERT_EQUAL_UINT16(0, d);
}

void test_since_zero_is_everything(void) {
    fresh();
    for (uint32_t i = 0; i < 65537; i++) gRing.append(i, LC_GBS, LL_INFO, "s");   // wrapped
    LogRecord r, o;
    uint16_t d = 0, d2 = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(0, r, &d));
    TEST_ASSERT_TRUE(gRing.readAfter(-1, o, &d2));
    TEST_ASSERT_EQUAL_UINT16(o.seq, r.seq);                // the oldest kept, not "after 0"
    TEST_ASSERT_EQUAL_UINT16(0, d);
}

void test_dropped_across_wrap(void) {
    fresh();
    uint32_t n = 0;
    for (;;) {
        gRing.append(n, LC_GBS, LL_INFO, "s");
        n++;
        if (n > 65536 && oldestSeq() == 10) break;
    }
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(65530, r, &d));
    TEST_ASSERT_EQUAL_UINT16(10, r.seq);
    TEST_ASSERT_EQUAL_UINT16(14, d);   // 65531..65535 and 1..9 (no seq 0)
}

void test_text_cut_at_128(void) {
    fresh();
    char t[151];
    memset(t, 'x', 150); t[150] = '\0';
    gRing.append(0, LC_GBS, LL_INFO, t);
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(-1, r, &d));
    TEST_ASSERT_EQUAL_UINT8(128, r.len);
    TEST_ASSERT_EQUAL_size_t(128, strlen(r.text));
}

void test_percent_text_is_verbatim(void) {
    fresh();
    gRing.append(0, LC_GBS, LL_INFO, "100% %s %d");
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(-1, r, &d));
    TEST_ASSERT_EQUAL_STRING("100% %s %d", r.text);
    char line[160];
    logFormatLine(r, line, sizeof(line));
    TEST_ASSERT_NOT_NULL(strstr(line, "100% %s %d"));
}

static LogRecord rec(uint16_t seq, uint32_t ms, uint8_t cat, uint8_t level, const char* text) {
    LogRecord r;
    r.seq = seq; r.ms = ms; r.cat = cat; r.level = level;
    r.len = (uint8_t)strlen(text);
    memcpy(r.text, text, r.len + 1);
    return r;
}

void test_format_line(void) {
    char out[160];
    LogRecord r = rec(1, 123456, LC_SCAN, LL_INFO, "start");
    size_t n = logFormatLine(r, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("[   123.456] [scan]     start\n", out);
    TEST_ASSERT_EQUAL_size_t(strlen(out), n);
    r = rec(1, 5, LC_PROFILE, LL_INFO, "x");
    logFormatLine(r, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("[     0.005] [profile]  x\n", out);
}

void test_json_escapes_quotes_controls_and_high_bytes(void) {
    char out[200];
    LogRecord r = rec(1, 5, LC_GBS, LL_INFO, "a\"b\\c\x01\xC3");
    TEST_ASSERT_TRUE(logRecordJson(r, out, sizeof(out)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"s\":1,\"t\":5,\"c\":\"gbs\",\"l\":\"info\",\"m\":\"a\\\"b\\\\c\\u0001?\"}", out);
    r = rec(7, 9, LC_ADV, LL_DEBUG, "x");
    TEST_ASSERT_TRUE(logRecordJson(r, out, sizeof(out)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"s\":7,\"t\":9,\"c\":\"adv\",\"l\":\"debug\",\"m\":\"x\"}", out);
    TEST_ASSERT_EQUAL_size_t(0, logRecordJson(r, out, 10));   // does not fit
}

void test_json_worst_case_fits_pending(void) {
    char t[129];
    memset(t, 0x01, 128); t[128] = '\0';
    LogRecord r = rec(65535, 4294967295u, LC_PROFILE, LL_DEBUG, t);
    LogJsonCursor c;
    TEST_ASSERT_TRUE(logRecordJson(r, c.pending, sizeof(c.pending)) > 0);   // the /api/log filler buffer
}

void test_cat_names(void) {
    TEST_ASSERT_EQUAL_STRING("gbs", logCatName(LC_GBS));
    TEST_ASSERT_EQUAL_STRING("menu", logCatName(LC_MENU));
    TEST_ASSERT_EQUAL_STRING("?", logCatName(LC_COUNT));
}

static size_t drain(LogJsonCursor& c, char* out, size_t cap, size_t step) {
    size_t k = 0;
    for (;;) {
        char buf[64];
        size_t n = logJsonFill(c, gRing, buf, step);
        if (!n) break;
        TEST_ASSERT_TRUE(k + n < cap);
        memcpy(out + k, buf, n);
        k += n;
    }
    out[k] = '\0';
    return k;
}

void test_json_fill_small_chunks(void) {
    fresh();
    gRing.append(1, LC_GBS, LL_INFO, "a");
    gRing.append(2, LC_SCAN, LL_DEBUG, "b\"c");
    LogJsonCursor c;
    logJsonBegin(c, -1);
    char out[512];
    drain(c, out, sizeof(out), 7);
    TEST_ASSERT_EQUAL_STRING("{\"lines\":[{\"s\":1,\"t\":1,\"c\":\"gbs\",\"l\":\"info\",\"m\":\"a\"},"
                             "{\"s\":2,\"t\":2,\"c\":\"scan\",\"l\":\"debug\",\"m\":\"b\\\"c\"}],\"next\":2,\"dropped\":0,\"boot\":0}", out);
}

void test_json_fill_since_and_nothing_new(void) {
    fresh();
    for (int i = 0; i < 3; i++) gRing.append(0, LC_GBS, LL_INFO, "x");
    LogJsonCursor c;
    char out[512];
    logJsonBegin(c, 2);
    drain(c, out, sizeof(out), 60);
    TEST_ASSERT_EQUAL_STRING("{\"lines\":[{\"s\":3,\"t\":0,\"c\":\"gbs\",\"l\":\"info\",\"m\":\"x\"}],\"next\":3,\"dropped\":0,\"boot\":0}", out);
    logJsonBegin(c, 3);
    drain(c, out, sizeof(out), 60);
    TEST_ASSERT_EQUAL_STRING("{\"lines\":[],\"next\":3,\"dropped\":0,\"boot\":0}", out);
    fresh();
    logJsonBegin(c, -1);
    drain(c, out, sizeof(out), 60);
    TEST_ASSERT_EQUAL_STRING("{\"lines\":[],\"next\":0,\"dropped\":0,\"boot\":0}", out);
}

void test_json_fill_counts_lines_overwritten_while_streaming(void) {
    fresh();
    char t[41];
    memset(t, 'y', 40); t[40] = '\0';
    for (int i = 0; i < 10; i++) gRing.append(0, LC_GBS, LL_INFO, t);
    LogJsonCursor c;
    logJsonBegin(c, -1);
    char buf[64];
    TEST_ASSERT_TRUE(logJsonFill(c, gRing, buf, 20) > 0);   // the first record is under way
    for (int i = 0; i < 100; i++) gRing.append(0, LC_GBS, LL_INFO, t);   // the loop runs between two chunks
    char out[8192];
    drain(c, out, sizeof(out), 63);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"dropped\":"));
    TEST_ASSERT_NULL(strstr(out, "\"dropped\":0,\"boot\":0}"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"next\":110,"));
}

// The client keeps since across a device restart: with the boot id of another boot, everything, nothing "dropped"
void test_json_another_boot_is_everything(void) {
    fresh();
    gRing.setBoot(77);
    for (int i = 0; i < 3; i++) gRing.append(0, LC_GBS, LL_INFO, "x");
    LogJsonCursor c;
    char out[512];
    logJsonBegin(c, 2, 5, gRing);                          // since from boot 5
    drain(c, out, sizeof(out), 60);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"s\":1,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"dropped\":0,\"boot\":77}"));
    logJsonBegin(c, 2, 77, gRing);                          // the same boot: after 2
    drain(c, out, sizeof(out), 60);
    TEST_ASSERT_NULL(strstr(out, "\"s\":1,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"s\":3,"));
}

void test_wrap_edges(void) {
    fresh();
    for (uint32_t i = 0; i < 65535; i++) gRing.append(i, LC_GBS, LL_INFO, "s");
    TEST_ASSERT_EQUAL_UINT16(65535, gRing.lastSeq());      // next_ is 1 again
    LogRecord r;
    uint16_t d = 0;
    TEST_ASSERT_FALSE(gRing.readAfter(65535, r, &d));      // nothing after the last
    gRing.append(0, LC_GBS, LL_INFO, "s");                 // seq 1
    TEST_ASSERT_TRUE(gRing.readAfter(65535, r, &d));
    TEST_ASSERT_EQUAL_UINT16(1, r.seq);
    TEST_ASSERT_EQUAL_UINT16(0, d);
    fresh();
    uint32_t n = 0;
    do { gRing.append(n++, LC_GBS, LL_INFO, "s"); } while (n < 65536 || oldestSeq() != 1);   // oldest == 1
    d = 0;
    TEST_ASSERT_TRUE(gRing.readAfter(65534, r, &d));
    TEST_ASSERT_EQUAL_UINT16(1, r.seq);
    TEST_ASSERT_EQUAL_UINT16(1, d);                        // 65535 lost
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_append_and_read_in_order);
    RUN_TEST(test_wrap_evicts_oldest_and_counts_dropped);
    RUN_TEST(test_since_ahead_of_last_returns_all);
    RUN_TEST(test_seq_wraps_past_65535_skipping_0);
    RUN_TEST(test_since_zero_is_everything);
    RUN_TEST(test_json_another_boot_is_everything);
    RUN_TEST(test_dropped_across_wrap);
    RUN_TEST(test_wrap_edges);
    RUN_TEST(test_text_cut_at_128);
    RUN_TEST(test_percent_text_is_verbatim);
    RUN_TEST(test_format_line);
    RUN_TEST(test_json_escapes_quotes_controls_and_high_bytes);
    RUN_TEST(test_json_worst_case_fits_pending);
    RUN_TEST(test_cat_names);
    RUN_TEST(test_json_fill_small_chunks);
    RUN_TEST(test_json_fill_since_and_nothing_new);
    RUN_TEST(test_json_fill_counts_lines_overwritten_while_streaming);
    return UNITY_END();
}
