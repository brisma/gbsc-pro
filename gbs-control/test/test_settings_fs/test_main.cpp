#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <unity.h>
#include "settings-fs.h"
#include "settings-saver.h"

struct FakeFs : SettingsFs {
    std::map<std::string, std::string> files;
    bool failRename = false;
    bool failWrite = false;
    bool exists(const char* p) override { return files.count(p) > 0; }
    int removedTargets = 0;               // remove() calls on a non-.tmp path
    bool remove(const char* p) override { if (!strstr(p, ".tmp")) removedTargets++; return files.erase(p) > 0; }
    bool rename(const char* f, const char* t) override {
        if (failRename || !files.count(f)) return false;
        files[t] = files[f]; files.erase(f); return true;
    }
    bool writeAll(const char* p, const char* d, size_t n) override {
        if (failWrite) return false;
        files[p] = std::string(d, n); return true;
    }
    size_t readAt(const char* p, size_t off, char* buf, size_t len) override {
        if (!files.count(p) || off >= files[p].size()) return 0;
        size_t n = files[p].size() - off < len ? files[p].size() - off : len;
        memcpy(buf, files[p].data() + off, n); return n;
    }
    size_t fileSize(const char* p) override { return files.count(p) ? files[p].size() : 0; }
    bool readLines(const char* p, void (*cb)(const char*, void*), void* ctx, size_t maxBytes) override {
        if (!files.count(p)) return false;
        size_t n = files[p].size() < maxBytes ? files[p].size() : maxBytes;
        settingsSplitLines(files[p].data(), n, cb, ctx); return true;
    }
};

static void collect(const char* l, void* ctx) { ((std::vector<std::string>*)ctx)->push_back(l); }

void setUp(void) {}
void tearDown(void) {}

void test_atomic_write_replaces_file(void) {
    FakeFs fs; fs.files["/cfg/a.cfg"] = "old";
    TEST_ASSERT_TRUE(settingsAtomicWrite(fs, "/cfg/a.cfg", "new", 3));
    TEST_ASSERT_EQUAL_STRING("new", fs.files["/cfg/a.cfg"].c_str());
    TEST_ASSERT_FALSE(fs.exists("/cfg/a.cfg.tmp"));
}

void test_recover_promotes_orphan_tmp(void) {
    FakeFs fs; fs.files["/cfg/a.cfg.tmp"] = "x";
    settingsRecoverTmp(fs, "/cfg/a.cfg");
    TEST_ASSERT_EQUAL_STRING("x", fs.files["/cfg/a.cfg"].c_str());
    TEST_ASSERT_FALSE(fs.exists("/cfg/a.cfg.tmp"));
}

void test_recover_drops_tmp_when_final_exists(void) {
    FakeFs fs; fs.files["/cfg/a.cfg"] = "good"; fs.files["/cfg/a.cfg.tmp"] = "half";
    settingsRecoverTmp(fs, "/cfg/a.cfg");
    TEST_ASSERT_EQUAL_STRING("good", fs.files["/cfg/a.cfg"].c_str());
    TEST_ASSERT_FALSE(fs.exists("/cfg/a.cfg.tmp"));
}

void test_split_lines_handles_crlf_and_no_trailing_newline(void) {
    std::vector<std::string> lines;
    const char* t = "a=1\r\nb=2\nc=3";
    settingsSplitLines(t, strlen(t), collect, &lines);
    TEST_ASSERT_EQUAL_INT(3, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("a=1", lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("c=3", lines[2].c_str());
}

// The ADV store accepts 127-character lines of CRLF files; the restore reader must keep them
void test_split_lines_keeps_a_full_line_of_a_crlf_file(void) {
    std::string full(127, 'y');
    std::string t = full + "\r\nb=2\r\n";
    std::vector<std::string> lines;
    settingsSplitLines(t.data(), t.size(), collect, &lines);
    TEST_ASSERT_EQUAL_INT(2, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING(full.c_str(), lines[0].c_str());
}

void test_readlines_skips_overlong_line(void) {
    std::string t = "a=1\n" + std::string(300, 'x') + "\nb=2\n";
    std::vector<std::string> lines;
    settingsSplitLines(t.data(), t.size(), collect, &lines);
    TEST_ASSERT_EQUAL_INT(2, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("b=2", lines[1].c_str());
}

void test_textbuf_grows(void) {
    TextBuf b; textBufInit(b);
    for (int i = 0; i < 500; i++) TEST_ASSERT_TRUE(textBufAppend(b, "0123456789"));
    TEST_ASSERT_EQUAL_UINT32(5000, (uint32_t)b.len);
    textBufFree(b);
}

void test_saver_debounce(void) {
    SettingsSaver s;
    TEST_ASSERT_FALSE(s.due(0));
    s.markDirty(SettingScope::Profile, 1000);
    TEST_ASSERT_FALSE(s.due(2999));
    s.markDirty(SettingScope::Device, 2500);   // restarts the window
    TEST_ASSERT_FALSE(s.due(4499));
    TEST_ASSERT_TRUE(s.due(4500));
    TEST_ASSERT_EQUAL_UINT8(0x03, s.take());
    TEST_ASSERT_FALSE(s.dirty());
}

// A failed append must poison the buffer: a truncated file must never replace a good one
void test_textbuf_failure_is_sticky(void) {
    TextBuf b; textBufInit(b, 300);
    std::string big(256, 'a');
    TEST_ASSERT_TRUE(textBufAppend(b, big.c_str()));
    TEST_ASSERT_FALSE(textBufAppend(b, std::string(100, 'b').c_str()));
    TEST_ASSERT_TRUE(b.failed);
    TEST_ASSERT_FALSE(textBufAppend(b, "x"));
    TEST_ASSERT_EQUAL_UINT32(256, (uint32_t)b.len);
    textBufFree(b);
}

// Huge or garbage files are read only up to the cap (boot must not trip the watchdog)
void test_split_lines_stops_at_byte_cap(void) {
    std::string t;
    for (int i = 0; i < 5000; i++) t += "a=1\n";
    std::vector<std::string> lines;
    settingsSplitLines(t.data(), t.size(), collect, &lines);
    TEST_ASSERT_TRUE((int)lines.size() <= (int)(SETTINGS_MAX_FILE_BYTES / 4));
    TEST_ASSERT_TRUE(lines.size() > 0);
}

void test_writer_commit_and_failure(void) {
    FakeFs fs;
    fs.files["/p/a.cfg"] = "old";
    SettingsFileWriter* w = fs.openWriter("/p/a.cfg");
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_TRUE(w->write("ne", 2));
    TEST_ASSERT_TRUE(w->write("w\n", 2));
    TEST_ASSERT_EQUAL_STRING("old", fs.files["/p/a.cfg"].c_str());   // nothing visible before commit
    TEST_ASSERT_TRUE(w->commit());
    delete w;
    TEST_ASSERT_EQUAL_STRING("new\n", fs.files["/p/a.cfg"].c_str());
    TEST_ASSERT_EQUAL_UINT32(0, fs.files.count("/p/a.cfg.tmp"));
    fs.failWrite = true;
    w = fs.openWriter("/p/a.cfg");
    w->write("broken", 6);
    TEST_ASSERT_FALSE(w->commit());
    delete w;
    TEST_ASSERT_EQUAL_STRING("new\n", fs.files["/p/a.cfg"].c_str());
    TEST_ASSERT_EQUAL_UINT32(0, fs.files.count("/p/a.cfg.tmp"));
    w = fs.openWriter("/p/b.cfg");
    w->write("x", 1);
    w->abort();
    delete w;
    TEST_ASSERT_EQUAL_UINT32(0, fs.files.count("/p/b.cfg"));
}

void test_read_at(void) {
    FakeFs fs;
    fs.files["/f"] = "0123456789";
    char b[4];
    TEST_ASSERT_EQUAL_UINT32(4, fs.readAt("/f", 3, b, 4));
    TEST_ASSERT_EQUAL_MEMORY("3456", b, 4);
    TEST_ASSERT_EQUAL_UINT32(2, fs.readAt("/f", 8, b, 4));
    TEST_ASSERT_EQUAL_UINT32(0, fs.readAt("/f", 10, b, 4));
    TEST_ASSERT_EQUAL_UINT32(10, fs.fileSize("/f"));
}


void test_saver_hold_reasons_are_independent(void) {
    SettingsSaver s;
    s.markDirty(SettingScope::Device, 0);
    s.hold(SAVER_HOLD_UPDATE, true);
    s.hold(SAVER_HOLD_MENU, true);
    TEST_ASSERT_FALSE(s.due(5000));
    s.hold(SAVER_HOLD_MENU, false);
    TEST_ASSERT_TRUE(s.held());
    TEST_ASSERT_FALSE(s.due(5000));
    s.hold(SAVER_HOLD_UPDATE, false);
    TEST_ASSERT_FALSE(s.held());
    TEST_ASSERT_TRUE(s.due(5000));
}

// The target is replaced by the rename (LittleFS rename is atomic): it never disappears, so a backup read
// at the same moment always finds it (a backup on the device missed current.cfg)
void test_atomic_write_never_removes_the_target(void) {
    FakeFs fs;
    fs.files["/cfg/a.cfg"] = "old";
    TEST_ASSERT_TRUE(settingsAtomicWrite(fs, "/cfg/a.cfg", "new", 3));
    TEST_ASSERT_EQUAL_INT(0, fs.removedTargets);
    TEST_ASSERT_EQUAL_STRING("new", fs.files["/cfg/a.cfg"].c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_saver_hold_reasons_are_independent);
    RUN_TEST(test_textbuf_failure_is_sticky);
    RUN_TEST(test_split_lines_stops_at_byte_cap);
    RUN_TEST(test_atomic_write_replaces_file);
    RUN_TEST(test_recover_promotes_orphan_tmp);
    RUN_TEST(test_recover_drops_tmp_when_final_exists);
    RUN_TEST(test_split_lines_handles_crlf_and_no_trailing_newline);
    RUN_TEST(test_split_lines_keeps_a_full_line_of_a_crlf_file);
    RUN_TEST(test_readlines_skips_overlong_line);
    RUN_TEST(test_textbuf_grows);
    RUN_TEST(test_saver_debounce);
    RUN_TEST(test_writer_commit_and_failure);
    RUN_TEST(test_read_at);
    RUN_TEST(test_atomic_write_never_removes_the_target);
    return UNITY_END();
}
