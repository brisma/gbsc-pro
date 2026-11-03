#include "menu-catalog.h"
#include "../settings/settings-pgm.h"

// Alphabetical (case-insensitive), names up to 18 characters; profileNameWithRegion shortens the base when
// "<name> <region>" exceeds 24
static const char kCatalog[] SET_PROGMEM =
    "3DO\0Amiga\0Atari 2600\0Atari 7800\0Atari Jaguar\0Atari ST\0C64\0CD-i\0ColecoVision\0Dreamcast\0"
    "GameCube\0Genesis/Mega Drive\0Intellivision\0Master System\0Mega CD\0MSX\0N64\0Neo Geo\0NES/Famicom\0"
    "PC Engine/TG-16\0PlayStation\0PlayStation 2\0Saturn\0Sega 32X\0SNES/Super Famicom\0Wii\0Xbox\0ZX Spectrum\0";
static const uint8_t kCount = 28;

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
