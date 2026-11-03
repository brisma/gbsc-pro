// Console catalog for the quick profile names: alphabetical, one packed PROGMEM string
#pragma once
#include <stdint.h>
#include <stddef.h>

uint8_t menuCatalogCount();
void menuCatalogName(uint8_t i, char* out, size_t n);   // "" when out of range
