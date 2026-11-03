#include <string.h>
#include "../settings/settings-pgm.h"
#include "menu-strings.h"

#define MENU_STR_DEF(id, text) static const char kMs_##id[] SET_PROGMEM = text;
MENU_STRINGS(MENU_STR_DEF)
#undef MENU_STR_DEF

#define MENU_STR_PTR(id, text) kMs_##id,
static const char* const kMenuStrs[] SET_PROGMEM = { MENU_STRINGS(MENU_STR_PTR) };
#undef MENU_STR_PTR

static const char* strAt(uint16_t id) {
    if (id >= S_COUNT) id = S_NONE;
    const char* p;
    memcpy_P(&p, &kMenuStrs[id], sizeof(p));
    return p;
}

void menuStrCopy(uint16_t id, char* out, size_t n) {
    if (!n) return;
    strncpy_P(out, strAt(id), n - 1);
    out[n - 1] = '\0';
}

uint8_t menuStrLen(uint16_t id) { return (uint8_t)strlen_P(strAt(id)); }
