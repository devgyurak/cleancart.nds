# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Lightenette's own title picture: a lantern glowing over a dark stage, a
marionette hanging from a control bar and the game's name. Drawn in code
from integer maths only, so the bytes never change between runs."""

from . import font
from .gfx import ramp, rgb15

W, H = 256, 192
# Palette indices start at BASE so the text layer's 16-colour lines (entries
# 0-95 of the BG palette) stay untouched. Index 0 is transparent (backdrop).
BASE = 96
GLOW = BASE            # 48 steps, dark to warm light
GLOW_N = 48
WORD = BASE + GLOW_N   # 16 steps for the wordmark
WORD_N = 16
STRING = WORD + WORD_N       # thread
WOOD = STRING + 1            # control bar
PUPPET = STRING + 2          # marionette silhouette
EDGE = STRING + 3            # wordmark outline
SHADOW = STRING + 4          # wordmark drop shadow


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


def title_art():
    """Returns (8bpp pixels 256x192, 256-entry BGR555 palette bytes)."""
    pix = [[0] * W for _ in range(H)]

    # Lantern glow: concentric steps around the lantern at (204, 52).
    lx, ly = 204, 52
    for y in range(H):
        for x in range(W):
            d = _isqrt((x - lx) ** 2 + (y - ly) ** 2)
            if d < 96:
                step = (96 - d) * GLOW_N // 96
                if step > 0:
                    pix[y][x] = GLOW + min(GLOW_N - 1, step)

    # Lantern: hook, cap, glass body, base.
    _line(pix, lx, 0, lx, 22, STRING)
    _rect(pix, lx - 10, 22, lx + 11, 27, PUPPET)
    _rect(pix, lx - 8, 27, lx + 9, 29, WOOD)
    for y in range(29, 61):
        half = 8 + (y - 29) // 6
        for x in range(lx - half, lx + half + 1):
            pix[y][x] = GLOW + GLOW_N - 1 - abs(x - lx) // 4
    _rect(pix, lx - 11, 61, lx + 12, 66, PUPPET)

    # Marionette: control bar, strings, jointed puppet.
    bx0, bx1, by = 40, 132, 14
    _rect(pix, bx0, by, bx1, by + 4, WOOD)
    _rect(pix, 84, by - 8, 88, by, WOOD)
    px, py = 86, 62
    pts = {
        'head': (px, py - 18), 'hand_l': (px - 24, py + 10), 'hand_r': (px + 24, py + 6),
        'foot_l': (px - 10, py + 46), 'foot_r': (px + 12, py + 44),
    }
    for key, bar_x in (('head', 86), ('hand_l', bx0 + 6), ('hand_r', bx1 - 6),
                       ('foot_l', bx0 + 30), ('foot_r', bx1 - 30)):
        tx, ty = pts[key]
        _line(pix, bar_x, by + 4, tx, ty, STRING)
    _disc(pix, px, py - 12, 7, PUPPET)
    _rect(pix, px - 7, py - 4, px + 8, py + 22, PUPPET)
    _line(pix, px - 6, py, pts['hand_l'][0], pts['hand_l'][1], PUPPET, 1)
    _line(pix, px + 6, py, pts['hand_r'][0], pts['hand_r'][1], PUPPET, 1)
    _line(pix, px - 3, py + 22, pts['foot_l'][0], pts['foot_l'][1], PUPPET, 1)
    _line(pix, px + 3, py + 22, pts['foot_r'][0], pts['foot_r'][1], PUPPET, 1)
    # The puppet's side facing the lantern catches a warm rim.
    for y in range(py - 4, py + 22):
        pix[y][px + 7] = GLOW + 30

    # Floor line under the stage.
    _rect(pix, 0, 110, W, 111, WOOD)

    # Wordmark: the game font at 3x with a bevelled warm ramp and a shadow.
    text = "LIGHTENETTE"
    scale = 3
    cw = 6 * scale
    x0 = (W - len(text) * cw) // 2 + scale
    y0 = 116
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
    cols += ramp((6, 5, 14), (255, 214, 120), GLOW_N)
    cols += ramp((255, 238, 170), (214, 96, 40), WORD_N)
    cols += [(150, 150, 170), (92, 60, 36), (74, 62, 104), (255, 255, 255), (30, 12, 20)]
    cols += [(0, 0, 0)] * (256 - len(cols))
    pal = bytearray()
    for r, g, b in cols:
        v = rgb15(r, g, b)
        pal += bytes((v & 0xFF, v >> 8))
    return bytes(v for row in pix for v in row), bytes(pal)
