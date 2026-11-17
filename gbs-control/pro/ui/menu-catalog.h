// Console catalog for the quick profile names: alphabetical, families in order of release, one packed PROGMEM string
#pragma once
#include <stdint.h>
#include <stddef.h>

// Most names the catalog may hold: the menu keeps a region per row in an array of this size (test)
static const uint8_t MENU_CATALOG_MAX = 48;

uint8_t menuCatalogCount();
void menuCatalogName(uint8_t i, char* out, size_t n);   // "" when out of range
