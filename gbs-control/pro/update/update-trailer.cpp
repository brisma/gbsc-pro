#include <string.h>
#include "update-trailer.h"

static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

UpdateImageKind updateImageKind(const uint8_t* head, size_t n) {
    if (n >= 2 && head[0] == 0x1F && head[1] == 0x8B) return UPDATE_IMAGE_GZIP;
    if (n >= 1 && head[0] == 0xE9) return UPDATE_IMAGE_RAW;
    return UPDATE_IMAGE_UNKNOWN;
}

UpdateTrailer updateParseTrailer(const uint8_t* tail, size_t tailLen, UpdateImageKind kind) {
    UpdateTrailer t = {false, 0, false};
    if (tailLen < 4) return t;
    t.hasSignature = tailLen >= UPDATE_TAIL_BYTES && le32(tail + tailLen - 4) == UPDATE_SIG_LEN;
    if (kind == UPDATE_IMAGE_GZIP) {
        size_t isizeAt = t.hasSignature ? tailLen - UPDATE_TAIL_BYTES : tailLen - 4;
        t.isize = le32(tail + isizeAt);
    }
    t.valid = true;
    return t;
}

const char* updateCheckImage(UpdateImageKind kind, const UpdateTrailer& t, uint32_t maxSketch) {
    if (kind == UPDATE_IMAGE_UNKNOWN) return "format";
    if (!t.valid) return "trailer";
    if (kind == UPDATE_IMAGE_GZIP && (t.isize == 0 || t.isize > maxSketch)) return "too_big";
    return nullptr;
}

void TailBuffer::add(const uint8_t* d, size_t n) {
    if (n >= UPDATE_TAIL_BYTES) {
        memcpy(buf_, d + n - UPDATE_TAIL_BYTES, UPDATE_TAIL_BYTES);
        len_ = UPDATE_TAIL_BYTES;
        return;
    }
    size_t keep = len_ + n > UPDATE_TAIL_BYTES ? UPDATE_TAIL_BYTES - n : len_;
    memmove(buf_, buf_ + len_ - keep, keep);
    memcpy(buf_ + keep, d, n);
    len_ = keep + n;
}

