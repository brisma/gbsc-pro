#include "stv9426-map.h"
uint8_t stvCharCode(char c) {
    switch (c) {
        case '+': return 0x3C;
        case '=': return 0x3D;
        case '-': return 0x3E;
        default: return (uint8_t)c;   // ' . / : and the alphanumerics are at their ASCII positions
    }
}

namespace stv {
uint16_t spacingDesc(uint8_t lines) { return lines; }
uint16_t charDesc(uint16_t addr, bool zoom) {
    return (uint16_t)(0x8000 | 0x4000 | (zoom ? 0x1000 : 0) | (addr & 0x3FE));
}
bool buildDescriptors(const Layout& l, uint8_t out[kDescSlots * 2]) {
    if (l.count > kMaxContent) return false;
    for (uint8_t i = 0; i < kDescSlots; i++) {
        uint16_t v = spacingDesc(255);
        if (i < l.count) {
            const Strip& s = l.strips[i];
            switch (s.kind) {
                case STRIP_SPACE: if (s.arg == 0) return false; v = spacingDesc(s.arg); break;
                case STRIP_ROW: if (s.arg >= kRows) return false; v = charDesc(rowAddr(s.arg), s.zoom); break;
                default: return false;
            }
        }
        out[2 * i] = (uint8_t)(v & 0xFF);
        out[2 * i + 1] = (uint8_t)(v >> 8);
    }
    return true;
}
Layout compatLayout() {
    Layout l; l.count = 4;
    l.strips[0] = {STRIP_SPACE, 58, false};
    for (uint8_t r = 0; r < 3; r++) l.strips[1 + r] = {STRIP_ROW, r, true};
    return l;
}
}
