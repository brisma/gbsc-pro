// STV9426 RAM map and strip descriptors (datasheet §I.3, §II; pure, host-testable)
#pragma once
#include <stdint.h>

uint8_t stvCharCode(char c);   // ASCII -> STV9426 ROM code (the ROM remaps + = -)

namespace stv {
// RAM (1 KB; no arrow strips, glyphs loaded on demand):
//   0x000-0x025 descriptor table (page 0, 19 slots; inside character number 0's bytes, so UDC 0 is unused)
//   0x026-0x1C7 11 UDC slots, character numbers 1..11 (38 x n), filled by Stv9426::glyph() per screen
//   0x1D2-0x3FF text: 9 rows x (30 cells + end of line)
const uint8_t  kUdcSlots = 11, kUdcBytes = 38, kUdcFirst = 1;
const uint16_t kPageAddr = 0x000;  const uint8_t kPageP = 0;  const uint8_t kDescSlots = 19;
const uint16_t kTextAddr = 0x1D2;      const uint8_t kRows = 9, kRowCells = 31, kRowWidth = 30;
const uint8_t  kEol = 0xFF, kMaxContent = 15;
inline uint16_t rowAddr(uint8_t r) { return (uint16_t)(kTextAddr + r * kRowCells * 2); }
uint16_t spacingDesc(uint8_t lines);            // lines 1..255
uint16_t charDesc(uint16_t addr, bool zoom);
enum StripKind : uint8_t { STRIP_SPACE, STRIP_ROW };
struct Strip { uint8_t kind; uint8_t arg; bool zoom; };   // arg: lines (SPACE) or row 0..8 (ROW)
struct Layout { uint8_t count; Strip strips[kMaxContent]; };
bool buildDescriptors(const Layout& l, uint8_t out[kDescSlots * 2]);   // false: too many strips or bad arg
Layout compatLayout();   // 58-line spacing + rows 0,1,2 zoomed (set by begin())
}
