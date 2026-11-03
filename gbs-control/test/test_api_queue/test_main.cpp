#include <unity.h>
#include "api-queue.h"
#include "api-pending.h"
#include "api-watchdog.h"

void setUp(void) {}
void tearDown(void) {}

void test_fifo_order_and_capacity(void) {
    RingQueue<int, 4> q;
    TEST_ASSERT_TRUE(q.empty());
    TEST_ASSERT_EQUAL_UINT8(3, q.freeSlots());
    TEST_ASSERT_TRUE(q.push(1)); TEST_ASSERT_TRUE(q.push(2)); TEST_ASSERT_TRUE(q.push(3));
    TEST_ASSERT_FALSE(q.push(4));   // full: capacity N - 1
    TEST_ASSERT_EQUAL_UINT8(0, q.freeSlots());
    int v;
    TEST_ASSERT_TRUE(q.pop(v)); TEST_ASSERT_EQUAL_INT(1, v);
    TEST_ASSERT_TRUE(q.push(4));
    TEST_ASSERT_TRUE(q.pop(v)); TEST_ASSERT_EQUAL_INT(2, v);
    TEST_ASSERT_TRUE(q.pop(v)); TEST_ASSERT_EQUAL_INT(3, v);
    TEST_ASSERT_TRUE(q.pop(v)); TEST_ASSERT_EQUAL_INT(4, v);
    TEST_ASSERT_FALSE(q.pop(v));
}

void test_upload_watchdog_expires_without_chunks(void) {
    UploadWatchdog w;
    TEST_ASSERT_FALSE(w.expired(50000));          // idle: never expires
    w.touch(1000);                                 // 30 s: a disturbed Wi-Fi link stalls TCP for >10 s
    TEST_ASSERT_FALSE(w.expired(30999));
    TEST_ASSERT_TRUE(w.expired(31000));
    w.touch(31000);
    TEST_ASSERT_FALSE(w.expired(45000));
    w.stop();
    TEST_ASSERT_FALSE(w.expired(90000));
    w.touch(0xFFFFF000u);                          // millis() wrap
    TEST_ASSERT_FALSE(w.expired(0x00000100u));
}

void test_queue_peek(void) {
    RingQueue<int, 5> q;
    q.push(10); q.push(20); q.push(30);
    TEST_ASSERT_EQUAL_UINT8(3, q.size());
    TEST_ASSERT_EQUAL_INT(10, q.at(0));
    TEST_ASSERT_EQUAL_INT(30, q.at(2));
    int v; q.pop(v);
    TEST_ASSERT_EQUAL_UINT8(2, q.size());
    TEST_ASSERT_EQUAL_INT(20, q.at(0));
    q.push(40); q.push(50);                         // wraps around
    TEST_ASSERT_EQUAL_UINT8(4, q.size());
    TEST_ASSERT_EQUAL_INT(50, q.at(3));
}

void test_pending_value_of_queue_and_bridge(void) {
    RingQueue<SettingsPair, 8> q;
    BridgePending b;
    uint16_t v = 0;
    TEST_ASSERT_FALSE(pendingValueOf(q, b, 3, 0, 1000, v));
    b.set(3, 7, 1000);
    TEST_ASSERT_TRUE(pendingValueOf(q, b, 3, 0, 1100, v));    // bridge target
    TEST_ASSERT_EQUAL_UINT16(7, v);
    SettingsPair p1 = {3, 1}, p2 = {4, 9}, p3 = {3, 2};
    q.push(p1); q.push(p2); q.push(p3);
    TEST_ASSERT_TRUE(pendingValueOf(q, b, 3, 0, 1100, v));    // newest queued write wins
    TEST_ASSERT_EQUAL_UINT16(2, v);
    TEST_ASSERT_TRUE(pendingValueOf(q, b, 4, 0, 1100, v));
    TEST_ASSERT_EQUAL_UINT16(9, v);
    TEST_ASSERT_FALSE(pendingValueOf(q, b, 5, 0, 1100, v));   // other index
    RingQueue<SettingsPair, 8> empty;
    TEST_ASSERT_FALSE(pendingValueOf(empty, b, 3, 7, 1100, v));   // bridge already applied
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pending_value_of_queue_and_bridge);
    RUN_TEST(test_queue_peek);
    RUN_TEST(test_fifo_order_and_capacity);
    RUN_TEST(test_upload_watchdog_expires_without_chunks);
    return UNITY_END();
}
