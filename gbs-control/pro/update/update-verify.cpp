#include <Arduino.h>
#include <Updater.h>
#include <bearssl/bearssl.h>
#include "update-verify.h"

// No firmware signatures: the firmware is open and anyone can rebuild it. Images of older releases
// still end with a signature trailer: a verifier that accepts it lets the core Updater strip it,
// otherwise eboot would read its length field as the gzip ISIZE and brick the board.
namespace {

class LeanHash : public UpdaterHashClass {
public:
    void begin() override { br_sha256_init(&ctx_); }
    void add(const void* d, uint32_t l) override { br_sha256_update(&ctx_, d, l); }
    void end() override { br_sha256_out(&ctx_, hash_); }
    int len() override { return 32; }
    const void* hash() override { return hash_; }
    const unsigned char* oid() override { return BR_HASH_OID_SHA256; }
private:
    br_sha256_context ctx_;
    uint8_t hash_[32];
};

class TrailerOnlyVerifier : public UpdaterVerifyClass {
public:
    uint32_t length() override { return 256; }
    bool verify(UpdaterHashClass*, const void*, uint32_t sigLen) override { return sigLen == 256; }
};

LeanHash gHash;
TrailerOnlyVerifier gTrailerOnly;

} // namespace

static_assert(sizeof(br_sha256_context) <= sizeof(UpdateSha256::ctx), "UpdateSha256 context too small");
void UpdateSha256::begin() { br_sha256_init((br_sha256_context*)ctx); }
void UpdateSha256::add(const void* data, size_t len) { br_sha256_update((br_sha256_context*)ctx, data, len); }
void UpdateSha256::end(uint8_t out[32]) { br_sha256_out((const br_sha256_context*)ctx, out); }

void updateInstallTrailerOnly(bool signedImage) {
    if (signedImage) Update.installSignature(&gHash, &gTrailerOnly);   // the Updater strips the trailer
    else Update.installSignature(nullptr, nullptr);
}
