// WebSocket slot watch: one line when a slot opens and one when it closes, with how long it stayed open, read from
// the slot state once a second (the TCP layer closes a vanished client without any event from the library)
#include <unity.h>
#include "ws-watch.h"

void setUp(void) {}
void tearDown(void) {}

void test_nothing_while_a_slot_stays_as_it_was(void) {
    WsSlotWatch w;
    TEST_ASSERT_EQUAL_UINT8(WSW_NONE, w.update(false, 1000));
    TEST_ASSERT_EQUAL_UINT8(WSW_NONE, w.update(false, 2000));
}

void test_open_then_closed_with_its_length(void) {
    WsSlotWatch w;
    TEST_ASSERT_EQUAL_UINT8(WSW_OPENED, w.update(true, 5000));
    TEST_ASSERT_EQUAL_UINT8(WSW_NONE, w.update(true, 6000));
    TEST_ASSERT_EQUAL_UINT8(WSW_CLOSED, w.update(false, 65000));
    TEST_ASSERT_EQUAL_UINT32(60000, w.openForMs());
    TEST_ASSERT_EQUAL_UINT8(WSW_NONE, w.update(false, 66000));
}

void test_length_across_the_millis_wrap(void) {
    WsSlotWatch w;
    TEST_ASSERT_EQUAL_UINT8(WSW_OPENED, w.update(true, 0xFFFFF000u));
    TEST_ASSERT_EQUAL_UINT8(WSW_CLOSED, w.update(false, 0x00001000u));
    TEST_ASSERT_EQUAL_UINT32(0x2000, w.openForMs());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_while_a_slot_stays_as_it_was);
    RUN_TEST(test_open_then_closed_with_its_length);
    RUN_TEST(test_length_across_the_millis_wrap);
    return UNITY_END();
}
