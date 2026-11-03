// Firmware image checks done while streaming an upload (pure, host-testable)
#pragma once
#include <stdint.h>
#include <stddef.h>

static const uint32_t UPDATE_SIG_LEN = 256;
static const uint32_t UPDATE_TAIL_BYTES = 264;   // gzip ISIZE (4) + an older image's signature (256) + its length (4)

enum UpdateImageKind : uint8_t { UPDATE_IMAGE_UNKNOWN, UPDATE_IMAGE_GZIP, UPDATE_IMAGE_RAW };
struct UpdateTrailer { bool hasSignature; uint32_t isize; bool valid; };

UpdateImageKind updateImageKind(const uint8_t* head, size_t n);
// One byte that may start a gzip image (0x1F): wait for the second before deciding (a 1-byte first network chunk)
inline bool updateKindNeedsMore(const uint8_t* head, size_t n) { return n == 1 && head[0] == 0x1F; }
UpdateTrailer updateParseTrailer(const uint8_t* tail, size_t tailLen, UpdateImageKind kind);
// nullptr when the image may be installed (gzip or raw, no signatures); otherwise "format",
// "too_big" or "trailer". A signature trailer of an older release (hasSignature) must still be stripped by the
// Updater (updateInstallTrailerOnly), else eboot reads its length field as the gzip ISIZE and bricks the board
const char* updateCheckImage(UpdateImageKind kind, const UpdateTrailer& t, uint32_t maxSketch);

// An upload chunk belongs to the running session only if it starts where the previous one ended; `held`: the first
// byte of a gzip image (one-byte first chunk) is kept back until the second one says what the image is
inline bool updateChunkContinues(size_t index, size_t received, bool held = false) { return index == received + (held ? 1 : 0); }

// Keeps the last UPDATE_TAIL_BYTES bytes of a stream
class TailBuffer {
public:
    void add(const uint8_t* d, size_t n);
    const uint8_t* data() const { return buf_; }
    size_t size() const { return len_; }
private:
    uint8_t buf_[UPDATE_TAIL_BYTES];
    size_t len_ = 0;
};
