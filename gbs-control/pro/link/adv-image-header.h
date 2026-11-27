// ADV Controller image header and CRC (pure, host-tested): the checks tools/adv-controller-image.py makes on the build
// machine, for an image uploaded to LittleFS. Header at 0x400 (adv-controller/tools/fw-header.py): "ADVC", u16 header
// version, u16 reserved, char[12] version, u32 length, u32 CRC32 (zlib) of the image with this CRC field zeroed.
#pragma once
#include <stdint.h>
#include <stddef.h>

static const uint32_t ADV_IMAGE_HDR_OFF = 0x400;
static const uint32_t ADV_IMAGE_HDR_BYTES = 28;
static const uint32_t ADV_IMAGE_CRC_OFF = ADV_IMAGE_HDR_OFF + 24;   // the CRC field, zeroed for the CRC
static const uint32_t ADV_IMAGE_MAX_BYTES = 256u * 1024u;          // larger is not an ADV Controller application

struct AdvImageHeader {
    uint32_t length;
    uint32_t crc;
    char version[13];   // NUL-terminated, [0-9A-Za-z.+-] only (it goes into JSON and the OSD)
};

// The 28 header bytes (from offset 0x400). false: no "ADVC", a length outside 0x41C..ADV_IMAGE_MAX_BYTES, or an
// empty / unprintable version
bool advImageHeaderParse(const uint8_t* hdr, AdvImageHeader& out);

// Streaming check of an image arriving in chunks: feed every byte in order, then done()
struct AdvImageCheck {
    uint32_t received = 0;
    uint32_t crc = 0;            // running CRC32, the CRC field fed as zeros
    uint8_t hdr[ADV_IMAGE_HDR_BYTES];
    void add(const uint8_t* d, size_t n);
    // nullptr when the image is whole and its CRC matches, else "format" (no header), "size" (length differs from the
    // bytes received, or too big) or "crc"; out gets the header when it parsed
    const char* done(AdvImageHeader& out) const;
};
