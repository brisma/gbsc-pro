// STV9426 user-definable characters (datasheet §VI): 12 x 18 pixels + one shadow slice, 38 bytes each
#pragma once
#include <stdint.h>

enum StvUdc : uint8_t { UDC_SPEAKER, UDC_MUTED, UDC_CHECK, UDC_LOCK, UDC_WIFI, UDC_SYNC, UDC_MONITOR,
                        UDC_SLIDER_FULL, UDC_SLIDER_HALF, UDC_SLIDER_TRACK, UDC_PERCENT,   // PERCENT: '%' (not in the ROM)
                        UDC_CHEV_L, UDC_CHEV_R, UDC_CHEV_U, UDC_CHEV_D, UDC_BAR_TRACK, UDC_BAR_THUMB,
                        UDC_QUESTION,   // '?' (not in the ROM)
                        UDC_COMMA,      // ',' (not in the ROM): the Extra hosts keyboard
                        UDC_COUNT };

// Slice s: out[2s] = PX7..PX0, out[2s+1] = PX11..PX8 (PX11 = leftmost pixel); slice 18 repeats slice 17
// (the slice interpolator shows slice 18 on the last line of a strip when CH > 18)
void stvUdcBytes(uint8_t n, uint8_t out[38]);
// Codes for the glyphs come from Stv9426::glyph() (dynamic slots, stv9426-map.h)
