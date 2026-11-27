"""Writes a synthetic 4096x2048 RGBA PNG (no game artwork) to test the native library in CI.

usage: python make_test_atlas.py <output.png>
Only the standard library is used: the image is deterministic noise with soft alpha.
"""
import struct
import sys
import zlib

W, H = 4096, 2048


def main(path):
    rows = []
    state = 12345
    for y in range(H):
        row = bytearray(b"\x00")  # filter: none
        for x in range(W):
            state = (state * 1103515245 + 12345) & 0x7FFFFFFF
            r = (x * 7 + (state >> 16)) & 0xFF
            g = (y * 5 + (state >> 8)) & 0xFF
            b = (x ^ y) & 0xFF
            a = 255 if (x // 37 + y // 29) % 3 else (state >> 4) & 0xFF
            row += bytes((r, g, b, a))
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


if __name__ == "__main__":
    main(sys.argv[1])
