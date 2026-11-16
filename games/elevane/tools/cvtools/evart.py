# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Elevane's own title picture: an isometric voxel gate on a dark workbench
floor grid, its two silhouettes (front and plan, the player's orders) as
insets, and the name built from small cubes. Drawn in code from integer
maths only, so the bytes never change between runs.

The structure goes through the puzzle model's own rules (support, views), so
the picture shows a deliverable order, not an arbitrary pile."""

import os
import sys

from . import font
from .gfx import ramp, rgb15

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from evpuzzle import model  # noqa: E402

W, H = 256, 192
TEXT_TOP = 144          # rows 18-23 are the text layer's: the picture stays dark there

# Palette indices start at BASE so the text layer's 16-colour lines (entries
# 0-95 of the BG palette) stay untouched. Every pixel is opaque.
BASE = 96
BG_N = 16
BG = BASE                       # bench backdrop, top to bottom
GRID = BG + BG_N                # floor grid line
GRID_AXIS = GRID + 1            # the two lines that cross at the origin cell
LEVELS = 4                      # one tint per layer of the structure
TOP = GRID_AXIS + 1             # LEVELS entries per face
LEFT = TOP + LEVELS
RIGHT = LEFT + LEVELS
EDGE = RIGHT + LEVELS           # cube outline
WORD_N = 8
WORD = EDGE + 1                 # wordmark face, top to bottom
WORD_TOP = WORD + WORD_N
WORD_LEFT = WORD_TOP + 1
WORD_DARK = WORD_LEFT + 1
SHADOW = WORD_DARK + 1
FRONT_CLEAR = SHADOW + 1        # = RGB15(3, 4, 10), the front view's clear colour in play.c
PLAN_CLEAR = FRONT_CLEAR + 1    # = RGB15(4, 3, 8), the plan view's
FRAME = PLAN_CLEAR + 1
CELL_LIT = FRAME + 1            # a filled cell of a silhouette
LABEL = CELL_LIT + 1
LAST = LABEL

HW, HH, CH = 12, 6, 12          # half width, half height of a top face, cube height
CX, BASEY = 116, 96

# The gate: a floor slab, two pillars, a lintel and a cap. Layers z0.., rows y0.., x left to right.
LAYERS = (
    '#####/#####/.###.',
    '##.##/##.##/.....',
    '#####/#####/.....',
    '..#../..#../.....',
)


def structure():
    cells = set()
    for z, layer in enumerate(LAYERS):
        for y, row in enumerate(layer.split('/')):
            for x, ch in enumerate(row):
                if ch == '#':
                    cells.add((x, y, z))
    s = model.Structure(5, 3, len(LAYERS), cells)
    assert model.supported(s.cells), 'the title structure must stand'
    assert not model.check_structure(model.Order.of(s), s.cells)
    return s


def _rect(pix, x0, y0, x1, y1, v):
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            pix[y][x] = v


def _line(pix, x0, y0, x1, y1, v):
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i // n
        y = y0 + (y1 - y0) * i // n
        if 0 <= x < W and 0 <= y < TEXT_TOP:
            pix[y][x] = v


def _cube(pix, cx, cy, z):
    """One cube whose top face is centred on (cx, cy): top, left and right faces."""
    for x in range(cx - HW, cx + HW):
        d = abs(x - cx)
        h = HH * (HW - d) // HW
        # Top rhombus.
        for y in range(cy - h, cy + h):
            if 0 <= x < W and 0 <= y < H:
                pix[y][x] = EDGE if y in (cy - h, cy + h - 1) else TOP + z
        # Side face below the rhombus' lower edge.
        face = LEFT if x < cx else RIGHT
        for y in range(cy + h, cy + h + CH):
            if 0 <= x < W and 0 <= y < H:
                edge = x in (cx - HW, cx + HW - 1, cx) or y == cy + h + CH - 1
                pix[y][x] = EDGE if edge and x != cx else face + z


def _floor(pix):
    # Grid lines on the floor plane (the bottom of the z = 0 cubes).
    def pt(gx, gy):
        return CX + (gx - gy) * HW, BASEY + (gx + gy - 1) * HH

    for g in range(-3, 9):
        a, b = pt(g, -3), pt(g, 8)
        _line(pix, a[0], a[1], b[0], b[1], GRID_AXIS if g == 0 else GRID)
        a, b = pt(-3, g), pt(8, g)
        _line(pix, a[0], a[1], b[0], b[1], GRID_AXIS if g == 0 else GRID)


def _glyphs(pix, text, x0, y0, scale, paint):
    cw = 6 * scale
    for i, ch in enumerate(text):
        g = font.font_pixels(ch)
        for gy in range(8):
            for gx in range(8):
                if g[gy][gx]:
                    paint(x0 + i * cw + (gx - 1) * scale, y0 + gy * scale, gy)
    return len(text) * cw


def _wordmark(pix, text, x0, y0, scale):
    lit = set()

    def collect(x, y, gy):
        lit.add((x, y, gy))
    _glyphs(pix, text, x0, y0, scale, collect)
    # Drop shadow first, then each lit pixel as a small cube (top row, left column, shaded face).
    for (x, y, gy) in sorted(lit):
        _rect(pix, x + 2, y + 2, x + scale + 2, y + scale + 2, SHADOW)
    for (x, y, gy) in sorted(lit):
        for sy in range(scale):
            for sx in range(scale):
                px, py = x + sx, y + sy
                if not (0 <= px < W and 0 <= py < H):
                    continue
                if sy == 0:
                    v = WORD_TOP
                elif sx == 0:
                    v = WORD_LEFT
                elif sx == scale - 1 or sy == scale - 1:
                    v = WORD_DARK
                else:
                    v = WORD + min(WORD_N - 1, gy)
                pix[py][px] = v


def _inset(pix, x0, y0, w, h, clear, cols, rows, has, label):
    """A silhouette box: `has(c, r)` says whether cell column c, row r (0 = top) is filled."""
    _rect(pix, x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2, FRAME)
    _rect(pix, x0, y0, x0 + w, y0 + h, clear)
    cell = min(w // cols, h // rows)
    ox, oy = x0 + (w - cols * cell) // 2, y0 + (h - rows * cell) // 2
    for r in range(rows):
        for c in range(cols):
            if has(c, r):
                _rect(pix, ox + c * cell + 1, oy + r * cell + 1, ox + (c + 1) * cell, oy + (r + 1) * cell, CELL_LIT)

    def paint(x, y, gy):
        if 0 <= x < W and 0 <= y < H:
            pix[y][x] = LABEL
    _glyphs(pix, label, x0 + (w - 6 * len(label)) // 2, y0 + h + 6, 1, paint)


def title_art():
    """Returns (8bpp pixels 256x192, 256-entry BGR555 palette bytes)."""
    s = structure()
    pix = [[BG + y * BG_N // H for _ in range(W)] for y in range(H)]
    _floor(pix)

    # Cubes far to near: the sum of the three axes orders them for this camera.
    for (x, y, z) in sorted(s.cells, key=lambda c: (c[0] + c[1] + c[2], c[2], c[0])):
        _cube(pix, CX + (x - y) * HW, BASEY + (x + y) * HH - (z + 1) * CH, z)

    # The player's orders: front view (x across, z up) and plan view (x across, y up, like the play scene).
    order = model.Order.of(s)
    _inset(pix, 14, 68, 48, 40, FRONT_CLEAR, s.w, s.h,
           lambda c, r: order.front_has(c, s.h - 1 - r), 'FRONT')
    _inset(pix, 194, 68, 48, 40, PLAN_CLEAR, s.w, s.d,
           lambda c, r: order.plan_has(c, s.d - 1 - r), 'PLAN')

    _wordmark(pix, 'ELEVANE', (W - 7 * 24) // 2 + 4, 4, 4)

    cols = [(0, 0, 0)] * BASE
    cols += ramp((10, 14, 38), (4, 5, 16), BG_N)
    cols += [(26, 34, 78), (58, 78, 150)]
    for base_col in ((236, 200, 120), (176, 138, 70), (124, 94, 50)):
        cols += [tuple(int(c * (0.78 + 0.07 * z)) for c in base_col) for z in range(LEVELS)]
    cols += [(30, 18, 12)]
    cols += ramp((150, 196, 250), (64, 108, 196), WORD_N)
    cols += [(244, 248, 255), (190, 218, 252), (24, 36, 98), (6, 8, 28)]
    cols += [(3 * 8, 4 * 8, 10 * 8), (4 * 8, 3 * 8, 8 * 8), (92, 104, 160), (226, 188, 100), (190, 200, 235)]
    assert len(cols) == LAST + 1
    cols += [(0, 0, 0)] * (256 - len(cols))
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(v for row in pix for v in row), bytes(pal)
