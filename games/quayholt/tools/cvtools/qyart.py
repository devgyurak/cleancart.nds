# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Quayholt's own art, drawn in code from integer maths only: the title picture (a harbour at
dusk, two cranes on the quay, stacked cargo boxes, the game's name in stencil lettering with the
bridges cut out of the letters) and the banner icon."""

from . import font
from .gfx import ramp, rgb15

W, H = 256, 192
# Palette indices start at BASE so the text layer's 16-colour lines (entries 0-95 of the BG
# palette) stay untouched. Index 0 is transparent (backdrop).
BASE = 96
SKY = BASE             # 40 steps, indigo at the top to amber at the horizon
SKY_N = 40
WORD = SKY + SKY_N     # 16 steps for the lettering
WORD_N = 16
WATER = WORD + WORD_N  # 24 steps, dark sea to a copper glint
WATER_N = 24
QUAY = WATER + WATER_N  # fixed colours from here
STEEL = QUAY + 1        # new crane (slim, bright)
IRON = QUAY + 2         # old crane (lattice, dark)
BOX_A = QUAY + 3
BOX_B = QUAY + 4
BOX_C = QUAY + 5
LAMP = QUAY + 6
EDGE = QUAY + 7         # lettering outline
SUN = QUAY + 8
HULL = QUAY + 9


def _rect(pix, x0, y0, x1, y1, v):
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            pix[y][x] = v


def _line(pix, x0, y0, x1, y1, v, thick=0):
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i // n
        y = y0 + (y1 - y0) * i // n
        _rect(pix, x - thick, y - thick, x + thick + 1, y + thick + 1, v)


def _disc(pix, cx, cy, r, v):
    for y in range(max(0, cy - r), min(H, cy + r + 1)):
        for x in range(max(0, cx - r), min(W, cx + r + 1)):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                pix[y][x] = v


def _old_crane(pix, x):
    """A lattice jib on a braced tower: the steam crane."""
    _rect(pix, x - 6, 78, x + 6, 112, IRON)
    for y in range(80, 110, 8):
        _line(pix, x - 6, y, x + 6, y + 8, SKY + 6)
        _line(pix, x + 6, y, x - 6, y + 8, SKY + 6)
    _line(pix, x, 78, x + 52, 64, IRON, 1)
    _line(pix, x, 78, x - 20, 70, IRON, 1)
    _line(pix, x + 52, 64, x + 52, 86, IRON)
    _rect(pix, x + 48, 86, x + 57, 92, IRON)


def _new_crane(pix, x):
    """A slim box boom on two legs: the electric crane."""
    _rect(pix, x - 8, 70, x - 5, 112, STEEL)
    _rect(pix, x + 5, 70, x + 8, 112, STEEL)
    _rect(pix, x - 10, 66, x + 11, 71, STEEL)
    _rect(pix, x - 62, 58, x + 10, 63, STEEL)
    _line(pix, x - 62, 63, x - 20, 71, STEEL)
    _rect(pix, x - 50, 63, x - 48, 84, STEEL)
    _rect(pix, x - 56, 84, x - 42, 90, BOX_B)


def _stack(pix, x, y_floor, cols, rows):
    for r in range(rows):
        for c in range(cols):
            tone = (BOX_A, BOX_B, BOX_C)[(r * 2 + c) % 3]
            _rect(pix, x + c * 14, y_floor - (r + 1) * 9, x + c * 14 + 13, y_floor - r * 9 - 1, tone)


def title_art():
    """Returns (8bpp pixels 256x192, 256-entry BGR555 palette bytes)."""
    pix = [[0] * W for _ in range(H)]

    # Dusk sky: a vertical ramp, darker at the top.
    for y in range(0, 112):
        step = y * SKY_N // 112
        for x in range(W):
            pix[y][x] = SKY + min(SKY_N - 1, step)
    _disc(pix, 150, 100, 12, SUN)
    # Sea with the sun's glint (a column of lighter pixels that widens downward).
    for y in range(112, H):
        t = (y - 112) * WATER_N // (H - 112)
        for x in range(W):
            v = WATER + min(WATER_N - 1, t // 2)
            glint = abs(x - 150) < 3 + (y - 112) // 6 and (y + x // 3) % 3 == 0
            pix[y][x] = WATER + min(WATER_N - 1, t // 2 + 7) if glint else v

    # The quay, the two cranes, the cargo.
    _rect(pix, 0, 112, W, 124, HULL)
    _rect(pix, 0, 112, W, 114, EDGE)
    _old_crane(pix, 40)
    _new_crane(pix, 198)
    _stack(pix, 80, 112, 4, 3)
    _stack(pix, 226, 112, 2, 2)
    for lx in (12, 76, 142, 236):
        _rect(pix, lx, 100, lx + 2, 112, EDGE)
        _rect(pix, lx - 1, 98, lx + 4, 101, LAMP)

    # Wordmark: the game font at 3x in stencil style (a one-pixel-wide cut through the middle
    # of each tall letter, as on a stencilled crate), with a bevelled warm ramp and an outline.
    text = "QUAYHOLT"
    scale = 3
    cw = 6 * scale
    x0 = (W - len(text) * cw) // 2 + scale
    y0 = 134
    for i, ch in enumerate(text):
        g = font.font_pixels(ch)
        for gy in range(8):
            for gx in range(8):
                if not g[gy][gx]:
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        x = x0 + i * cw + (gx - 1) * scale + sx
                        y = y0 + gy * scale + sy
                        if not (0 <= x < W and 0 <= y < H):
                            continue
                        if gy == 3 and sy == 1 and ch in "QUHO":   # the stencil bridge, cut
                            continue
                        edge = sx == 0 or sy == 0
                        shade = int((WORD_N - 1) * (gy * scale + sy) / (8 * scale))
                        pix[y][x] = EDGE if edge else WORD + shade

    cols = [(0, 0, 0)] * BASE
    cols += ramp((20, 22, 64), (244, 150, 70), SKY_N)
    cols += ramp((252, 232, 170), (190, 96, 40), WORD_N)
    cols += ramp((10, 20, 44), (214, 120, 70), WATER_N)
    cols += [(150, 170, 190), (34, 36, 48), (184, 70, 44), (210, 156, 60), (70, 124, 130),
             (255, 220, 120), (20, 10, 8), (255, 190, 90), (24, 26, 38)]
    cols += [(0, 0, 0)] * (256 - len(cols))
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(v for row in pix for v in row), bytes(pal)


def icon():
    """32x32 banner icon: a cargo box under a crane hook. Returns (rows, 16 colours)."""
    pal = [(0, 0, 0), (28, 26, 60), (214, 120, 70), (184, 70, 44), (255, 220, 120),
           (150, 170, 190), (70, 124, 130), (20, 10, 8)] + [(0, 0, 0)] * 8
    pix = [[1] * 32 for _ in range(32)]
    for y in range(20, 32):
        for x in range(32):
            pix[y][x] = 6
    for y in range(14, 26):
        for x in range(8, 24):
            pix[y][x] = 3
    for x in range(8, 24):
        pix[14][x] = pix[25][x] = 7
        pix[19][x] = 2
    for y in range(14, 26):
        pix[y][8] = pix[y][23] = 7
    for y in range(0, 10):
        pix[y][16] = 5
        pix[y][15] = 5
    for x in range(10, 22):
        pix[10][x] = pix[11][x] = 5
    pix[12][10] = pix[12][21] = pix[13][10] = pix[13][21] = 5
    for y in range(26, 28):
        for x in range(4, 28):
            pix[y][x] = 4 if (x // 2) % 2 else 2
    return pix, pal
