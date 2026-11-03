/* Self-update: copies the verified staging image over the app and resets (link v3) */
#ifndef FW_COPY_H
#define FW_COPY_H
#include <stdint.h>
void hc32CopyAndReset(uint32_t size);   /* never returns */
#endif
