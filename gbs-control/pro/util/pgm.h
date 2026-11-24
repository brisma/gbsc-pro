// Flash strings in pure code (compiled for the ESP8266 and for the host tests): on the ESP8266 a string literal sits
// in RAM, a PSTR() one in flash; *_P formats read their format from flash
#pragma once
#ifdef ARDUINO
#include <pgmspace.h>
#else
#include <stdio.h>
#include <string.h>
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef snprintf_P
#define snprintf_P snprintf
#endif
#ifndef vsnprintf_P
#define vsnprintf_P vsnprintf
#endif
#ifndef strlen_P
#define strlen_P strlen
#endif
#ifndef memcpy_P
#define memcpy_P memcpy
#endif
#ifndef strncpy_P
#define strncpy_P strncpy
#endif
#ifndef strcmp_P
#define strcmp_P strcmp
#endif
#ifndef strncmp_P
#define strncmp_P strncmp
#endif
#ifndef PGM_P
#define PGM_P const char*
#endif
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#endif
#endif

#include <stdint.h>
// Name tables in flash: "a\0b\0c" in one PROGMEM string, not an array of pointers to RAM strings (on the ESP8266 each
// literal and each pointer costs RAM). The i-th of 'count' strings, past the end the last one; print it with %S.
static inline PGM_P pgmListAt(PGM_P list, uint8_t i, uint8_t count) {
    if (count == 0) return list;
    if (i >= count) i = (uint8_t)(count - 1);
    PGM_P p = list;
    while (i--) p += strlen_P(p) + 1;
    return p;
}
// The same, copied to RAM (code shared with the host tests prints it with %s): out, cut to n - 1 characters
static inline const char* pgmListCopy(char* out, size_t n, PGM_P list, uint8_t i, uint8_t count) {
    if (!n) return out;
    strncpy_P(out, pgmListAt(list, i, count), n - 1);
    out[n - 1] = '\0';
    return out;
}
// The number of strings in a list literal (its separators and its end): static_assert a table against its enum
constexpr unsigned pgmListCount(const char* s, unsigned n) { return n == 0 ? 0 : (s[n - 1] == '\0' ? 1u : 0u) + pgmListCount(s, n - 1); }
#define PGM_LIST_COUNT(lit) pgmListCount(lit, sizeof(lit))
// The length of the longest string in a list literal: static_assert the buffer a name is copied to
constexpr unsigned pgmListMaxLen(const char* s, unsigned n, unsigned i, unsigned cur, unsigned best) {
    return i >= n ? best
                  : (s[i] == '\0' ? pgmListMaxLen(s, n, i + 1, 0, cur > best ? cur : best) : pgmListMaxLen(s, n, i + 1, cur + 1, best));
}
#define PGM_LIST_MAXLEN(lit) pgmListMaxLen(lit, sizeof(lit), 0, 0, 0)
