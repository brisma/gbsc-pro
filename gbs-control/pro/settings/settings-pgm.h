// PROGMEM shims: pure settings code compiles on the ESP8266 and on the host
#pragma once
#ifdef ARDUINO
#include <pgmspace.h>
#define SET_PROGMEM PROGMEM
#else
#include <string.h>
#define SET_PROGMEM
#define memcpy_P memcpy
#define strcmp_P strcmp
#define strncpy_P strncpy
#define strlen_P strlen
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#endif
