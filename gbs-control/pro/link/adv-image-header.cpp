#include "adv-image-header.h"
#include <string.h>
#include "../update/update-rtc.h"   // crc32Update (zlib CRC32)

static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static bool versionChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '+';
}

bool advImageHeaderParse(const uint8_t* hdr, AdvImageHeader& out) {
    if (memcmp(hdr, "ADVC", 4) != 0) return false;
    uint8_t n = 0;
    while (n < 12 && hdr[8 + n]) {
        if (!versionChar((char)hdr[8 + n])) return false;
        n++;
    }
    if (n == 0) return false;
    memcpy(out.version, hdr + 8, n);
    out.version[n] = 0;
    out.length = le32(hdr + 20);
    out.crc = le32(hdr + 24);
    return out.length >= ADV_IMAGE_HDR_OFF + ADV_IMAGE_HDR_BYTES && out.length <= ADV_IMAGE_MAX_BYTES;
}

void AdvImageCheck::add(const uint8_t* d, size_t n) {
    static const uint8_t kZero[4] = {0, 0, 0, 0};
    while (n) {
        const uint32_t at = received;
        size_t take = n;
        // header bytes are kept for done(); the CRC field is fed as zeros
        if (at < ADV_IMAGE_HDR_OFF + ADV_IMAGE_HDR_BYTES && at + take > ADV_IMAGE_HDR_OFF) {
            const uint32_t from = at > ADV_IMAGE_HDR_OFF ? at : ADV_IMAGE_HDR_OFF;
            const uint32_t to = at + take < ADV_IMAGE_HDR_OFF + ADV_IMAGE_HDR_BYTES ? at + (uint32_t)take : ADV_IMAGE_HDR_OFF + ADV_IMAGE_HDR_BYTES;
            memcpy(hdr + (from - ADV_IMAGE_HDR_OFF), d + (from - at), to - from);
        }
        if (at < ADV_IMAGE_CRC_OFF + 4 && at + take > ADV_IMAGE_CRC_OFF) {
            if (at < ADV_IMAGE_CRC_OFF) take = ADV_IMAGE_CRC_OFF - at;       // up to the field
            else {                                                            // inside the field
                take = ADV_IMAGE_CRC_OFF + 4 - at;
                if (take > n) take = n;
                crc = crc32Update(crc, kZero, take);
                received += (uint32_t)take; d += take; n -= take;
                continue;
            }
        }
        crc = crc32Update(crc, d, take);
        received += (uint32_t)take; d += take; n -= take;
    }
}

const char* AdvImageCheck::done(AdvImageHeader& out) const {
    if (received < ADV_IMAGE_HDR_OFF + ADV_IMAGE_HDR_BYTES || !advImageHeaderParse(hdr, out)) return "format";
    if (out.length != received) return "size";
    return out.crc == crc ? nullptr : "crc";
}
