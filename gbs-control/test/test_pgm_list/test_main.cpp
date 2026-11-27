// Name lists in flash ("a\0b\0c"): the i-th string, the copy to RAM, the compile-time count and longest length
#include <unity.h>
#include <string.h>
#include "pro/util/pgm.h"

void setUp(void) {}
void tearDown(void) {}

#define NAMES "one\0two\0\0longest\0?"
static const char kNames[] PROGMEM = NAMES;

static_assert(PGM_LIST_COUNT(NAMES) == 5, "an empty entry counts");
static_assert(PGM_LIST_MAXLEN(NAMES) == 7, "the longest entry");
static_assert(PGM_LIST_COUNT("only") == 1, "one string");
static_assert(PGM_LIST_MAXLEN("") == 0, "an empty list");

void test_each_entry_and_the_empty_one(void) {
    TEST_ASSERT_EQUAL_STRING("one", pgmListAt(kNames, 0, 5));
    TEST_ASSERT_EQUAL_STRING("two", pgmListAt(kNames, 1, 5));
    TEST_ASSERT_EQUAL_STRING("", pgmListAt(kNames, 2, 5));
    TEST_ASSERT_EQUAL_STRING("longest", pgmListAt(kNames, 3, 5));
    TEST_ASSERT_EQUAL_STRING("?", pgmListAt(kNames, 4, 5));
}

void test_an_index_past_the_end_gives_the_last_entry(void) {
    TEST_ASSERT_EQUAL_STRING("?", pgmListAt(kNames, 5, 5));
    TEST_ASSERT_EQUAL_STRING("?", pgmListAt(kNames, 255, 5));
    TEST_ASSERT_EQUAL_STRING("one", pgmListAt(kNames, 3, 0));   // no entries declared: the list itself
}

void test_the_copy_ends_the_string_and_cuts_a_long_one(void) {
    char out[8];
    memset(out, 'x', sizeof(out));
    TEST_ASSERT_EQUAL_STRING("longest", pgmListCopy(out, sizeof(out), kNames, 3, 5));
    char small[4];
    memset(small, 'x', sizeof(small));
    TEST_ASSERT_EQUAL_STRING("lon", pgmListCopy(small, sizeof(small), kNames, 3, 5));
    char guard[2] = {'k', 'k'};
    pgmListCopy(guard, 0, kNames, 0, 5);   // no room: nothing written
    TEST_ASSERT_EQUAL_CHAR('k', guard[0]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_each_entry_and_the_empty_one);
    RUN_TEST(test_an_index_past_the_end_gives_the_last_entry);
    RUN_TEST(test_the_copy_ends_the_string_and_cuts_a_long_one);
    return UNITY_END();
}
