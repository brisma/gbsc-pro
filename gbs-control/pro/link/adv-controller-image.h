// ADV Controller firmware image embedded in the ESP build; only adv-controller-image.cpp includes the generated data
#pragma once
#include <stdint.h>

uint32_t advControllerImageSize();                                // 0 when the build has no ADV Controller image
uint32_t advControllerImageCrc();                                 // build id (image CRC32, header field zeroed)
const char* advControllerImageVersion();
void advControllerImageRead(uint32_t offset, uint8_t* out, uint8_t n);
