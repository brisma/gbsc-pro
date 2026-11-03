// The declarative menu tree: PROGMEM nodes, option lists, choices and adjust mappings
#pragma once
#include <stdint.h>
#include "menu-types.h"

const MenuNode* menuRoot();                                     // PROGMEM: the root SUB node
const MenuNode* menuQuickRoot();                                // PROGMEM: the Quick menu
void menuNodeRead(const MenuNode* pgm, MenuNode& out);

// One option of a choice: up to two setting writes (sid2 0xFF: unused) and CHO_* flags
struct MenuChoiceOpt { uint16_t label; uint8_t sid1; uint16_t v1; uint8_t sid2; uint16_t v2; uint8_t flags; };
enum : uint8_t {
    CHO_ADVANCED = 1,      // shown in Advanced menu mode only (Easy still shows it when it is the current value)
    CHO_ORIGINAL = 2,      // standard board only (CAP_ORIGINAL)
    CHO_ALIAS = 4,         // matches but is never offered: reads as the option with the same label
    CHO_SID1_ONLY = 8,     // matches on the first setting alone (Scanlines Off ignores the strength)
};
struct MenuChoice { uint8_t count; const MenuChoiceOpt* opts; };   // opts: PROGMEM, read with menuChoiceOptRead
void menuChoiceRead(uint8_t index, MenuChoice& out);
void menuChoiceOptRead(const MenuChoice& c, uint8_t i, MenuChoiceOpt& out);

struct MenuAdjust { uint8_t left, right, up, down; };             // ActionIds run by the arrow keys
void menuAdjustRead(uint8_t index, MenuAdjust& out);

const char* menuOptList(uint8_t index);                         // PROGMEM "Label=value|..." (0 and unknown: nullptr)

enum : uint8_t { OL_NONE, OL_DEINT, OL_FILTER_Y, OL_FILTER_WY, OL_WY_MODE, OL_FILTER_C, OL_COMB_LUMA, OL_COMB_CHROMA,
                 OL_SYNCWATCHER, OL_COUNT };   // comb bandwidth and chroma taps: registry options (settings-table.h)
enum : uint8_t { OPTS_STEP16 = 116 };                           // numeric setting stepped by 16
enum : uint8_t { CH_FILTER, CH_SCANLINES, CH_RESOLUTION };
enum : uint8_t { AJ_MOVE, AJ_SIZE, AJ_BORDERS, AJ_BORDER_MOVE };
