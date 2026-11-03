/* Firmware image header accessors (link v3) */
#ifndef FW_HEADER_H
#define FW_HEADER_H
#include <stdint.h>
uint32_t fwBuildId(void);   /* image CRC32 patched after link (read through a volatile pointer) */
#endif
