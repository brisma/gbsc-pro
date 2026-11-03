// Console log: upstream byte stream -> tagged lines, and the sync mark counters (log-lines.h)
#include <unity.h>
#include <string.h>
#include "log-lines.h"

void setUp(void) {}
void tearDown(void) {}

struct Got { LogTag tag; char text[LOG_TEXT_MAX + 1]; };
static Got gGot[8];
static int gN = 0;

static void collect(const LogTag& tag, const char* text, void*) {
    if (gN >= 8) return;
    gGot[gN].tag = tag;
    strncpy(gGot[gN].text, text, LOG_TEXT_MAX);
    gGot[gN].text[LOG_TEXT_MAX] = '\0';
    gN++;
}

static void feed(LogLineSplitter& s, const char* t, uint32_t now = 0) { s.feed(t, strlen(t), now, collect, nullptr); }

static void assertTag(const Got& g, uint8_t cat, uint8_t level, bool ring) {
    TEST_ASSERT_EQUAL_UINT8(cat, g.tag.cat);
    TEST_ASSERT_EQUAL_UINT8(level, g.tag.level);
    TEST_ASSERT_EQUAL(ring, g.tag.ring);
}

void test_newline_ends_line_default_tag_gbs_info(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "abc\n");
    TEST_ASSERT_EQUAL_INT(1, gN);
    TEST_ASSERT_EQUAL_STRING("abc", gGot[0].text);
    assertTag(gGot[0], LC_GBS, LL_INFO, true);
}

void test_crlf_is_one_line(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "x\r\n");
    feed(s, "\n\n");
    TEST_ASSERT_EQUAL_INT(1, gN);
    TEST_ASSERT_EQUAL_STRING("x", gGot[0].text);
}

void test_tag_applies_to_line_starting_after_it(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "partial");
    s.tagNext({LC_PRESET, LL_INFO, true}, collect, nullptr);
    feed(s, "\npreset applied: 1280x960\n");
    feed(s, "after\n");
    TEST_ASSERT_EQUAL_INT(3, gN);
    TEST_ASSERT_EQUAL_STRING("partial", gGot[0].text);
    assertTag(gGot[0], LC_GBS, LL_INFO, true);
    TEST_ASSERT_EQUAL_STRING("preset applied: 1280x960", gGot[1].text);
    assertTag(gGot[1], LC_PRESET, LL_INFO, true);
    assertTag(gGot[2], LC_GBS, LL_INFO, true);
}

// Pending text ends when a tag arrives: it keeps its own category (a debug tag would hide it without devmode)
void test_tag_ends_the_pending_text_first(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "partial");
    s.tagNext({LC_SYNC, LL_DEBUG, true}, collect, nullptr);
    feed(s, "rest");
    feed(s, "\n");
    feed(s, "next\n");
    TEST_ASSERT_EQUAL_INT(3, gN);
    TEST_ASSERT_EQUAL_STRING("partial", gGot[0].text);
    assertTag(gGot[0], LC_GBS, LL_INFO, true);
    TEST_ASSERT_EQUAL_STRING("rest", gGot[1].text);
    assertTag(gGot[1], LC_SYNC, LL_DEBUG, true);
    assertTag(gGot[2], LC_GBS, LL_INFO, true);
}

void test_lone_cr_ends_a_line(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "Progress: 1%\rProgress: 2%\r");
    s.tick(5000, collect, nullptr);                        // the last one ends with the quiet time
    TEST_ASSERT_EQUAL_INT(2, gN);
    TEST_ASSERT_EQUAL_STRING("Progress: 1%", gGot[0].text);
    TEST_ASSERT_EQUAL_STRING("Progress: 2%", gGot[1].text);
    gN = 0;
    feed(s, "a\r");
    feed(s, "\nb\n");                                    // CR LF split across two writes: one line
    TEST_ASSERT_EQUAL_INT(2, gN);
    TEST_ASSERT_EQUAL_STRING("a", gGot[0].text);
    TEST_ASSERT_EQUAL_STRING("b", gGot[1].text);
}

void test_tag_skips_empty_lines(void) {
    LogLineSplitter s; gN = 0;
    s.tagNext({LC_SYNC, LL_INFO, true}, collect, nullptr);
    feed(s, "\n\nno signal\n");
    TEST_ASSERT_EQUAL_INT(1, gN);
    TEST_ASSERT_EQUAL_STRING("no signal", gGot[0].text);
    assertTag(gGot[0], LC_SYNC, LL_INFO, true);
}

void test_partial_flushes_after_1s(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "abc", 0);
    s.tick(999, collect, nullptr);
    TEST_ASSERT_EQUAL_INT(0, gN);
    s.tick(1000, collect, nullptr);
    TEST_ASSERT_EQUAL_INT(1, gN);
    TEST_ASSERT_EQUAL_STRING("abc", gGot[0].text);
    s.tick(5000, collect, nullptr);
    TEST_ASSERT_EQUAL_INT(1, gN);
}

void test_long_line_splits_without_loss(void) {
    LogLineSplitter s; gN = 0;
    s.tagNext({LC_GBS, LL_DEBUG, true}, collect, nullptr);
    char t[152];
    memset(t, 'z', 150); t[150] = '\n'; t[151] = '\0';
    feed(s, t);
    TEST_ASSERT_EQUAL_INT(2, gN);
    TEST_ASSERT_EQUAL_size_t(128, strlen(gGot[0].text));
    TEST_ASSERT_EQUAL_size_t(22, strlen(gGot[1].text));
    assertTag(gGot[0], LC_GBS, LL_DEBUG, true);
    assertTag(gGot[1], LC_GBS, LL_DEBUG, true);
}

void test_ring_false_tag_kept(void) {
    LogLineSplitter s; gN = 0;
    s.tagNext({LC_GBS, LL_DEBUG, false}, collect, nullptr);
    feed(s, "h: 433 v: 625\n");
    TEST_ASSERT_EQUAL_INT(1, gN);
    assertTag(gGot[0], LC_GBS, LL_DEBUG, false);
}

void test_marks_summary_and_quiet_time(void) {
    LogMarks m;
    TEST_ASSERT_FALSE(m.pending());
    for (uint32_t i = 0; i < 12; i++) m.mark(LM_SYNC_DROP, i * 10);
    for (uint32_t i = 0; i < 3; i++) m.mark(LM_SOG_BAD, 200 + i);
    TEST_ASSERT_TRUE(m.pending());
    TEST_ASSERT_FALSE(m.due(999));
    TEST_ASSERT_FALSE(m.due(202 + 999));
    TEST_ASSERT_TRUE(m.due(202 + 1000));
    char out[96];
    m.take(out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("no sync x12, SOG bad x3", out);
    TEST_ASSERT_FALSE(m.pending());
    m.mark(LM_MODE_WATCH, 0); m.mark(LM_SP_REFRESH, 0);
    m.take(out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SP refresh x1, mode watch x1", out);
}

void test_activity_lost_repeats_become_a_count(void) {
    TEST_ASSERT_FALSE(logActivityRepeat(false, 5000, 0));       // nothing lost before: the whole line
    TEST_ASSERT_TRUE(logActivityRepeat(true, 1000 + 1999, 1000));
    TEST_ASSERT_FALSE(logActivityRepeat(true, 1000 + 2000, 1000));
    LogMarks m;
    for (uint32_t i = 0; i < 3; i++) m.mark(LM_ACTIVITY_LOST, i * 900);
    m.mark(LM_SYNC_DROP, 2000);
    char out[96];
    m.take(out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("no sync x1, activity lost x3", out);
}

void test_cr_edge_cases(void) {
    LogLineSplitter s; gN = 0;
    feed(s, "a\r\r\n");                                 // one line, no empty one
    TEST_ASSERT_EQUAL_INT(1, gN);
    TEST_ASSERT_EQUAL_STRING("a", gGot[0].text);
    gN = 0;
    feed(s, "e\r");                                       // a tag while a CR waits
    s.tagNext({LC_SYNC, LL_INFO, true}, collect, nullptr);
    feed(s, "\nf\n");
    TEST_ASSERT_EQUAL_INT(2, gN);
    TEST_ASSERT_EQUAL_STRING("e", gGot[0].text);
    assertTag(gGot[0], LC_GBS, LL_INFO, true);
    TEST_ASSERT_EQUAL_STRING("f", gGot[1].text);
    assertTag(gGot[1], LC_SYNC, LL_INFO, true);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_newline_ends_line_default_tag_gbs_info);
    RUN_TEST(test_crlf_is_one_line);
    RUN_TEST(test_tag_applies_to_line_starting_after_it);
    RUN_TEST(test_tag_ends_the_pending_text_first);
    RUN_TEST(test_lone_cr_ends_a_line);
    RUN_TEST(test_cr_edge_cases);
    RUN_TEST(test_tag_skips_empty_lines);
    RUN_TEST(test_partial_flushes_after_1s);
    RUN_TEST(test_long_line_splits_without_loss);
    RUN_TEST(test_ring_false_tag_kept);
    RUN_TEST(test_marks_summary_and_quiet_time);
    RUN_TEST(test_activity_lost_repeats_become_a_count);
    return UNITY_END();
}
