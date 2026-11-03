#include <string.h>
#include "update-rtc.h"

uint32_t crc32Update(uint32_t crc, const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

uint8_t crashCollectPcs(const uint32_t* from, const uint32_t* to, uint32_t* out, uint8_t max) {
    uint8_t n = 0;
    for (const uint32_t* p = from; p < to && n < max; p++) {
        uint32_t v = *p;
        if ((v >= 0x40100000u && v < 0x4010C000u) || (v >= 0x40200000u && v < 0x40300000u)) out[n++] = v;
    }
    return n;
}

ResetKind resetKindFromReason(uint32_t reason, bool crashMark) {
    switch (reason) {
        case 1: case 2: case 3: return RESET_CRASH;   // hardware WDT, exception, software WDT
        case 4: return crashMark ? RESET_CRASH : RESET_SOFT;   // ESP.restart()/ESP.reset(), or abort()/panic()
        default: return RESET_POWER;                    // power-on, external reset, deep-sleep wake
    }
}

static uint32_t recordCrc(const RtcRecord& r) { return crc32Update(0, &r, offsetof(RtcRecord, crc)); }

void rtcRecordInit(RtcRecord& r) {
    memset(&r, 0, sizeof(r));
    r.magic = RTC_MAGIC;
    r.version = RTC_RECORD_VERSION;
    rtcRecordSeal(r);
}

void rtcRecordSeal(RtcRecord& r) { r.crc = recordCrc(r); }

bool rtcRecordValid(const RtcRecord& r) {
    return r.magic == RTC_MAGIC && r.version == RTC_RECORD_VERSION && r.crc == recordCrc(r);
}

bool safeModeOnBoot(RtcRecord& r, ResetKind kind) {
    if (!rtcRecordValid(r)) rtcRecordInit(r);
    r.crashCount = (kind == RESET_CRASH) ? (uint8_t)(r.crashCount + 1) : 0;
    bool enter = r.crashCount >= SAFE_MODE_CRASHES;
    if (enter) r.crashCount = 0;
    rtcRecordSeal(r);
    return enter;
}

void safeModeMarkHealthy(RtcRecord& r) {
    if (!rtcRecordValid(r)) rtcRecordInit(r);
    r.crashCount = 0;
    rtcRecordSeal(r);
}


void rtcSetNote(RtcRecord& r, uint8_t note) {
    if (!rtcRecordValid(r)) rtcRecordInit(r);
    r.note = note;
    rtcRecordSeal(r);
}

uint8_t rtcTakeNote(RtcRecord& r) {
    if (!rtcRecordValid(r)) return RN_NONE;
    uint8_t n = r.note;
    r.note = RN_NONE;
    rtcRecordSeal(r);
    return n;
}
