#include <string.h>
#include <string>
#include <vector>
#include <unity.h>
#include "profile-file.h"
#include "profile-modes.h"
#include "profile-list.h"
#include "profile-names.h"

void setUp(void) {}
void tearDown(void) {}

static void sink(const char* t, void* c) { ((std::string*)c)->append(t); }

static std::vector<std::string> lines(const std::string& s) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == '\n') { out.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void test_header_round_trip(void) {
    ProfileMeta m; profileMetaInit(m);
    strcpy(m.name, "Mega Drive NTSC"); strcpy(m.region, "None");
    m.basePreset = 0; m.modes = (1 << 0) | (1 << 1);
    m.hasFp = true; m.fpInput = 1; m.fpMode = 1; m.fpLines = 525; m.fpInterlaced = 1; m.fpMilliHz = 59940;
    m.adcGain = 12;
    std::string out; profileWriteHeader(m, sink, &out);
    TEST_ASSERT_TRUE(out.find("schema=1\nprofile.name=Mega Drive NTSC\n") == 0);
    TEST_ASSERT_TRUE(out.find("profile.modes=ntsc,pal\n") != std::string::npos);
    ProfileMeta r; profileMetaInit(r);
    for (const std::string& l : lines(out)) TEST_ASSERT_TRUE_MESSAGE(profileParseHeaderLine(l.c_str(), r), l.c_str());
    TEST_ASSERT_EQUAL_STRING("Mega Drive NTSC", r.name);
    TEST_ASSERT_EQUAL_STRING("None", r.region);
    TEST_ASSERT_EQUAL_UINT8(0, r.basePreset);
    TEST_ASSERT_EQUAL_UINT8(3, r.modes);
    TEST_ASSERT_TRUE(r.hasFp);
    TEST_ASSERT_EQUAL_UINT16(525, r.fpLines);
    TEST_ASSERT_EQUAL_UINT8(1, r.fpInterlaced);
    TEST_ASSERT_TRUE(out.find("profile.fp.mhz=59940\n") != std::string::npos);   // the field rate, in mHz
    TEST_ASSERT_EQUAL_UINT32(59940, r.fpMilliHz);
    ProfileEntry e = {}; profileEntryFromMeta(e, r);
    TEST_ASSERT_EQUAL_UINT32(59940, e.fpMilliHz);
    // a fingerprint saved before the rate was kept, or with a rate that could not be measured: none
    ProfileMeta old; profileMetaInit(old);
    TEST_ASSERT_TRUE(profileParseHeaderLine("profile.fp.lines=523", old));
    TEST_ASSERT_EQUAL_UINT32(0, old.fpMilliHz);
    old.hasFp = true; old.fpInput = 1; old.fpMode = 1;
    std::string plain; profileWriteHeader(old, sink, &plain);
    TEST_ASSERT_TRUE(plain.find("profile.fp.mhz") == std::string::npos);
    TEST_ASSERT_EQUAL_UINT8(12, r.adcGain);
    TEST_ASSERT_TRUE(out.find("profile.sharpness") == std::string::npos);   // menu v3: picture.sharpness is a setting
    TEST_ASSERT_TRUE(profileParseHeaderLine("profile.sharpness=1", r));          // older files: ignored, still a header line
    TEST_ASSERT_FALSE(profileParseHeaderLine("adv.brightness=1", r));
}

void test_header_without_fingerprint_and_unknown_mode(void) {
    ProfileMeta m; profileMetaInit(m);
    strcpy(m.name, "X");
    std::string out; profileWriteHeader(m, sink, &out);
    TEST_ASSERT_TRUE(out.find("profile.fp.") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("profile.base_preset") == std::string::npos);   // 0xFF = unknown: omitted
    ProfileMeta r; profileMetaInit(r);
    profileParseHeaderLine("profile.modes=ntsc,ntsc_1080p,vga_upscale", r);
    TEST_ASSERT_EQUAL_UINT8((1 << 0) | (1 << 5), r.modes);
    TEST_ASSERT_FALSE(r.hasFp);
}

void test_register_round_trip(void) {
    uint8_t regs[PROFILE_REG_BYTES];
    for (int i = 0; i < PROFILE_REG_BYTES; i++) regs[i] = (uint8_t)(i * 7);
    std::string out; profileWriteRegs(0, regs, sink, &out);
    std::vector<std::string> ls = lines(out);
    TEST_ASSERT_EQUAL_UINT32(PROFILE_REG_LINES, ls.size());
    for (const std::string& l : ls) TEST_ASSERT_TRUE(l.size() < 127);
    TEST_ASSERT_TRUE(ls[3].find("reg.ntsc.3=") == 0);
    uint8_t back[PROFILE_REG_BYTES] = {0};
    ProfileRegReader rd; rd.begin(0, back);
    rd.line("reg.pal.0=00");   // other mode: ignored
    for (const std::string& l : ls) rd.line(l.c_str());
    TEST_ASSERT_TRUE(rd.complete());
    TEST_ASSERT_EQUAL_MEMORY(regs, back, PROFILE_REG_BYTES);
}

void test_register_reader_rejects_bad_input(void) {
    uint8_t regs[PROFILE_REG_BYTES] = {0}, back[PROFILE_REG_BYTES];
    std::string out; profileWriteRegs(1, regs, sink, &out);
    std::vector<std::string> ls = lines(out);
    ProfileRegReader rd; rd.begin(1, back);
    for (size_t i = 0; i + 1 < ls.size(); i++) rd.line(ls[i].c_str());
    TEST_ASSERT_FALSE(rd.complete());                         // last line missing
    rd.begin(1, back);
    for (const std::string& l : ls) rd.line(l.c_str());
    rd.line("reg.pal.4=zz");                                  // bad hex
    TEST_ASSERT_FALSE(rd.complete());
    rd.begin(1, back);
    for (const std::string& l : ls) rd.line(l.c_str());
    rd.line("reg.pal.2=0011");                                // wrong length
    TEST_ASSERT_FALSE(rd.complete());
}

void test_keep_old_lines(void) {
    TEST_ASSERT_TRUE(profileKeepOldLine("reg.pal.3=00", 0));
    TEST_ASSERT_FALSE(profileKeepOldLine("reg.ntsc.3=00", 0));
    TEST_ASSERT_FALSE(profileKeepOldLine("reg.ntsc_480p.3=00", 2));
    TEST_ASSERT_TRUE(profileKeepOldLine("reg.ntsc_480p.3=00", 0));
    TEST_ASSERT_FALSE(profileKeepOldLine("adv.brightness=1", 0));
    TEST_ASSERT_FALSE(profileKeepOldLine("profile.name=x", 0));
    TEST_ASSERT_FALSE(profileKeepOldLine("reg.bogus.1=00", 0));
}

// Issue #45: Reset geometry drops one mode's registers from the active profile; the header is
// rewritten (modes bit cleared), settings and the other modes' registers stay
void test_forget_mode_keeps_everything_else(void) {
    TEST_ASSERT_FALSE(profileForgetKeepsLine("reg.ntsc.3=00", 0));
    TEST_ASSERT_TRUE(profileForgetKeepsLine("reg.pal.3=00", 0));
    TEST_ASSERT_TRUE(profileForgetKeepsLine("reg.ntsc.3=00", 1));
    TEST_ASSERT_FALSE(profileForgetKeepsLine("profile.modes=3", 0));   // header: written again
    TEST_ASSERT_TRUE(profileForgetKeepsLine("picture.scanlines=1", 0));
    TEST_ASSERT_TRUE(profileForgetKeepsLine("output.preset=Custom", 0));
}

void test_keep_all_registers_when_nothing_is_captured(void) {
    TEST_ASSERT_TRUE(profileKeepOldLine("reg.ntsc.0=00", PROFILE_KEEP_ALL));
    TEST_ASSERT_TRUE(profileKeepOldLine("reg.vga_upscale.8=00", PROFILE_KEEP_ALL));
    TEST_ASSERT_FALSE(profileKeepOldLine("profile.name=x", PROFILE_KEEP_ALL));
}

void test_region_round_trip_and_entry(void) {
    ProfileMeta m; profileMetaInit(m);
    strcpy(m.name, "Saturn NTSC-J"); strcpy(m.region, "NTSC-J");
    std::string out; profileWriteHeader(m, sink, &out);
    TEST_ASSERT_TRUE(out.find("profile.region=NTSC-J\n") != std::string::npos);
    ProfileMeta r; profileMetaInit(r);
    for (const std::string& l : lines(out)) profileParseHeaderLine(l.c_str(), r);
    TEST_ASSERT_EQUAL_STRING("NTSC-J", r.region);
    ProfileEntry e; memset(&e, 0, sizeof(e));
    profileEntryFromMeta(e, r);
    TEST_ASSERT_EQUAL_STRING("Saturn NTSC-J", e.name);
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NTSC_J, e.region);
    ProfileMeta old; profileMetaInit(old);                         // a file from before the rename
    profileParseHeaderLine("profile.region=JP", old);
    TEST_ASSERT_EQUAL_STRING("NTSC-J", old.region);               // rewritten with the new name on the next save
    profileEntryFromMeta(e, old);
    TEST_ASSERT_EQUAL_UINT8(PROFILE_REGION_NTSC_J, e.region);
}

// A hand-edited backup with '"' or '\\' in a name used to break the whole GET /api/profiles JSON
void test_header_name_is_sanitized(void) {
    ProfileMeta m; profileMetaInit(m);
    profileParseHeaderLine("profile.name=Mega \"Drive\" \\ 2", m);
    TEST_ASSERT_TRUE(strchr(m.name, '"') == nullptr);
    TEST_ASSERT_TRUE(strchr(m.name, '\\') == nullptr);
    TEST_ASSERT_EQUAL_STRING("Mega  Drive    2", m.name);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_header_name_is_sanitized);
    RUN_TEST(test_header_round_trip);
    RUN_TEST(test_header_without_fingerprint_and_unknown_mode);
    RUN_TEST(test_register_round_trip);
    RUN_TEST(test_register_reader_rejects_bad_input);
    RUN_TEST(test_keep_old_lines);
    RUN_TEST(test_forget_mode_keeps_everything_else);
    RUN_TEST(test_keep_all_registers_when_nothing_is_captured);
    RUN_TEST(test_region_round_trip_and_entry);
    return UNITY_END();
}
