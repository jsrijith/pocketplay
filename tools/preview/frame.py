"""Puts a thin black border around README screenshots, using only the standard library.

    python3 -I tools/preview/frame.py screenshot.png ...

The border goes outside the screen, so nothing drawn is covered: a 480x800 screenshot
becomes 484x804. Without it, a white screen fades into a white page. It reads the 8-bit
greyscale PNGs that topng.py writes (any row filter) and rewrites each file in place, so run
it once per fresh screenshot, as screenshots.sh does, not twice on the same file.
"""
import struct
import sys
import zlib

BORDER = 2  # pixels; shown at half size in the READMEs, it draws as about 1 px
BLACK = 0


def chunks(data):
    at = 8
    while at < len(data):
        (length,) = struct.unpack(">I", data[at:at + 4])
        yield data[at + 4:at + 8], data[at + 8:at + 8 + length]
        at += 12 + length


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    idat = b""
    for kind, body in chunks(data):
        if kind == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if (depth, colour, interlace) != (8, 0, 0):
                sys.exit(f"{path}: expected 8-bit greyscale, not interlaced")
        elif kind == b"IDAT":
            idat += body
    raw = zlib.decompress(idat)
    rows, previous, stride = [], bytearray(width), width + 1
    for y in range(height):
        kind, line = raw[y * stride], bytearray(raw[y * stride + 1:(y + 1) * stride])
        for x in range(width):
            left = line[x - 1] if x else 0
            up = previous[x]
            corner = previous[x - 1] if x else 0
            if kind == 1:
                line[x] = (line[x] + left) & 0xFF
            elif kind == 2:
                line[x] = (line[x] + up) & 0xFF
            elif kind == 3:
                line[x] = (line[x] + (left + up) // 2) & 0xFF
            elif kind == 4:
                line[x] = (line[x] + paeth(left, up, corner)) & 0xFF
            elif kind != 0:
                sys.exit(f"{path}: unknown row filter {kind}")
        rows.append(bytes(line))
        previous = line
    return width, rows


def write(path, width, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    height = len(rows)
    raw = b"".join(b"\x00" + row for row in rows)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as handle:
        handle.write(png)


def frame(path):
    width, rows = read(path)
    edge = bytes([BLACK]) * BORDER
    framedWidth = width + 2 * BORDER
    solid = [bytes([BLACK]) * framedWidth] * BORDER
    write(path, framedWidth, solid + [edge + row + edge for row in rows] + solid)


for name in sys.argv[1:]:
    frame(name)
