#include "screen-snapshot.h"
#include <string.h>

static const uint8_t kMagic[4] = {'G', 'S', 'N', 'P'};

size_t screenSnapshotBuild(OsdBus* osd, uint8_t output, const uint8_t* oled, uint8_t* out) {
    memset(out, 0, snap::kBytes);
    memcpy(out, kMagic, sizeof(kMagic));
    out[4] = snap::kVersion;
    out[5] = (uint8_t)((osd ? snap::HAS_OSD : 0) | (oled ? snap::HAS_OLED : 0));
    out[6] = output;
    uint8_t* regs = out + snap::kHeader;
    uint8_t* ram = regs + snap::kRegs;
    if (osd) {
        osd->read(0x3FF0, regs, snap::kRegs);
        for (uint16_t a = 0; a < snap::kRam; a += snap::kReadChunk) osd->read(a, ram + a, snap::kReadChunk);
    }
    if (oled) memcpy(ram + snap::kRam, oled, snap::kOled);
    return snap::kBytes;
}

bool screenSnapshotParse(const uint8_t* in, size_t n, uint8_t& flags, uint8_t& output, uint8_t reg[16],
                         uint8_t ram[1024], uint8_t oled[1024]) {
    if (n != snap::kBytes || memcmp(in, kMagic, sizeof(kMagic)) != 0 || in[4] != snap::kVersion) return false;
    flags = in[5];
    output = in[6];
    memcpy(reg, in + snap::kHeader, snap::kRegs);
    memcpy(ram, in + snap::kHeader + snap::kRegs, snap::kRam);
    memcpy(oled, in + snap::kHeader + snap::kRegs + snap::kRam, snap::kOled);
    return true;
}
