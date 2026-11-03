// Channel guard: a router that announces a channel switch and stays where it is must not cost the link
#include <unity.h>
#include "channel-guard.h"

void setUp(void) {}
void tearDown(void) {}

static void expect(ChannelGuard& g, uint8_t radio, uint32_t now, uint8_t set, uint8_t event) {
    ChannelGuardStep s = g.tick(radio, now);
    TEST_ASSERT_EQUAL_UINT8(set, s.setChannel);
    TEST_ASSERT_EQUAL_UINT8(event, s.event);
}

void test_nothing_to_do_while_the_radio_stays_home(void) {
    ChannelGuard g;
    expect(g, 6, 0, 0, CG_NONE);                 // not connected: not the guard's business
    g.connected(2);
    for (uint32_t t = 0; t < 60000; t += 100) expect(g, 2, t, 0, CG_NONE);
    TEST_ASSERT_FALSE(g.distrust());
    TEST_ASSERT_EQUAL_UINT8(2, g.home());
}

// The router repeats "moving to 6" in every beacon and stays on 2: the SDK leaves again about a second after
// each return, which proves the router is still on 2
void test_false_announcement_is_caught_by_the_second_move(void) {
    ChannelGuard g;
    g.connected(2);
    expect(g, 6, 1000, 2, CG_PROBE);             // first move: back home, wait and see
    expect(g, 2, 1010, 0, CG_NONE);
    expect(g, 6, 2150, 2, CG_FALSE);             // the SDK heard the announcement on 2 again
    TEST_ASSERT_TRUE(g.distrust());
    expect(g, 2, 2160, 0, CG_NONE);
    expect(g, 6, 3300, 2, CG_NONE);              // from now on: silently back, counted
    expect(g, 6, 4450, 2, CG_NONE);
    TEST_ASSERT_EQUAL_UINT32(4, g.reverts());
    TEST_ASSERT_EQUAL_UINT8(2, g.home());
}

// The router really moved: nothing is heard on the old channel, so the SDK does not move a second time
void test_real_move_is_followed_after_the_probe(void) {
    ChannelGuard g;
    g.connected(2);
    expect(g, 6, 1000, 2, CG_PROBE);
    for (uint32_t t = 1010; t < 1000 + kChannelProbeMs; t += 100) expect(g, 2, t, 0, CG_NONE);
    expect(g, 2, 1000 + kChannelProbeMs, 6, CG_FOLLOWED);
    TEST_ASSERT_EQUAL_UINT8(6, g.home());
    expect(g, 6, 1100 + kChannelProbeMs, 0, CG_NONE);
    expect(g, 6, 60000, 0, CG_NONE);
    TEST_ASSERT_FALSE(g.distrust());
}

// A longer countdown than the probe: the guard follows, the link dies of a beacon timeout, the lesson stays
void test_beacon_timeout_after_following_means_false(void) {
    ChannelGuard g;
    g.connected(2);
    expect(g, 6, 1000, 2, CG_PROBE);
    expect(g, 2, 1000 + kChannelProbeMs, 6, CG_FOLLOWED);
    g.dropped(kWifiReasonBeaconTimeout, 1000 + kChannelProbeMs + 9000);
    TEST_ASSERT_TRUE(g.distrust());
    expect(g, 6, 20000, 0, CG_NONE);             // not connected
    g.connected(2);
    expect(g, 6, 24000, 2, CG_NONE);             // no probe any more
    TEST_ASSERT_EQUAL_UINT32(2, g.reverts());    // the probe's and this one
}

void test_late_or_other_drops_after_following_do_not_blame_the_router(void) {
    ChannelGuard g;
    g.connected(2);
    expect(g, 6, 1000, 2, CG_PROBE);
    expect(g, 2, 1000 + kChannelProbeMs, 6, CG_FOLLOWED);
    g.dropped(8, 1000 + kChannelProbeMs + 2000);   // the router sent the station away: not a beacon timeout
    TEST_ASSERT_FALSE(g.distrust());
    g.connected(6);
    expect(g, 6, 30000, 0, CG_NONE);
    g.dropped(kWifiReasonBeaconTimeout, 30000 + kChannelProvenMs + 1);   // no move before this one
    TEST_ASSERT_FALSE(g.distrust());
}

void test_a_drop_during_the_probe_starts_over(void) {
    ChannelGuard g;
    g.connected(2);
    expect(g, 6, 1000, 2, CG_PROBE);
    g.dropped(kWifiReasonBeaconTimeout, 1500);
    TEST_ASSERT_FALSE(g.distrust());
    g.connected(11);
    TEST_ASSERT_EQUAL_UINT8(11, g.home());
    expect(g, 11, 5000, 0, CG_NONE);
    expect(g, 2, 9000, 11, CG_PROBE);            // a fresh probe on the new connection
}

void test_the_clock_may_wrap(void) {
    ChannelGuard g;
    g.connected(2);
    uint32_t t = 0xFFFFFF00u;
    expect(g, 6, t, 2, CG_PROBE);
    expect(g, 2, t + 0x80, 0, CG_NONE);
    expect(g, 2, t + kChannelProbeMs, 6, CG_FOLLOWED);   // past the wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_to_do_while_the_radio_stays_home);
    RUN_TEST(test_false_announcement_is_caught_by_the_second_move);
    RUN_TEST(test_real_move_is_followed_after_the_probe);
    RUN_TEST(test_beacon_timeout_after_following_means_false);
    RUN_TEST(test_late_or_other_drops_after_following_do_not_blame_the_router);
    RUN_TEST(test_a_drop_during_the_probe_starts_over);
    RUN_TEST(test_the_clock_may_wrap);
    return UNITY_END();
}
