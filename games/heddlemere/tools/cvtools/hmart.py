# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Heddlemere's title lettering: the name woven in six thread colours with a dark outline, a 4bpp tile BG (BG2),
30 x 4 tiles, one 16-colour palette line; the title scene puts it over a raster-woven cloth (gfx/loom.c).
Integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import rgb15, tiles_4bpp

TEXT = 'HEDDLEMERE'
SCALE = 4
COLS = len(TEXT) * 6 * SCALE // 8     # 30 tiles wide
ROWS = 4                              # 32 pixels tall (an 8-row glyph at scale 4)
W, H = COLS * 8, ROWS * 8
PAL_LINE = 8                          # BG palette line: entries 0-95 are the text layer's (lines 0-5)

LEAD = 1                              # index 0 is transparent; 1 = the dark outline; 2-7 threads; 8-13 the same, darker
THREADS = (
    (232, 60, 64), (240, 140, 36), (236, 208, 70), (70, 180, 96), (60, 120, 232), (160, 80, 210),
)
PANE_BASE = 2
SHADE_BASE = PANE_BASE + len(THREADS)
PANEL = SHADE_BASE + len(THREADS)     # the dark strip the bottom text rows sit on
PANEL_COLOUR = (10, 12, 36)
# Two tiles follow the lettering tiles: a blank one (the map's empty cells; tile 0 would show the
# first letter tile) and a solid panel one.
BLANK_TILE = COLS * ROWS
PANEL_TILE = BLANK_TILE + 1



def _lit():
    lit = set()
    for i, ch in enumerate(TEXT):
        g = font.font_pixels(ch)
        for gy in range(8):
            for gx in range(8):
                if g[gy][gx]:
                    for sy in range(SCALE):
                        for sx in range(SCALE):
                            lit.add((i * 6 * SCALE + (gx - 1) * SCALE + sx, gy * SCALE + sy))
    # Fatten the strokes by a pixel so there is room for the outline and the thread in each.
    fat = {(x + dx, y + dy) for (x, y) in lit for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    return {p for p in fat if 0 <= p[0] < W and 0 <= p[1] < H}


def title_lettering():
    """Returns (4bpp tile bytes incl. the blank and panel tiles, map entries, 32-byte palette line);
    the map is COLS x ROWS entries in raster order with the palette line already in bits 12-15.
    A letter is woven: its thread colour changes every 12 columns and 8 lines, and a 2x2 checker of
    darker threads shows the weft going over and under the warp."""
    lit = _lit()
    pix = [[0] * W for _ in range(H)]
    for (x, y) in lit:
        outside = any((x + dx, y + dy) not in lit for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if outside:
            pix[y][x] = LEAD
            continue
        c = (x // 12 + y // 8) % len(THREADS)
        under = ((x >> 1) + (y >> 1)) & 1
        pix[y][x] = (SHADE_BASE if under else PANE_BASE) + c
    tiles = tiles_4bpp(pix, W, H) + bytes(32) + bytes([PANEL | PANEL << 4]) * 32
    entries = [i | PAL_LINE << 12 for i in range(COLS * ROWS)]
    shaded = [(r * 5 // 8, g * 5 // 8, b * 5 // 8) for (r, g, b) in THREADS]
    cols = [(0, 0, 0), (34, 26, 40)] + list(THREADS) + shaded + [PANEL_COLOUR] + [(0, 0, 0)]
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(tiles), entries, bytes(pal)
