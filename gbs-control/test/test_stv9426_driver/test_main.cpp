#include <unity.h>
#include <string.h>
#include <string>
#include "stv9426-map.h"
#include "stv9426-timing.h"
#include "stv9426-udc.h"
#include "stv9426-driver.h"
#include "stv-fake.h"
#include "golden.h"
using namespace stv;
void setUp(void) {}
void tearDown(void) {}

static size_t logIndex(const FakeStv& f, const char* entry, size_t from = 0) {
    for (size_t i = from; i < f.log.size(); i++) if (f.log[i] == entry) return i;
    return (size_t)-1;
}

void test_udc_bytes_bit_order(void) {
    uint8_t b[38];
    stvUdcBytes(UDC_CHECK, b);
    TEST_ASSERT_EQUAL_HEX8(0x01, b[6]); TEST_ASSERT_EQUAL_HEX8(0x00, b[7]);     // slice 3 "...........#"
    stvUdcBytes(UDC_MONITOR, b);
    TEST_ASSERT_EQUAL_HEX8(0xFF, b[4]); TEST_ASSERT_EQUAL_HEX8(0x0F, b[5]);     // slice 2 "############"
    // The slice interpolator (datasheet: slice = round(line x 18 / CH)) shows slice 18 on the last line of a strip
    // when CH > 18: it repeats slice 17, so the scrollbar is continuous from row to row
    for (uint8_t n = 0; n < UDC_COUNT; n++) {
        stvUdcBytes(n, b);
        TEST_ASSERT_EQUAL_HEX8(b[34], b[36]); TEST_ASSERT_EQUAL_HEX8(b[35], b[37]);
    }
    stvUdcBytes(UDC_BAR_TRACK, b);
    TEST_ASSERT_TRUE(b[36] || b[37]);
}

void test_begin_programs_pll_and_powers_off(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    TEST_ASSERT_EQUAL_HEX8(10, f.reg[7]);
    TEST_ASSERT_EQUAL_HEX8(STV_LD_IDLE, f.reg[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.reg[3]);                 // page 0
    TEST_ASSERT_EQUAL_HEX8(0x02, f.reg[4]);
    TEST_ASSERT_EQUAL_HEX8(STV_CAPTURE_TC, f.reg[5]);
    TEST_ASSERT_EQUAL_HEX8(0x24, f.reg[5]);
    uint8_t desc[kDescSlots * 2];
    buildDescriptors(compatLayout(), desc);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(desc, &f.ram[kPageAddr], sizeof(desc));
    TEST_ASSERT_FALSE(d.powered());
}

void test_udc_glyphs_golden(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    std::string art;
    for (uint8_t n = 0; n < UDC_COUNT; n++) {
        d.glyphFrame();
        uint8_t code = d.glyph(n);
        art += "UDC " + std::to_string(n) + "\n";
        art += f.udcArt((uint8_t)(code & 0x3F));
    }
    TEST_ASSERT_TRUE(goldenCheck("udc-glyphs", art));
}

// Dynamic UDC slots: a glyph is uploaded the first time a screen uses it
void test_glyph_loaded_on_demand(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.glyphFrame();
    uint8_t code = d.glyph(UDC_LOCK);
    TEST_ASSERT_EQUAL_HEX8(0x80 | kUdcFirst, code);
    uint8_t b[38]; stvUdcBytes(UDC_LOCK, b);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(b, &f.ram[38 * kUdcFirst], 38);
    size_t writes = f.log.size();
    TEST_ASSERT_EQUAL_HEX8(code, d.glyph(UDC_LOCK));            // already there: no upload
    TEST_ASSERT_EQUAL_UINT32(writes, f.log.size());
}

void test_glyph_never_evicts_one_used_by_the_screen(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.glyphFrame();
    for (uint8_t g = 0; g < kUdcSlots; g++) TEST_ASSERT_EQUAL_HEX8(0x80 | (kUdcFirst + g), d.glyph(g));
    TEST_ASSERT_EQUAL_HEX8(0x9A, d.glyph(kUdcSlots));            // all slots in use: a space, nothing evicted
    for (uint8_t g = 0; g < kUdcSlots; g++) TEST_ASSERT_EQUAL_HEX8(0x80 | (kUdcFirst + g), d.glyph(g));
    d.glyphFrame();                                              // next screen
    d.glyph(1);
    uint8_t code = d.glyph(kUdcSlots);                           // evicts the least recently used: glyph 0
    TEST_ASSERT_EQUAL_HEX8(0x80 | kUdcFirst, code);
    uint8_t b[38]; stvUdcBytes(kUdcSlots, b);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(b, &f.ram[38 * kUdcFirst], 38);
}

void test_glyphs_reloaded_after_power_on(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.glyphFrame();
    uint8_t code = d.glyph(UDC_CHECK);
    memset(&f.ram[38 * kUdcFirst], 0, 38);                       // RAM lost while closed
    d.powerOn(0);
    d.glyphFrame();
    TEST_ASSERT_EQUAL_HEX8(code, d.glyph(UDC_CHECK));
    uint8_t b[38]; stvUdcBytes(UDC_CHECK, b);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(b, &f.ram[38 * kUdcFirst], 38);
}

void test_power_on_applies_output_timing(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.setOutput(OUT_1080P60);
    d.powerOn(1000);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.reg[3]);              // timing first, display bit after the PLL settles
    d.service(1000 + Stv9426::kSettleMs - 1);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.reg[3]);
    d.service(1000 + Stv9426::kSettleMs);
    TEST_ASSERT_EQUAL_UINT8(55, f.reg[0]);                // 45.4 MHz pixel clock
    TEST_ASSERT_EQUAL_UINT8(32, f.reg[1]);
    TEST_ASSERT_EQUAL_UINT8(13, f.reg[6]);
    TEST_ASSERT_EQUAL_UINT8(21, f.reg[2]);
    TEST_ASSERT_EQUAL_HEX8(0x80, f.reg[3]);
    f.log.clear();
    d.powerOff();
    TEST_ASSERT_EQUAL_UINT8(STV_LD_IDLE, f.reg[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.reg[3]);
    TEST_ASSERT_EQUAL_STRING("R F3=00", f.log[0].c_str());   // display off before the idle line duration
}

void test_set_layout_rewrites_with_display_off(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.powerOn(0); d.service(Stv9426::kSettleMs);
    f.log.clear();
    Layout l; l.count = 2;
    l.strips[0] = {STRIP_SPACE, 40, false};
    l.strips[1] = {STRIP_ROW, 0, false};
    d.setLayout(l, 24);
    d.service(Stv9426::kSettleMs * 2);
    size_t off = logIndex(f, "R F3=00");
    size_t burst = logIndex(f, "W 000+38");
    size_t on = logIndex(f, "R F3=80", burst == (size_t)-1 ? 0 : burst);
    TEST_ASSERT_TRUE(off != (size_t)-1 && burst != (size_t)-1 && on != (size_t)-1);
    TEST_ASSERT_TRUE(off < burst && burst < on);
    TEST_ASSERT_EQUAL_UINT8(24, f.reg[2]);
    f.log.clear();
    d.setLayout(l, 24);
    TEST_ASSERT_EQUAL_UINT32(0, f.log.size());
}

void test_always_on_keeps_display_and_ld(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.setAlwaysOn(true);
    d.powerOn(0); d.service(Stv9426::kSettleMs);
    uint8_t ld = f.reg[0];
    d.powerOff();
    TEST_ASSERT_EQUAL_HEX8(0x80, f.reg[3]);
    TEST_ASSERT_EQUAL_UINT8(ld, f.reg[0]);
}

void test_request_on_applied_by_service_and_cancelled_by_power_off(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.requestOn(); d.powerOff(); d.service(0);
    TEST_ASSERT_FALSE(d.powered());
    d.requestOn(); d.service(100);
    TEST_ASSERT_TRUE(d.powered());
    TEST_ASSERT_EQUAL_HEX8(0x00, f.reg[3]);
    d.service(100 + Stv9426::kSettleMs);
    TEST_ASSERT_EQUAL_HEX8(0x80, f.reg[3]);
}

static int gDumpLines = 0;
static char gFirst[16];
static void dumpSink(const char* text, void*) { if (!gDumpLines) { strncpy(gFirst, text, 5); gFirst[5] = 0; } gDumpLines++; }

void test_rom_dump_line_count(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    gDumpLines = 0;
    d.romDump(dumpSink, nullptr);
    TEST_ASSERT_EQUAL_INT(152, gDumpLines);
    TEST_ASSERT_EQUAL_STRING("2000:", gFirst);
}

void test_timing_override(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.powerOn(0);
    d.setTimingOverride(50, 30, 20);
    TEST_ASSERT_EQUAL_UINT8(50, f.reg[0]);
    TEST_ASSERT_EQUAL_UINT8(30, f.reg[1]);
    TEST_ASSERT_EQUAL_UINT8(20, f.reg[2]);
    d.setOutput(OUT_480P);
    TEST_ASSERT_EQUAL_UINT8(63, f.reg[0]);
    TEST_ASSERT_EQUAL_UINT8(38, f.reg[1]);
    TEST_ASSERT_EQUAL_UINT8(21, f.reg[2]);
}

void test_power_on_invalidates_text(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    f.ram[stv::rowAddr(0)] = 0x55;                       // chip lost its RAM (brown-out, I2C glitch)
    d.powerOn(0); d.service(Stv9426::kSettleMs);
    TEST_ASSERT_EQUAL_HEX8(0xC0, f.ram[stv::rowAddr(0)]);
}

static std::string testScreen(StvOutStd out, unsigned frameLines) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.setOutput(out);
    stvDrawTestScreen(d);
    d.powerOn(0);
    d.service(Stv9426::kSettleMs);
    return f.decode(frameLines);
}

static void checkTestScreen(const std::string& out) {
    TEST_ASSERT_TRUE(out.find("NO-EOL") == std::string::npos);
    TEST_ASSERT_TRUE(out.find("R 10A") == std::string::npos);   // no arrow strips
    TEST_ASSERT_TRUE(out.find("R 1D2") != std::string::npos);   // title row
    TEST_ASSERT_TRUE(out.find("⓪①②③④⑤⑥⑦⑧⑨⑩") != std::string::npos);   // all 11 slots
}

void test_test_screen_golden(void) {
    std::string out = testScreen(OUT_1080P60, 1125);
    checkTestScreen(out);
    TEST_ASSERT_TRUE(goldenCheck("test-screen-1080p", out));
}

void test_test_screen_480p(void) {
    checkTestScreen(testScreen(OUT_480P, 525));
}

void test_failed_descriptor_write_is_retried(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    Layout l; l.count = 2;
    l.strips[0] = {STRIP_SPACE, 40, false};
    l.strips[1] = {STRIP_ROW, 0, false};
    f.failWrites = 1;
    d.setLayout(l, 24);
    f.log.clear();
    d.setLayout(l, 24);                                   // not stored after the failure: written again
    TEST_ASSERT_TRUE(logIndex(f, "W 000+38") != (size_t)-1);
}

void test_bypass_rate_override(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.setOutput(OUT_BYPASS, 45000);
    d.powerOn(0);
    TEST_ASSERT_EQUAL_UINT8(63, f.reg[0]);
    d.setOutput(OUT_BYPASS);
    TEST_ASSERT_EQUAL_UINT8(32, f.reg[0]);
}

void test_power_on_rewrites_descriptors(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    f.ram[kPageAddr] = 0x00;                              // table lost while closed
    d.powerOn(0);
    TEST_ASSERT_EQUAL_HEX8(0x3A, f.ram[kPageAddr]);
}

void test_layout_switch_keeps_display_off_until_flush(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.powerOn(0); d.service(Stv9426::kSettleMs);
    TEST_ASSERT_EQUAL_HEX8(0x80, f.reg[3]);
    Layout l; l.count = 2;
    l.strips[0] = {STRIP_SPACE, 40, false};
    l.strips[1] = {STRIP_ROW, 0, false};
    TEST_ASSERT_TRUE(d.setLayout(l, 24));
    TEST_ASSERT_EQUAL_HEX8(0x00, f.reg[3]);             // stays off: the text is not flushed yet
    d.shadow().set(0, 0, 'Q', 0x17);
    f.log.clear();
    d.service(Stv9426::kSettleMs + 20);
    TEST_ASSERT_EQUAL_HEX8(0x80, f.reg[3]);
    size_t text = logIndex(f, "W 1D2+2"), on = logIndex(f, "R F3=80");
    TEST_ASSERT_TRUE(text != (size_t)-1 && on != (size_t)-1 && text < on);
    TEST_ASSERT_FALSE(d.setLayout(l, 24));               // unchanged: nothing rewritten
}

// Horizontal placement: DD from the scaler window and the alignment,
// the Developer override wins, no window keeps the legacy edge
void test_window_and_alignment_set_dd(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.setOutput(OUT_960P60);
    d.powerOn(0); d.service(Stv9426::kSettleMs);
    uint8_t ld = d.timing().ld;
    TEST_ASSERT_EQUAL_HEX8(stvDdFor(ld), f.reg[1]);
    StvWindow w = {2704, 160, 612, 2594, true};                   // ntsc_240p.h (960p)
    d.setWindow(w);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)stvDdForWindow(ld, w, STV_ALIGN_CENTER, 0), f.reg[1]);
    d.setHAlign(STV_ALIGN_RIGHT);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)stvDdForWindow(ld, w, STV_ALIGN_RIGHT, 0), f.reg[1]);
    d.setTimingOverride(ld, 30, 21);
    TEST_ASSERT_EQUAL_HEX8(30, f.reg[1]);                         // dev.osd_timing wins
    d.setOutput(OUT_960P60);                                       // clears the override, keeps the window
    TEST_ASSERT_EQUAL_HEX8((uint8_t)stvDdForWindow(ld, w, STV_ALIGN_RIGHT, 0), f.reg[1]);
}

// LD bits 7-6 are the sync polarity bits and CH is 6 bits: the Developer override never writes above 63
void test_timing_override_clamps_to_six_bits(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.powerOn(0); d.service(Stv9426::kSettleMs);
    d.setTimingOverride(200, 30, 90);
    TEST_ASSERT_EQUAL_HEX8(63, f.reg[0]);
    TEST_ASSERT_EQUAL_HEX8(63, f.reg[2]);
    TEST_ASSERT_EQUAL_HEX8(30, f.reg[1]);
}

// The display bit comes on only after the descriptors and the text reached the chip (a NACK keeps it off and the
// next service retries the text)
void test_display_stays_off_after_failed_writes(void) {
    FakeStv f; Stv9426 d(f);
    d.begin(8000000UL);
    d.powerOn(0);
    d.shadow().set(0, 0, 'Q', 0x17);
    f.failWrites = 1;                                               // the text write NACKs
    d.service(Stv9426::kSettleMs);
    TEST_ASSERT_TRUE(f.reg[3] != 0x80);
    d.service(Stv9426::kSettleMs + 10);                            // retried: text on the chip, display on
    TEST_ASSERT_EQUAL_HEX8(0x80, f.reg[3]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_window_and_alignment_set_dd);
    RUN_TEST(test_display_stays_off_after_failed_writes);
    RUN_TEST(test_timing_override_clamps_to_six_bits);
    RUN_TEST(test_udc_bytes_bit_order);
    RUN_TEST(test_begin_programs_pll_and_powers_off);
    RUN_TEST(test_udc_glyphs_golden);
    RUN_TEST(test_power_on_applies_output_timing);
    RUN_TEST(test_set_layout_rewrites_with_display_off);
    RUN_TEST(test_always_on_keeps_display_and_ld);
    RUN_TEST(test_request_on_applied_by_service_and_cancelled_by_power_off);
    RUN_TEST(test_rom_dump_line_count);
    RUN_TEST(test_timing_override);
    RUN_TEST(test_power_on_invalidates_text);
    RUN_TEST(test_test_screen_golden);
    RUN_TEST(test_test_screen_480p);
    RUN_TEST(test_failed_descriptor_write_is_retried);
    RUN_TEST(test_bypass_rate_override);
    RUN_TEST(test_power_on_rewrites_descriptors);
    RUN_TEST(test_layout_switch_keeps_display_off_until_flush);
    RUN_TEST(test_glyph_loaded_on_demand);
    RUN_TEST(test_glyph_never_evicts_one_used_by_the_screen);
    RUN_TEST(test_glyphs_reloaded_after_power_on);
    return UNITY_END();
}
