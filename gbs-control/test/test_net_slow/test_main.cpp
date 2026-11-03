// Slow web connections: a port-80 connection with data still in flight after kSlowMs is reported once
// (its TCP retransmits then tell the radio from the device)
#include <unity.h>
#include "net-slow.h"

void setUp(void) {}
void tearDown(void) {}

void test_reported_once_after_the_threshold(void) {
    NetSlowConns s;
    NetConn c[2] = {{0x10, true}, {0x20, true}};
    NetSlowHit h;
    TEST_ASSERT_FALSE(s.update(c, 2, 1000, h));                   // both new
    TEST_ASSERT_FALSE(s.update(c, 2, 1000 + NetSlowConns::kSlowMs - 1, h));
    TEST_ASSERT_TRUE(s.update(c, 2, 1000 + NetSlowConns::kSlowMs, h));
    TEST_ASSERT_EQUAL_UINT32(0x10, h.id);
    TEST_ASSERT_EQUAL_UINT32(NetSlowConns::kSlowMs, h.ageMs);
    TEST_ASSERT_TRUE(s.update(c, 2, 1000 + NetSlowConns::kSlowMs, h));     // the second one, on the next call
    TEST_ASSERT_EQUAL_UINT32(0x20, h.id);
    TEST_ASSERT_FALSE(s.update(c, 2, 9000, h));                    // never twice
}

void test_idle_connection_is_not_slow(void) {   // a browser's spare connection: open, nothing to send
    NetSlowConns s;
    NetConn c[1] = {{0x10, false}};
    NetSlowHit h;
    s.update(c, 1, 0, h);
    TEST_ASSERT_FALSE(s.update(c, 1, 5000, h));
    c[0].busy = true;                                              // a request came and its reply is stuck
    TEST_ASSERT_TRUE(s.update(c, 1, 5100, h));
    TEST_ASSERT_EQUAL_UINT32(5100, h.ageMs);                       // the age counts from the connection's start
}

void test_a_closed_connection_is_forgotten(void) {
    NetSlowConns s;
    NetConn a[1] = {{0x10, true}};
    NetSlowHit h;
    s.update(a, 1, 0, h);
    TEST_ASSERT_FALSE(s.update(a, 0, 1500, h));                     // closed before the threshold
    TEST_ASSERT_FALSE(s.update(a, 1, 1600, h));                     // the same pcb address again: a new connection
    TEST_ASSERT_FALSE(s.update(a, 1, 1600 + NetSlowConns::kSlowMs - 1, h));
    TEST_ASSERT_TRUE(s.update(a, 1, 1600 + NetSlowConns::kSlowMs, h));
}

void test_more_connections_than_slots(void) {
    NetSlowConns s;
    NetConn c[NetSlowConns::kSlots + 2];
    for (uint8_t i = 0; i < NetSlowConns::kSlots + 2; i++) c[i] = {(uintptr_t)(0x100 + i), true};
    NetSlowHit h;
    s.update(c, NetSlowConns::kSlots + 2, 0, h);                    // the extra ones are not tracked, no crash
    uint8_t hits = 0;
    while (s.update(c, NetSlowConns::kSlots + 2, NetSlowConns::kSlowMs, h)) hits++;
    TEST_ASSERT_EQUAL_UINT8(NetSlowConns::kSlots, hits);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reported_once_after_the_threshold);
    RUN_TEST(test_idle_connection_is_not_slow);
    RUN_TEST(test_a_closed_connection_is_forgotten);
    RUN_TEST(test_more_connections_than_slots);
    return UNITY_END();
}
