# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tonewick's own title picture: dusk over a moor, a lattice signal mast with a lit lamp and radio arcs, the
listening shed with an oscilloscope window (a square trace that sinks to the floor line) and the game's name.
Drawn in code from integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import ramp, rgb15

W, H = 256, 192
# Palette indices start at BASE so the text layer's 16-colour lines (entries
# 0-95 of the BG palette) stay untouched. Index 0 is transparent (backdrop).
BASE = 96
SKY = BASE             # 40 steps, night blue to amber at the horizon
SKY_N = 40
WORD = BASE + SKY_N    # 16 steps for the wordmark
WORD_N = 16
GROUND = WORD + WORD_N       # far ridge
GROUND2 = GROUND + 1         # near ridge
HEATHER = GROUND + 2
MAST = GROUND + 3            # mast lattice
LAMP = GROUND + 4
ARC = GROUND + 5             # radio arcs
SHED = GROUND + 6
ROOF = GROUND + 7
PHOS_BG = GROUND + 8         # scope window
PHOS_GRID = GROUND + 9
TRACE = GROUND + 10          # the ear trace
TARGET = GROUND + 11         # the target trace
EDGE = GROUND + 12           # wordmark outline
SHADOW = GROUND + 13         # wordmark drop shadow
STAR = GROUND + 14


def _isqrt(n):
    return int(n ** 0.5)


def _disc(pix, cx, cy, r, v):
    for y in range(max(0, cy - r), min(H, cy + r + 1)):
        for x in range(max(0, cx - r), min(W, cx + r + 1)):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                pix[y][x] = v


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


def _title_pixels():
    pix = [[0] * W for _ in range(H)]

    # Dusk sky: vertical ramp, a handful of stars in the dark part.
    for y in range(112):
        step = min(SKY_N - 1, y * SKY_N // 104)
        for x in range(W):
            pix[y][x] = SKY + step
    seed = 12345
    for _ in range(46):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        x = seed >> 8 & 255
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        y = (seed >> 8) % 58
        pix[y][x] = STAR

    # Two ridges of moor and a few heather flecks.
    for x in range(W):
        far = 100 + abs((x * 3) % 72 - 36) // 5
        near = 118 + abs((x * 5) % 96 - 48) // 6
        for y in range(far, H):
            pix[y][x] = GROUND
        for y in range(near, H):
            pix[y][x] = GROUND2
    for _ in range(90):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        x = seed >> 8 & 255
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        y = 116 + (seed >> 8) % 76
        if pix[y][x] == GROUND2:
            pix[y][x] = HEATHER

    # The mast: two converging legs, cross braces, a lamp and three radio arcs.
    top, foot, mx = 26, 112, 204
    for y in range(top, foot):
        half = 2 + (y - top) * 7 // (foot - top)
        _rect(pix, mx - half, y, mx - half + 1, y + 1, MAST)
        _rect(pix, mx + half, y, mx + half + 1, y + 1, MAST)
    for y in range(top + 6, foot - 6, 12):
        half0 = 2 + (y - top) * 7 // (foot - top)
        half1 = 2 + (y + 12 - top) * 7 // (foot - top)
        _line(pix, mx - half0, y, mx + half1, y + 12, MAST)
        _line(pix, mx + half0, y, mx - half1, y + 12, MAST)
    _disc(pix, mx, top - 4, 3, LAMP)
    for r in (12, 20, 28):
        for t in range(-40, 41, 2):
            # a quarter-turn each side of straight up, dotted
            ang_x = r * t // 40
            ang_y = _isqrt(max(0, r * r - ang_x * ang_x))
            if (t // 2) % 2 == 0:
                x, y = mx + ang_x, top - 4 - ang_y
                if 0 <= x < W and 0 <= y < H:
                    pix[y][x] = ARC

    # The listening shed with its scope window.
    _rect(pix, 24, 66, 104, 112, SHED)
    for i in range(0, 14):
        _rect(pix, 33 - i, 52 + i, 95 + i, 53 + i, ROOF)
    _rect(pix, 34, 74, 94, 104, PHOS_BG)
    for gx in range(34, 94, 10):
        for gy in range(74, 104, 10):
            pix[gy][gx] = PHOS_GRID
    # target trace (thin, behind) and the ear trace: a square wave whose last stretch sits on the floor.
    pts = [(36, 92), (44, 92), (44, 80), (56, 80), (56, 92), (68, 92), (68, 80), (80, 80), (80, 92), (92, 92)]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        _line(pix, x0, y0 + 2, x1, y1 + 2, TARGET)
    pts = [(36, 90), (44, 90), (44, 78), (56, 78), (56, 90), (68, 90), (68, 78), (80, 78), (80, 90), (92, 90)]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        _line(pix, x0, y0, x1, y1, TRACE)
    _rect(pix, 56, 108, 72, 112, ROOF)    # door step

    # Wordmark: the game font at 3x with a bevelled warm ramp and a shadow.
    text = "TONEWICK"
    scale = 3
    cw = 6 * scale
    x0 = (W - len(text) * cw) // 2 + scale
    y0 = 114
    ink = set()
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
                        if 0 <= x < W and 0 <= y < H:
                            ink.add((x, y))
                            shade = int((WORD_N - 1) * (gy * scale + sy) / (8 * scale))
                            pix[y][x] = WORD + shade
    # a dark outline around the letters and a drop shadow on the moor
    for (x, y) in sorted(ink):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if (nx, ny) not in ink and 0 <= nx < W and 0 <= ny < H:
                pix[ny][nx] = EDGE
    for (x, y) in sorted(ink):
        if x + 2 < W and y + 2 < H and (x + 2, y + 2) not in ink and pix[y + 2][x + 2] != EDGE:
            pix[y + 2][x + 2] = SHADOW
    return pix


def title_art():
    """Returns (8bpp pixels 256x192, 256-entry BGR555 palette bytes)."""
    pix = _title_pixels()
    cols = [(0, 0, 0)] * BASE
    cols += ramp((6, 10, 36), (238, 156, 84), SKY_N)
    cols += ramp((252, 232, 168), (190, 110, 52), WORD_N)
    cols += [(22, 26, 40), (12, 14, 24), (150, 70, 120), (150, 160, 170), (255, 230, 120), (120, 150, 190),
             (60, 44, 36), (36, 26, 24), (10, 34, 24), (26, 78, 52), (120, 255, 150), (80, 190, 255),
             (30, 14, 10), (8, 6, 6), (240, 240, 255)]
    cols += [(0, 0, 0)] * (256 - len(cols))
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(v for row in pix for v in row), bytes(pal)


def icon():
    """32x32 banner icon: an oscilloscope screen with a square trace. Returns (rows, 16 colours)."""
    pal = [(0, 0, 0), (40, 30, 24), (120, 80, 44), (214, 160, 60), (10, 34, 24), (26, 78, 52),
           (120, 255, 150), (80, 190, 255)] + [(0, 0, 0)] * 8
    pix = [[0] * 32 for _ in range(32)]
    for y in range(3, 29):
        for x in range(2, 30):
            pix[y][x] = 2 if (x in (2, 29) or y in (3, 28)) else 1
    for y in range(6, 25):
        for x in range(5, 27):
            pix[y][x] = 4
    for y in range(6, 25, 6):
        for x in range(5, 27):
            pix[y][x] = 5
    for x in range(5, 27, 6):
        for y in range(6, 25):
            pix[y][x] = 5
    level = [(5, 16), (10, 16), (10, 10), (16, 10), (16, 16), (21, 16), (21, 10), (26, 10)]
    for (x0, y0), (x1, y1) in zip(level, level[1:]):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for k in range(n + 1):
            x, y = x0 + (x1 - x0) * k // n, y0 + (y1 - y0) * k // n
            pix[y][x] = 6
            pix[y + 1][x] = 7
    for y in range(26, 28):
        for x in (8, 12, 24):
            pix[y][x] = 3
    return pix, pal
