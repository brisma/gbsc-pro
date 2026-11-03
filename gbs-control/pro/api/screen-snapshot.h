// Developer screen readback (pure, host-tested): the STV9426 RAM and registers read back from the chip, and the
// OLED frame buffer, packed for GET /api/dev/snapshot. A host tool decodes the OSD part with the
// simulator's decoder (FakeStv::decode) and writes the OLED part as a PBM, so device screens compare with goldens.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../drivers/stv9426-bus.h"

namespace snap {
// Layout: "GSNP" version flags output reserved | registers 0x3FF0-0x3FFF | RAM 0x000-0x3FF | OLED 128x64 buffer
const uint8_t kVersion = 1;
enum : uint8_t { HAS_OSD = 1, HAS_OLED = 2 };
const size_t kHeader = 8, kRegs = 16, kRam = 1024, kOled = 1024;
const size_t kBytes = kHeader + kRegs + kRam + kOled;
const uint8_t kReadChunk = 32;   // bytes per bus read (the device Wire buffer)
}

// osd/oled null: that part is absent (zeros, flag clear). output: the driver's StvOutStd (frame lines to decode).
size_t screenSnapshotBuild(OsdBus* osd, uint8_t output, const uint8_t* oled, uint8_t* out);
bool screenSnapshotParse(const uint8_t* in, size_t n, uint8_t& flags, uint8_t& output, uint8_t reg[16],
                         uint8_t ram[1024], uint8_t oled[1024]);
