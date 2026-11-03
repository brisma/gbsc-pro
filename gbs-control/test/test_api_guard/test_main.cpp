#include <string>
#include <unity.h>
#include "api-guard.h"

void setUp(void) {}
void tearDown(void) {}

static bool host(const char* h, const char* extra = "") { return guardHostAllowed(h, "gbscontrol", extra); }

void test_normalize(void) {
    char out[64];
    TEST_ASSERT_TRUE(guardNormalizeHost("GBSControl.Local.:80", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("gbscontrol.local", out);
    TEST_ASSERT_TRUE(guardNormalizeHost("http://192.168.4.1:81/x", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", out);
    TEST_ASSERT_FALSE(guardNormalizeHost("[::1]:80", out, sizeof(out)));
    TEST_ASSERT_FALSE(guardNormalizeHost("", out, sizeof(out)));
}

void test_ipv4_literals_allowed(void) {
    TEST_ASSERT_TRUE(host("192.168.1.50"));
    TEST_ASSERT_TRUE(host("192.168.1.50:80"));
    TEST_ASSERT_TRUE(host("10.0.0.1"));
    TEST_ASSERT_FALSE(host("192.168.1.256"));
    TEST_ASSERT_FALSE(host("192.168.1"));
    TEST_ASSERT_FALSE(host("1.2.3.4.5"));
}

void test_hostname_rules(void) {
    TEST_ASSERT_TRUE(host("gbscontrol"));
    TEST_ASSERT_TRUE(host("gbscontrol.local"));
    TEST_ASSERT_TRUE(host("GBSCONTROL.local."));
    TEST_ASSERT_FALSE(host("gbscontrol.evil.com"));
    TEST_ASSERT_FALSE(host("evil.com"));
    TEST_ASSERT_FALSE(host("gbscontrol.local.evil.com"));
}

void test_extra_hosts(void) {
    TEST_ASSERT_FALSE(host("gbscontrol.fritz.box"));
    TEST_ASSERT_TRUE(host("gbscontrol.fritz.box", "gbscontrol.fritz.box, gbs.lan"));
    TEST_ASSERT_TRUE(host("GBS.LAN:80", "gbscontrol.fritz.box, gbs.lan"));
    TEST_ASSERT_FALSE(host("lan", "gbscontrol.fritz.box, gbs.lan"));
}

void test_origin(void) {
    TEST_ASSERT_TRUE(guardOriginAllowed("http://gbscontrol.local", "gbscontrol", ""));
    TEST_ASSERT_TRUE(guardOriginAllowed("http://192.168.4.1", "gbscontrol", ""));
    TEST_ASSERT_FALSE(guardOriginAllowed("https://evil.com", "gbscontrol", ""));
    TEST_ASSERT_FALSE(guardOriginAllowed("null", "gbscontrol", ""));
}

// system.extra_hosts: comma-separated names the guard can match (no spaces, no IPv4, not localhost)
void test_extra_hosts_valid(void) {
    TEST_ASSERT_TRUE(guardExtraHostsValid("tv.local,gbs.lan"));
    TEST_ASSERT_TRUE(guardExtraHostsValid(""));
    TEST_ASSERT_TRUE(guardExtraHostsValid("TV.Local"));
    TEST_ASSERT_TRUE(guardExtraHostsValid("gbs-control.fritz.box"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("a b"));
    TEST_ASSERT_TRUE(guardExtraHostsValid("tv.lan, gbs.lan"));      // spaces around commas: the guard trims them
    TEST_ASSERT_FALSE(guardExtraHostsValid(" , tv.lan"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("10.0.0.5"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("tv.lan,10.0.0.5"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("localhost"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("LocalHost"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("tv.lan,"));
    TEST_ASSERT_FALSE(guardExtraHostsValid(",tv.lan"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("a,,b"));
    TEST_ASSERT_FALSE(guardExtraHostsValid("tv_lan"));
    std::string label(64, 'a');
    TEST_ASSERT_FALSE(guardExtraHostsValid(label.c_str()));
    std::string ok63(63, 'a');
    TEST_ASSERT_TRUE(guardExtraHostsValid(ok63.c_str()));
    std::string list;
    for (int i = 0; i < 13; i++) list += (i ? "," : "") + std::string("abcd");   // 64 chars
    TEST_ASSERT_FALSE(guardExtraHostsValid(list.c_str()));
}

// Writing a list that no longer contains the name the page was opened with locks that tab out: refused (409 in_use)
void test_host_lost_by_change(void) {
    TEST_ASSERT_TRUE(guardHostLostByChange("tv.lan", "gbscontrol", "tv.lan,gbs.lan", "gbs.lan"));
    TEST_ASSERT_FALSE(guardHostLostByChange("TV.LAN:80", "gbscontrol", "tv.lan", "gbs.lan,tv.lan"));
    TEST_ASSERT_FALSE(guardHostLostByChange("gbscontrol.local", "gbscontrol", "tv.lan", ""));   // hostname always passes
    TEST_ASSERT_FALSE(guardHostLostByChange("192.168.1.9", "gbscontrol", "tv.lan", ""));        // so does an IP
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_normalize);
    RUN_TEST(test_ipv4_literals_allowed);
    RUN_TEST(test_hostname_rules);
    RUN_TEST(test_extra_hosts);
    RUN_TEST(test_origin);
    RUN_TEST(test_extra_hosts_valid);
    RUN_TEST(test_host_lost_by_change);
    return UNITY_END();
}
