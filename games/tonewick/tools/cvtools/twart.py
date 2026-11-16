# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tonewick's own title picture: two counting-house towers joined by a
pneumatic tube with a capsule in it, a ruled-paper glow and the game's name. Drawn in code
from integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import ramp, rgb15

W, H = 256, 192
# Palette indices start at BASE so the text layer's 16-colour lines (entries
# 0-95 of the BG palette) stay untouched. Index 0 is transparent (backdrop).
BASE = 96
GLOW = BASE            # 48 steps, dark teal to warm paper
GLOW_N = 48
WORD = BASE + GLOW_N   # 16 steps for the wordmark
WORD_N = 16
TUBE = WORD + WORD_N         # pneumatic tube
BRICK = TUBE + 1             # tower wall
WINDOW = TUBE + 2            # lit window
EDGE = TUBE + 3              # wordmark outline
SHADOW = TUBE + 4            # wordmark drop shadow
CAPSULE = TUBE + 5           # capsule in the tube


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


def _tower(pix, x0, x1, top, bottom):
    _rect(pix, x0, top, x1, bottom, BRICK)
    _rect(pix, x0 - 4, top - 4, x1 + 4, top, EDGE)
    for wy in range(top + 8, bottom - 8, 16):
        for wx in range(x0 + 6, x1 - 8, 14):
            _rect(pix, wx, wy, wx + 7, wy + 9, WINDOW)


def title_art():
    """Returns (8bpp pixels 256x192, 256-entry BGR555 palette bytes)."""
    pix = [[0] * W for _ in range(H)]

    # Ledger-paper sky: a soft glow behind the tube, ruled lines across it.
    lx, ly = 128, 60
    for y in range(H):
        for x in range(W):
            d = _isqrt((x - lx) ** 2 + (y - ly) ** 2)
            if d < 120:
                step = (120 - d) * GLOW_N // 120
                if step > 0:
                    pix[y][x] = GLOW + min(GLOW_N - 1, step)
    for y in range(8, 112, 12):
        for x in range(W):
            if pix[y][x] >= GLOW:
                pix[y][x] = max(GLOW, pix[y][x] - 6)

    # Two towers and the tube between them (with a capsule on its way).
    _tower(pix, 20, 78, 30, 111)
    _tower(pix, 178, 236, 46, 111)
    _rect(pix, 78, 62, 178, 70, TUBE)
    _rect(pix, 78, 64, 178, 68, GLOW + GLOW_N - 6)
    _rect(pix, 118, 63, 134, 69, CAPSULE)
    _rect(pix, 0, 111, W, 114, EDGE)

    # Wordmark: the game font at 3x with a bevelled warm ramp and a shadow.
    text = "TONEWICK"
    scale = 3
    cw = 6 * scale
    x0 = (W - len(text) * cw) // 2 + scale
    y0 = 122
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
                        edge = sx == 0 or sy == 0
                        shade = int((WORD_N - 1) * (gy * scale + sy) / (8 * scale))
                        pix[y][x] = EDGE if edge else WORD + shade
                        if y + 2 < H and x + 2 < W and pix[y + 2][x + 2] in (0,) + tuple(range(GLOW, GLOW + GLOW_N)):
                            pix[y + 2][x + 2] = SHADOW

    cols = [(0, 0, 0)] * BASE
    cols += ramp((8, 22, 26), (236, 222, 186), GLOW_N)
    cols += ramp((250, 226, 150), (176, 92, 44), WORD_N)
    cols += [(70, 120, 124), (96, 62, 40), (255, 214, 120), (34, 18, 12), (20, 10, 8), (210, 150, 70)]
    cols += [(0, 0, 0)] * (256 - len(cols))
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(v for row in pix for v in row), bytes(pal)


def icon():
    """32x32 banner icon: a closed ledger with a brass clasp. Returns (rows, 16 colours)."""
    pal = [(0, 0, 0), (40, 24, 16), (120, 70, 40), (196, 150, 90), (236, 222, 186), (70, 120, 124),
           (214, 160, 60), (20, 40, 44)] + [(0, 0, 0)] * 8
    pix = [[7] * 32 for _ in range(32)]
    for y in range(4, 29):
        for x in range(5, 28):
            pix[y][x] = 2
    for y in range(4, 29):
        pix[y][5] = pix[y][6] = 1
        pix[y][27] = 1
    for x in range(5, 28):
        pix[4][x] = pix[28][x] = 1
    for y in range(7, 26):
        for x in range(9, 25):
            pix[y][x] = 3 if (x + y) % 11 else 4
    for y in range(11, 21):
        for x in range(13, 21):
            pix[y][x] = 5
    for y in range(14, 18):
        for x in range(15, 19):
            pix[y][x] = 6
    return pix, pal
