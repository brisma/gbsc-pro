#include <string.h>
#include <unity.h>
#include "api-route-match.h"
#include "regmap-status.h"

void setUp(void) {}
void tearDown(void) {}

// The real tables: the device router builds its PROGMEM rows from the same X-macro (handlers ignored here)
#define KEY(methods, match, path, onRequest, onUpload, arg) {path, methods, match},
static const ApiRouteKey kNormal[] = {API_ROUTES_NORMAL(KEY)};
static const ApiRouteKey kSafe[] = {API_ROUTES_SAFE(KEY)};
#undef KEY
static const uint8_t kN = sizeof(kNormal) / sizeof(kNormal[0]);
static const uint8_t kS = sizeof(kSafe) / sizeof(kSafe[0]);

static int find(uint8_t method, const char* path) {
    return apiRouteFind(kNormal, sizeof(ApiRouteKey), kN, method, path);
}
static const char* pathOf(int i) { return i < 0 ? "(none)" : kNormal[i].path; }

void test_exact_routes(void) {
    TEST_ASSERT_EQUAL_STRING("/", pathOf(find(ROUTE_GET, "/")));
    TEST_ASSERT_EQUAL_STRING("/api/info", pathOf(find(ROUTE_GET, "/api/info")));
    TEST_ASSERT_EQUAL_STRING("/api/log", pathOf(find(ROUTE_GET, "/api/log")));   // console log ring
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/log"));
    TEST_ASSERT_EQUAL_STRING("/api/settings", pathOf(find(ROUTE_POST, "/api/settings")));
    TEST_ASSERT_EQUAL_STRING("/api/profiles", pathOf(find(ROUTE_GET, "/api/profiles")));
    TEST_ASSERT_EQUAL_STRING("/api/profiles/load", pathOf(find(ROUTE_POST, "/api/profiles/load")));
    TEST_ASSERT_EQUAL_STRING("/api/profiles/import", pathOf(find(ROUTE_POST, "/api/profiles/import")));
}

void test_get_and_post_on_one_path_are_two_rows(void) {
    int g = find(ROUTE_GET, "/api/settings"), p = find(ROUTE_POST, "/api/settings");
    TEST_ASSERT_TRUE(g >= 0 && p >= 0);
    TEST_ASSERT_NOT_EQUAL(g, p);
}

void test_actions_prefix_and_list(void) {
    TEST_ASSERT_EQUAL_STRING("/api/actions", pathOf(find(ROUTE_GET, "/api/actions")));
    TEST_ASSERT_EQUAL_STRING("/api/actions/", pathOf(find(ROUTE_POST, "/api/actions/system.restart")));
    TEST_ASSERT_EQUAL_STRING("/api/actions/", pathOf(find(ROUTE_POST, "/api/actions/dev.menu_key")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/actions"));    // no name
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/actions/x"));   // actions run by POST only
}

void test_method_mismatch_and_unknown(void) {
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/info"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/system/update"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/nothing"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/infox"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/wifi/status"));   // merged into /api/info
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/profiles/nothing"));
    TEST_ASSERT_EQUAL_INT(-1, find(0, "/"));                          // HEAD/PUT/...: no route (404 as before)
    // Exact paths: the library's "P also matches P/..." is gone (no client used it)
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/info/"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/settings/x"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/system/update/x"));   // admission sees exact upload paths only
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/actions/"));         // prefix without a name
}

void test_first_match_in_table_order(void) {
    static const ApiRouteKey t[] = {{"/a/x", ROUTE_POST, ROUTE_EXACT}, {"/a/", ROUTE_POST, ROUTE_PREFIX}};
    TEST_ASSERT_EQUAL_INT(0, apiRouteFind(t, sizeof(ApiRouteKey), 2, ROUTE_POST, "/a/x"));
    TEST_ASSERT_EQUAL_INT(1, apiRouteFind(t, sizeof(ApiRouteKey), 2, ROUTE_POST, "/a/y"));
}

// Rows wider than the key (the device rows carry handlers): the stride skips them
void test_stride_skips_row_payload(void) {
    struct Row { ApiRouteKey key; void* payload[3]; };
    static const Row t[] = {{{"/one", ROUTE_GET, ROUTE_EXACT}, {}}, {{"/two", ROUTE_GET, ROUTE_EXACT}, {}}};
    TEST_ASSERT_EQUAL_INT(1, apiRouteFind(t, sizeof(Row), 2, ROUTE_GET, "/two"));
}

void test_no_duplicate_rows(void) {
    for (uint8_t i = 0; i < kN; i++)
        for (uint8_t j = i + 1; j < kN; j++)
            if (!strcmp(kNormal[i].path, kNormal[j].path)) TEST_ASSERT_EQUAL_UINT8(0, kNormal[i].methods & kNormal[j].methods);
}

void test_every_row_has_one_method(void) {
    for (uint8_t i = 0; i < kN; i++) TEST_ASSERT_TRUE(kNormal[i].methods == ROUTE_GET || kNormal[i].methods == ROUTE_POST);
    for (uint8_t i = 0; i < kS; i++) TEST_ASSERT_TRUE(kSafe[i].methods == ROUTE_GET || kSafe[i].methods == ROUTE_POST);
}

void test_safe_mode_table(void) {
    TEST_ASSERT_EQUAL_UINT8(6, kS);
    const char* const want[][2] = {{"G", "/"}, {"G", "/api/info"}, {"G", "/api/log"}, {"G", "/api/system/backup"},
                                   {"P", "/api/system/update"}, {"P", "/api/actions/system.restart"}};
    for (uint8_t k = 0; k < 6; k++) {
        uint8_t m = want[k][0][0] == 'G' ? ROUTE_GET : ROUTE_POST;
        TEST_ASSERT_TRUE(apiRouteFind(kSafe, sizeof(ApiRouteKey), kS, m, want[k][1]) >= 0);
    }
    TEST_ASSERT_EQUAL_INT(-1, apiRouteFind(kSafe, sizeof(ApiRouteKey), kS, ROUTE_POST, "/api/actions/dev.menu_key"));
}

// Load > Defaults and Manage > Replace with current for the web and the mobile app
#define ARG(methods, match, path, onRequest, onUpload, arg) arg,
static const uint8_t kArgs[] = {API_ROUTES_NORMAL(ARG)};
#undef ARG
void test_profile_defaults_and_replace_routes(void) {
    int d = find(ROUTE_POST, "/api/profiles/defaults"), r = find(ROUTE_POST, "/api/profiles/replace");
    TEST_ASSERT_EQUAL_STRING("/api/profiles/defaults", pathOf(d));
    TEST_ASSERT_EQUAL_STRING("/api/profiles/replace", pathOf(r));
    TEST_ASSERT_EQUAL_UINT8(PROFILE_ROUTE_DEFAULTS, kArgs[d]);
    TEST_ASSERT_EQUAL_UINT8(PROFILE_ROUTE_REPLACE, kArgs[r]);
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/profiles/defaults"));
}

void test_route_wifi_get(void) {
    TEST_ASSERT_EQUAL_STRING("/api/wifi", pathOf(find(ROUTE_GET, "/api/wifi")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/wifi"));
    TEST_ASSERT_EQUAL_STRING("/api/wifi/scan", pathOf(find(ROUTE_GET, "/api/wifi/scan")));   // exact paths: no clash
}

void test_route_events_get(void) {
    TEST_ASSERT_EQUAL_STRING("/api/events", pathOf(find(ROUTE_GET, "/api/events")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/events"));
}

void test_route_dev_regs(void) {
    TEST_ASSERT_EQUAL_STRING("/api/dev/regs", pathOf(find(ROUTE_GET, "/api/dev/regs")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/dev/regs"));
}

void test_route_adv_read(void) {
    TEST_ASSERT_EQUAL_STRING("/api/adv/read", pathOf(find(ROUTE_GET, "/api/adv/read")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/adv/read"));
}

void test_route_live(void) {
    TEST_ASSERT_EQUAL_STRING("/api/live", pathOf(find(ROUTE_GET, "/api/live")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/live"));
}

// The register map file: GET only, normal mode only; the answer from size, heap and ETag
void test_route_regmap(void) {
    TEST_ASSERT_EQUAL_STRING("/api/regmap", pathOf(find(ROUTE_GET, "/api/regmap")));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/regmap"));
    TEST_ASSERT_EQUAL_INT(-1, apiRouteFind(kSafe, sizeof(ApiRouteKey), kS, ROUTE_GET, "/api/regmap"));
    TEST_ASSERT_EQUAL_INT(200, regmapStatus(18000, 20000, false));
    TEST_ASSERT_EQUAL_INT(304, regmapStatus(18000, 20000, true));
    TEST_ASSERT_EQUAL_INT(503, regmapStatus(18000, 10 * 1024, false));   // free heap <= 10 KB
    TEST_ASSERT_EQUAL_INT(404, regmapStatus(0, 20000, false));           // built with GBSC_NO_REGMAP
}

// ADV presets and macros: exact paths, normal mode only
void test_route_adv_store(void) {
    const char* gets[] = {"/api/adv/presets", "/api/adv/macros", "/api/adv/preset", "/api/adv/macro"};
    for (const char* p : gets) {
        TEST_ASSERT_EQUAL_STRING(p, pathOf(find(ROUTE_GET, p)));
        TEST_ASSERT_EQUAL_INT(-1, apiRouteFind(kSafe, sizeof(ApiRouteKey), kS, ROUTE_GET, p));
    }
    const char* posts[] = {"/api/adv/preset", "/api/adv/macro", "/api/adv/preset/delete", "/api/adv/macro/delete"};
    for (const char* p : posts) TEST_ASSERT_EQUAL_STRING(p, pathOf(find(ROUTE_POST, p)));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/adv/presets"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_GET, "/api/adv/preset/delete"));
    TEST_ASSERT_EQUAL_INT(-1, find(ROUTE_POST, "/api/adv/preset/x"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_profile_defaults_and_replace_routes);
    RUN_TEST(test_exact_routes);
    RUN_TEST(test_get_and_post_on_one_path_are_two_rows);
    RUN_TEST(test_actions_prefix_and_list);
    RUN_TEST(test_method_mismatch_and_unknown);
    RUN_TEST(test_first_match_in_table_order);
    RUN_TEST(test_stride_skips_row_payload);
    RUN_TEST(test_no_duplicate_rows);
    RUN_TEST(test_every_row_has_one_method);
    RUN_TEST(test_safe_mode_table);
    RUN_TEST(test_route_wifi_get);
    RUN_TEST(test_route_events_get);
    RUN_TEST(test_route_dev_regs);
    RUN_TEST(test_route_adv_read);
    RUN_TEST(test_route_live);
    RUN_TEST(test_route_regmap);
    RUN_TEST(test_route_adv_store);
    return UNITY_END();
}
