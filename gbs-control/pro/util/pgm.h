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
#define snprintf_P snprintf
#define vsnprintf_P vsnprintf
#define strlen_P strlen
#define memcpy_P memcpy
#ifndef PROGMEM
#define PROGMEM
#endif
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#endif
