// Device side of the ADV preset and macro store (adv-store-web.cpp)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "adv-store.h"

void advStoreLoop();                                                  // from apiLoop(): one queued save or delete
uint8_t advStoreIds(AdvKind k, char ids[][4], const char* idp[]);    // the files present (backup snapshot)
void advStoreDeleteAll();                                             // restore with [adv]
uint32_t advStoreBlocks(size_t blockSize);                            // blocks the ADV files take now
