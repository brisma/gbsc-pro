#include <unity.h>
#include <string.h>
#include "adv-link.h"
#include "adv-link-sched.h"
#include "adv-state.h"

void setUp(void) {}
void tearDown(void) {}

static AdvState st(uint8_t input, uint8_t bri) {
    AdvState s; memset(&s, 0, sizeof(s)); s.version = ADV_STATE_VERSION; s.input = input; s.brightness = bri; return s;
}

void test_first_tick_sends(void) {
    AdvLinkSched k;
    TEST_ASSERT_TRUE(k.tick(st(1, 128), 1000));
    TEST_ASSERT_FALSE(k.tick(st(1, 128), 1010));
}

void test_change_coalesced_20ms(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    TEST_ASSERT_FALSE(k.tick(st(1, 129), 2000));
    TEST_ASSERT_FALSE(k.tick(st(1, 129), 2010));
    TEST_ASSERT_TRUE(k.tick(st(1, 129), 2020));
    TEST_ASSERT_FALSE(k.tick(st(1, 129), 2030));
}

void test_burst_sends_once_after_the_last_change(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    int sends = 0;
    for (uint32_t t = 2000, v = 129; t < 2060; t += 10, v++) sends += k.tick(st(1, (uint8_t)v), t);
    TEST_ASSERT_EQUAL_INT(0, sends);
    TEST_ASSERT_TRUE(k.tick(st(1, 134), 2070));
}

void test_slider_drag_has_max_latency(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    bool sent = false;
    uint32_t t = 2000;
    for (uint8_t v = 129; v < 160 && !sent; v++, t += 10) sent = k.tick(st(1, v), t);
    TEST_ASSERT_TRUE(sent);
    TEST_ASSERT_TRUE(t - 2000 <= 110);
}

void test_kick_sends_at_once(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    k.kick();
    TEST_ASSERT_TRUE(k.tick(st(5, 128), 2000));
}

void test_keep_alive(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    TEST_ASSERT_FALSE(k.tick(st(1, 128), 5999));
    TEST_ASSERT_TRUE(k.tick(st(1, 128), 6000));
    TEST_ASSERT_FALSE(k.tick(st(1, 128), 6001));
}

void test_request_quiet_after_input_change(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    TEST_ASSERT_TRUE(k.requestAllowed(1500));
    k.kick();
    k.tick(st(3, 128), 2000);
    TEST_ASSERT_FALSE(k.requestAllowed(2100));
    TEST_ASSERT_TRUE(k.requestAllowed(2400));
    k.tick(st(3, 140), 3000); k.tick(st(3, 140), 3020);
    TEST_ASSERT_TRUE(k.requestAllowed(3030));     // brightness only: no quiet window
}

void test_seq_increments(void) {
    AdvLinkSched k;
    uint8_t a = k.nextSeq(), b = k.nextSeq();
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(a + 1), b);
}

// Decoder levels 0..100: 50 = the register's neutral 128, 100 = its top 254
void test_adv_levels_map(void) {
    userOptions u; memset(&u, 0, sizeof(u));
    AdvState s;
    const uint8_t in[7] = {0, 1, 25, 50, 51, 75, 100}, out[7] = {0, 2, 64, 128, 130, 191, 254};
    for (uint8_t i = 0; i < 7; i++) {
        u.advBrightness = u.advContrast = u.advSaturation = u.advHue = in[i];
        advStateFromOptions(u, false, false, s);
        TEST_ASSERT_EQUAL_UINT8(out[i], s.brightness);
        TEST_ASSERT_EQUAL_UINT8(out[i], s.contrast);
        TEST_ASSERT_EQUAL_UINT8(out[i], s.saturation);
        TEST_ASSERT_EQUAL_UINT8(out[i], s.hue);
    }
    u.advBrightness = 200;                                           // out of range clamps to the top
    advStateFromOptions(u, false, false, s);
    TEST_ASSERT_EQUAL_UINT8(254, s.brightness);
}

void test_state_from_options_maps_every_field(void) {
    userOptions u; memset(&u, 0, sizeof(u));
    u.activeInputType = 5; u.advSyncStripper = 1; u.svVideoFormat = 2; u.avVideoFormat = 3;
    u.advI2P = 1; u.advSmooth = 1; u.advACE = 1;
    u.advACELumaGain = 11; u.advACEChromaGain = 12; u.advACEChromaMax = 13; u.advACEGammaGain = 14; u.advACEResponseSpeed = 9;
    u.advBrightness = 0; u.advContrast = 50; u.advSaturation = 100; u.advHue = 25;
    u.advFilterYShaping = 21; u.advFilterCShaping = 6; u.advFilterWYShaping = 17; u.advFilterWYOverride = 1;
    u.advFilterCombNTSC = 2; u.advFilterCombPAL = 3; u.advCombLumaModeNTSC = 4; u.advCombChromaModeNTSC = 5;
    u.advCombChromaTapsNTSC = 1; u.advCombLumaModePAL = 6; u.advCombChromaModePAL = 7; u.advCombChromaTapsPAL = 2;
    u.advDnr = 1; u.advCti = 1;                                      // issue #56
    AdvState s;
    advStateFromOptions(u, true, false, s);
    TEST_ASSERT_EQUAL_UINT8(ADV_STATE_VERSION, s.version);
    TEST_ASSERT_EQUAL_UINT8(5, s.input); TEST_ASSERT_EQUAL_UINT8(1, s.syncStripper); TEST_ASSERT_EQUAL_UINT8(1, s.termination);
    TEST_ASSERT_EQUAL_UINT8(2, s.formatSv); TEST_ASSERT_EQUAL_UINT8(3, s.formatAv);
    TEST_ASSERT_EQUAL_UINT8(1, s.i2p); TEST_ASSERT_EQUAL_UINT8(1, s.smooth); TEST_ASSERT_EQUAL_UINT8(1, s.ace);
    TEST_ASSERT_EQUAL_UINT8(11, s.aceLumaGain); TEST_ASSERT_EQUAL_UINT8(12, s.aceChromaGain);
    TEST_ASSERT_EQUAL_UINT8(13, s.aceChromaMax); TEST_ASSERT_EQUAL_UINT8(14, s.aceGammaGain);
    TEST_ASSERT_EQUAL_UINT8(9, s.aceResponseSpeed);
    TEST_ASSERT_EQUAL_UINT8(0, s.brightness); TEST_ASSERT_EQUAL_UINT8(128, s.contrast);   // levels 0..100
    TEST_ASSERT_EQUAL_UINT8(254, s.saturation); TEST_ASSERT_EQUAL_UINT8(64, s.hue);
    TEST_ASSERT_EQUAL_UINT8(21, s.filterY); TEST_ASSERT_EQUAL_UINT8(6, s.filterC); TEST_ASSERT_EQUAL_UINT8(17, s.filterWy);
    TEST_ASSERT_EQUAL_UINT8(1, s.filterWyOverride); TEST_ASSERT_EQUAL_UINT8(2, s.combNtsc); TEST_ASSERT_EQUAL_UINT8(3, s.combPal);
    TEST_ASSERT_EQUAL_UINT8(4, s.combLumaNtsc); TEST_ASSERT_EQUAL_UINT8(5, s.combChromaNtsc);
    TEST_ASSERT_EQUAL_UINT8(1, s.combTapsNtsc); TEST_ASSERT_EQUAL_UINT8(6, s.combLumaPal);
    TEST_ASSERT_EQUAL_UINT8(7, s.combChromaPal); TEST_ASSERT_EQUAL_UINT8(2, s.combTapsPal);
    TEST_ASSERT_EQUAL_UINT8(1, s.dnr); TEST_ASSERT_EQUAL_UINT8(1, s.cti);
}

void test_state_from_options_clamps(void) {
    userOptions u; memset(&u, 0, sizeof(u));
    u.activeInputType = 9; u.svVideoFormat = 14;
    AdvState s;
    advStateFromOptions(u, false, false, s);
    TEST_ASSERT_EQUAL_UINT8(1, s.input);
    TEST_ASSERT_EQUAL_UINT8(0, s.formatSv);
    uint8_t b[ADV_STATE_BYTES]; AdvState r;
    advStatePack(&s, b);
    TEST_ASSERT_TRUE(advStateUnpack(b, sizeof(b), &r));   // always a valid frame for the ADV Controller
}

void test_resend_rearms_the_quiet_window(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    TEST_ASSERT_TRUE(k.requestAllowed(2000));
    k.kickResend();
    TEST_ASSERT_TRUE(k.tick(st(1, 128), 2000));           // same state, sent again at once
    TEST_ASSERT_FALSE(k.requestAllowed(2100));             // the lost frame may have changed the input
    TEST_ASSERT_TRUE(k.requestAllowed(2400));
}

// An input change to S-Video or Composite runs the whole decoder init on the ADV Controller (link.c: halAdvInit,
// even from the other decoder input): ~750 ms without answers (measured: INFO and SIGNAL unanswered
// 440-731 ms after the STATE); a change to a TV5725 path keeps the short window
void test_decoder_init_gets_a_longer_quiet(void) {
    AdvLinkSched k; k.tick(st(1, 128), 1000);
    k.kick();
    k.tick(st(5, 128), 2000);                              // RGB SCART -> S-Video
    TEST_ASSERT_FALSE(k.requestAllowed(2400));
    TEST_ASSERT_FALSE(k.requestAllowed(2000 + AdvLinkSched::kDecoderQuietMs - 1));
    TEST_ASSERT_TRUE(k.requestAllowed(2000 + AdvLinkSched::kDecoderQuietMs));
    k.kick();
    k.tick(st(6, 128), 4000);                              // S-Video -> Composite: the init runs again
    TEST_ASSERT_FALSE(k.requestAllowed(4000 + AdvLinkSched::kInputQuietMs));
    TEST_ASSERT_TRUE(k.requestAllowed(4000 + AdvLinkSched::kDecoderQuietMs));
    k.kick();
    k.tick(st(4, 128), 6000);                              // Composite -> Component: decoder off, short window
    TEST_ASSERT_TRUE(k.requestAllowed(6000 + AdvLinkSched::kInputQuietMs));
    AdvLinkSched b; b.tick(st(6, 128), 0);                 // boot on Composite: powered up too
    TEST_ASSERT_FALSE(b.requestAllowed(AdvLinkSched::kInputQuietMs));
    TEST_ASSERT_TRUE(b.requestAllowed(AdvLinkSched::kDecoderQuietMs));
}

// Listen: only on RGB SCART; the decoder init it starts gets the decoder quiet window,
// RGB SCART listening -> Composite keeps the decoder (no init: no decoder quiet)
void test_state_carries_listen_only_on_rgbs(void) {
    userOptions u; memset(&u, 0, sizeof(u));
    AdvState s;
    u.activeInputType = 1;
    advStateFromOptions(u, false, true, s);
    TEST_ASSERT_EQUAL_UINT8(1, s.listen); TEST_ASSERT_EQUAL_UINT8(1, s.input);
    advStateFromOptions(u, false, false, s);
    TEST_ASSERT_EQUAL_UINT8(0, s.listen);
    u.activeInputType = 2;                                           // RGB SoG: never (no false sync on green)
    advStateFromOptions(u, false, true, s);
    TEST_ASSERT_EQUAL_UINT8(0, s.listen);
    u.activeInputType = 6;
    advStateFromOptions(u, false, true, s);
    TEST_ASSERT_EQUAL_UINT8(0, s.listen);
}

void test_sched_quiet_on_decoder_init(void) {
    AdvLinkSched k; AdvState rgbs = st(1, 128), listen = rgbs; listen.listen = 1;
    k.tick(rgbs, 1000);
    TEST_ASSERT_TRUE(k.requestAllowed(1500));
    k.kick(); TEST_ASSERT_TRUE(k.tick(listen, 2000));                // listen on: the ADV Controller runs ADV_Init
    TEST_ASSERT_FALSE(k.requestAllowed(2000 + AdvLinkSched::kDecoderQuietMs - 1));
    TEST_ASSERT_TRUE(k.requestAllowed(2000 + AdvLinkSched::kDecoderQuietMs));
    k.kick(); TEST_ASSERT_TRUE(k.tick(st(6, 128), 4000));           // to Composite: switches only
    TEST_ASSERT_TRUE(k.requestAllowed(4000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_tick_sends);
    RUN_TEST(test_change_coalesced_20ms);
    RUN_TEST(test_burst_sends_once_after_the_last_change);
    RUN_TEST(test_slider_drag_has_max_latency);
    RUN_TEST(test_kick_sends_at_once);
    RUN_TEST(test_keep_alive);
    RUN_TEST(test_request_quiet_after_input_change);
    RUN_TEST(test_seq_increments);
    RUN_TEST(test_state_from_options_maps_every_field);
    RUN_TEST(test_state_from_options_clamps);
    RUN_TEST(test_resend_rearms_the_quiet_window);
    RUN_TEST(test_decoder_init_gets_a_longer_quiet);
    RUN_TEST(test_adv_levels_map);
    RUN_TEST(test_state_carries_listen_only_on_rgbs);
    RUN_TEST(test_sched_quiet_on_decoder_init);
    return UNITY_END();
}
