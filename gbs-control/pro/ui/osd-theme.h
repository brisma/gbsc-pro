// OSD colour roles per theme -> STV9426 attribute bytes
#pragma once
#include <stdint.h>

enum OsdRole : uint8_t {
    R_N,     // normal row text
    R_S,     // selected text (non-cursor)
    R_C,     // cursor bar
    R_D,     // disabled
    R_T,     // title bar, pills
    R_H,     // help row
    R_A,     // accent: changed values, arrows, countdown
    R_F,     // faint: group headers
    R_LB,    // info labels
    R_GOOD,  // positive status tag (not R_OK: the access() macro of <unistd.h>, <io.h> on MinGW)
    R_BAD,   // negative status tag, mute
    R_BF,    // slider fill on normal rows
    R_DC,    // disabled row under the cursor: disabled text on the cursor background
    R_MSG,   // a short message on the help row over a menu: toast, revert countdown
    R_COUNT
};

uint8_t osdAttr(uint8_t theme, uint8_t role);   // theme = osd.theme: 0 GBSC PRO (default), 1 Classic, 2 Light, 3 Retro
static const uint8_t OSD_ATTR_TRANSPARENT = 0x80;   // BK3 = 1, BK1 = 0: transparent background
static const uint8_t OSD_BLANK = 0x9A;              // SET = 1, character 26: a space (datasheet §III)
