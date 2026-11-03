// Event ring for the Status history: the last 20 events in RAM
#include <unity.h>
#include <string.h>
#include <string>
#include "event-ring.h"

void setUp(void) {}
void tearDown(void) {}

static std::string allTypes(const EventRing& r, uint16_t since) {
    std::string out;
    Event e;
    while (r.readAfter(since, e)) { out += (char)('0' + e.type); since = e.seq; }
    return out;
}

void test_event_ring_keeps_last_20(void) {
    EventRing r;
    for (uint8_t i = 0; i < 25; i++) r.add(1000u + i, EV_INPUT_FOUND, i, 0);
    Event e;
    TEST_ASSERT_TRUE(r.readAfter(0, e));
    TEST_ASSERT_EQUAL_UINT16(6, e.seq);              // 1..5 dropped
    TEST_ASSERT_EQUAL_UINT8(5, e.a);
    TEST_ASSERT_EQUAL_UINT32(1005, e.ms);
    TEST_ASSERT_EQUAL_UINT16(25, r.lastSeq());
    TEST_ASSERT_EQUAL_UINT32(20, allTypes(r, 0).size());
    TEST_ASSERT_EQUAL_UINT8(20, EventRing::kCap);
    TEST_ASSERT_EQUAL_UINT32(12, sizeof(Event));
}

void test_event_ring_read_after(void) {
    EventRing r;
    Event e;
    TEST_ASSERT_FALSE(r.readAfter(0, e));            // empty
    r.add(10, EV_INPUT_FOUND, 3, 0);
    r.add(20, EV_PROFILE_LOADED, 4, 0);
    r.add(30, EV_INPUT_LOST, 3, 0);
    TEST_ASSERT_TRUE(r.readAfter(1, e));
    TEST_ASSERT_EQUAL_UINT16(2, e.seq);
    TEST_ASSERT_EQUAL_UINT8(EV_PROFILE_LOADED, e.type);
    TEST_ASSERT_FALSE(r.readAfter(3, e));            // up to date
    TEST_ASSERT_TRUE(r.readAfter(200, e));           // a since from before a restart: from the oldest
    TEST_ASSERT_EQUAL_UINT16(1, e.seq);
}

void test_event_ring_seq_wraps(void) {
    EventRing r;
    for (uint32_t i = 0; i < 65535u + 3; i++) r.add(i, EV_RESOLUTION, 0, 0);
    TEST_ASSERT_EQUAL_UINT16(3, r.lastSeq());        // 65535, then 1 (0 is never used: since=0 means everything)
    Event e;
    TEST_ASSERT_TRUE(r.readAfter(0, e));
    TEST_ASSERT_EQUAL_UINT16(65535u - 16, e.seq);    // the oldest of the 20 kept
    TEST_ASSERT_TRUE(r.readAfter(65535, e));
    TEST_ASSERT_EQUAL_UINT16(1, e.seq);
}

void test_event_json_names(void) {
    Event e = {7, 123456, EV_INPUT_FOUND, 1, 0};
    char buf[96];
    size_t n = eventJson(e, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"seq\":7,\"ms\":123456,\"type\":\"input_found\",\"a\":1,\"b\":0}", buf);
    TEST_ASSERT_EQUAL_UINT32(strlen(buf), n);
    const char* names[] = {"input_found", "input_lost", "input_changed", "profile_loaded", "profile_saved",
                           "resolution", "update_offered", "update_done"};
    for (uint8_t t = EV_INPUT_FOUND; t <= EV_UPDATE_DONE; t++) {
        e.type = t;
        eventJson(e, buf, sizeof(buf));
        TEST_ASSERT_TRUE_MESSAGE(strstr(buf, names[t - 1]) != nullptr, names[t - 1]);
    }
    e.type = 99;
    eventJson(e, buf, sizeof(buf));
    TEST_ASSERT_TRUE(strstr(buf, "\"type\":\"unknown\"") != nullptr);
    TEST_ASSERT_EQUAL_UINT32(0, eventJson(e, buf, 10));   // does not fit: nothing
}

// GET /api/events body: the boot id of the log ring and the events after since, oldest first
void test_events_json_body(void) {
    EventRing r;
    char buf[EVENTS_JSON_MAX];
    eventsJson(r, 0, 42, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"boot\":42,\"events\":[]}", buf);
    r.add(5, EV_INPUT_FOUND, 1, 0);
    r.add(9, EV_RESOLUTION, 3, 1);
    eventsJson(r, 1, 42, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"boot\":42,\"events\":[{\"seq\":2,\"ms\":9,\"type\":\"resolution\",\"a\":3,\"b\":1}]}", buf);
    for (uint8_t i = 0; i < 30; i++) r.add(0xFFFFFFFFu, EV_UPDATE_OFFERED, 255, 255);   // widest records fit
    size_t n = eventsJson(r, 0, 65535, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0 && n < sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('}', buf[n - 1]);
}

// The "resolution" event only when the preset or the input changed: a scanlines toggle or a re-init reloads the same
// preset and must not push input/profile events out of the 20 slots
void test_resolution_event_only_on_change(void) {
    uint16_t last = EVENT_RESOLUTION_NONE;
    TEST_ASSERT_TRUE(eventResolutionChanged(&last, 0x11, 1));
    TEST_ASSERT_FALSE(eventResolutionChanged(&last, 0x11, 1));    // same preset again (scanlines, resync)
    TEST_ASSERT_TRUE(eventResolutionChanged(&last, 0x12, 1));
    TEST_ASSERT_TRUE(eventResolutionChanged(&last, 0x12, 5));     // other input
    TEST_ASSERT_FALSE(eventResolutionChanged(&last, 0x12, 5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_event_ring_keeps_last_20);
    RUN_TEST(test_event_ring_read_after);
    RUN_TEST(test_event_ring_seq_wraps);
    RUN_TEST(test_event_json_names);
    RUN_TEST(test_events_json_body);
    RUN_TEST(test_resolution_event_only_on_change);
    return UNITY_END();
}
