# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Buoyane's title lettering: the name in a chart engraver's hand, pale ink with a deep-blue shadow
stroke and a dotted line of depth marks under it. A 4bpp tile BG (BG2 over the 3D harbour scene),
21 x 4 tiles, one 16-colour palette line. Integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import rgb15, tiles_4bpp

TEXT = 'BUOYANE'
SCALE = 4
COLS = len(TEXT) * 6 * SCALE // 8 + 1      # 22 tiles wide (a spare tile for the shadow)
ROWS = 5                                    # 40 pixels tall: an 8-row glyph at scale 4, a shadow and the depth marks
W, H = COLS * 8, ROWS * 8
PAL_LINE = 8                                # BG palette line: entries 0-95 are the text layer's (lines 0-5)

INK, SHADOW, MARK, PANEL = 1, 2, 3, 4       # index 0 is transparent
PANEL_COLOUR = (8, 10, 28)
# Two tiles follow the lettering tiles: a blank one (empty map cells; tile 0 would show a letter tile)
# and a solid panel one.
BLANK_TILE = COLS * ROWS
PANEL_TILE = BLANK_TILE + 1


def _ink():
    lit = set()
    for i, ch in enumerate(TEXT):
        g = font.font_pixels(ch)
        for gy in range(8):
            for gx in range(8):
                if g[gy][gx]:
                    for sy in range(SCALE):
                        for sx in range(SCALE):
                            lit.add((4 + i * 6 * SCALE + (gx - 1) * SCALE + sx, 1 + gy * SCALE + sy))
    return lit


def title_lettering():
    """Returns (4bpp tile bytes incl. the blank and panel tiles, map entries, 32-byte palette line);
    the map is COLS x ROWS entries in raster order with the palette line already in bits 12-15."""
    ink = _ink()
    pix = [[0] * W for _ in range(H)]
    for (x, y) in ink:                       # the shadow stroke: the glyph shifted down and right
        for d in (2, 3):
            sx, sy = x + d, y + d
            if 0 <= sx < W and 0 <= sy < H and (sx, sy) not in ink:
                pix[sy][sx] = SHADOW
    for (x, y) in ink:
        if 0 <= x < W and 0 <= y < H:
            pix[y][x] = INK
    y = 4 + 8 * SCALE + 3                    # depth marks: a dotted line with a longer tick every fifth dot
    for x in range(4, W - 4, 4):
        long_tick = ((x - 4) // 4) % 5 == 0
        for dy in range(3 if long_tick else 1):
            if y + dy < H:
                pix[y + dy][x] = MARK
    tiles = tiles_4bpp(pix, W, H) + bytes(32) + bytes([PANEL | PANEL << 4]) * 32
    entries = [i | PAL_LINE << 12 for i in range(COLS * ROWS)]
    cols = [(0, 0, 0), (226, 232, 246), (22, 34, 92), (236, 196, 90), PANEL_COLOUR] + [(0, 0, 0)] * 11
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(tiles), entries, bytes(pal)
