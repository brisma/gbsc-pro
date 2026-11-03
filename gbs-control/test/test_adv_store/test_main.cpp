// ADV presets and macros stored on the device: text rules, ids, space, list entries
#include <string.h>
#include <string>
#include <unity.h>
#include "adv-store.h"

void setUp(void) {}
void tearDown(void) {}

static bool check(const std::string& text, size_t max, AdvYamlCheck* out = nullptr, size_t chunk = 7) {
    static AdvYamlCheck c;
    advYamlBegin(c, max);
    bool ok = true;
    for (size_t i = 0; i < text.size() && ok; i += chunk) {
        size_t n = text.size() - i < chunk ? text.size() - i : chunk;
        ok = advYamlFeed(c, (const uint8_t*)text.data() + i, n);
    }
    ok = advYamlFinish(c) && ok;
    if (out) *out = c;
    return ok;
}

static const char* kPreset =
    "name: Soft composite\n"
    "description: 'Less sharp, for a noisy RF mod'\n"
    "devices:\n"
    "  ADV7280:\n"
    "    maps:\n"
    "      user_sub_map:\n"
    "        map_value: '0x00'\n"
    "        registers:\n"
    "          '0x00': '0x09'\n"
    "          '0x0A': '0x10'\n"
    "          '0x0B': '0x00'\n";

void test_yaml_rules(void) {
    TEST_ASSERT_TRUE(check(kPreset, ADV_PRESET_MAX_BYTES));
    TEST_ASSERT_FALSE(check(std::string("name: x\nbad\x01line\n"), ADV_PRESET_MAX_BYTES));        // control byte
    TEST_ASSERT_TRUE(check(std::string("name: x\r\n\tcommands:\r\n"), ADV_PRESET_MAX_BYTES));     // CR and tab are fine
    TEST_ASSERT_FALSE(check("name: x\n" + std::string(128, 'a') + "\n", ADV_PRESET_MAX_BYTES));   // 128-byte line
    TEST_ASSERT_TRUE(check("name: x\n" + std::string(127, 'a') + "\r\n", ADV_PRESET_MAX_BYTES));  // 127 + CRLF
    TEST_ASSERT_FALSE(check("name: x\n[profile p01]\n", ADV_PRESET_MAX_BYTES));                   // a backup section
    TEST_ASSERT_FALSE(check("description: no name\n", ADV_PRESET_MAX_BYTES));
    TEST_ASSERT_FALSE(check("name: ''\n", ADV_PRESET_MAX_BYTES));                                 // empty name
    TEST_ASSERT_FALSE(check(kPreset, 100));                                                       // too long
    AdvYamlCheck c;
    TEST_ASSERT_TRUE(check("name: Caffè ✓\n", ADV_MACRO_MAX_BYTES, &c));                          // UTF-8 name
    TEST_ASSERT_EQUAL_STRING("Caffè ✓", c.name);
    TEST_ASSERT_FALSE(check("name: x\nend without newline \x02", ADV_PRESET_MAX_BYTES));          // last line checked too
    check("name: x\n" + std::string(200, 'a'), ADV_PRESET_MAX_BYTES, &c);
    TEST_ASSERT_EQUAL_STRING("line", c.why);
}

void test_yaml_collects_name_description_count(void) {
    AdvYamlCheck c;
    TEST_ASSERT_TRUE(check(kPreset, ADV_PRESET_MAX_BYTES, &c, 3));
    TEST_ASSERT_EQUAL_STRING("Soft composite", c.name);
    TEST_ASSERT_EQUAL_STRING("Less sharp, for a noisy RF mod", c.desc);
    TEST_ASSERT_EQUAL_UINT16(3, c.count);   // register lines
    TEST_ASSERT_TRUE(check("name: \"Enable ACE\"\ncommands:\n  - wm 42 40 80 80\n  - w 56 17 02\n", ADV_MACRO_MAX_BYTES, &c));
    TEST_ASSERT_EQUAL_STRING("Enable ACE", c.name);
    TEST_ASSERT_EQUAL_STRING("", c.desc);
    TEST_ASSERT_EQUAL_UINT16(2, c.count);   // commands
    std::string longName = "name: " + std::string(40, 'n') + "\n";
    TEST_ASSERT_TRUE(check(longName, ADV_MACRO_MAX_BYTES, &c));
    TEST_ASSERT_EQUAL_UINT32(ADV_NAME_MAX, strlen(c.name));   // cut to fit, never refused for it
    TEST_ASSERT_TRUE(check("name: " + std::string(31, 'a') + "è\n", ADV_MACRO_MAX_BYTES, &c));
    TEST_ASSERT_EQUAL_UINT32(31, strlen(c.name));             // never cut inside a UTF-8 character
}

void test_ids_and_paths(void) {
    TEST_ASSERT_EQUAL_INT(3, advParseId("p03", ADV_PRESET));
    TEST_ASSERT_EQUAL_INT(15, advParseId("p15", ADV_PRESET));
    TEST_ASSERT_EQUAL_INT(-1, advParseId("p16", ADV_PRESET));
    TEST_ASSERT_EQUAL_INT(31, advParseId("m31", ADV_MACRO));
    TEST_ASSERT_EQUAL_INT(-1, advParseId("m32", ADV_MACRO));
    TEST_ASSERT_EQUAL_INT(-1, advParseId("p03", ADV_MACRO));
    TEST_ASSERT_EQUAL_INT(-1, advParseId("p3", ADV_PRESET));
    TEST_ASSERT_EQUAL_INT(-1, advParseId("p03x", ADV_PRESET));
    TEST_ASSERT_EQUAL_INT(-1, advParseId(nullptr, ADV_PRESET));
    char p[32];
    advPath(ADV_PRESET, 4, p, sizeof(p));
    TEST_ASSERT_EQUAL_STRING("/adv/presets/p04.yaml", p);
    advPath(ADV_MACRO, 12, p, sizeof(p));
    TEST_ASSERT_EQUAL_STRING("/adv/macros/m12.yaml", p);
}

// upload refused when space is short: a save never eats the profile reserve (one profile file + one block)
void test_space_reserve(void) {
    const size_t block = 8192, reserve = 16384 + 8192;
    TEST_ASSERT_TRUE(advSpaceOk(reserve + block, block, 100, reserve));       // one block for the file, reserve left
    TEST_ASSERT_FALSE(advSpaceOk(reserve + block - 1, block, 100, reserve));
    TEST_ASSERT_FALSE(advSpaceOk(reserve + block, block, block + 1, reserve));   // two blocks needed
    TEST_ASSERT_TRUE(advSpaceOk(reserve + 2 * block, block, block + 1, reserve));
    TEST_ASSERT_FALSE(advSpaceOk(1000, block, 10, reserve));
}

void test_list_entry_json(void) {
    AdvYamlCheck c;
    check("name: Say \"hi\"\ndescription: a\\b\ncommands:\n  - w 42 0A 10\n", ADV_MACRO_MAX_BYTES, &c);
    char out[256];
    size_t n = advListEntryJson("m01", c, 1234, out, sizeof(out));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_STRING("{\"id\":\"m01\",\"name\":\"Say \\\"hi\\\"\",\"description\":\"a\\\\b\",\"count\":1,\"bytes\":1234}", out);
    TEST_ASSERT_EQUAL_UINT32(0, advListEntryJson("m01", c, 1234, out, 20));   // does not fit
}

// LittleFS: rename never creates folders and remove drops a folder left empty, so the upload lands in the target
// folder (open for write creates it) and the save is a same-folder rename that replaces the old file atomically
void test_upload_temp_lives_in_the_target_folder(void) {
    char tmp[32], path[32];
    for (uint8_t k = 0; k < 2; k++) {
        advTempPath((AdvKind)k, tmp, sizeof(tmp));
        advPath((AdvKind)k, 0, path, sizeof(path));
        TEST_ASSERT_EQUAL_INT((int)(strrchr(path, '/') - path), (int)(strrchr(tmp, '/') - tmp));
        TEST_ASSERT_EQUAL_INT(0, strncmp(tmp, path, (size_t)(strrchr(path, '/') - path + 1)));
        TEST_ASSERT_EQUAL_INT(-1, advParseId(strrchr(tmp, '/') + 1, (AdvKind)k));   // never taken for a stored id
    }
    advTempPath(ADV_PRESET, tmp, sizeof(tmp));
    TEST_ASSERT_EQUAL_STRING("/adv/presets/upload.tmp", tmp);
}

// One upload at a time: a second request's chunks never touch the first one's file; the owner is released when its
// request ends or its client goes away
void test_upload_gate(void) {
    AdvUploadGate g = {};
    TEST_ASSERT_TRUE(advGateBegin(g, 0xA));
    TEST_ASSERT_TRUE(advGateOwns(g, 0xA));
    TEST_ASSERT_FALSE(advGateBegin(g, 0xB));      // a second upload while the first runs
    TEST_ASSERT_FALSE(advGateOwns(g, 0xB));
    TEST_ASSERT_TRUE(advGateBusy(g));
    advGateEnd(g, 0xB);                            // not the owner: nothing changes
    TEST_ASSERT_TRUE(advGateOwns(g, 0xA));
    TEST_ASSERT_TRUE(advGateBegin(g, 0xA));       // the owner may restart (a retried index 0)
    advGateEnd(g, 0xA);
    TEST_ASSERT_FALSE(advGateBusy(g));
    TEST_ASSERT_TRUE(advGateBegin(g, 0xB));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_yaml_rules);
    RUN_TEST(test_yaml_collects_name_description_count);
    RUN_TEST(test_ids_and_paths);
    RUN_TEST(test_space_reserve);
    RUN_TEST(test_list_entry_json);
    RUN_TEST(test_upload_temp_lives_in_the_target_folder);
    RUN_TEST(test_upload_gate);
    return UNITY_END();
}
