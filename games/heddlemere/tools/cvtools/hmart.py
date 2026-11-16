# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Heddlemere's title lettering (scaffold placeholder, TODO(title): replaced by the loom picture). The name is
woven: dark warp lines around each letter and between the pieces, every piece a coloured thread. It is
a 4bpp tile BG (BG2), 24 x 4 tiles, one 16-colour palette line.
Integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import rgb15, tiles_4bpp

TEXT = 'HEDDLEMERE'
SCALE = 4
COLS = len(TEXT) * 6 * SCALE // 8     # 24 tiles wide
ROWS = 4                              # 32 pixels tall (an 8-row glyph at scale 4)
W, H = COLS * 8, ROWS * 8
PAL_LINE = 8                          # BG palette line: entries 0-95 are the text layer's (lines 0-5)

LEAD = 1                              # index 0 is transparent; 1 = lead; 2.. = panes
PANES = (
    (222, 64, 54), (236, 156, 40), (238, 214, 84), (70, 170, 96),
    (60, 168, 200), (84, 96, 214), (156, 78, 190), (230, 120, 170),
)
PANE_BASE = 2
LAST_INDEX = PANE_BASE + len(PANES) - 1
PANEL = LAST_INDEX + 1                # the dark strip the bottom text rows sit on
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
    # Fatten the strokes by a pixel so there is room for lead and glass in each.
    fat = {(x + dx, y + dy) for (x, y) in lit for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    return {p for p in fat if 0 <= p[0] < W and 0 <= p[1] < H}


def _piece(x, y):
    # Diamond panes of about 6 px; a cheap integer hash picks the colour.
    a, b = (x + y) // 6, (x - y + 64) // 6
    return ((a * 73856093) ^ (b * 19349663) ^ 0x5BD1) >> 3, a, b


def title_lettering():
    """Returns (4bpp tile bytes incl. the blank and panel tiles, map entries, 32-byte palette line);
    the map is COLS x ROWS entries in raster order with the palette line already in bits 12-15."""
    lit = _lit()
    pix = [[0] * W for _ in range(H)]
    for (x, y) in lit:
        outside = any((x + dx, y + dy) not in lit for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if outside:
            pix[y][x] = LEAD
            continue
        h, a, b = _piece(x, y)
        edge = any(_piece(x + dx, y + dy)[1:] != (a, b) for dx, dy in ((1, 0), (0, 1)))
        pix[y][x] = LEAD if edge else PANE_BASE + (h % len(PANES))
    tiles = tiles_4bpp(pix, W, H) + bytes(32) + bytes([PANEL | PANEL << 4]) * 32
    entries = [i | PAL_LINE << 12 for i in range(COLS * ROWS)]
    cols = [(0, 0, 0), (34, 26, 40)] + list(PANES) + [PANEL_COLOUR] + [(0, 0, 0)] * (16 - 3 - len(PANES))
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(tiles), entries, bytes(pal)
