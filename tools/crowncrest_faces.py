#!/usr/bin/env python3
"""Draws Crown & Crest's 24 dice faces and writes src/games/crowncrest/crowncrest_faces.h.

Each face is a shield drawn from a few curves on a 100 x 100 grid, with a pattern (dots, bands,
stripes, quarters or an inner field) and up to two X marks beside it, or a question mark. The
faces are built as SVG, rendered at 180 x 180 by rsvg-convert, and packed 1 bit a pixel.

    python3 tools/crowncrest_faces.py

Needs rsvg-convert (librsvg2-bin). Change a face in DICE, run this, and check the result with
tools/preview/run.sh.
"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib

FACE = 180
ROW_BYTES = (FACE + 7) // 8
OUTLINE = 4.6
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'src', 'games', 'crowncrest', 'crowncrest_faces.h')


# ---- Shapes ----

def heater(x, y, w, h):
    """A shield with a flat top and a point at the bottom."""
    return (f"M{x},{y} H{x+w} V{y+h*0.42} C{x+w},{y+h*0.72} {x+w*0.72},{y+h*0.9} {x+w/2},{y+h} "
            f"C{x+w*0.28},{y+h*0.9} {x},{y+h*0.72} {x},{y+h*0.42} Z")


def arch(x, y, w, h):
    """A shield with a pointed top and a flat bottom."""
    return (f"M{x},{y+h} V{y+h*0.5} C{x},{y+h*0.24} {x+w*0.26},{y+h*0.1} {x+w/2},{y} "
            f"C{x+w*0.74},{y+h*0.1} {x+w},{y+h*0.24} {x+w},{y+h*0.5} V{y+h} Z")


def shape_path(kind, box):
    """The outline of a shield filling box, and the SVG transform that turns it.

    'heater' and 'arch' stand upright. 'side' is a heater on its side with the point to the
    left, and 'bullet' an arch on its side with the point to the right.
    """
    x, y, w, h = box
    cx, cy = x + w / 2, y + h / 2
    if kind == 'heater':
        return heater(x, y, w, h), ''
    if kind == 'arch':
        return arch(x, y, w, h), ''
    if kind == 'side':
        return heater(cx - h / 2, cy - w / 2, h, w), f'rotate(90 {cx} {cy})'
    if kind == 'bullet':
        return arch(cx - h / 2, cy - w / 2, h, w), f'rotate(90 {cx} {cy})'
    raise ValueError(kind)


def pattern(kind, pat, box, clip):
    """What's on the shield, clipped to it where it could spill over."""
    x, y, w, h = box
    cx, cy = x + w / 2, y + h / 2
    if pat == 'dot':
        return f'<circle cx="{cx}" cy="{cy + (4 if kind == "arch" else -2)}" r="6.2"/>'
    if pat == 'dots-v':
        return f'<circle cx="{cx}" cy="{cy - 6}" r="6.2"/><circle cx="{cx}" cy="{cy + 10}" r="6.2"/>'
    if pat == 'dots-h':
        return f'<circle cx="{cx - 8}" cy="{cy}" r="6.2"/><circle cx="{cx + 8}" cy="{cy}" r="6.2"/>'
    if pat == 'bordure':  # a solid field inside a white border
        p, t = shape_path(kind, (x + 8, y + 8, w - 16, h - 16 * 1.05))
        return f'<path d="{p}" transform="{t}" stroke="none"/>'
    if pat == 'quarters':
        return (f'<g clip-path="url(#{clip})" stroke="none">'
                f'<rect x="{cx}" y="{y-5}" width="{w}" height="{h/2+11}"/>'
                f'<rect x="{x-5}" y="{cy+6}" width="{w/2+5}" height="{h}"/></g>')
    if pat == 'bend':  # one band, rising to the right
        return (f'<g clip-path="url(#{clip})" stroke="none">'
                f'<path d="M{x-20},{cy+16} L{x+w+20},{cy-16} V{cy-4} L{x-20},{cy+28} Z"/></g>')
    if pat == 'bends':  # two bands, rising to the right
        return (f'<g clip-path="url(#{clip})" stroke="none">'
                f'<path d="M{x-20},{cy+4} L{x+w+20},{cy-26} V{cy-15} L{x-20},{cy+15} Z"/>'
                f'<path d="M{x-20},{cy+26} L{x+w+20},{cy-4} V{cy+7} L{x-20},{cy+37} Z"/></g>')
    if pat == 'stripes':  # three bands, falling to the right
        return (f'<g clip-path="url(#{clip})" stroke="none">' + ''.join(
            f'<path d="M{x+o},{y-10} L{x+o+30},{y+h+10} H{x+o+40} L{x+o+10},{y-10} Z"/>'
            for o in (-14, 6, 26)) + '</g>')
    raise ValueError(pat)


def cross(cx, cy, r=8.5):
    return (f'<path d="M{cx-r},{cy-r} L{cx+r},{cy+r} M{cx+r},{cy-r} L{cx-r},{cy+r}" '
            f'stroke-width="4.4" stroke-linecap="butt" fill="none"/>')


def question():
    return ('<path d="M33,36 C33,20 46,13 54,13 C66,13 72,22 72,31 C72,42 62,46 56,51 '
            'C52,54 51,58 51,64" stroke-width="13" stroke-linecap="round" fill="none"/>'
            '<ellipse cx="51" cy="82" rx="10" ry="8" stroke="none"/>')


# Where the shield sits for each placing of X marks, and where the marks go.
FULL = (20, 12, 60, 74)
MARKS = {
    None: (FULL, []),
    'bl': ((30, 10, 56, 66), [(16, 84)]),
    'tr': ((14, 22, 56, 66), [(84, 16)]),
    'tl': ((30, 22, 56, 66), [(14, 16)]),
    'br': ((14, 10, 56, 66), [(86, 86)]),
    'left2': ((34, 18, 52, 64), [(15, 36), (15, 64)]),
    'right2': ((14, 18, 52, 64), [(85, 36), (85, 64)]),
}

# Each die's six faces: (shape, pattern, X marks), or ('?', None, None).
DICE = [
    [('bullet', 'dots-h', 'bl'), ('side', 'stripes', 'tr'), ('heater', 'bordure', None),
     ('arch', 'quarters', None), ('arch', 'dot', None), ('heater', 'bend', None)],
    [('arch', 'dots-v', None), ('heater', 'bends', None), ('heater', 'bordure', 'left2'),
     ('arch', 'quarters', 'right2'), ('arch', 'bend', None), ('heater', 'dot', None)],
    [('bullet', 'dots-h', None), ('side', 'stripes', None), ('heater', 'bend', None),
     ('?', None, None), ('arch', 'bend', 'br'), ('heater', 'dot', 'tl')],
    [('bullet', 'dots-h', 'bl'), ('side', 'stripes', 'tr'), ('heater', 'bend', None),
     ('?', None, None), ('arch', 'dot', None), ('heater', 'dot', None)],
]


def face_svg(kind, pat, mark):
    if kind == '?':
        body = question()
    else:
        box, crosses = MARKS[mark]
        if kind in ('side', 'bullet'):  # wider than tall
            x, y, w, h = box
            box = (x - 4, y + 6, w + 8, h - 12)
        p, t = shape_path(kind, box)
        turn = f' transform="{t}"' if t else ''
        body = f'<defs><clipPath id="shield"><path d="{p}"{turn}/></clipPath></defs>'
        # Stripes are laid out on the unturned box, so they lean the same way on every shape.
        body += pattern('heater' if kind == 'side' else kind, pat, box, 'shield')
        body += f'<path d="{p}"{turn} fill="none" stroke-width="{OUTLINE}" stroke-linejoin="round"/>'
        body += ''.join(cross(cx, cy) for cx, cy in crosses)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{FACE}" height="{FACE}" viewBox="0 0 100 100">'
            f'<rect width="100" height="100" fill="white"/><g fill="black" stroke="black">{body}</g></svg>')


# ---- Rendering and packing ----

def read_png(path):
    """Rows of booleans, True for ink, from rsvg-convert's 8-bit RGB or RGBA PNG."""
    data = open(path, 'rb').read()
    i, idat = 8, b''
    while i < len(data):
        n, = struct.unpack('>I', data[i:i + 4])
        kind, body = data[i + 4:i + 8], data[i + 8:i + 8 + n]
        if kind == b'IHDR':
            w, h, depth, colour = struct.unpack('>IIBB', body[:10])
            if depth != 8 or colour not in (2, 6):
                sys.exit(f'{path}: unexpected PNG format')
        elif kind == b'IDAT':
            idat += body
        i += 12 + n
    bpp = 3 if colour == 2 else 4
    raw = zlib.decompress(idat)
    stride = w * bpp
    prev, rows, o = bytearray(stride), [], 0
    for _ in range(h):
        f = raw[o]
        cur = bytearray(raw[o + 1:o + 1 + stride])
        o += 1 + stride
        for x in range(stride):
            a = cur[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                cur[x] = (cur[x] + a) & 255
            elif f == 2:
                cur[x] = (cur[x] + b) & 255
            elif f == 3:
                cur[x] = (cur[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                cur[x] = (cur[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([cur[x * bpp] < 128 for x in range(w)])
        prev = cur
    return rows


def pack(rows):
    """1 bit a pixel, MSB first, 0 = ink, rows padded to whole bytes."""
    out = []
    for row in rows:
        row = row + [False] * (ROW_BYTES * 8 - FACE)
        for j in range(0, len(row), 8):
            v = 0
            for ink in row[j:j + 8]:
                v = (v << 1) | (0 if ink else 1)
            out.append(v)
    return out


def main():
    lines = [
        '#pragma once\n',
        '\n',
        '#include <cstdint>\n',
        '\n',
        "// Crown & Crest's dice faces, drawn by tools/crowncrest_faces.py: change them there and run it,\n",
        '// rather than editing this file. Each face is 180 x 180, 1 bit a pixel, MSB first, 0 = ink, as the\n',
        '// icons are. Include only from crowncrest.cpp.\n',
        'namespace crowncrestFaces {\n',
        '\n',
        f'constexpr int FACE = {FACE};\n',
        'constexpr int ROW_BYTES = (FACE + 7) / 8;\n',
        '\n',
        'constexpr uint8_t FACES[4][6][ROW_BYTES * FACE] = {\n',
    ]
    with tempfile.TemporaryDirectory() as tmp:
        for d, faces in enumerate(DICE):
            lines.append('    {\n')
            for f, (kind, pat, mark) in enumerate(faces):
                svg = os.path.join(tmp, 'face.svg')
                png = os.path.join(tmp, 'face.png')
                open(svg, 'w').write(face_svg(kind, pat, mark))
                subprocess.run(['rsvg-convert', '-w', str(FACE), '-h', str(FACE), svg, '-o', png], check=True)
                data = pack(read_png(png))
                what = 'question mark' if kind == '?' else f'{kind}, {pat}' + (f', X {mark}' if mark else '')
                lines.append(f'        // die {d + 1}, face {f + 1}: {what}\n        {{\n')
                for j in range(0, len(data), 20):
                    lines.append('            ' + ', '.join(f'0x{v:02X}' for v in data[j:j + 20]) + ',\n')
                lines.append('        },\n')
            lines.append('    },\n')
    lines.append('};\n\n}  // namespace crowncrestFaces\n')
    open(OUT, 'w').write(''.join(lines))
    print(f'wrote {os.path.normpath(OUT)}')


if __name__ == '__main__':
    main()
