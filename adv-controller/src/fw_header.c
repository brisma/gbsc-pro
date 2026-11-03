/* Firmware image header (link v3): linked at app offset 0x400 (section .fw_header); length and CRC32
 * are patched into the .bin after linking by tools/fw-header.py */
#include <stdint.h>
#include "fw_header.h"

typedef struct {
    uint32_t magic;          /* bytes 'A','D','V','C' (ADV Controller image) */
    uint16_t headerVersion;
    uint16_t reserved;
    char version[12];
    uint32_t imageLen;       /* patched after link */
    uint32_t imageCrc32;     /* patched after link; also the build id */
} FwHeader;

_Static_assert(sizeof(ADV_FW_VERSION) <= 12, "ADV_FW_VERSION must fit version[12] with its NUL");

__attribute__((section(".fw_header"), used))
const FwHeader g_fwHeader = {0x43564441u, 1, 0, ADV_FW_VERSION, 0, 0};

uint32_t fwBuildId(void)
{
    /* The compiler sees imageCrc32 = 0 in the initializer; the real value is patched into the image */
    return *(volatile const uint32_t*)&g_fwHeader.imageCrc32;
}
