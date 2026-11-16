# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Glass motif textures, glass palettes and the light-box texture (all drawn by code).

Textures are 8 bits per texel (format 4, 256-colour) but only use indices 0-7, so a
palette set is 8 colours (16 bytes = one PLTT_BASE unit). The palette block holds
eight sets: 0-3 glass colours (amber, ruby, sea, violet), 4-7 light box colours.
Index 0 is the "cut out" colour of a motif; the light box has four index-0 texels.
"""

import colorsys

MOTIF_SIZE = 32
LBOX_SIZE = 8

# The light box texture, row by row (index per texel). Deliberately asymmetric so a
# mirror, a repeat or a transposition changes the sampled texels. Zeros are the holes.
LBOX = [
    [1, 2, 3, 4, 5, 6, 7, 1],
    [2, 5, 7, 1, 3, 4, 6, 2],
    [3, 6, 0, 2, 0, 5, 1, 4],
    [4, 1, 5, 7, 6, 2, 3, 5],
    [5, 7, 2, 3, 1, 6, 4, 7],
    [6, 3, 0, 5, 0, 1, 2, 3],
    [7, 4, 6, 1, 2, 3, 5, 6],
    [1, 2, 4, 6, 7, 5, 3, 1],
]
LBOX_HOLES = [(2, 2), (4, 2), (2, 5), (4, 5)]    # (u, v)


def rgb15(r, g, b):
    return (r & 31) | (g & 31) << 5 | (b & 31) << 10


def glass_set(hue):
    """Eight colours of one glass: index 0 a pale highlight, 1-7 from dark to bright."""
    out = [rgb15(28, 28, 26)]
    for i in range(1, 8):
        v = 0.30 + 0.10 * i
        s = 0.95 - 0.06 * i
        r, g, b = colorsys.hsv_to_rgb(hue, s, v)
        out.append(rgb15(int(r * 31 + .5), int(g * 31 + .5), int(b * 31 + .5)))
    return out


def lbox_set(shift):
    """Eight light-box colours, all different from each other and from the scene's other colours."""
    out = [rgb15(3, 3, 3)]
    for i in range(1, 8):
        h = ((i * 5 + shift * 2) % 7) / 7.0 + 0.04
        r, g, b = colorsys.hsv_to_rgb(h, 0.9, 0.75 + 0.03 * ((i + shift) % 4))
        out.append(rgb15(int(r * 31 + .5), int(g * 31 + .5), int(b * 31 + .5)))
    return out


def palette_block():
    """64 RGB555 entries: glass sets 0-3, light box sets 4-7."""
    sets = [glass_set(h) for h in (0.10, 0.99, 0.52, 0.76)]
    sets += [lbox_set(k) for k in range(4)]
    out = []
    for s in sets:
        out += s
    return out


def motif(kind):
    """32x32 index map. Every motif is asymmetric so a mirror shows. 0 is the cut-out area."""
    n = MOTIF_SIZE
    px = [[0] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            if kind == 'chevron':
                d = abs(x - 12) // 2 + y   # a V that points down, leaning left
                px[y][x] = 1 + (d // 3) % 6 if (d // 3) % 7 != 6 else 0
            elif kind == 'arc':
                dx, dy = x - 2, y - 2       # quarter circle bands round the top-left corner
                r2 = dx * dx + dy * dy
                band = int(r2 ** 0.5) // 4
                px[y][x] = 1 + band % 6 if band < 8 else 0
            else:                            # bars, staggered
                row = y // 5
                shift = (row * 7) % 13
                bar = ((x + shift) // 6) % 3
                px[y][x] = 0 if bar == 2 else 1 + (row + bar * 2) % 6
            if x in (0, n - 1) or y in (0, n - 1):
                px[y][x] = 7                 # lead rim
    return px


def flat(px):
    return bytes(v for row in px for v in row)
