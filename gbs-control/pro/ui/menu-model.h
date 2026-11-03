// Menu model rules (pure): visibility, enable state with reasons, value formatting and stepping
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "menu-types.h"
#include "menu-backend.h"

uint8_t menuInput(MenuBackend& be);                                   // the active input (1..6)
// Source rate word: "PAL" under 55 Hz, "NTSC" above, "" while the rate is not measured yet (srcHzX10 = 0)
const char* menuRateStandard(uint16_t srcHzX10);
bool menuVisible(const MenuNode& n, MenuBackend& be);
bool menuEnabled(const MenuNode& n, MenuBackend& be, uint16_t* reason);   // reason: MenuStr, S_NONE when enabled
bool menuOptAt(const char* pgmList, uint8_t i, char* name, size_t n, uint16_t& value);   // streaming parser
struct ProfileEntry;
// "<name without its region word> <region>" (region omitted when none), sanitized
void menuProfileEntryLabel(const ProfileEntry& e, char* out, size_t n);
// LT_PROFILE_LABEL: the active profile's label, or "Defaults" when none is loaded
void menuProfileLabel(MenuBackend& be, char* out, size_t n);
void menuFormatValue(const MenuNode& n, MenuBackend& be, char* out, size_t len);   // "" when the row has no value
bool menuIsChanged(const MenuNode& n, MenuBackend& be);                // SETTING value != registry default
int menuSliderHalves(const MenuNode& n, MenuBackend& be);              // 0..20 for numeric settings, -1 otherwise
bool menuStepValue(const MenuNode& n, MenuBackend& be, int dir, uint16_t& out);
bool menuStepFrom(const MenuNode& n, MenuBackend& be, uint16_t cur, int dir, uint16_t& out);   // from a given value
void menuFormatSettingValue(const MenuNode& n, uint16_t v, char* out, size_t len);           // SETTING row value
void menuSanitize(char* s);                                           // '_' -> ' ', other non-ROM chars -> '.'
// Choices: current option (-1 = Custom; an alias reads as the option with its label), stepping over
// the offered options (Easy hides CHO_ADVANCED, CHO_ORIGINAL needs that board), the writes of an option
int menuChoiceCurrent(uint8_t choice, MenuBackend& be);
bool menuChoiceStep(uint8_t choice, MenuBackend& be, int cur, int dir, uint8_t& next);
uint8_t menuChoiceWrites(uint8_t choice, uint8_t opt, MenuWrite* w);   // 1 or 2 writes
void menuFormatChoice(uint8_t choice, int opt, char* out, size_t len);  // option label or "Custom"
