#include <string.h>
#include <string>
#include <unity.h>
#include "update-manifest.h"

void setUp(void) {}
void tearDown(void) {}

static const char kSha[] = "6fb116c15db854a68b0eb4f396b48bece9d51ec8ac8308aa4ef7b797e114dd7a";
static std::string body() {
    return std::string("version=3.1.0-beta2\nsize=654069\nsha256=") + kSha +
           "\nfile=fw/gbsc-pro-3.1.0-beta2.bin.gz\nnotes=https://github.com/brisma/gbsc-pro\n";
}
static bool parse(const std::string& t, Manifest& m) { return manifestParse(t.data(), t.size(), m); }

// Manifest v1: key=value lines in any order, file= required
void test_manifest_v1_any_order(void) {
    std::string t = std::string("file=fw/gbsc-pro-3.1.0.bin.gz\nsha256=") + kSha + "\nversion=3.1.0\nsize=654069\n";
    Manifest m;
    TEST_ASSERT_TRUE(parse(t, m));
    TEST_ASSERT_EQUAL_STRING("3.1.0", m.version);
    TEST_ASSERT_EQUAL_UINT32(654069, m.size);
    TEST_ASSERT_EQUAL_HEX8(0x6f, m.sha256[0]);
    TEST_ASSERT_EQUAL_HEX8(0x7a, m.sha256[31]);
    TEST_ASSERT_EQUAL_STRING("fw/gbsc-pro-3.1.0.bin.gz", m.file);
    TEST_ASSERT_EQUAL_STRING("", m.notes);
    TEST_ASSERT_TRUE(parse(body(), m));
    TEST_ASSERT_EQUAL_STRING("https://github.com/brisma/gbsc-pro", m.notes);
}

void test_manifest_unknown_keys_ignored(void) {
    Manifest m;
    TEST_ASSERT_TRUE(parse(body() + "channel=beta\nmin_from=3.0.0\n", m));
    TEST_ASSERT_EQUAL_STRING("3.1.0-beta2", m.version);
}

void test_manifest_repeated_key_rejected(void) {
    Manifest m;
    TEST_ASSERT_FALSE(parse(body() + "size=1\n", m));
    TEST_ASSERT_FALSE(parse(body() + "channel=a\nchannel=b\n", m));
}

void test_manifest_requires_file(void) {
    Manifest m;
    std::string t = std::string("version=3.1.0\nsize=1\nsha256=") + kSha + "\n";
    TEST_ASSERT_FALSE(parse(t, m));
    TEST_ASSERT_TRUE(parse(t + "file=fw/x.bin.gz\n", m));
    TEST_ASSERT_FALSE(parse(std::string("size=1\nsha256=") + kSha + "\nfile=fw/x\n", m));          // no version
    TEST_ASSERT_FALSE(parse("version=3.1.0\nsize=1\nfile=fw/x\n", m));                              // no sha256
    TEST_ASSERT_FALSE(parse(std::string("version=3.1.0\nsha256=") + kSha + "\nfile=fw/x\n", m));   // no size
}

static bool fileOk(const char* f) {
    Manifest m;
    return parse(std::string("version=3.1.0\nsize=1\nsha256=") + kSha + "\nfile=" + f + "\n", m);
}
void test_manifest_file_rules(void) {
    TEST_ASSERT_TRUE(fileOk("fw/gbsc-pro-3.1.0-beta2.bin.gz"));
    TEST_ASSERT_FALSE(fileOk("../x"));
    TEST_ASSERT_FALSE(fileOk("fw/../x"));
    TEST_ASSERT_FALSE(fileOk("fw/a b"));
    TEST_ASSERT_FALSE(fileOk("/fw/x"));
    TEST_ASSERT_FALSE(fileOk("fw/x?y"));
    TEST_ASSERT_TRUE(fileOk(std::string(63, 'a').c_str()));
    TEST_ASSERT_FALSE(fileOk(std::string(64, 'a').c_str()));
}

// A captive portal or a proxy answering 200 with a page: required keys missing, so it is no manifest
void test_manifest_html_rejected(void) {
    Manifest m;
    TEST_ASSERT_FALSE(parse("<!doctype html>\n<html><head><meta charset=utf-8>\n<title>Login</title></head></html>\n", m));
    TEST_ASSERT_FALSE(parse(body() + "<html>\n", m));                                // a line without '='
}

void test_manifest_blank_lines_and_no_final_lf(void) {
    Manifest m;
    std::string t = body();
    t.pop_back();                                                                     // no final LF
    TEST_ASSERT_TRUE(parse("\n" + t, m));
    TEST_ASSERT_TRUE(parse(body() + "\n\n", m));
}

void test_manifest_line_without_equals(void) {
    Manifest m;
    TEST_ASSERT_FALSE(parse(body() + "garbage\n", m));
    TEST_ASSERT_FALSE(parse(body() + "=x\n", m));                                    // empty key
    TEST_ASSERT_FALSE(parse(body() + "Bad-Key=1\n", m));                             // keys are [a-z0-9_]
}

void test_manifest_limits(void) {
    Manifest m;
    std::string base = std::string("size=1\nsha256=") + kSha + "\nfile=fw/x\n";
    TEST_ASSERT_TRUE(parse("version=" + std::string(15, '1') + "\n" + base, m));
    TEST_ASSERT_FALSE(parse("version=" + std::string(16, '1') + "\n" + base, m));
    TEST_ASSERT_FALSE(parse("version=3.0.0;rm\n" + base, m));
    TEST_ASSERT_TRUE(parse("version=3\n" + base + "notes=" + std::string(95, 'n') + "\n", m));
    TEST_ASSERT_FALSE(parse("version=3\n" + base + "notes=" + std::string(96, 'n') + "\n", m));
    TEST_ASSERT_FALSE(parse("version=3.0.0\r\n" + base, m));                        // CR
    TEST_ASSERT_FALSE(parse("version=3\nsize=1\nsha256=xyz\nfile=fw/x\n", m));
    TEST_ASSERT_FALSE(parse("version=3\nsize=0\nsha256=" + std::string(kSha) + "\nfile=fw/x\n", m));
    TEST_ASSERT_FALSE(parse(body() + "pad=" + std::string(1024, 'p') + "\n", m));     // over 1024 bytes
}

void test_version_compare(void) {
    TEST_ASSERT_EQUAL_INT(0, versionCompare("3.0.0", "3.0.0"));
    TEST_ASSERT_EQUAL_INT(-1, versionCompare("2.4.1", "3.0.0"));
    TEST_ASSERT_EQUAL_INT(1, versionCompare("3.0.10", "3.0.9"));
    TEST_ASSERT_EQUAL_INT(-1, versionCompare("3.0.0-rc1", "3.0.0"));
    TEST_ASSERT_EQUAL_INT(1, versionCompare("3.0.0", "3.0.0-rc1"));
    TEST_ASSERT_EQUAL_INT(1, versionCompare("3.1", "3.0.9"));
}

// Channel builds: X.Y.Z numeric, then dev < beta < release, N numeric, any other suffix
// (a local build) below dev; frozen from 3.0.0 (the same vectors in tools/ota/test_version.py and the web harness)
static void lower(const char* a, const char* b) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, versionCompare(a, b), a);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, versionCompare(b, a), b);
}
void test_version_order_channels(void) {
    lower("3.0.0", "3.1.0");
    lower("3.0.9", "3.0.10");
    lower("3.1.0-dev9", "3.1.0-dev10");
    lower("3.1.0-dev50", "3.1.0-beta1");
    lower("3.1.0-beta7", "3.1.0");
    lower("3.1.0-local", "3.1.0-dev0");
    lower("3.1.0-a", "3.1.0-b");
    lower("3.0.0", "3.0.0.1");                         // malformed strings keep one order on device, web and tools
    lower("3.0.0", "3.0.0a");
    lower("3.0", "3.0.1");
    TEST_ASSERT_EQUAL_INT(0, versionCompare("3.1.0-beta2", "3.1.0-beta2"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_manifest_v1_any_order);
    RUN_TEST(test_manifest_unknown_keys_ignored);
    RUN_TEST(test_manifest_repeated_key_rejected);
    RUN_TEST(test_manifest_requires_file);
    RUN_TEST(test_manifest_file_rules);
    RUN_TEST(test_manifest_html_rejected);
    RUN_TEST(test_manifest_blank_lines_and_no_final_lf);
    RUN_TEST(test_manifest_line_without_equals);
    RUN_TEST(test_manifest_limits);
    RUN_TEST(test_version_compare);
    RUN_TEST(test_version_order_channels);
    return UNITY_END();
}
