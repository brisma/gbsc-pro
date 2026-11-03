// OTA manifest v1 (pure, host-testable; frozen from 3.0.0). ASCII, LF, at most 1024 bytes, key=value lines in any
// order, keys [a-z0-9_]+, blank lines and a missing final LF accepted, unknown keys ignored, a repeated key or a line without '=' rejected:
//   version=<1..15 of [0-9A-Za-z.-]>   size=<image bytes>   sha256=<64 hex>   file=<fw/... relative to v1/>
//   notes=<optional URL, < 96>
// No signature: size and sha256 catch a corrupted download.
#pragma once
#include <stdint.h>
#include <stddef.h>

static const size_t MANIFEST_MAX_BYTES = 1024;

struct Manifest {
    char version[16];
    uint32_t size;
    uint8_t sha256[32];
    char file[64];
    char notes[96];
};

bool manifestParse(const char* text, size_t len, Manifest& m);
// -1, 0, 1. X.Y.Z numeric, then "-devN" < "-betaN" < no suffix with N numeric; any other suffix sorts below
// "-dev" (a local build). Frozen from 3.0.0: tools/ota/version.py and the web app use the same order
int versionCompare(const char* a, const char* b);
