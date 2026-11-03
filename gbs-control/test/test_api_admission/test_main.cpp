// Request admission (on the device, 4+ parallel clients exhausted the heap and the ESP aborted in operator new):
// requests are refused at header time when memory is short or too many HTTP connections are open (counted from
// the lwIP list, so nothing can leak); a firmware upload is exclusive
#include <unity.h>
#include "api-admission.h"

void setUp(void) {}
void tearDown(void) {}

static const uint32_t kPlenty = 16000, kBlock = 15000;

// connections: open HTTP connections including the new one
void test_admits_up_to_the_connection_limit(void) {
    Admission a;
    TEST_ASSERT_EQUAL(ADMIT, a.decide(kPlenty, kBlock, Admission::kMaxConnections, false, false));
    TEST_ASSERT_EQUAL(BUSY_LOAD, a.decide(kPlenty, kBlock, Admission::kMaxConnections + 1, false, false));
    TEST_ASSERT_EQUAL(ADMIT, a.decide(kPlenty, kBlock, 1, false, false));   // others closed: back at once
}

void test_low_memory_with_hysteresis(void) {
    Admission a;
    TEST_ASSERT_EQUAL(BUSY_MEMORY, a.decide(Admission::kHeapLow - 1, kBlock, 1, false, false));
    TEST_ASSERT_TRUE(a.low());
    TEST_ASSERT_EQUAL(BUSY_MEMORY, a.decide(Admission::kHeapLow + 100, kBlock, 1, false, false));   // not back yet
    TEST_ASSERT_EQUAL(ADMIT, a.decide(Admission::kHeapOk, Admission::kBlockOk, 1, false, false));
    Admission b;
    TEST_ASSERT_EQUAL(BUSY_MEMORY, b.decide(kPlenty, Admission::kBlockLow - 1, 1, false, false));   // fragmented heap
}

void test_update_upload_is_exclusive(void) {
    Admission a;
    TEST_ASSERT_EQUAL(BUSY_UPDATE, a.decide(kPlenty, kBlock, 1, true, false));   // an upload runs: nothing else
    TEST_ASSERT_EQUAL(BUSY_UPDATE, a.decide(kPlenty, kBlock, 1, true, true));    // nor a second upload
}

void test_update_upload_needs_room(void) {
    Admission a;
    TEST_ASSERT_EQUAL(BUSY_MEMORY, a.decide(kPlenty, Admission::kUploadBlock - 1, 1, false, true));
    // web uploads no longer check the RSA signature: a 6 KB block (was refused) is enough
    TEST_ASSERT_TRUE(Admission::kUploadBlock <= 6144);
    TEST_ASSERT_EQUAL(ADMIT, a.decide(kPlenty, 6144, 1, false, true));
    TEST_ASSERT_EQUAL(BUSY_LOAD, a.decide(kPlenty, kBlock, 3, false, true));     // two other connections open
    TEST_ASSERT_EQUAL(ADMIT, a.decide(kPlenty, kBlock, 2, false, true));         // one other: the upload may start
}

// On the device, after a burst, 6 TIME-WAIT connections left 14.5 KB free but a largest block of 4968 B; the low
// state must end there (the allocations that failed were 136 B and 1.5 KB), not wait for a reboot
void test_fragmented_but_roomy_heap_leaves_the_low_state(void) {
    Admission a;
    TEST_ASSERT_EQUAL(BUSY_MEMORY, a.decide(9000, 4968, 1, false, false));
    TEST_ASSERT_EQUAL(ADMIT, a.decide(14480, 4968, 1, false, false));
}

// TCP accept guard (on the device, 8 looping clients still rebooted the ESP now and then: every accepted
// connection costs heap before any handler runs): new connections to port 80 are reset at accept time when
// too many are open or the heap is nearly gone; the portal (<= 3) never reaches it
void test_accept_guard(void) {
    TEST_ASSERT_TRUE(admitTcp(16000, 1));
    TEST_ASSERT_TRUE(admitTcp(16000, kTcpMaxConnections));
    TEST_ASSERT_FALSE(admitTcp(16000, kTcpMaxConnections + 1));
    TEST_ASSERT_FALSE(admitTcp(kTcpAcceptHeap - 1, 1));
    TEST_ASSERT_TRUE(kTcpMaxConnections > Admission::kMaxConnections + 1);   // room for 503 answers
}

// OTA pull: a running pull does not make web requests busy; a web upload still does ... but a request that changes
// something (settings, actions, restore) waits: no scaler I2C and no LittleFS writes while the Updater writes flash
void test_pull_does_not_block_requests(void) {
    TEST_ASSERT_FALSE(admitUpdateBusy(US_NONE, false));
    TEST_ASSERT_FALSE(admitUpdateBusy(US_PULL, true));
    TEST_ASSERT_TRUE(admitUpdateBusy(US_PULL, false));
    TEST_ASSERT_TRUE(admitUpdateBusy(US_UPLOAD, true));
    Admission a;
    TEST_ASSERT_EQUAL_UINT8(ADMIT, a.decide(20000, 8000, 1, admitUpdateBusy(US_PULL, true), false));
    TEST_ASSERT_EQUAL_UINT8(BUSY_UPDATE, a.decide(20000, 8000, 1, admitUpdateBusy(US_PULL, false), false));
    TEST_ASSERT_EQUAL_UINT8(BUSY_UPDATE, a.decide(20000, 8000, 1, admitUpdateBusy(US_UPLOAD, true), false));
}

// The API answers about 1.5 s before LittleFS is mounted (an empty backup with HTTP 200 came right after a start):
// until the start is over every request waits, before memory, load or update are even looked at
void test_starting_refuses_every_request(void) {
    Admission a;
    TEST_ASSERT_EQUAL_UINT8(BUSY_STARTING, a.decide(20000, 8000, 1, false, false, true));
    TEST_ASSERT_EQUAL_UINT8(BUSY_STARTING, a.decide(20000, 8000, 1, false, true, true));    // an upload too
    TEST_ASSERT_EQUAL_UINT8(BUSY_STARTING, a.decide(20000, 8000, 1, true, false, true));
    TEST_ASSERT_EQUAL_UINT8(ADMIT, a.decide(20000, 8000, 1, false, false, false));
    TEST_ASSERT_FALSE(a.low());                                                              // no memory state
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_starting_refuses_every_request);
    RUN_TEST(test_admits_up_to_the_connection_limit);
    RUN_TEST(test_low_memory_with_hysteresis);
    RUN_TEST(test_update_upload_is_exclusive);
    RUN_TEST(test_update_upload_needs_room);
    RUN_TEST(test_fragmented_but_roomy_heap_leaves_the_low_state);
    RUN_TEST(test_accept_guard);
    RUN_TEST(test_pull_does_not_block_requests);
    return UNITY_END();
}
