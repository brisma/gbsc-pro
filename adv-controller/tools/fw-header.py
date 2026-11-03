#!/usr/bin/env python3
"""ADV Controller firmware image header (link v3).

The header (section .fw_header, app offset 0x400) is:
  u32 magic ('A','D','V','C'), u16 header_version, u16 reserved, char version[12], u32 image_len, u32 image_crc32
image_crc32 = zlib CRC32 of the whole image with the image_crc32 field zeroed (image_len already written);
it doubles as the build id.

Usage: fw-header.py --patch <bin>   writes image_len and image_crc32
       fw-header.py --check <bin>   exits 1 when the header is missing or wrong
"""
import struct
import sys
import zlib

OFFSET = 0x400
MAGIC = b"ADVC"
FMT = "<4sHH12sII"
SIZE = struct.calcsize(FMT)


def parse(data):
    if len(data) < OFFSET + SIZE:
        return None
    return struct.unpack_from(FMT, data, OFFSET)


def crc_of(data):
    body = bytearray(data)
    struct.pack_into("<I", body, OFFSET + SIZE - 4, 0)
    return zlib.crc32(bytes(body)) & 0xFFFFFFFF


def version_of(h):
    return h[3].split(b"\x00")[0].decode()


def patch(path):
    data = bytearray(open(path, "rb").read())
    h = parse(data)
    if not h or h[0] != MAGIC:
        sys.exit(f"{path}: no firmware header at 0x{OFFSET:X}")
    struct.pack_into("<I", data, OFFSET + SIZE - 8, len(data))
    struct.pack_into("<I", data, OFFSET + SIZE - 4, crc_of(data))
    open(path, "wb").write(bytes(data))
    print(f"{path}: {len(data)} bytes, crc32 {crc_of(data):08X}, version {version_of(h)}")


def check(path):
    data = open(path, "rb").read()
    h = parse(data)
    if not h or h[0] != MAGIC:
        print(f"{path}: no firmware header")
        return 1
    if h[4] != len(data):
        print(f"{path}: image_len {h[4]} != file size {len(data)}")
        return 1
    if h[5] != crc_of(data):
        print(f"{path}: crc32 {h[5]:08X} != {crc_of(data):08X}")
        return 1
    print(f"{path}: header ok, version {version_of(h)}, {len(data)} bytes, crc32 {h[5]:08X}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("--patch", "--check"):
        sys.exit(__doc__)
    if sys.argv[1] == "--patch":
        patch(sys.argv[2])
    else:
        sys.exit(check(sys.argv[2]))
