#include <unity.h>
#include <vector>
#include "stv9426-map.h"
#include "stv9426-bus.h"
#include "stv9426-shadow.h"
using namespace stv;
struct Rec { uint16_t addr; std::vector<uint8_t> data; };
struct RecordingBus : OsdBus {
    std::vector<Rec> writes;
    bool fail = false;
    bool write(uint16_t a, const uint8_t* d, uint8_t n) override {
        if (fail) return false;
        writes.push_back({a, std::vector<uint8_t>(d, d + n)});
        return true;
    }
    void read(uint16_t, uint8_t* o, uint8_t n) override { for (uint8_t i = 0; i < n; i++) o[i] = 0; }
};
void setUp(void) {}
void tearDown(void) {}

void test_reset_flushes_every_row_with_eol(void) {
    StvShadow s; RecordingBus b; s.reset(); s.flush(b);
    TEST_ASSERT_EQUAL_UINT32(kRows, b.writes.size());              // one run per row: cells 0..30
    TEST_ASSERT_EQUAL_HEX16(rowAddr(0), b.writes[0].addr);
    TEST_ASSERT_EQUAL_UINT32(kRowCells * 2, b.writes[0].data.size());
    TEST_ASSERT_EQUAL_HEX8(kEol, b.writes[0].data[2 * 30 + 1]);
    TEST_ASSERT_FALSE(s.dirty());
}

void test_only_changed_cells_are_sent(void) {
    StvShadow s; RecordingBus b; s.reset(); s.flush(b); b.writes.clear();
    s.set(2, 5, 'A', 0x17); s.set(2, 6, 'B', 0x17); s.set(4, 0, 'C', 0x16);
    s.set(4, 0, 'C', 0x16);                                        // same value: no extra dirt
    s.flush(b);
    TEST_ASSERT_EQUAL_UINT32(2, b.writes.size());
    TEST_ASSERT_EQUAL_HEX16(rowAddr(2) + 10, b.writes[0].addr);
    const uint8_t run[4] = {0x17, 'A', 0x17, 'B'};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(run, b.writes[0].data.data(), 4);
    TEST_ASSERT_EQUAL_HEX16(rowAddr(4), b.writes[1].addr);
}

void test_unchanged_value_is_not_dirty(void) {
    StvShadow s; RecordingBus b; s.reset(); s.set(0, 0, 'X', 1); s.flush(b); b.writes.clear();
    s.set(0, 0, 'X', 1); s.flush(b);
    TEST_ASSERT_EQUAL_UINT32(0, b.writes.size());
}

void test_width_moves_eol_and_clips_writes(void) {
    StvShadow s; RecordingBus b; s.reset(); s.setWidth(1, 28);
    s.set(1, 28, 'Z', 0x17);                                       // beyond width: ignored
    TEST_ASSERT_EQUAL_HEX8(kEol, s.code(1, 28));
    s.flush(b);
    bool sawEol = false;
    for (auto& w : b.writes)
        if (w.addr <= rowAddr(1) + 57 && w.addr + w.data.size() > (size_t)(rowAddr(1) + 57)) sawEol = w.data[rowAddr(1) + 57 - w.addr] == kEol;
    TEST_ASSERT_TRUE(sawEol);
}

void test_shadow_every_row_has_eol(void) {
    StvShadow s; s.reset();
    for (uint8_t r = 0; r < kRows; r++) {
        s.setWidth(r, (uint8_t)(1 + r * 3));
        s.fill(r, 0, kRowWidth, 'Q', 0x17);
        TEST_ASSERT_EQUAL_HEX8(kEol, s.code(r, s.width(r)));
    }
}

void test_failed_write_keeps_cells_dirty(void) {
    StvShadow s; RecordingBus b; s.reset(); s.flush(b); b.writes.clear();
    s.set(3, 2, 'K', 0x17);
    b.fail = true; s.flush(b);
    TEST_ASSERT_TRUE(s.dirty());
    b.fail = false; s.flush(b);
    TEST_ASSERT_EQUAL_UINT32(1, b.writes.size());
    TEST_ASSERT_EQUAL_HEX16(rowAddr(3) + 4, b.writes[0].addr);
    TEST_ASSERT_FALSE(s.dirty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reset_flushes_every_row_with_eol);
    RUN_TEST(test_only_changed_cells_are_sent);
    RUN_TEST(test_unchanged_value_is_not_dirty);
    RUN_TEST(test_width_moves_eol_and_clips_writes);
    RUN_TEST(test_shadow_every_row_has_eol);
    RUN_TEST(test_failed_write_keeps_cells_dirty);
    return UNITY_END();
}
