#include "api-route-match.h"
#include <string.h>
#include "../settings/settings-pgm.h"

int apiRouteFind(const void* rows, size_t stride, uint8_t n, uint8_t method, const char* path) {
    for (uint8_t i = 0; i < n; i++) {
        ApiRouteKey k;
        memcpy_P(&k, (const char*)rows + (size_t)i * stride, sizeof(k));
        if (!(k.methods & method)) continue;
        if (k.match == ROUTE_PREFIX) {
            size_t len = strnlen(k.path, sizeof(k.path));
            if (!strncmp(path, k.path, len) && path[len]) return i;   // a name must follow the prefix
        } else if (!strncmp(path, k.path, sizeof(k.path)) && strlen(path) < sizeof(k.path)) {
            return i;
        }
    }
    return -1;
}
