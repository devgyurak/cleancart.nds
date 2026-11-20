# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The font and UI tiles of Buoyane, drawn in code."""

from . import font
from .gfx import tiles_4bpp, palette_bytes  # noqa: F401  (palette_bytes is used through images.)


def blank(w, h, v=0):
    return [[v] * w for _ in range(h)]


# ---------------------------------------------------------------------------
# Font (4bpp, colour 1 = ink, colour 2 = shadow)

def font_tiles():
    out = bytearray()
    for ch in font.all_chars():
        px = font.font_pixels(ch)
        cell = blank(8, 8)
        for y in range(8):
            for x in range(8):
                if px[y][x]:
                    cell[y][x] = 1
                    if y + 1 < 8 and x + 1 < 8 and not px[y + 1][x + 1]:
                        if cell[y + 1][x + 1] == 0:
                            cell[y + 1][x + 1] = 2
        out += tiles_4bpp(cell, 8, 8)
    return bytes(out)


def ui_tiles():
    """Window frame and gauge tiles that follow the font in BG VRAM.
    0 solid fill, 1-8 frame (tl t tr l r bl b br), 9-17 gauge 0..8 px."""
    tiles = []
    solid = blank(8, 8, 3)
    tiles.append(solid)
    def frame(edges):
        t = blank(8, 8, 3)
        for y in range(8):
            for x in range(8):
                if ('t' in edges and y == 1) or ('b' in edges and y == 6) or \
                   ('l' in edges and x == 1) or ('r' in edges and x == 6):
                    t[y][x] = 4
                if ('t' in edges and y == 0) or ('b' in edges and y == 7) or \
                   ('l' in edges and x == 0) or ('r' in edges and x == 7):
                    t[y][x] = 0
        return t
    for e in ('tl', 't', 'tr', 'l', 'r', 'bl', 'b', 'br'):
        tiles.append(frame(e))
    for n in range(9):
        t = blank(8, 8, 0)
        for y in range(2, 6):
            for x in range(8):
                t[y][x] = 5 if x < n else 6
        tiles.append(t)
    out = bytearray()
    for t in tiles:
        out += tiles_4bpp(t, 8, 8)
    return bytes(out)


UI_PALETTE = [
    (0, 0, 0), (240, 244, 255), (20, 24, 48), (14, 22, 44), (90, 170, 255),
    (255, 200, 60), (40, 50, 80), (255, 90, 90), (120, 255, 160),
    (255, 255, 255), (60, 60, 60), (200, 120, 255), (255, 150, 40),
    (80, 220, 255), (255, 255, 120), (128, 128, 160)]
