#!/usr/bin/env python3
"""Generates platform/keyglyphs.c: the keyboard glyphs of the Windows release, from its
PICS/HI-RES/HUD2.TGA.

The Windows release draws four keyboard glyphs, from the places in HUD2 its button table
gives: the ESC and ENTER caps and the gold left and right arrows. These are copied pixel
for pixel. The port also shows SPACE, ALT, S, F1, A and D, which the Windows release never
drew; those caps are made from the same pieces: the cap's face (the parts of the ESC and
ENTER caps that the legends leave uncovered), letters in the pixel style of the ESC legend
(E, S and C are its own), and the engraved rim the ESC legend leaves in the face.

usage: gen_keyglyphs.py [path/to/HUD2.TGA]"""
import statistics
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TGA = ROOT.parent / 'pc-xl' / 'iso' / 'PICS' / 'HI-RES' / 'HUD2.TGA'

# The Windows release's button table: u, v, width, height in HUD2 (v counts the file's rows).
ESC = (158, 162, 30, 30)
ENTER = (158, 194, 30, 30)
LEFT = (160, 226, 32, 16)
RIGHT = (194, 226, 32, 16)

# The ESC legend: three letters 6 texels wide and 12 tall, 2 apart, from (4, 9) of the cap.
LETTER_W, LETTER_H, LETTER_GAP, LEGEND_TOP = 6, 12, 2, 9
ORIGINAL_LETTERS = 'ESC'

# Letters the ESC legend does not have, as its letters are drawn: strokes two texels thick,
# bars at rows 0, 5 and 10, uprights at columns 0 and 4.
NEW_LETTERS = {
    'A': ['######', '######', '##..##', '##..##', '##..##', '######', '######', '##..##', '##..##', '##..##',
          '##..##', '##..##'],
    'P': ['######', '######', '##..##', '##..##', '##..##', '######', '######', '##....', '##....', '##....',
          '##....', '##....'],
    'L': ['##....', '##....', '##....', '##....', '##....', '##....', '##....', '##....', '##....', '##....',
          '######', '######'],
    'T': ['######', '######', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..',
          '..##..', '..##..'],
    'F': ['######', '######', '##....', '##....', '##....', '######', '######', '##....', '##....', '##....',
          '##....', '##....'],
    'D': ['#####.', '######', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##',
          '######', '#####.'],
    '1': ['####..', '####..', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..',
          '..##..', '..##..'],
}

# The keys the port shows that the Windows release has no cap for.
DERIVED_KEYS = ['SPACE', 'ALT', 'S', 'F1', 'A', 'D']


def read_tga(path):
    """An uncompressed 16-bit TGA as rows of (r, g, b) in the file's order."""
    data = Path(path).read_bytes()
    id_length, colour_map, kind = data[0], data[1], data[2]
    width, height, depth = struct.unpack_from('<HHB', data, 12)
    if colour_map or kind != 2 or depth != 16:
        raise SystemExit('%s: not an uncompressed 16-bit TGA' % path)
    offset = 18 + id_length
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            v = struct.unpack_from('<H', data, offset + 2 * (y * width + x))[0]
            row.append(tuple(((v >> s & 31) << 3) | ((v >> s & 31) >> 2) for s in (10, 5, 0)))
        rows.append(row)
    return rows


def crop(rows, box):
    u, v, w, h = box
    return [row[u:u + w] for row in rows[v:v + h]]


def is_legend(p):
    return p[0] > p[1] + 40


def blank_face(esc, enter):
    """The cap without a legend: each texel from whichever cap's legend does not cover or shade
    it; texels both legends touch are filled from their neighbours."""
    h, w = len(esc), len(esc[0])
    esc_legend = {(x, y) for y in range(h) for x in range(w) if is_legend(esc[y][x])}
    enter_legend = {(x, y) for y in range(h) for x in range(w) if is_legend(enter[y][x])}

    def touched(legend, x, y):
        return any((x - dx, y - dy) in legend for dx in range(-2, 3) for dy in range(-2, 3))

    face, unknown = {}, []
    for y in range(h):
        for x in range(w):
            a, b = esc[y][x], enter[y][x]
            by_esc, by_enter = touched(esc_legend, x, y), touched(enter_legend, x, y)
            if a == b and not by_esc and not by_enter:
                face[x, y] = a
            elif by_esc and not by_enter:
                face[x, y] = b
            elif by_enter and not by_esc:
                face[x, y] = a
            else:
                unknown.append((x, y))
    while unknown:
        left = []
        for x, y in unknown:
            near = [face[n] for n in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)) if n in face]
            if len(near) >= 2:
                face[x, y] = tuple(sum(c[i] for c in near) // len(near) for i in range(3))
            else:
                left.append((x, y))
        if len(left) == len(unknown):
            raise SystemExit('cannot rebuild the cap face')
        unknown = left
    return [[face[x, y] for x in range(w)] for y in range(h)], esc_legend


def rim_kernel(esc, face, legend):
    """How much darker than the face the ESC legend leaves each texel around a letter texel,
    by offset (the median over the legend)."""
    kernel = {}
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            if dx or dy:
                darker = [(sum(face[y][x]) - sum(esc[y][x])) / 3 for y in range(len(esc)) for x in range(len(esc[0]))
                          if (x, y) not in legend and (x - dx, y - dy) in legend]
                kernel[dx, dy] = max(0.0, statistics.median(darker)) if darker else 0.0
    return kernel


def shade_letter(mask, colours):
    """Colours a letter mask as the ESC legend's letters are coloured: a shadow on the right and
    bottom edges of each stroke, a highlight at its top left corners."""
    lit = {(x, y) for y, line in enumerate(mask) for x, c in enumerate(line) if c == '#'}
    corner = {(x, y) for x, y in lit if (x - 1, y) not in lit and (x, y - 1) not in lit}
    texels = {}
    for x, y in lit:
        if (x + 1, y) not in lit or (x, y + 1) not in lit:
            texels[x, y] = colours['shadow']
        elif (x, y) in corner:
            texels[x, y] = colours['corner']
        elif (x - 1, y) in corner or (x, y - 1) in corner:
            texels[x, y] = colours['edge']
        else:
            texels[x, y] = colours['face']
    return texels


def make_cap(text, face, letters, kernel):
    width = max(30, len(text) * LETTER_W + (len(text) - 1) * LETTER_GAP + 8)
    # a wider cap repeats the middle of the face, back and forth so that no seam shows
    cap = []
    for row in face:
        middle = row[8:22]
        sweep = middle + middle[-2:0:-1]
        cap.append(row[:15] + [sweep[(i + 7) % len(sweep)] for i in range(width - 30)] + row[15:])
    cap = [list(row) for row in cap]
    legend = {}
    x0 = (width - (len(text) * LETTER_W + (len(text) - 1) * LETTER_GAP)) // 2
    for i, c in enumerate(text):
        for (x, y), colour in letters[c].items():
            legend[x0 + i * (LETTER_W + LETTER_GAP) + x, LEGEND_TOP + y] = colour
    for y in range(len(cap)):
        for x in range(width):
            if (x, y) in legend:
                cap[y][x] = legend[x, y]
                continue
            darker = max([k for (dx, dy), k in kernel.items() if (x - dx, y - dy) in legend] + [0.0])
            cap[y][x] = tuple(max(0, int(round(v - darker))) for v in cap[y][x])
    # the corners outside the cap stay black (transparent)
    for y in range(len(cap)):
        for x in range(width):
            source = x if x < 15 else 29 - (width - 1 - x) if x >= width - 15 else 15
            if face[y][source] == (0, 0, 0):
                cap[y][x] = (0, 0, 0)
    return cap


def pixels(image):
    """RGBA words (red in the low byte), black transparent as the Windows release's colour key."""
    return [0 if p == (0, 0, 0) else 0xFF000000 | p[2] << 16 | p[1] << 8 | p[0] for row in image for p in row]


def main():
    tga = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_TGA
    hud = read_tga(tga)
    esc, enter = crop(hud, ESC), crop(hud, ENTER)
    face, legend = blank_face(esc, enter)
    kernel = rim_kernel(esc, face, legend)

    letters = {}
    for i, c in enumerate(ORIGINAL_LETTERS):
        x0 = 4 + i * (LETTER_W + LETTER_GAP)
        letters[c] = {(x, y): esc[LEGEND_TOP + y][x0 + x] for y in range(LETTER_H) for x in range(LETTER_W)
                      if is_legend(esc[LEGEND_TOP + y][x0 + x])}
    e = letters['E']
    colours = {'corner': e[0, 0], 'edge': e[1, 0], 'face': e[0, 2], 'shadow': e[1, 2]}
    for c, mask in NEW_LETTERS.items():
        letters[c] = shade_letter(mask, colours)

    glyphs = [('ESC', esc), ('ENTER', enter), ('LEFT', crop(hud, LEFT)), ('RIGHT', crop(hud, RIGHT))]
    glyphs += [(key, make_cap(key, face, letters, kernel)) for key in DERIVED_KEYS]

    lines = ['/* Generated by tools/gen_keyglyphs.py from the Windows release\'s PICS/HI-RES/HUD2.TGA: do not edit. */',
             '#include "keyglyphs.h"', '']
    for name, image in glyphs:
        words = pixels(image)
        lines.append('static const uint32_t key%s[%d] = {' % (name.title(), len(words)))
        for i in range(0, len(words), 8):
            lines.append('    ' + ', '.join('0x%08x' % w for w in words[i:i + 8]) + ',')
        lines.append('};')
    lines.append('')
    lines.append('const KeyArt keyArt[KEY_ART_COUNT] = {')
    for name, image in glyphs:
        lines.append('    {"%s", %d, %d, key%s},' % (name, len(image[0]), len(image), name.title()))
    lines.append('};')
    (ROOT / 'platform' / 'keyglyphs.c').write_text('\n'.join(lines) + '\n')
    print('%d glyphs from %s' % (len(glyphs), tga))


if __name__ == '__main__':
    main()
