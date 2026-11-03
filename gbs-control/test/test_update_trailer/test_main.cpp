#include <string.h>
#include <vector>
#include <unity.h>
#include "update-trailer.h"

void setUp(void) {}
void tearDown(void) {}

static std::vector<uint8_t> gzipImage(uint32_t isize, bool signedImage) {
    std::vector<uint8_t> v = {0x1F, 0x8B, 0x08, 0x00};
    v.resize(1000, 0xAA);
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)(isize >> (8 * i)));   // gzip ISIZE, little endian
    if (signedImage) {
        v.resize(v.size() + 256, 0x55);
        uint32_t len = 256;
        for (int i = 0; i < 4; i++) v.push_back((uint8_t)(len >> (8 * i)));
    }
    return v;
}

static UpdateTrailer parse(const std::vector<uint8_t>& img, UpdateImageKind k) {
    TailBuffer t;
    for (size_t i = 0; i < img.size(); i += 100) t.add(&img[i], img.size() - i < 100 ? img.size() - i : 100);
    return updateParseTrailer(t.data(), t.size(), k);
}

void test_kind(void) {
    uint8_t gz[] = {0x1F, 0x8B}, raw[] = {0xE9, 0x03}, bad[] = {0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(UPDATE_IMAGE_GZIP, updateImageKind(gz, 2));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_IMAGE_RAW, updateImageKind(raw, 2));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_IMAGE_UNKNOWN, updateImageKind(bad, 2));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_IMAGE_UNKNOWN, updateImageKind(gz, 1));
}

// A first network chunk of one byte (0x1F) cannot tell a gzip image yet: the caller waits for the next byte
// instead of failing a valid image with "format"
void test_kind_needs_a_second_byte(void) {
    uint8_t gz[] = {0x1F}, raw[] = {0xE9}, bad[] = {0x00};
    TEST_ASSERT_TRUE(updateKindNeedsMore(gz, 1));
    TEST_ASSERT_FALSE(updateKindNeedsMore(raw, 1));    // a raw image is known from its first byte
    TEST_ASSERT_FALSE(updateKindNeedsMore(bad, 1));
    uint8_t gz2[] = {0x1F, 0x8B};
    TEST_ASSERT_FALSE(updateKindNeedsMore(gz2, 2));
}

// No signatures: every gzip or raw build may be installed; a trailer from an older signed
// release is still recognised so the Updater can strip it (else eboot reads its length as the gzip ISIZE)
void test_signed_gzip_from_an_older_release(void) {
    UpdateTrailer t = parse(gzipImage(900000, true), UPDATE_IMAGE_GZIP);
    TEST_ASSERT_TRUE(t.valid);
    TEST_ASSERT_TRUE(t.hasSignature);
    TEST_ASSERT_EQUAL_UINT32(900000, t.isize);
    TEST_ASSERT_NULL(updateCheckImage(UPDATE_IMAGE_GZIP, t, 1044464));
}

void test_plain_gzip(void) {
    UpdateTrailer t = parse(gzipImage(900000, false), UPDATE_IMAGE_GZIP);
    TEST_ASSERT_TRUE(t.valid);
    TEST_ASSERT_FALSE(t.hasSignature);
    TEST_ASSERT_EQUAL_UINT32(900000, t.isize);
    TEST_ASSERT_NULL(updateCheckImage(UPDATE_IMAGE_GZIP, t, 1044464));
}

void test_oversized_raw_and_unknown(void) {
    UpdateTrailer t = parse(gzipImage(1100000, false), UPDATE_IMAGE_GZIP);
    TEST_ASSERT_EQUAL_STRING("too_big", updateCheckImage(UPDATE_IMAGE_GZIP, t, 1044464));
    UpdateTrailer r = {false, 0, true};
    TEST_ASSERT_NULL(updateCheckImage(UPDATE_IMAGE_RAW, r, 1044464));
    TEST_ASSERT_EQUAL_STRING("format", updateCheckImage(UPDATE_IMAGE_UNKNOWN, r, 1044464));
}

void test_short_or_wrong_trailer(void) {
    uint8_t tiny[3] = {1, 2, 3};
    UpdateTrailer t = updateParseTrailer(tiny, 3, UPDATE_IMAGE_GZIP);
    TEST_ASSERT_FALSE(t.valid);
    TEST_ASSERT_EQUAL_STRING("trailer", updateCheckImage(UPDATE_IMAGE_GZIP, t, 1044464));
    std::vector<uint8_t> img = gzipImage(900000, true);
    img[img.size() - 4] = 0x00; img[img.size() - 3] = 0x02;   // sigLen 512: not ours → a plain gzip
    UpdateTrailer u = parse(img, UPDATE_IMAGE_GZIP);
    TEST_ASSERT_FALSE(u.hasSignature);
}

void test_chunks_must_continue_the_stream(void) {
    // A retry whose request reuses the old address must not append to a dead upload
    TEST_ASSERT_TRUE(updateChunkContinues(4096, 4096));
    TEST_ASSERT_FALSE(updateChunkContinues(1024, 4096));
    TEST_ASSERT_FALSE(updateChunkContinues(8192, 4096));
    // A first chunk of one byte (0x1F) is held, not counted: the next chunk starts at index 1
    TEST_ASSERT_TRUE(updateChunkContinues(1, 0, true));
    TEST_ASSERT_FALSE(updateChunkContinues(0, 0, true));
    TEST_ASSERT_FALSE(updateChunkContinues(1, 0, false));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_kind);
    RUN_TEST(test_signed_gzip_from_an_older_release);
    RUN_TEST(test_plain_gzip);
    RUN_TEST(test_oversized_raw_and_unknown);
    RUN_TEST(test_short_or_wrong_trailer);
    RUN_TEST(test_chunks_must_continue_the_stream);
    RUN_TEST(test_kind_needs_a_second_byte);
    return UNITY_END();
}
