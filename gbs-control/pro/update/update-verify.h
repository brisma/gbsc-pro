#pragma once
#include <stdint.h>
#include <stddef.h>
// No firmware signatures. A signature trailer of an older release image is stripped by the Updater
// when signedImage is true (call before Update.end); plain images need no verifier.
void updateInstallTrailerOnly(bool signedImage);

// Streaming SHA-256 (BearSSL): the GitHub image against its manifest
struct UpdateSha256 {
    void begin();
    void add(const void* data, size_t len);
    void end(uint8_t out[32]);
    alignas(8) uint8_t ctx[112];   // br_sha256_context, kept opaque to avoid pulling bearssl.h into every includer
};
