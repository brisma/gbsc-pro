#include <string.h>
#include <unity.h>
#include "update-rtc.h"

void setUp(void) {}
void tearDown(void) {}

void test_crc32_known_value(void) {
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32Update(0, "123456789", 9));
}

void test_reset_kinds(void) {
    TEST_ASSERT_EQUAL_UINT8(RESET_POWER, resetKindFromReason(0));
    TEST_ASSERT_EQUAL_UINT8(RESET_CRASH, resetKindFromReason(1));
    TEST_ASSERT_EQUAL_UINT8(RESET_CRASH, resetKindFromReason(2));
    TEST_ASSERT_EQUAL_UINT8(RESET_CRASH, resetKindFromReason(3));
    TEST_ASSERT_EQUAL_UINT8(RESET_SOFT, resetKindFromReason(4));
    TEST_ASSERT_EQUAL_UINT8(RESET_POWER, resetKindFromReason(6));
    // abort()/panic() restart through the postmortem as a "soft restart": its crash mark makes it a crash
    TEST_ASSERT_EQUAL_UINT8(RESET_CRASH, resetKindFromReason(4, true));
    TEST_ASSERT_EQUAL_UINT8(RESET_SOFT, resetKindFromReason(4, false));
    TEST_ASSERT_EQUAL_UINT8(RESET_POWER, resetKindFromReason(0, true));   // a stale mark after a power cycle
}

void test_seal_and_validate(void) {
    RtcRecord r; rtcRecordInit(r);
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    r.crashCount = 2;
    TEST_ASSERT_FALSE(rtcRecordValid(r));   // changed without resealing
    rtcRecordSeal(r);
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    RtcRecord garbage; memset(&garbage, 0xA5, sizeof(garbage));
    TEST_ASSERT_FALSE(rtcRecordValid(garbage));
}

void test_three_crashes_enter_safe_mode(void) {
    RtcRecord r; memset(&r, 0, sizeof(r));   // power-loss garbage
    TEST_ASSERT_FALSE(safeModeOnBoot(r, RESET_POWER));
    TEST_ASSERT_FALSE(safeModeOnBoot(r, RESET_CRASH));
    TEST_ASSERT_FALSE(safeModeOnBoot(r, RESET_CRASH));
    TEST_ASSERT_TRUE(safeModeOnBoot(r, RESET_CRASH));
    TEST_ASSERT_EQUAL_UINT8(0, r.crashCount);          // a restart from safe mode boots normally
    TEST_ASSERT_FALSE(safeModeOnBoot(r, RESET_SOFT));
}

void test_non_crash_resets_and_healthy_clear_the_counter(void) {
    RtcRecord r; rtcRecordInit(r);
    safeModeOnBoot(r, RESET_CRASH);
    safeModeOnBoot(r, RESET_CRASH);
    safeModeOnBoot(r, RESET_SOFT);
    TEST_ASSERT_EQUAL_UINT8(0, r.crashCount);
    safeModeOnBoot(r, RESET_CRASH);
    safeModeOnBoot(r, RESET_CRASH);
    safeModeMarkHealthy(r);
    TEST_ASSERT_EQUAL_UINT8(0, r.crashCount);
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    TEST_ASSERT_FALSE(safeModeOnBoot(r, RESET_CRASH));
}

// The update mode is gone (OTA over HTTP): the record keeps only the crash counter
void test_record_layout(void) {
    TEST_ASSERT_EQUAL_UINT32(12, sizeof(RtcRecord));
    RtcRecord r; memset(&r, 0, sizeof(r));
    rtcRecordInit(r);
    r.crashCount = 2;
    rtcRecordSeal(r);
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    TEST_ASSERT_EQUAL_UINT8(2, r.crashCount);
}

// A restart note (the backup restore before its restart) is read once at the next boot: the console log is in RAM
// and the restart comes right after the restore
void test_restart_note_is_read_once(void) {
    RtcRecord r; rtcRecordInit(r);
    TEST_ASSERT_EQUAL_UINT8(RN_NONE, rtcTakeNote(r));
    rtcSetNote(r, RN_RESTORE_OK);
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    TEST_ASSERT_EQUAL_UINT8(RN_RESTORE_OK, rtcTakeNote(r));
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    TEST_ASSERT_EQUAL_UINT8(RN_NONE, rtcTakeNote(r));
}

void test_restart_note_survives_the_boot_count(void) {
    RtcRecord r; rtcRecordInit(r);
    safeModeOnBoot(r, RESET_CRASH);
    rtcSetNote(r, RN_RESTORE_WRITE);
    TEST_ASSERT_EQUAL_UINT8(1, r.crashCount);         // setting a note keeps the counter
    safeModeOnBoot(r, RESET_SOFT);
    TEST_ASSERT_EQUAL_UINT8(RN_RESTORE_WRITE, rtcTakeNote(r));
}

void test_restart_note_on_an_invalid_record(void) {
    RtcRecord r; memset(&r, 0xA5, sizeof(r));         // power-loss garbage: no note, and a note starts a record
    TEST_ASSERT_EQUAL_UINT8(RN_NONE, rtcTakeNote(r));
    memset(&r, 0xA5, sizeof(r));
    rtcSetNote(r, RN_RESTORE_OK);
    TEST_ASSERT_TRUE(rtcRecordValid(r));
    TEST_ASSERT_EQUAL_UINT8(0, r.crashCount);
    TEST_ASSERT_EQUAL_UINT8(RN_RESTORE_OK, rtcTakeNote(r));
}

void test_record_stays_clear_of_the_eboot_command(void) {
    // rtcUserMemory offset 0 is 0x60001200, where eboot keeps its 128-byte command (user blocks 0-31)
    TEST_ASSERT_TRUE(RTC_RECORD_BLOCK >= 32);
    TEST_ASSERT_TRUE(RTC_RECORD_BLOCK * 4 + sizeof(RtcRecord) <= 512);
}

// Postmortem crash record: code addresses on the stack (IRAM and flash), in stack order, at most `max`
void test_crash_pcs_keeps_code_addresses_in_order(void) {
    const uint32_t stack[] = {0x3FFF1234, 0x40201000, 0x00000000, 0x4010F000, 0x40107FFC, 0x402FFFFC, 0x40300000,
                              0x40100000, 0x3FFE8000, 0x40235678};
    uint32_t out[8];
    uint8_t n = crashCollectPcs(stack, stack + 10, out, 8);
    TEST_ASSERT_EQUAL_UINT8(5, n);   // IRAM 0x40100000-0x4010BFFF, flash 0x40200000-0x402FFFFF
    TEST_ASSERT_EQUAL_HEX32(0x40201000, out[0]);
    TEST_ASSERT_EQUAL_HEX32(0x40107FFC, out[1]);
    TEST_ASSERT_EQUAL_HEX32(0x402FFFFC, out[2]);
    TEST_ASSERT_EQUAL_HEX32(0x40100000, out[3]);
    TEST_ASSERT_EQUAL_HEX32(0x40235678, out[4]);
    TEST_ASSERT_EQUAL_UINT8(2, crashCollectPcs(stack, stack + 10, out, 2));   // capped
    TEST_ASSERT_EQUAL_UINT8(0, crashCollectPcs(stack, stack, out, 8));        // empty range
}

void test_crash_info_fits_after_the_mark(void) {
    TEST_ASSERT_EQUAL_UINT32(RTC_CRASH_MARK_BLOCK + 1, RTC_CRASH_INFO_BLOCK);
    TEST_ASSERT_TRUE(RTC_CRASH_INFO_BLOCK * 4 + sizeof(CrashInfo) <= 512);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc32_known_value);
    RUN_TEST(test_reset_kinds);
    RUN_TEST(test_seal_and_validate);
    RUN_TEST(test_three_crashes_enter_safe_mode);
    RUN_TEST(test_non_crash_resets_and_healthy_clear_the_counter);
    RUN_TEST(test_record_layout);
    RUN_TEST(test_restart_note_is_read_once);
    RUN_TEST(test_restart_note_survives_the_boot_count);
    RUN_TEST(test_restart_note_on_an_invalid_record);
    RUN_TEST(test_record_stays_clear_of_the_eboot_command);
    RUN_TEST(test_crash_pcs_keeps_code_addresses_in_order);
    RUN_TEST(test_crash_info_fits_after_the_mark);
    return UNITY_END();
}
