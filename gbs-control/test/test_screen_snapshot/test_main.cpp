// Developer screen readback: the snapshot packs the STV9426 RAM + registers read back from the chip and the OLED
// frame buffer; parsing it and decoding the RAM gives the same screen the simulator decodes
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include "host-fs.h"
#include <string.h>
#include <string>
#include "screen-snapshot.h"
#include "stv9426-driver.h"
#include "stv-fake.h"
#include "osd-renderer.h"
#include "oled-fake.h"
#include "menu-fake.h"
#include "menu-engine.h"

void setUp(void) {}
void tearDown(void) {}

// Records the largest read: the device bus (Wire) must stay within its buffer
struct CountingBus : public OsdBus {
    FakeStv& f;
    uint8_t maxRead = 0;
    unsigned reads = 0;
    explicit CountingBus(FakeStv& s) : f(s) {}
    bool write(uint16_t a, const uint8_t* d, uint8_t n) override { return f.write(a, d, n); }
    void read(uint16_t a, uint8_t* o, uint8_t n) override { if (n > maxRead) maxRead = n; reads++; f.read(a, o, n); }
};

static uint8_t gSnap[snap::kBytes];

void test_osd_round_trip_decodes_the_same_screen(void) {
    FakeStv chip;
    CountingBus bus(chip);
    Stv9426 d(bus);
    FakeBackend be;
    MenuEngine e(be);
    OsdRenderer r(d);
    d.begin(STV_XTAL_HZ);
    d.setOutput(OUT_1080P60);
    e.key(K_MENU, false, 1000);
    for (uint32_t t = 1000; t <= 1200; t += 50) { r.render(e.view(), 0, t); d.service(t); }
    unsigned lines = stvActiveLines(d.output()) + 60;
    std::string want = chip.decode(lines);
    TEST_ASSERT_TRUE(want.find("GBSC PRO") != std::string::npos);

    TEST_ASSERT_EQUAL(snap::kBytes, screenSnapshotBuild(&bus, (uint8_t)d.output(), nullptr, gSnap));
    TEST_ASSERT_TRUE(bus.maxRead <= 32);

    FakeStv back;
    uint8_t flags = 0, out = 0xFF, oled[1024];
    TEST_ASSERT_TRUE(screenSnapshotParse(gSnap, sizeof(gSnap), flags, out, back.reg, back.ram, oled));
    TEST_ASSERT_EQUAL_HEX8(snap::HAS_OSD, flags);
    TEST_ASSERT_EQUAL(OUT_1080P60, out);
    TEST_ASSERT_EQUAL_STRING(want.c_str(), back.decode(stvActiveLines((StvOutStd)out) + 60).c_str());
}

void test_oled_buffer_is_copied(void) {
    FakeOled o;
    o.clear();
    o.fillRect(3, 5, 10, 7);
    o.setPixel(127, 63);
    TEST_ASSERT_EQUAL(snap::kBytes, screenSnapshotBuild(nullptr, 0, o.buffer, gSnap));
    uint8_t flags = 0, out = 0, reg[16], ram[1024], oled[1024];
    TEST_ASSERT_TRUE(screenSnapshotParse(gSnap, sizeof(gSnap), flags, out, reg, ram, oled));
    TEST_ASSERT_EQUAL_HEX8(snap::HAS_OLED, flags);
    TEST_ASSERT_EQUAL_MEMORY(o.buffer, oled, 1024);
    uint8_t zero[1024] = {0};
    TEST_ASSERT_EQUAL_MEMORY(zero, ram, 1024);   // no OSD: zeros
}

void test_parse_rejects_bad_input(void) {
    uint8_t flags, out, reg[16], ram[1024], oled[1024];
    screenSnapshotBuild(nullptr, 0, nullptr, gSnap);
    TEST_ASSERT_TRUE(screenSnapshotParse(gSnap, sizeof(gSnap), flags, out, reg, ram, oled));
    TEST_ASSERT_FALSE(screenSnapshotParse(gSnap, sizeof(gSnap) - 1, flags, out, reg, ram, oled));   // short
    gSnap[0] ^= 0xFF;
    TEST_ASSERT_FALSE(screenSnapshotParse(gSnap, sizeof(gSnap), flags, out, reg, ram, oled));       // magic
    gSnap[0] ^= 0xFF;
    gSnap[4] = snap::kVersion + 1;
    TEST_ASSERT_FALSE(screenSnapshotParse(gSnap, sizeof(gSnap), flags, out, reg, ram, oled));       // version
}

// Decodes a snapshot file into <prefix>.txt (OSD, golden format) and <prefix>.pbm (OLED)
static void decodeSnapshot(const char* in, const std::string& out) {
    FILE* f = fopen(in, "rb");
    TEST_ASSERT_NOT_NULL(f);
    size_t n = fread(gSnap, 1, sizeof(gSnap), f);
    fclose(f);
    FakeStv chip;
    FakeOled o;
    uint8_t flags = 0, output = 0;
    TEST_ASSERT_TRUE_MESSAGE(screenSnapshotParse(gSnap, n, flags, output, chip.reg, chip.ram, o.buffer), "bad snapshot");
    if (flags & snap::HAS_OSD) {
        std::string text = chip.decode(stvActiveLines((StvOutStd)output) + 60);
        f = fopen((out + ".txt").c_str(), "wb");
        TEST_ASSERT_NOT_NULL(f);
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
    }
    if (flags & snap::HAS_OLED) {
        std::string pbm = o.pbm();
        f = fopen((out + ".pbm").c_str(), "wb");
        TEST_ASSERT_NOT_NULL(f);
        fwrite(pbm.data(), 1, pbm.size(), f);
        fclose(f);
    }
}

// Review tool: SNAPSHOT_IN=<snapshot.bin> SNAPSHOT_OUT=<prefix> decodes a snapshot read
// from the device. Without them the snapshot of the round-trip test goes through the same file path as a check.
void test_decode_device_snapshot(void) {
    const char* in = getenv("SNAPSHOT_IN");
    const char* out = getenv("SNAPSHOT_OUT");
    if (in && *in && out && *out) { decodeSnapshot(in, out); return; }
    FakeStv chip;                                             // the root menu, as in the round-trip test
    Stv9426 d(chip);
    FakeBackend be;
    MenuEngine e(be);
    OsdRenderer r(d);
    d.begin(STV_XTAL_HZ);
    d.setOutput(OUT_1080P60);
    e.key(K_MENU, false, 1000);
    for (uint32_t t = 1000; t <= 1200; t += 50) { r.render(e.view(), 0, t); d.service(t); }
    TEST_ASSERT_EQUAL(snap::kBytes, screenSnapshotBuild(&chip, (uint8_t)d.output(), nullptr, gSnap));
    std::string dir = hostTempDir("gbsc-snapshot-");
    TEST_ASSERT_FALSE(dir.empty());
    std::string path = dir + "/snapshot.bin";
    FILE* f = fopen(path.c_str(), "wb");
    bool written = f && fwrite(gSnap, 1, sizeof(gSnap), f) == sizeof(gSnap);
    if (f) fclose(f);
    std::string prefix = dir + "/out";
    char buf[4096] = "";
    if (written) {
        decodeSnapshot(path.c_str(), prefix);
        f = fopen((prefix + ".txt").c_str(), "rb");
        if (f) { buf[fread(buf, 1, sizeof(buf) - 1, f)] = '\0'; fclose(f); }
    }
    hostRemoveTree(dir);                                      // before the checks: a failure must not leave files
    TEST_ASSERT_TRUE(written);
    TEST_ASSERT_NOT_NULL(strstr(buf, "GBSC PRO"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_osd_round_trip_decodes_the_same_screen);
    RUN_TEST(test_oled_buffer_is_copied);
    RUN_TEST(test_parse_rejects_bad_input);
    RUN_TEST(test_decode_device_snapshot);
    return UNITY_END();
}
