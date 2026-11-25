#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include "adv-image-header.h"
#include "update-rtc.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t gImg[4096];

static void put32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

// An image as tools/adv-controller-image.py's self-test builds it: header at 0x400, CRC over the image with the field zeroed
static void build(uint32_t len, const char* ver) {
    srand(7);
    for (uint32_t i = 0; i < len; i++) gImg[i] = (uint8_t)rand();
    uint8_t* h = gImg + ADV_IMAGE_HDR_OFF;
    memcpy(h, "ADVC", 4);
    h[4] = 1; h[5] = 0; h[6] = 0; h[7] = 0;
    memset(h + 8, 0, 12);
    memcpy(h + 8, ver, strlen(ver));
    put32(h + 20, len);
    put32(h + 24, 0);
    put32(h + 24, crc32Update(0, gImg, len));
}

static const char* feed(uint32_t len, size_t chunk, AdvImageHeader& out) {
    AdvImageCheck c;
    for (uint32_t at = 0; at < len; at += (uint32_t)chunk) c.add(gImg + at, at + chunk > len ? len - at : chunk);
    return c.done(out);
}

void test_whole_image_any_chunking(void) {
    build(4096, "3.1.0");
    static const size_t kChunks[] = {1, 3, 7, 16, 28, 100, 1024, 1460, 4096};
    for (size_t k = 0; k < sizeof(kChunks) / sizeof(kChunks[0]); k++) {
        AdvImageHeader h;
        TEST_ASSERT_NULL_MESSAGE(feed(4096, kChunks[k], h), "valid image refused");
        TEST_ASSERT_EQUAL_STRING("3.1.0", h.version);
        TEST_ASSERT_EQUAL_UINT32(4096, h.length);
    }
}

void test_crc_field_is_not_part_of_the_crc(void) {
    build(2048, "9.8.7");
    AdvImageHeader h;
    TEST_ASSERT_NULL(feed(2048, 5, h));
    uint32_t stored = h.crc;
    put32(gImg + ADV_IMAGE_CRC_OFF, 0);
    TEST_ASSERT_EQUAL_HEX32(stored, crc32Update(0, gImg, 2048));
}

void test_flipped_bit_is_crc(void) {
    build(4096, "3.1.0");
    gImg[10] ^= 1;
    AdvImageHeader h;
    TEST_ASSERT_EQUAL_STRING("crc", feed(4096, 512, h));
}

void test_truncated_is_size(void) {
    build(4096, "3.1.0");
    AdvImageHeader h;
    TEST_ASSERT_EQUAL_STRING("size", feed(4000, 512, h));
}

void test_too_short_or_no_magic_is_format(void) {
    build(4096, "3.1.0");
    AdvImageHeader h;
    TEST_ASSERT_EQUAL_STRING("format", feed(0x410, 64, h));    // ends inside the header
    gImg[ADV_IMAGE_HDR_OFF] = 'X';
    TEST_ASSERT_EQUAL_STRING("format", feed(4096, 512, h));    // an ESP image, a random file
}

void test_header_checks(void) {
    build(4096, "3.1.0-beta2");
    AdvImageHeader h;
    TEST_ASSERT_TRUE(advImageHeaderParse(gImg + ADV_IMAGE_HDR_OFF, h));
    TEST_ASSERT_EQUAL_STRING("3.1.0-beta2", h.version);
    gImg[ADV_IMAGE_HDR_OFF + 8 + 2] = '"';                     // would break the JSON
    TEST_ASSERT_FALSE(advImageHeaderParse(gImg + ADV_IMAGE_HDR_OFF, h));
    build(4096, "");
    TEST_ASSERT_FALSE(advImageHeaderParse(gImg + ADV_IMAGE_HDR_OFF, h));
    build(4096, "123456789012");                               // all 12 bytes used, no NUL
    TEST_ASSERT_TRUE(advImageHeaderParse(gImg + ADV_IMAGE_HDR_OFF, h));
    TEST_ASSERT_EQUAL_STRING("123456789012", h.version);
    put32(gImg + ADV_IMAGE_HDR_OFF + 20, ADV_IMAGE_MAX_BYTES + 1);
    TEST_ASSERT_FALSE(advImageHeaderParse(gImg + ADV_IMAGE_HDR_OFF, h));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_whole_image_any_chunking);
    RUN_TEST(test_crc_field_is_not_part_of_the_crc);
    RUN_TEST(test_flipped_bit_is_crc);
    RUN_TEST(test_truncated_is_size);
    RUN_TEST(test_too_short_or_no_magic_is_format);
    RUN_TEST(test_header_checks);
    return UNITY_END();
}
