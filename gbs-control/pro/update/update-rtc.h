// Record kept in RTC user memory across resets (not across power loss): crash counter for safe mode and a restart
// note for the next boot's console log (pure, host-testable)
#pragma once
#include <stdint.h>
#include <stddef.h>

uint32_t crc32Update(uint32_t crc, const void* data, size_t n);

enum ResetKind : uint8_t { RESET_POWER, RESET_CRASH, RESET_SOFT };
// rst_info.reason; crashMark: the postmortem crash callback ran before this reset (abort()/panic() restart as a
// soft restart, reason 4, but are crashes)
ResetKind resetKindFromReason(uint32_t reason, bool crashMark = false);

static const uint32_t RTC_MAGIC = 0x47425343;      // "GBSC"
static const uint8_t RTC_RECORD_VERSION = 2;   // 2: the update-mode fields are gone (OTA over HTTP)
// rtcUserMemory offset 0 is 0x60001200: eboot keeps its 128-byte command there (offsets 0-31), so the record
// starts at offset 32; it must end within the 512-byte user area
static const uint32_t RTC_RECORD_BLOCK = 32;
static const uint8_t SAFE_MODE_CRASHES = 3;

struct RtcRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t crashCount;      // consecutive exception/WDT resets before the firmware was marked healthy
    uint8_t note;            // RestartNote set before a restart, logged and cleared by the next boot
    uint8_t reserved;
    uint32_t crc;            // CRC32 of every byte before this field
};
static_assert(sizeof(RtcRecord) % 4 == 0, "RTC memory is written in 4-byte blocks");
static_assert(RTC_RECORD_BLOCK * 4 + sizeof(RtcRecord) <= 512, "RTC record must fit in the RTC user memory");
// One block after the record: RTC_CRASH_MAGIC written by custom_crash_callback, cleared at boot
static const uint32_t RTC_CRASH_MARK_BLOCK = RTC_RECORD_BLOCK + sizeof(RtcRecord) / 4;
static const uint32_t RTC_CRASH_MAGIC = 0x43525348;   // "CRSH"
static_assert(RTC_CRASH_MARK_BLOCK * 4 + 4 <= 512, "crash mark must fit in the RTC user memory");

// After the mark: the last crash (custom_crash_callback), kept until the next crash or a power loss; /api/info
// reports it so a crash on a device without a serial console can be decoded with addr2line
static const uint32_t RTC_CRASH_INFO_BLOCK = RTC_CRASH_MARK_BLOCK + 1;
static const uint8_t CRASH_PCS = 16;
struct CrashInfo {
    uint32_t magic;          // RTC_CRASH_MAGIC when valid
    uint32_t exccause, epc1, epc2, epc3, excvaddr, depc;
    uint32_t pcs[CRASH_PCS]; // code addresses found on the stack, innermost first; 0 = unused
};
static_assert(RTC_CRASH_INFO_BLOCK * 4 + sizeof(CrashInfo) <= 512, "crash info must fit in the RTC user memory");
// Code addresses (IRAM or mapped flash) in [from, to), in order, at most max; returns the count
uint8_t crashCollectPcs(const uint32_t* from, const uint32_t* to, uint32_t* out, uint8_t max);

// What happened right before a restart (the console log is in RAM and does not survive it)
enum RestartNote : uint8_t { RN_NONE, RN_RESTORE_OK, RN_RESTORE_WRITE };

void rtcRecordInit(RtcRecord& r);
void rtcRecordSeal(RtcRecord& r);
bool rtcRecordValid(const RtcRecord& r);

bool safeModeOnBoot(RtcRecord& r, ResetKind kind);   // true: enter safe mode now
void safeModeMarkHealthy(RtcRecord& r);
void rtcSetNote(RtcRecord& r, uint8_t note);   // keeps the crash counter; starts a record when r is not valid
uint8_t rtcTakeNote(RtcRecord& r);             // the note (RN_NONE on an invalid record), then cleared
