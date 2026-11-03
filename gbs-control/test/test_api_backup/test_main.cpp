#include <string.h>
#include <map>
#include <string>
#include <unity.h>
#include "options.h"
#include "platform-caps.h"
#include "settings-registry.h"
#include "settings-store.h"
#include "settings-device.h"
#include "api-backup.h"

struct MemFs : SettingsFs {
    std::map<std::string, std::string> files;
    bool failWrites = false;
    bool exists(const char* p) override { return files.count(p) > 0; }
    bool remove(const char* p) override { return files.erase(p) > 0; }
    bool rename(const char* f, const char* t) override { files[t] = files[f]; files.erase(f); return true; }
    bool writeAll(const char* p, const char* d, size_t n) override {
        if (failWrites) return false;
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
        const std::string& s = files[p];
        settingsSplitLines(s.data(), s.size() < maxBytes ? s.size() : maxBytes, cb, ctx);
        return true;
    }
};

static userOptions u;
static MemFs fs;
static void sink(const char* t, void* c) { ((std::string*)c)->append(t); }

void setUp(void) {
    platformSetCaps(0xFF);
    memset(&u, 0, sizeof(u)); settingsApplyDefaults(u);
    fs.files.clear();
    fs.failWrites = false;
    fs.files[SETTINGS_DEVICE_PATH] = "schema=1\nosd.theme=Light\nsystem.adv_controller_skip=1234ABCD\n";
    fs.files[SETTINGS_CURRENT_PATH] = "schema=1\nadv.brightness=150\n";
}
void tearDown(void) {}

static std::string streamAll(size_t chunk, const char* const* ids, uint8_t n) {
    BackupStreamer st;
    st.begin(&fs, ids, n);
    std::string out;
    char buf[4096];
    for (int guard = 0; guard < 100000; guard++) {
        size_t got = st.fill(buf, chunk);
        if (!got) break;
        out.append(buf, got);
    }
    return out;
}

static const char* kExpected =
    "# gbsc-backup 1\n"
    "[file cfg/device.cfg]\nschema=1\nosd.theme=Light\n"
    "[file cfg/current.cfg]\nschema=1\nadv.brightness=150\n"
    "[profiles]\n"
    "[raw cfg/profiles.cfg]\np03=7\n"
    "[profile p03]\nschema=1\nprofile.name=Mega Drive\nreg.ntsc.0=00\n";

void test_streamer_output_is_independent_of_buffer_size(void) {
    fs.files["/cfg/profiles.cfg"] = "p03=7\n";
    fs.files["/profiles/p03.cfg"] = "schema=1\nprofile.name=Mega Drive\nreg.ntsc.0=00\n";
    const char* ids[] = {"p03"};
    TEST_ASSERT_EQUAL_STRING(kExpected, streamAll(4096, ids, 1).c_str());
    TEST_ASSERT_EQUAL_STRING(kExpected, streamAll(7, ids, 1).c_str());
    TEST_ASSERT_EQUAL_STRING(kExpected, streamAll(1, ids, 1).c_str());   // excluded key split across fills
}

void test_streamer_without_profiles(void) {
    std::string out = streamAll(64, nullptr, 0);
    TEST_ASSERT_TRUE(out.find("[profiles]\n") != std::string::npos);    // marker: restore replaces profiles
    TEST_ASSERT_TRUE(out.find("[profile ") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("[raw ") == std::string::npos);           // missing file: no section
}

void test_validate_and_apply_profiles(void) {
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Retro\n[profiles]\n[raw cfg/profiles.cfg]\np03=7\n"
                     "[profile p03]\nschema=1\nprofile.name=Mega Drive\n";
    RestorePlan plan;
    TEST_ASSERT_EQUAL_UINT8(BACKUP_OK, backupValidate(fs, "/r", &plan));
    TEST_ASSERT_TRUE(plan.hasProfiles);
    TEST_ASSERT_EQUAL_UINT8(1, plan.profileCount);
    TEST_ASSERT_EQUAL_UINT32(1, plan.profileBlocks);
    fs.files["/profiles/p01.cfg"] = "old";
    int deletes = 0;
    backupApply(fs, "/r", u, [](void* c) { (*(int*)c)++; }, &deletes);
    TEST_ASSERT_EQUAL_INT(1, deletes);
    TEST_ASSERT_EQUAL_STRING("schema=1\nprofile.name=Mega Drive\n", fs.files["/profiles/p03.cfg"].c_str());
    TEST_ASSERT_EQUAL_STRING("p03=7\n", fs.files["/cfg/profiles.cfg"].c_str());
    TEST_ASSERT_EQUAL_UINT8(3, u.osdTheme);
}

void test_apply_without_profiles_marker_keeps_profiles(void) {
    fs.files["/profiles/p01.cfg"] = "keep";
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Retro\n";
    RestorePlan plan;
    TEST_ASSERT_EQUAL_UINT8(BACKUP_OK, backupValidate(fs, "/r", &plan));
    TEST_ASSERT_FALSE(plan.hasProfiles);
    int deletes = 0;
    backupApply(fs, "/r", u, [](void* c) { (*(int*)c)++; }, &deletes);
    TEST_ASSERT_EQUAL_INT(0, deletes);
    TEST_ASSERT_EQUAL_STRING("keep", fs.files["/profiles/p01.cfg"].c_str());
}

void test_validate_rejects_bad_profile_sections(void) {
    fs.files["/r"] = "# gbsc-backup 1\n[profiles]\n[profile p99]\nprofile.name=x\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SECTION, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[profiles]\n[profile x]\nprofile.name=x\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SECTION, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[profiles]\n[profile p02]\nprofile.name=x\nnot a line\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[profiles]\n[profile p02]\nadv.brightness=1\n";   // no profile.name
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[profile p02]\nprofile.name=x\n";                // profile without marker
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SECTION, backupValidate(fs, "/r", nullptr));
}

void test_restore_space_rule(void) {
    TEST_ASSERT_TRUE(restoreSpaceOk(10, 0, 8));     // 8 needed + 2 spare <= 10 free
    TEST_ASSERT_FALSE(restoreSpaceOk(9, 0, 8));
    TEST_ASSERT_TRUE(restoreSpaceOk(5, 5, 8));      // the current profiles' blocks are freed first
}

void test_validate(void) {
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Light\n\n# note\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_OK, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "[file cfg/device.cfg]\nosd.theme=Light\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_HEADER, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/other.cfg]\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SECTION, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nnot a line\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\nosd.theme=Light\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));   // key outside a section
    fs.files["/r"] = "# gbsc-backup 1\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_EMPTY, backupValidate(fs, "/r", nullptr));
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_READ, backupValidate(fs, "/missing", nullptr));
}

void test_apply_restores_values_and_keeps_excluded(void) {
    StrSettingDef skip; settingsStrDefAt((uint8_t)settingsStrFind("system.adv_controller_skip"), skip);
    settingsStrPut(u, skip, "0000BEEF");
    u.advContrast = 90;
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Retro\nsystem.adv_controller_skip=1234ABCD\nsystem.extra_hosts=tv.lan\n"
                     "[file cfg/current.cfg]\nadv.brightness=99\n";
    backupApply(fs, "/r", u, [](void*) {}, nullptr);
    TEST_ASSERT_EQUAL_UINT8(3, u.osdTheme);
    TEST_ASSERT_EQUAL_UINT8(99, u.advBrightness);
    TEST_ASSERT_EQUAL_UINT8(50, u.advContrast);               // missing from the backup: default
    TEST_ASSERT_EQUAL_STRING("0000BEEF", u.advControllerSkip);   // excluded: current value kept
    TEST_ASSERT_EQUAL_STRING("tv.lan", u.extraHosts);             // extra hosts travel with backups (web redesign)
}

// The working settings, the profiles and which profile they belong to travel together: a restore that kept the unit's
// own active profile and hash showed a profile of the backup as changed, with another profile's settings under it
void test_backup_carries_the_active_profile(void) {
    fs.files[SETTINGS_DEVICE_PATH] = "schema=1\nprofile.active=p03\nprofile.hash=1a2b3c4d\nsystem.adv_controller_skip=1234ABCD\n";
    std::string out = streamAll(64, nullptr, 0);
    TEST_ASSERT_TRUE(out.find("profile.active=p03\n") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("profile.hash=1a2b3c4d\n") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("adv_controller_skip") == std::string::npos);   // still the unit's own
}

static void putStr(const char* key, const char* value) {
    StrSettingDef d; settingsStrDefAt((uint8_t)settingsStrFind(key), d);
    settingsStrPut(u, d, value);
}

void test_apply_takes_the_active_profile_from_the_backup(void) {
    putStr("profile.active", "p01");
    putStr("profile.hash", "deadbeef");
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nprofile.active=p03\nprofile.hash=1a2b3c4d\n"
                     "[file cfg/current.cfg]\nadv.brightness=99\n";
    backupApply(fs, "/r", u, [](void*) {}, nullptr);
    TEST_ASSERT_EQUAL_STRING("p03", u.profileActive);
    TEST_ASSERT_EQUAL_STRING("1a2b3c4d", u.profileHash);
}

// A backup that does not say which profile was active (hand-made, or from an earlier build): no profile is, not
// the one the unit happened to have
void test_apply_without_an_active_profile_clears_the_units(void) {
    putStr("profile.active", "p01");
    putStr("profile.hash", "deadbeef");
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Retro\n[file cfg/current.cfg]\nadv.brightness=99\n";
    backupApply(fs, "/r", u, [](void*) {}, nullptr);
    TEST_ASSERT_EQUAL_STRING("", u.profileActive);
    TEST_ASSERT_EQUAL_STRING("", u.profileHash);
    // the same from a unit that had no profile loaded when the backup was taken
    putStr("profile.active", "p01");
    fs.files["/r"] = "# gbsc-backup 1\n[file cfg/device.cfg]\nprofile.active=\nprofile.hash=1a2b3c4d\n";
    backupApply(fs, "/r", u, [](void*) {}, nullptr);
    TEST_ASSERT_EQUAL_STRING("", u.profileActive);
}

// A hand-edited file ending with an excluded key and no newline: the last kept line is not written twice
void test_streamer_last_excluded_line_without_newline(void) {
    fs.files[SETTINGS_DEVICE_PATH] = "schema=1\nosd.theme=Light\nsystem.adv_controller_skip=1234ABCD";
    std::string out = streamAll(4096, nullptr, 0);
    size_t first = out.find("osd.theme=Light\n");
    TEST_ASSERT_TRUE(first != std::string::npos);
    TEST_ASSERT_TRUE(out.find("osd.theme=Light\n", first + 1) == std::string::npos);
    TEST_ASSERT_TRUE(out.find("adv_controller_skip") == std::string::npos);
}

// Restore write failures (filesystem full after the profile wipe) are reported, not "ok"
void test_apply_reports_write_failures(void) {
    fs.files["/r"] = "# gbsc-backup 1\n[profiles]\n[raw cfg/profiles.cfg]\np03=7\n[profile p03]\nschema=1\n";
    TEST_ASSERT_TRUE(backupApply(fs, "/r", u, nullptr, nullptr));
    fs.failWrites = true;
    TEST_ASSERT_FALSE(backupApply(fs, "/r", u, nullptr, nullptr));
}

// ---- ADV presets and macros: [adv] marker, verbatim YAML sections, the size check
static std::string streamAdv(size_t chunk, size_t* total = nullptr) {
    BackupStreamer st;
    st.begin(&fs, nullptr, 0);
    const char* presets[] = {"p02", "p07"};   // p07 deleted since the snapshot: no section
    const char* macros[] = {"m05"};
    st.beginAdv(presets, 2, macros, 1);
    std::string out;
    char buf[4096];
    for (int guard = 0; guard < 100000; guard++) {
        size_t got = st.fill(buf, chunk);
        if (!got) break;
        out.append(buf, got);
    }
    if (total) *total = st.total();
    return out;
}

static const char* kAdvPreset = "name: Soft\ndevices:\n  ADV7280:\n    maps:\n      user_sub_map:\n        registers:\n          '0x0A': '0x10'\n";
static const char* kAdvMacro = "name: ACE on\ncommands:\n  - wm 42 40 80 80\n";

void test_backup_round_trip_keeps_adv_files(void) {
    fs.files["/adv/presets/p02.yaml"] = kAdvPreset;
    fs.files["/adv/macros/m05.yaml"] = std::string(kAdvMacro) + "\n# trailing comment\n";
    std::string out = streamAdv(5);
    TEST_ASSERT_EQUAL_STRING(out.c_str(), streamAdv(4096).c_str());
    TEST_ASSERT_TRUE(out.find("[profiles]\n[adv]\n[adv preset p02]\nname: Soft\n") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("[adv preset p07]") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("[adv macro m05]\nname: ACE on\n") != std::string::npos);
    fs.files["/r"] = out;
    RestorePlan plan;
    TEST_ASSERT_EQUAL_UINT8(BACKUP_OK, backupValidate(fs, "/r", &plan));
    TEST_ASSERT_TRUE(plan.hasAdv);
    TEST_ASSERT_EQUAL_UINT8(2, plan.advCount);
    TEST_ASSERT_EQUAL_UINT32(2, plan.advBlocks);
    fs.files["/adv/presets/p09.yaml"] = "name: gone\n";   // replaced by the restore
    int advDeletes = 0;
    TEST_ASSERT_TRUE(backupApply(fs, "/r", u, nullptr, nullptr, [](void* c) {
        (*(int*)c)++;
        fs.files.erase("/adv/presets/p02.yaml"); fs.files.erase("/adv/presets/p09.yaml"); fs.files.erase("/adv/macros/m05.yaml");
    }, &advDeletes));
    TEST_ASSERT_EQUAL_INT(1, advDeletes);
    TEST_ASSERT_EQUAL_STRING(kAdvPreset, fs.files["/adv/presets/p02.yaml"].c_str());
    TEST_ASSERT_EQUAL_STRING((std::string(kAdvMacro) + "# trailing comment\n").c_str(), fs.files["/adv/macros/m05.yaml"].c_str());   // blank lines go
    TEST_ASSERT_EQUAL_UINT32(0, fs.files.count("/adv/presets/p09.yaml"));
}

void test_backup_adv_section_rejects_bad_yaml(void) {
    fs.files["/r"] = "# gbsc-backup 1\n[adv]\n[adv preset p01]\ndescription: no name\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[adv]\n[adv macro m01]\nname: x\nbad\x01\n";
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));
    std::string big = "# gbsc-backup 1\n[adv]\n[adv macro m01]\nname: x\n";
    for (int i = 0; i < 30; i++) big += "  - w 42 0A 10   # " + std::string(50, 'c') + "\n";   // > 2 KB in short lines
    fs.files["/r"] = big;
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SYNTAX, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[adv preset p01]\nname: x\n";                // section without marker
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SECTION, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[adv]\n[adv preset p16]\nname: x\n";        // no such id
    TEST_ASSERT_EQUAL_UINT8(BACKUP_ERR_SECTION, backupValidate(fs, "/r", nullptr));
    fs.files["/r"] = "# gbsc-backup 1\n[adv]\n";                                       // marker only: clears the store
    RestorePlan plan;
    TEST_ASSERT_EQUAL_UINT8(BACKUP_OK, backupValidate(fs, "/r", &plan));
    TEST_ASSERT_TRUE(plan.hasAdv);
    TEST_ASSERT_EQUAL_UINT8(0, plan.advCount);
}

void test_oversized_backup_is_flagged(void) {
    fs.files["/adv/presets/p02.yaml"] = kAdvPreset;
    size_t total = 0;
    std::string out = streamAdv(64, &total);
    TEST_ASSERT_EQUAL_UINT32(out.size(), total);   // the streamer counts what it sent
    TEST_ASSERT_FALSE(backupTooBig(BACKUP_MAX_BYTES));
    TEST_ASSERT_TRUE(backupTooBig(BACKUP_MAX_BYTES + 1));
}

// A file without a final newline (an imported hand-edited macro) must not swallow the next section header
void test_backup_adv_file_without_final_newline(void) {
    fs.files["/adv/macros/m05.yaml"] = "name: First\ncommands:\n  - w 42 0A 10";
    fs.files["/adv/macros/m06.yaml"] = "name: Second\ncommands:\n  - w 42 08 80\n";
    BackupStreamer st;
    st.begin(&fs, nullptr, 0);
    const char* macros[] = {"m05", "m06"};
    st.beginAdv(nullptr, 0, macros, 2);
    std::string out;
    char buf[64];
    for (size_t got; (got = st.fill(buf, 7)) > 0;) out.append(buf, got);
    TEST_ASSERT_TRUE(out.find("  - w 42 0A 10\n[adv macro m06]\n") != std::string::npos);
    fs.files["/r"] = out;
    RestorePlan plan;
    TEST_ASSERT_EQUAL_UINT8(BACKUP_OK, backupValidate(fs, "/r", &plan));
    TEST_ASSERT_EQUAL_UINT8(2, plan.advCount);
    TEST_ASSERT_TRUE(backupApply(fs, "/r", u, nullptr, nullptr, [](void*) {}, nullptr));
    TEST_ASSERT_EQUAL_STRING("name: First\ncommands:\n  - w 42 0A 10\n", fs.files["/adv/macros/m05.yaml"].c_str());
    TEST_ASSERT_EQUAL_STRING("name: Second\ncommands:\n  - w 42 08 80\n", fs.files["/adv/macros/m06.yaml"].c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_streamer_output_is_independent_of_buffer_size);
    RUN_TEST(test_streamer_without_profiles);
    RUN_TEST(test_validate_and_apply_profiles);
    RUN_TEST(test_apply_without_profiles_marker_keeps_profiles);
    RUN_TEST(test_validate_rejects_bad_profile_sections);
    RUN_TEST(test_restore_space_rule);
    RUN_TEST(test_validate);
    RUN_TEST(test_apply_restores_values_and_keeps_excluded);
    RUN_TEST(test_backup_carries_the_active_profile);
    RUN_TEST(test_apply_takes_the_active_profile_from_the_backup);
    RUN_TEST(test_apply_without_an_active_profile_clears_the_units);
    RUN_TEST(test_streamer_last_excluded_line_without_newline);
    RUN_TEST(test_apply_reports_write_failures);
    RUN_TEST(test_backup_round_trip_keeps_adv_files);
    RUN_TEST(test_backup_adv_section_rejects_bad_yaml);
    RUN_TEST(test_oversized_backup_is_flagged);
    RUN_TEST(test_backup_adv_file_without_final_newline);
    return UNITY_END();
}
