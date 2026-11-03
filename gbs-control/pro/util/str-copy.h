// Bounded string copy that always terminates (strlcpy is missing on some host libcs used by the native tests)
#pragma once
#include <stddef.h>
#include <string.h>

inline void copyStr(char* out, size_t n, const char* s) {
    if (!n) return;
    strncpy(out, s ? s : "", n - 1);
    out[n - 1] = '\0';
}
