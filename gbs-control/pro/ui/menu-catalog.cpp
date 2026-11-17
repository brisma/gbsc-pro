#include "menu-catalog.h"
#include "../settings/settings-pgm.h"

// Alphabetical by name; inside a family (same first word: Amiga, Amstrad, Atari, Commodore, Neo Geo, PlayStation) in
// order of release, the generic family name first. Names up to 18 characters: with any region they fit a label
// whole (PROFILE_LABEL_MAX)
static const char kCatalog[] SET_PROGMEM =
    "3DO\0Amiga\0Amiga 500\0Amiga 1200\0Amiga CD32\0Amstrad CPC 464\0Amstrad CPC 6128\0Apple II\0Arcade\0"
    "Atari 2600\0Atari 5200\0Atari 800XL\0Atari 130XE\0Atari ST\0Atari 7800\0Atari Jaguar\0BBC Micro\0CD-i\0"
    "ColecoVision\0Commodore VIC-20\0Commodore 64\0Commodore 128\0Dreamcast\0GameCube\0Genesis/Mega Drive\0"
    "Intellivision\0Master System\0Mega CD\0MSX\0N64\0Neo Geo\0Neo Geo CD\0NES/Famicom\0PC\0PC Engine/TG-16\0"
    "PlayStation\0PlayStation 2\0Saturn\0Sega 32X\0SNES/Super Famicom\0Wii\0Xbox\0ZX Spectrum\0";
static const uint8_t kCount = 43;

uint8_t menuCatalogCount() { return kCount; }

void menuCatalogName(uint8_t i, char* out, size_t n) {
    if (!n) return;
    out[0] = '\0';
    if (i >= kCount) return;
    const char* p = kCatalog;
    while (i--) while (pgm_read_byte(p++)) {}
    size_t k = 0;
    char c;
    while ((c = (char)pgm_read_byte(p++)) && k + 1 < n) out[k++] = c;
    out[k] = '\0';
}
