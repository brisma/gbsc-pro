#include <unity.h>
#include <string.h>
#include "menu-keyboard.h"
#include "menu-types.h"

void setUp(void) {}
void tearDown(void) {}

static void typeKey(MenuKeyboard& k, uint8_t row, uint8_t col) {
    while (k.row() != row) k.key(k.row() < row ? K_DOWN : K_UP);
    while (k.col() != col) k.key(K_RIGHT);
    k.key(K_OK);
}

void test_types_letters_and_case(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "");
    typeKey(k, 0, 0);                        // A
    typeKey(k, 3, 0);                        // aA
    typeKey(k, 1, 0);                        // n
    TEST_ASSERT_EQUAL_STRING("An", k.text());
    TEST_ASSERT_FALSE(k.upper());
}

void test_del_and_space(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "AB");
    typeKey(k, 3, MenuKeyboard::SP_DEL);
    typeKey(k, 3, MenuKeyboard::SP_SPACE);
    TEST_ASSERT_EQUAL_STRING("A ", k.text());
    MenuKeyboard e; e.open(KB_PROFILE_NEW, "");
    typeKey(e, 3, MenuKeyboard::SP_DEL);     // Del on empty
    TEST_ASSERT_EQUAL_STRING("", e.text());
}

void test_max_length_per_purpose(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "ABCDEFGHIJKLMNOPQRSTUVWX");   // 24
    typeKey(k, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(24, strlen(k.text()));
    MenuKeyboard p; p.open(KB_OTA_PASSWORD, "");
    for (int i = 0; i < 40; i++) typeKey(p, 0, 1);
    TEST_ASSERT_EQUAL_UINT32(32, strlen(p.text()));
    MenuKeyboard h; h.open(KB_EXTRA_HOSTS, "");
    for (int i = 0; i < 70; i++) typeKey(h, 0, 1);
    TEST_ASSERT_EQUAL_UINT32(63, strlen(h.text()));
    MenuKeyboard c; c.open(KB_RENAME, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");   // initial cut to 24
    TEST_ASSERT_EQUAL_UINT32(24, strlen(c.text()));
}

// The comma key only where a comma is allowed (Extra hosts separates names with it)
void test_comma_only_where_allowed(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "A");
    TEST_ASSERT_EQUAL_UINT8(13, k.rowLength(2));   // no comma key: the digit row ends with '/'
    char r[32];
    k.rowText(2, r, sizeof(r)); TEST_ASSERT_EQUAL_STRING("0123456789-./", r);
    MenuKeyboard h; h.open(KB_EXTRA_HOSTS, "a");
    TEST_ASSERT_EQUAL_UINT8(14, h.rowLength(2));
    h.rowText(2, r, sizeof(r)); TEST_ASSERT_EQUAL_STRING("0123456789-./,", r);
    typeKey(h, 2, 13);
    TEST_ASSERT_EQUAL_STRING("a,", h.text());
}

void test_validity(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "   ");
    TEST_ASSERT_FALSE(k.valid());
    k.open(KB_PROFILE_NEW, "Saturn JP");
    TEST_ASSERT_TRUE(k.valid());
    k.open(KB_PROFILE_NEW, " Saturn");       // leading space: trimmed by the store, still valid
    TEST_ASSERT_TRUE(k.valid());
    k.open(KB_OTA_PASSWORD, "");
    TEST_ASSERT_TRUE(k.valid());             // empty clears the password
    k.open(KB_EXTRA_HOSTS, "a b");
    TEST_ASSERT_FALSE(k.valid());
    k.open(KB_EXTRA_HOSTS, "tv.local, gbs.lan");
    TEST_ASSERT_TRUE(k.valid());             // spaces around commas are ignored by the host guard
    k.open(KB_EXTRA_HOSTS, "");
    TEST_ASSERT_TRUE(k.valid());
    k.open(KB_EXTRA_HOSTS, "10.0.0.5");       // the shared validator: no IPv4 literal, no localhost, no empty entry
    TEST_ASSERT_FALSE(k.valid());
    k.open(KB_EXTRA_HOSTS, "localhost");
    TEST_ASSERT_FALSE(k.valid());
    k.open(KB_EXTRA_HOSTS, "tv.lan,,gbs.lan");
    TEST_ASSERT_FALSE(k.valid());
}

void test_rotation_walks_all_keys_and_wraps(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "");
    for (int i = 0; i < 45; i++) k.key(K_ROT_R);   // 13 + 13 + 13 + 6 keys (no comma key in a profile name)
    TEST_ASSERT_EQUAL_UINT8(0, k.row());
    TEST_ASSERT_EQUAL_UINT8(0, k.col());
    k.key(K_ROT_L);
    TEST_ASSERT_EQUAL_UINT8(3, k.row());
    TEST_ASSERT_EQUAL_UINT8(MenuKeyboard::SP_DONE, k.col());
}

void test_up_down_keep_column_clamped(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "");
    for (int i = 0; i < 12; i++) k.key(K_RIGHT);   // M (col 12)
    k.key(K_DOWN); k.key(K_DOWN);                  // row 2, col 12 ('/')
    TEST_ASSERT_EQUAL_UINT8(12, k.col());
    k.key(K_DOWN);                                 // specials: col 12 of 14 -> the last key -> Done
    TEST_ASSERT_EQUAL_UINT8(3, k.row());
    TEST_ASSERT_EQUAL_UINT8(MenuKeyboard::SP_DONE, k.col());
    k.key(K_UP);                                   // back to digits, last key ('/': no comma key here)
    TEST_ASSERT_EQUAL_UINT8(2, k.row());
    TEST_ASSERT_EQUAL_UINT8(12, k.col());
    k.key(K_RIGHT);                                // wraps within the row
    TEST_ASSERT_EQUAL_UINT8(0, k.col());
}

void test_done_key_and_long_press(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "X");
    typeKey(k, 3, MenuKeyboard::SP_DONE);
    TEST_ASSERT_TRUE(k.done());
    k.clearDone();
    TEST_ASSERT_FALSE(k.done());
    k.key(K_LONG);
    TEST_ASSERT_TRUE(k.done());
}

void test_window_scrolls(void) {
    MenuKeyboard k; k.open(KB_EXTRA_HOSTS, "abcdefghijklmnopqrstuvwxyz0123456789");
    char w[32];
    uint8_t caret = k.window(10, w, sizeof(w));
    TEST_ASSERT_EQUAL_STRING("123456789", w);   // last cells-1 characters, the caret cell after them
    TEST_ASSERT_EQUAL_UINT8(9, caret);
    MenuKeyboard s; s.open(KB_PROFILE_NEW, "AB");
    caret = s.window(10, w, sizeof(w));
    TEST_ASSERT_EQUAL_STRING("AB", w);
    TEST_ASSERT_EQUAL_UINT8(2, caret);
}

// Cursor: < and > move it inside the text; typing, Space and Del act at the cursor
void test_cursor_moves_inserts_and_deletes(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "ABC");
    TEST_ASSERT_EQUAL_UINT8(3, k.cursor());                 // at the end
    typeKey(k, 3, MenuKeyboard::SP_LEFT); k.key(K_OK);       // two steps left
    TEST_ASSERT_EQUAL_UINT8(1, k.cursor());
    typeKey(k, 0, 23 - 13);                                  // row 0 holds A..M: 'K' is col 10
    TEST_ASSERT_EQUAL_STRING("AKBC", k.text());
    TEST_ASSERT_EQUAL_UINT8(2, k.cursor());
    typeKey(k, 3, MenuKeyboard::SP_DEL);                     // deletes left of the cursor
    TEST_ASSERT_EQUAL_STRING("ABC", k.text());
    TEST_ASSERT_EQUAL_UINT8(1, k.cursor());
    typeKey(k, 3, MenuKeyboard::SP_SPACE);
    TEST_ASSERT_EQUAL_STRING("A BC", k.text());
    for (int i = 0; i < 6; i++) typeKey(k, 3, MenuKeyboard::SP_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(4, k.cursor());                  // stops at the end
    for (int i = 0; i < 6; i++) typeKey(k, 3, MenuKeyboard::SP_LEFT);
    TEST_ASSERT_EQUAL_UINT8(0, k.cursor());                  // stops at the start
    typeKey(k, 3, MenuKeyboard::SP_DEL);                     // nothing left of it
    TEST_ASSERT_EQUAL_STRING("A BC", k.text());
}

void test_window_follows_the_cursor(void) {
    MenuKeyboard k; k.open(KB_EXTRA_HOSTS, "abcdefghijklmnopqrstuvwxyz0123456789");
    char w[32];
    for (int i = 0; i < 30; i++) typeKey(k, 3, MenuKeyboard::SP_LEFT);   // cursor at 6 ('g')
    uint8_t caret = k.window(10, w, sizeof(w));
    TEST_ASSERT_EQUAL_UINT8(6, caret);
    TEST_ASSERT_EQUAL_STRING("abcdefghi", w);               // the cursor stays inside the window
    for (int i = 0; i < 10; i++) typeKey(k, 3, MenuKeyboard::SP_RIGHT);   // cursor at 16 ('q')
    caret = k.window(10, w, sizeof(w));
    TEST_ASSERT_EQUAL_UINT8(9, caret);
    TEST_ASSERT_EQUAL_STRING("hijklmnop", w);               // scrolled: the cursor cell is the last one
}

void test_row_texts(void) {
    MenuKeyboard k; k.open(KB_PROFILE_NEW, "");
    char r[32];
    k.rowText(0, r, sizeof(r)); TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLM", r);
    k.rowText(2, r, sizeof(r)); TEST_ASSERT_EQUAL_STRING("0123456789-./", r);
    k.rowText(3, r, sizeof(r)); TEST_ASSERT_EQUAL_STRING("aA < > Space Del Done", r);
    typeKey(k, 3, 0);
    k.rowText(1, r, sizeof(r)); TEST_ASSERT_EQUAL_STRING("nopqrstuvwxyz", r);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_types_letters_and_case);
    RUN_TEST(test_del_and_space);
    RUN_TEST(test_max_length_per_purpose);
    RUN_TEST(test_comma_only_where_allowed);
    RUN_TEST(test_validity);
    RUN_TEST(test_rotation_walks_all_keys_and_wraps);
    RUN_TEST(test_up_down_keep_column_clamped);
    RUN_TEST(test_done_key_and_long_press);
    RUN_TEST(test_window_scrolls);
    RUN_TEST(test_row_texts);
    RUN_TEST(test_cursor_moves_inserts_and_deletes);
    RUN_TEST(test_window_follows_the_cursor);
    return UNITY_END();
}
