"""Turns the preview's .pgm files into .png, using only the standard library."""
import struct
import sys
import zlib


def convert(source, target):
    with open(source, "rb") as handle:
        header, pixels = handle.read().split(b"\n", 1)
    _, width, height, _ = header.split()
    width, height = int(width), int(height)
    rows = b"".join(b"\x00" + pixels[y * width:(y + 1) * width] for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows))
    png += chunk(b"IEND", b"")
    with open(target, "wb") as handle:
        handle.write(png)


for path in sys.argv[1:]:
    convert(path, path[:-4] + ".png")
