#include <string.h>
#include <string>
#include <unity.h>
#include "ota-plan.h"
#include "menu-charset.h"

void setUp(void) {}
void tearDown(void) {}

static Manifest man(const char* v) {
    Manifest m;
    memset(&m, 0, sizeof(m));
    strcpy(m.version, v);
    m.size = 1000;
    snprintf(m.file, sizeof(m.file), "fw/gbsc-pro-%s.bin.gz", v);
    return m;
}

void test_channel_names(void) {
    TEST_ASSERT_EQUAL_STRING("beta", otaChannelName(OTA_BETA));
    TEST_ASSERT_EQUAL_STRING("Stable", otaChannelLabel(OTA_STABLE));
    TEST_ASSERT_EQUAL_INT(OTA_BETA, otaChannelParse("beta"));
    TEST_ASSERT_EQUAL_INT(-1, otaChannelParse("dev"));      // two channels: Stable and Beta
    TEST_ASSERT_EQUAL_INT(-1, otaChannelParse("Beta"));
    TEST_ASSERT_EQUAL_UINT8(2, OTA_CHANNELS);
    TEST_ASSERT_EQUAL_INT(-1, otaChannelParse(""));
}

// The build channel and every more stable one, build channel first
void test_channels_to_read(void) {
    uint8_t c[OTA_CHANNELS];
    TEST_ASSERT_EQUAL_UINT8(1, otaChannelsToRead(OTA_STABLE, c));
    TEST_ASSERT_EQUAL_UINT8(OTA_STABLE, c[0]);
    TEST_ASSERT_EQUAL_UINT8(2, otaChannelsToRead(OTA_BETA, c));
    TEST_ASSERT_EQUAL_UINT8(OTA_BETA, c[0]);
    TEST_ASSERT_EQUAL_UINT8(OTA_STABLE, c[1]);
}

void test_urls(void) {
    char u[96];
    TEST_ASSERT_TRUE(otaManifestUrl("http://gbsc.brisma.it/v1/", OTA_BETA, u, sizeof(u)));
    TEST_ASSERT_EQUAL_STRING("http://gbsc.brisma.it/v1/beta/manifest.txt", u);
    TEST_ASSERT_TRUE(otaImageUrl("http://gbsc.brisma.it/v1/", "fw/gbsc-pro-3.1.0.bin.gz", u, sizeof(u)));
    TEST_ASSERT_EQUAL_STRING("http://gbsc.brisma.it/v1/fw/gbsc-pro-3.1.0.bin.gz", u);
    char small[20];
    TEST_ASSERT_FALSE(otaManifestUrl("http://gbsc.brisma.it/v1/", OTA_BETA, small, sizeof(small)));
    TEST_ASSERT_FALSE(otaManifestUrl("http://gbsc.brisma.it/v1/", 7, u, sizeof(u)));
}

static OtaOffer recommend(const char* running, uint8_t build, const char* const* versions, const uint8_t* channels,
                          uint8_t n) {
    OtaOffer best = {};
    for (uint8_t i = 0; i < n; i++) if (versions[i]) otaConsider(best, channels[i], man(versions[i]));
    otaFinishRecommended(best, running, build);
    return best;
}

// A newer Stable beats the Beta builds: users drift towards Stable, the row names the move
void test_recommend_newer_stable_wins(void) {
    const char* v[] = {"3.1.0-beta7", "3.1.0"};
    const uint8_t c[] = {OTA_BETA, OTA_STABLE};
    OtaOffer o = recommend("3.1.0-beta7", OTA_BETA, v, c, 2);
    TEST_ASSERT_TRUE(o.any);
    TEST_ASSERT_EQUAL_UINT8(OTA_STABLE, o.channel);
    TEST_ASSERT_EQUAL_STRING("3.1.0", o.m.version);
    TEST_ASSERT_TRUE(o.moves);
    TEST_ASSERT_FALSE(o.older);
    const char* v2[] = {"3.1.0-beta8", "3.0.0"};
    o = recommend("3.1.0-beta7", OTA_BETA, v2, c, 2);
    TEST_ASSERT_TRUE(o.any);
    TEST_ASSERT_EQUAL_UINT8(OTA_BETA, o.channel);
    TEST_ASSERT_FALSE(o.moves);
}

void test_recommend_never_older(void) {
    const char* v[] = {"3.1.0-beta3", "3.0.0"};
    const uint8_t c[] = {OTA_BETA, OTA_STABLE};
    TEST_ASSERT_FALSE(recommend("3.1.0-beta3", OTA_BETA, v, c, 2).any);   // same and older only
}

// A local build (X.Y.Z-dev0, Beta channel) sorts below every release of its X.Y.Z: it is offered the Beta
void test_recommend_beta_over_local_build(void) {
    const char* v[] = {"3.1.0-beta2", "3.0.0"};
    const uint8_t c[] = {OTA_BETA, OTA_STABLE};
    OtaOffer o = recommend("3.1.0-dev0", OTA_BETA, v, c, 2);
    TEST_ASSERT_TRUE(o.any);
    TEST_ASSERT_EQUAL_UINT8(OTA_BETA, o.channel);
    TEST_ASSERT_EQUAL_STRING("3.1.0-beta2", o.m.version);
    TEST_ASSERT_FALSE(o.moves);
}

void test_recommend_skips_failed_channel(void) {
    const char* v[] = {nullptr, "3.1.0"};
    const uint8_t c[] = {OTA_BETA, OTA_STABLE};
    OtaOffer o = recommend("3.0.0", OTA_BETA, v, c, 2);
    TEST_ASSERT_TRUE(o.any);
    TEST_ASSERT_EQUAL_STRING("3.1.0", o.m.version);
}

// Other channels: any different build, flagged
void test_manual_older_flag(void) {
    OtaOffer o = otaManual(OTA_STABLE, man("3.0.0"), "3.1.0-beta4", OTA_BETA);
    TEST_ASSERT_TRUE(o.any);
    TEST_ASSERT_TRUE(o.older);
    TEST_ASSERT_TRUE(o.moves);
    TEST_ASSERT_EQUAL_STRING("fw/gbsc-pro-3.0.0.bin.gz", o.m.file);
    o = otaManual(OTA_BETA, man("3.1.0-beta5"), "3.1.0-beta4", OTA_BETA);
    TEST_ASSERT_TRUE(o.any);
    TEST_ASSERT_FALSE(o.older);
    TEST_ASSERT_FALSE(o.moves);
}

void test_manual_same_version_none(void) {
    TEST_ASSERT_FALSE(otaManual(OTA_BETA, man("3.1.0-beta2"), "3.1.0-beta2", OTA_BETA).any);
}

// Daily check: 2 min after boot, then 24 h; a check that could not run retries after 10 min, doubling to 2 h
void test_auto_clock(void) {
    OtaAutoClock c;
    TEST_ASSERT_FALSE(c.due(119999, true));
    TEST_ASSERT_TRUE(c.due(120000, true));
    TEST_ASSERT_FALSE(c.due(120000, false));
    c.ran(120000, true);
    TEST_ASSERT_FALSE(c.due(120000 + 86399999UL, true));
    TEST_ASSERT_TRUE(c.due(120000 + 86400000UL, true));
    uint32_t now = 1000000;
    const uint32_t waits[] = {600000, 1200000, 2400000, 4800000, 7200000, 7200000};
    for (uint32_t w : waits) {
        c.ran(now, false);
        TEST_ASSERT_FALSE(c.due(now + w - 1, true));
        TEST_ASSERT_TRUE(c.due(now + w, true));
        now += w;
    }
    c.ran(now, true);                                   // success resets the back-off
    c.ran(now, false);
    TEST_ASSERT_TRUE(c.due(now + 600000, true));
    OtaAutoClock w;                                     // millis() wrap
    w.ran(0xFFFFFF00u, false);
    TEST_ASSERT_FALSE(w.due(0xFFFFFF00u + 599999u, true));
    TEST_ASSERT_TRUE(w.due(0xFFFFFF00u + 600000u, true));
    // Off for more than 24.9 days, then turned on: the check comes 2 min later, not ~25 days later
    OtaAutoClock off;
    TEST_ASSERT_FALSE(off.due(0x90000000u, false));
    TEST_ASSERT_FALSE(off.due(0x90000000u + 119999u, true));
    TEST_ASSERT_TRUE(off.due(0x90000000u + 120000u, true));
}

// The pull sequencer: 5 s to resolve and connect, 10 s without data, closed before the end = cut off
void test_pull_timeouts(void) {
    OtaPull p;
    p.start(1000);
    TEST_ASSERT_EQUAL_UINT8(OS_RESOLVE, p.step(1100, false, false, false, false, 0));
    TEST_ASSERT_EQUAL_UINT8(OS_CONNECT, p.step(1200, true, false, false, false, 0));
    TEST_ASSERT_EQUAL_UINT8(OS_FAILED, p.step(6000, true, false, false, false, 0));
    TEST_ASSERT_EQUAL_STRING("timeout", p.reason());

    p.start(0);
    p.step(10, true, true, false, false, 0);
    TEST_ASSERT_EQUAL_UINT8(OS_BODY, p.step(20, true, true, true, false, 0));
    TEST_ASSERT_EQUAL_UINT8(OS_BODY, p.step(10019, true, true, false, false, 0));
    TEST_ASSERT_EQUAL_UINT8(OS_FAILED, p.step(10020, true, true, false, false, 0));
    TEST_ASSERT_EQUAL_STRING("timeout", p.reason());

    p.start(0);
    p.step(10, true, true, true, false, 0);
    TEST_ASSERT_EQUAL_UINT8(OS_FAILED, p.step(20, true, true, false, true, 0));
    TEST_ASSERT_EQUAL_STRING("short", p.reason());

    p.start(0);
    p.step(10, true, true, true, false, 0);
    TEST_ASSERT_EQUAL_UINT8(OS_DONE, p.step(20, true, true, true, true, 1));
    p.start(0);
    p.step(10, true, true, true, false, 0);
    TEST_ASSERT_EQUAL_UINT8(OS_FAILED, p.step(20, true, true, true, false, -1));
    TEST_ASSERT_EQUAL_STRING("http", p.reason());
    p.start(0);
    TEST_ASSERT_EQUAL_UINT8(OS_FAILED, p.step(5000, false, false, false, false, 0));   // DNS never answered
    TEST_ASSERT_EQUAL_STRING("timeout", p.reason());
}

void test_pull_verdicts(void) {
    uint8_t a[32] = {1}, b[32] = {2};
    TEST_ASSERT_NULL(otaVerdict(1000, 1000, a, a, true));
    TEST_ASSERT_EQUAL_STRING("format", otaVerdict(1000, 1000, a, a, false));   // an HTML page is no gzip image
    TEST_ASSERT_EQUAL_STRING("size", otaVerdict(999, 1000, a, a, true));
    TEST_ASSERT_EQUAL_STRING("sha", otaVerdict(1000, 1000, a, b, true));
}

void test_reason_words_rom_safe(void) {
    const char* codes[] = {"no_wifi", "dns", "connect", "timeout", "short", "http", "manifest", "size", "sha", "format",
                           "memory", "busy", "space", "write", "changed", "something_new"};
    for (const char* c : codes) {
        const char* w = otaReasonWords(c);
        TEST_ASSERT_NOT_NULL(w);
        TEST_ASSERT_TRUE_MESSAGE(strlen(w) > 0 && strlen(w) <= 14, c);   // "Check failed: " + 14 = 28
        for (const char* p = w; *p; p++) TEST_ASSERT_TRUE_MESSAGE(menuCharOk(*p), c);
    }
    TEST_ASSERT_EQUAL_STRING("no Wi-Fi", otaReasonWords("no_wifi"));
    TEST_ASSERT_EQUAL_STRING("offer changed", otaReasonWords("changed"));   // install refusals reach the TV too
    char t[64];   // wider than a toast: a 29-byte buffer truncated the text before the length check
    otaInstallFailText("busy", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Not installed: busy", t);
    for (const char* c : codes) {
        otaInstallFailText(c, t, sizeof(t));
        TEST_ASSERT_TRUE_MESSAGE(strlen(t) <= 28, t);                       // a toast holds 28
        for (const char* p = t; *p; p++) TEST_ASSERT_TRUE(menuCharOk(*p));
        snprintf(t, sizeof(t), "Check failed: %s", otaReasonWords(c));     // ota-update.cpp's check toast
        TEST_ASSERT_TRUE_MESSAGE(strlen(t) <= 28, t);
    }
}

// Menu texts of an offer: OSD ROM characters, the 24-character row value, the 20-character confirm
void test_offer_texts_rom_safe(void) {
    for (uint8_t ch = 0; ch < OTA_CHANNELS; ch++)
        for (int older = 0; older < 2; older++)
            for (int moves = 0; moves < 2; moves++) {
                OtaOffer o = {};
                o.any = true; o.channel = ch; o.older = older; o.moves = moves;
                strcpy(o.m.version, "10.10.10-beta9");
                char t[32], c[32];
                otaOfferText(o, t, sizeof(t));
                otaConfirmText(o, c, sizeof(c));
                TEST_ASSERT_TRUE(strlen(t) > 0 && strlen(t) <= 24);
                TEST_ASSERT_TRUE(strlen(c) <= 28);
                for (const char* p = t; *p; p++) TEST_ASSERT_TRUE(menuCharOk(*p));
                for (const char* p = c; *p; p++) TEST_ASSERT_TRUE(menuCharOk(*p));
                TEST_ASSERT_TRUE(!*c || c[strlen(c) - 1] == '?');
            }
    OtaOffer o = {};
    o.any = true; o.channel = OTA_STABLE; o.moves = true;
    char c[32];
    otaConfirmText(o, c, sizeof(c));
    TEST_ASSERT_EQUAL_STRING("Move to the Stable channel?", c);
    o.older = true;
    otaConfirmText(o, c, sizeof(c));
    TEST_ASSERT_EQUAL_STRING("Install an older version?", c);
    o.older = o.moves = false;
    otaConfirmText(o, c, sizeof(c));
    TEST_ASSERT_EQUAL_STRING("", c);                   // the node's own question ("Install the update now?")
    char t[32];
    o.channel = OTA_BETA; o.older = true; o.moves = true;
    otaOfferText(o, t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Beta older switch", t);
}

// The daily check never puts "Up to date" or a failure on the TV: only a found update
void test_auto_check_quiet(void) {
    TEST_ASSERT_TRUE(otaToastWanted(true, false));
    TEST_ASSERT_TRUE(otaToastWanted(true, true));
    TEST_ASSERT_TRUE(otaToastWanted(false, true));
    TEST_ASSERT_FALSE(otaToastWanted(false, false));
}

// ESPAsyncTCP sets the client's pcb only on connect: a client that has not reported connect or an error must not be
// deleted (lwIP still holds it as the pcb argument). It waits as a zombie until its connect or error arrives
void test_client_life_never_frees_a_connecting_client(void) {
    OtaClientLife c;
    c.created();
    TEST_ASSERT_FALSE(c.finish());                     // the connect timeout fired before any answer
    TEST_ASSERT_TRUE(c.zombie());
    TEST_ASSERT_FALSE(c.deletable());
    c.ended();                                         // the late error (SYN retries ran out)
    TEST_ASSERT_TRUE(c.deletable());
    c.deleted();
    TEST_ASSERT_FALSE(c.zombie());

    c.created();
    c.connected();
    TEST_ASSERT_TRUE(c.finish());                      // connected: close + delete at once
    c.deleted();

    c.created();
    TEST_ASSERT_FALSE(c.finish());
    c.connected();                                     // a late connect of a zombie: the caller closes it
    TEST_ASSERT_TRUE(c.deletable());
    c.deleted();

    c.created();
    c.ended();                                         // refused at once
    TEST_ASSERT_TRUE(c.finish());
}

// A check that read no manifest at all (an outage on the daily check) keeps the offer it had
void test_failed_check_keeps_the_offer(void) {
    OtaOffer prev = {};
    prev.any = true;
    strcpy(prev.m.version, "3.1.0");
    OtaOffer fresh = {};
    TEST_ASSERT_TRUE(otaAfterCheck(prev, fresh, 0).any);
    TEST_ASSERT_EQUAL_STRING("3.1.0", otaAfterCheck(prev, fresh, 0).m.version);
    TEST_ASSERT_FALSE(otaAfterCheck(prev, fresh, 1).any);   // read and nothing newer: the offer goes
}

// ESPAsyncTCP's AsyncClient::connect() loses its new pcb when lwIP's tcp_connect() fails (no route, no memory for
// the SYN): the fetch rules both out before connecting
void test_connect_precheck(void) {
    TEST_ASSERT_NULL(otaConnectBlocked(true, OTA_CONNECT_MIN_BLOCK));
    TEST_ASSERT_EQUAL_STRING("connect", otaConnectBlocked(false, 40000));
    TEST_ASSERT_EQUAL_STRING("memory", otaConnectBlocked(true, OTA_CONNECT_MIN_BLOCK - 1));
    TEST_ASSERT_EQUAL_STRING("connect", otaConnectBlocked(false, 0));    // no route is named first
    TEST_ASSERT_EQUAL_STRING("no connection", otaReasonWords(otaConnectBlocked(false, 40000)));
    TEST_ASSERT_EQUAL_STRING("low memory", otaReasonWords(otaConnectBlocked(true, 0)));
}

// System > Update > Last result on the OSD/OLED says what the web app says, in words (not "none" or "error:http")
void test_last_result_text(void) {
    char t[24];
    otaLastResultText("", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Not checked", t);
    otaLastResultText("none", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Up to date", t);
    otaLastResultText("3.0.1-beta2", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("3.0.1-beta2", t);           // an offer: its version
    otaLastResultText("error:http", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Server error", t);
    otaLastResultText("error:no_wifi", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("No Wi-Fi", t);
    otaLastResultText("error:something_new", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Error", t);
    const char* codes[] = {"no_wifi", "dns", "connect", "timeout", "short", "http", "manifest", "size", "sha", "format",
                           "memory", "busy", "space", "write", "changed"};
    for (const char* c : codes) {                          // fits beside the label: at most 14, ROM characters
        char last[24];
        snprintf(last, sizeof(last), "error:%s", c);
        otaLastResultText(last, t, sizeof(t));
        TEST_ASSERT_TRUE_MESSAGE(strlen(t) > 0 && strlen(t) <= 14, c);
        for (const char* p = t; *p; p++) TEST_ASSERT_TRUE_MESSAGE(menuCharOk(*p), c);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_channel_names);
    RUN_TEST(test_channels_to_read);
    RUN_TEST(test_urls);
    RUN_TEST(test_recommend_newer_stable_wins);
    RUN_TEST(test_recommend_never_older);
    RUN_TEST(test_recommend_beta_over_local_build);
    RUN_TEST(test_recommend_skips_failed_channel);
    RUN_TEST(test_manual_older_flag);
    RUN_TEST(test_manual_same_version_none);
    RUN_TEST(test_auto_clock);
    RUN_TEST(test_pull_timeouts);
    RUN_TEST(test_pull_verdicts);
    RUN_TEST(test_reason_words_rom_safe);
    RUN_TEST(test_offer_texts_rom_safe);
    RUN_TEST(test_auto_check_quiet);
    RUN_TEST(test_client_life_never_frees_a_connecting_client);
    RUN_TEST(test_failed_check_keeps_the_offer);
    RUN_TEST(test_connect_precheck);
    RUN_TEST(test_last_result_text);
    return UNITY_END();
}
