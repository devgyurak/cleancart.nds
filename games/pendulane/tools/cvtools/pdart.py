# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Pendulane's title art, drawn by code: the name in brass lettering (the 5x7 font scaled 4x, strokes fattened, a dark
outline, four brass bands with a bright top edge and rivets) as a 4bpp tile BG, and the pendulum's pieces as sprites: a brass
bob (32x32) and a rod dot (8x8). Integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import rgb15, tiles_4bpp

TEXT = 'PENDULANE'
SCALE = 4
COLS = len(TEXT) * 6 * SCALE // 8 + 1     # 28 tiles: 216 px of letters plus a margin
ROWS = 4                                  # 32 pixels tall (an 8-row glyph at scale 4)
W, H = COLS * 8, ROWS * 8
PAL_LINE = 8                              # BG palette line (entries 0-95 are the text layer's)

# Palette indices of the lettering (0 transparent).
OUTLINE, BRASS0, BRASS1, BRASS2, BRASS3, SHINE, RIVET, PANEL = 1, 2, 3, 4, 5, 6, 7, 8
COLOURS = [(0, 0, 0), (22, 14, 8), (250, 214, 120), (226, 172, 72), (190, 128, 44), (140, 88, 30),
           (255, 250, 220), (80, 50, 20), (10, 12, 36)]
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
                            lit.add((4 + i * 6 * SCALE + (gx - 1) * SCALE + sx, gy * SCALE + sy))
    fat = {(x + dx, y + dy) for (x, y) in lit for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    return {p for p in fat if 0 <= p[0] < W and 0 <= p[1] < H}


def title_lettering():
    """Returns (4bpp tile bytes incl. the blank and panel tiles, map entries, 32-byte palette line); the map is COLS x ROWS entries
    in raster order with the palette line already in bits 12-15."""
    lit = _lit()
    pix = [[0] * W for _ in range(H)]
    for (x, y) in lit:
        if any((x + dx, y + dy) not in lit for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            pix[y][x] = OUTLINE
            continue
        band = (y * 4) // H                       # four brass bands from top to bottom
        pix[y][x] = (BRASS0, BRASS1, BRASS2, BRASS3)[band]
        if (x, y - 1) in lit and pix[y - 1][x] == OUTLINE:
            pix[y][x] = SHINE                     # the bright edge under the top outline
        elif (x + y) % 23 == 0 and x % 2 == 0:
            pix[y][x] = RIVET                     # a sparse scatter of rivets
    tiles = tiles_4bpp(pix, W, H) + bytes(32) + bytes([PANEL | PANEL << 4]) * 32
    entries = [i | PAL_LINE << 12 for i in range(COLS * ROWS)]
    cols = COLOURS + [(0, 0, 0)] * (16 - len(COLOURS))
    return bytes(tiles), entries, _pal(cols)


def _pal(cols):
    out = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        out += bytes((v & 0xFF, v >> 8))
    return bytes(out)


# --- sprites -------------------------------------------------------------------------------------------------
# OBJ palette line 0: 0 transparent, 1 dark rim, 2-4 brass from light to dark, 5 shine, 6 rod.
OBJ_COLOURS = [(0, 0, 0), (22, 14, 8), (250, 214, 120), (226, 172, 72), (170, 112, 40), (255, 250, 220), (120, 96, 70)]


def bob_pixels():
    """32x32 brass bob: a dark rim, three bands of brass by distance from a light spot at the upper left, a shine and an eye
    at the top where the rod ties on."""
    n = 32
    pix = [[0] * n for _ in range(n)]
    cx, cy, r = 15.5, 17.5, 14.0
    for y in range(n):
        for x in range(n):
            dx, dy = x - cx, y - cy
            d2 = dx * dx + dy * dy
            if d2 <= r * r:
                lx, ly = x - (cx - 5), y - (cy - 6)
                ld = lx * lx + ly * ly
                if d2 > (r - 1.6) ** 2:
                    pix[y][x] = 1
                elif ld < 30:
                    pix[y][x] = 5
                elif ld < 170:
                    pix[y][x] = 2
                elif ld < 400:
                    pix[y][x] = 3
                else:
                    pix[y][x] = 4
    for y in range(0, 4):                         # the eye the rod hangs from
        for x in range(14, 18):
            pix[y][x] = 1 if (x in (14, 17) or y in (0, 3)) else 6
    return pix


def dot_pixels():
    pix = [[0] * 8 for _ in range(8)]
    for y in range(2, 6):
        for x in range(2, 6):
            pix[y][x] = 6 if (x in (2, 5) or y in (2, 5)) else 5
    return pix


def sprites():
    """Returns (bob tiles 4bpp, dot tiles 4bpp, OBJ palette line)."""
    return (bytes(tiles_4bpp(bob_pixels(), 32, 32)), bytes(tiles_4bpp(dot_pixels(), 8, 8)),
            _pal(OBJ_COLOURS + [(0, 0, 0)] * (16 - len(OBJ_COLOURS))))
