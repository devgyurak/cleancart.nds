# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Every bitmap, tile set, sprite and texture in Hushwell, drawn in code."""

import math

from . import font
from .gfx import (Rng, fbm, mix, rgb15, ramp, tiles_4bpp, tiles_8bpp,
                  dedupe_tiles, u16s, palette_bytes, tex_a3i5, tex_a5i3,
                  tex_pal4, tex_pal16, tex_pal256, tex_direct, tex_4x4)


def blank(w, h, v=0):
    return [[v] * w for _ in range(h)]


def upscale(lo, lw, lh, f):
    """Bilinear upscale of a float grid by integer factor f."""
    w, h = lw * f, lh * f
    out = []
    for y in range(h):
        gy = y / f
        y0 = int(gy) % lh
        y1 = (y0 + 1) % lh
        ty = gy - int(gy)
        r0, r1 = lo[y0], lo[y1]
        row = []
        for x in range(w):
            gx = x / f
            x0 = int(gx) % lw
            x1 = (x0 + 1) % lw
            tx = gx - int(gx)
            a = r0[x0] + (r0[x1] - r0[x0]) * tx
            b = r1[x0] + (r1[x1] - r1[x0]) * tx
            row.append(a + (b - a) * ty)
        out.append(row)
    return out


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




# ---------------------------------------------------------------------------
# Sprites (4bpp sheet, 1D mapping)

SPRITE_PALETTE = [
    (0, 0, 0), (255, 255, 255), (190, 190, 196), (120, 120, 128),
    (64, 64, 72), (18, 18, 22), (255, 236, 170), (255, 150, 40),
    (220, 30, 30), (110, 10, 14), (232, 222, 192), (60, 54, 48),
    (110, 130, 150), (90, 230, 120), (120, 80, 50), (190, 240, 255)]


def _circle(size, r_in, r_out, c_in, c_out, cx=None, cy=None):
    g = blank(size, size)
    cx = (size - 1) / 2 if cx is None else cx
    cy = (size - 1) / 2 if cy is None else cy
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - cx, y - cy)
            if d <= r_in:
                g[y][x] = c_in
            elif d <= r_out:
                g[y][x] = c_out
    return g


def _poly_fill(g, pts, col):
    """Even-odd scanline fill of a polygon on a pixel grid."""
    h, w = len(g), len(g[0])
    for y in range(h):
        yc = y + 0.5
        xs = []
        for i in range(len(pts)):
            (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % len(pts)]
            if (y0 <= yc < y1) or (y1 <= yc < y0):
                xs.append(x0 + (yc - y0) * (x1 - x0) / (y1 - y0))
        xs.sort()
        for k in range(0, len(xs) - 1, 2):
            for x in range(max(0, int(xs[k] + 0.5)), min(w, int(xs[k + 1] + 0.5))):
                g[y][x] = col


def sprite_sheet():
    """Returns (tile bytes, dict name -> (tile index, w, h)). Tile index is in
    32-byte units (1D mapping, 32-byte boundary)."""
    items = []
    arrow = blank(16, 16)
    _poly_fill(arrow, [(8, 1), (14, 14), (8, 10), (2, 14)], 8)
    _poly_fill(arrow, [(8, 4), (11, 11), (8, 9), (5, 11)], 1)
    items.append(('arrow', 16, 16, arrow))
    pin = blank(8, 8)
    for i in range(7):
        pin[i][i] = pin[i][6 - i] = 11
    items.append(('pin', 8, 8, pin))
    reel = _circle(32, 14.5, 15.5, 4, 2)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x - 15.5, y - 15.5)
            a = math.atan2(y - 15.5, x - 15.5)
            if d < 3:
                reel[y][x] = 1
            elif d < 11 and math.cos(3 * a) > 0.55:
                reel[y][x] = 0 if d > 4.5 else 3
            elif 11 <= d < 14.5:
                reel[y][x] = 14
    items.append(('reel', 32, 32, reel))
    needle = blank(32, 32)
    for y in range(3, 16):
        needle[y][15] = needle[y][16] = 8
    needle[15][15] = needle[15][16] = needle[16][15] = needle[16][16] = 5
    items.append(('needle', 32, 32, needle))
    dial = blank(64, 32)
    for y in range(32):
        for x in range(64):
            d = math.hypot(x - 31.5, y - 31)
            a = math.degrees(math.atan2(31 - y, x - 31.5))
            if 28 < d < 30:
                dial[y][x] = 2
            elif 20 < d < 28 and 30 < a < 150:
                dial[y][x] = 5 if a > 50 else 9
            if 22 < d < 28 and int(a) % 20 == 0 and 30 <= a <= 150:
                dial[y][x] = 1
    items.append(('dial', 64, 32, dial))
    # Keyhole mask for the OBJ window: only its shape matters.
    hole = blank(64, 64)
    for y in range(64):
        for x in range(64):
            if math.hypot(x - 31.5, y - 22) < 15:
                hole[y][x] = 1
    _poly_fill(hole, [(24, 26), (40, 26), (47, 62), (17, 62)], 1)
    items.append(('keyhole', 64, 64, hole))
    items.append(('mote', 8, 8, _circle(8, 0.7, 1.8, 1, 2)))
    drop = blank(8, 8)
    for x in range(1, 7):
        drop[6][x] = 15
    drop[5][1] = drop[5][6] = drop[4][0] = drop[4][7] = 12
    items.append(('drop', 8, 8, drop))
    bug = blank(8, 8)
    _poly_fill(bug, [(3, 1), (5, 1), (6, 6), (2, 6)], 5)
    for k in (2, 4):
        bug[k][1] = bug[k][6] = 4
    bug[2][3] = bug[2][4] = 8
    items.append(('bug', 8, 8, bug))
    key = blank(16, 16)
    for y in range(16):
        for x in range(16):
            if math.hypot(x - 4.5, y - 7.5) < 3.6 and math.hypot(x - 4.5, y - 7.5) > 1.6:
                key[y][x] = 6
    for x in range(8, 15):
        key[7][x] = key[8][x] = 6
    key[9][12] = key[10][12] = key[9][14] = key[10][14] = 6
    items.append(('key', 16, 16, key))
    tape = blank(16, 16)
    for y in range(4, 12):
        for x in range(1, 15):
            tape[y][x] = 3 if 5 < y < 10 and 3 < x < 12 else 4
    for cx in (5, 10):
        for y in range(6, 10):
            for x in range(cx - 1, cx + 1):
                tape[y][x] = 1
    items.append(('tape', 16, 16, tape))
    fl = blank(16, 16)
    for y in range(3, 14):
        for x in range(6, 10):
            fl[y][x] = 8 if y > 4 else 7
    fl[2][7] = fl[2][8] = 6
    items.append(('flare', 16, 16, fl))
    bat = blank(16, 16)
    for y in range(4, 13):
        for x in range(2, 13):
            bat[y][x] = 4 if (y in (4, 12) or x in (2, 12)) else 13
    for y in range(7, 10):
        bat[y][13] = 4
    items.append(('battery', 16, 16, bat))
    cross = blank(16, 16)
    for i in range(16):
        if i < 6 or i > 9:
            cross[7][i] = cross[8][i] = 7
            cross[i][7] = cross[i][8] = 7
    items.append(('cross', 16, 16, cross))
    items.append(('spark', 8, 8, _circle(8, 0.8, 2.2, 6, 7)))
    # Wiper blade from the sprite centre to the right edge; it turns about
    # the centre as an affine sprite.
    wiper = blank(64, 64)
    for x in range(32, 63):
        for dy in (-1, 0, 1):
            wiper[31 + dy][x] = 5 if dy else 4
        if x % 3 == 0:
            wiper[33][x] = 4
    items.append(('wiper', 64, 64, wiper))

    out = bytearray()
    index = {}
    for name, w, h, pix in items:
        index[name] = (len(out) // 32, w, h)
        out += tiles_4bpp(pix, w, h)
    return bytes(out), index


def portrait_8bpp():
    """32x32 8bpp portrait of a patient. Indices 1..63 encode shading so the
    extended OBJ palettes can show it fresh, faded, wrong or negative."""
    pix = blank(32, 32)
    for y in range(32):
        for x in range(32):
            dx, dy = (x - 15.5) / 11, (y - 13) / 14
            face = dx * dx + dy * dy < 1
            shade = 0.25
            if face:
                shade = 0.75 - 0.25 * dx - 0.15 * dy
                for ex in (11, 20):
                    if math.hypot(x - ex, y - 12) < 2.2:
                        shade = 0.08
                if 12 < x < 19 and 20 <= y <= 21:
                    shade = 0.15
            if y > 26:
                shade = 0.4 - 0.1 * abs(x - 15.5) / 16
            pix[y][x] = 1 + int(max(0.0, min(1.0, shade)) * 62)
    return tiles_8bpp(pix, 32, 32)


def portrait_palettes():
    sets = [((30, 20, 16), (240, 220, 200)),   # as taken
            ((50, 34, 20), (220, 196, 150)),   # faded sepia
            ((10, 30, 10), (200, 255, 120)),   # wrong
            ((250, 250, 250), (10, 10, 30))]   # negative
    pals = []
    for lo, hi in sets:
        cols = [(0, 0, 0)] + ramp(lo, hi, 63) + [(0, 0, 0)] * 192
        pals.append(palette_bytes(cols))
    return pals


def photo_bitmap():
    """64x64 16bpp bitmap OBJ: a torn sepia photograph of the sanatorium
    porch with a blurred figure in the doorway."""
    rng = Rng(64)
    out = []
    for y in range(64):
        for x in range(64):
            torn = x + y > 112 + 4 * math.sin(x * 0.9) or (x < 3 or y < 3 or x > 60 or y > 60)
            if torn:
                out.append(0)
                continue
            v = 0.55 + 0.25 * (y / 64)
            if 10 < x < 54 and 14 < y < 50:
                v = 0.35 + 0.1 * ((x // 6 + y // 9) % 2)
                if 27 < x < 37 and 30 < y < 50:
                    v = 0.12
                    if 29 < x < 35 and 33 < y < 47 and abs(x - 32) < 3 - (y - 33) * 0.05:
                        v = 0.7
            if y < 14:
                v = 0.7 - y * 0.01
            v += (rng.uniform() - 0.5) * 0.08
            v = max(0.0, min(1.0, v))
            c = mix((40, 26, 16), (238, 214, 170), v)
            out.append(rgb15(*c, a=1))
    return u16s(out)


# ---------------------------------------------------------------------------
# Backgrounds

def title_bitmap():
    """256x192 16bpp title illustration: the sanatorium on its hill under a
    sick moon, rain, and the stone well in front."""
    rng = Rng(42)
    w, h = 256, 192
    lo = [[fbm(x / 9, y / 9, 3) for x in range(64)] for y in range(48)]
    clouds = upscale(lo, 64, 48, 4)
    hill = [118 - 26 * math.exp(-((x - 150) / 70) ** 2) - 8 * fbm(x / 30, 0.5, 7, 3)
            for x in range(w)]
    px = []
    for y in range(h):
        for x in range(w):
            c = mix((4, 6, 16), (30, 36, 58), (y / 150) ** 1.4)
            d = math.hypot(x - 60, y - 44)
            if d < 17:
                c = mix((240, 236, 200), (200, 196, 150), d / 17)
            elif d < 50:
                c = mix(c, (120, 120, 100), (1 - (d - 17) / 33) * 0.35)
            cl = clouds[y][x]
            if cl > 0.55 and y < 110:
                c = mix(c, (50, 54, 70), min(1, (cl - 0.55) * 3) * 0.8)
            if y > hill[x]:
                c = mix((14, 16, 18), (6, 6, 8), (y - hill[x]) / 70)
            px.append(c)

    def rect(x0, y0, x1, y1, col):
        for yy in range(max(0, y0), min(h, y1)):
            for xx in range(max(0, x0), min(w, x1)):
                px[yy * w + xx] = col

    # The sanatorium: two wings, a central tower, rows of dead windows.
    body = (10, 10, 13)
    rect(96, 62, 214, 104, body)
    rect(140, 40, 170, 104, body)
    for k in range(8):
        rect(150 - k * 2, 32 + k, 160 + k * 2, 33 + k, body)
    for row in range(3):
        for col in range(10):
            wx = 102 + col * 11
            wy = 68 + row * 11
            if 136 < wx < 172:
                continue
            lit = (row, col) == (1, 7)
            rect(wx, wy, wx + 5, wy + 7, (230, 190, 90) if lit else (22, 24, 30))
    rect(151, 46, 159, 56, (26, 28, 34))
    # The well: a stone ring with a pitched roof.
    for yy in range(146, 176):
        for xx in range(30, 110):
            dx = (xx - 70) / 38
            if abs(dx) <= 1 and yy > 156 - 6 * math.sqrt(max(0, 1 - dx * dx)):
                stone = ((xx // 7 + (yy // 5) % 2) % 2)
                px[yy * w + xx] = (52, 50, 48) if stone else (38, 36, 36)
    rect(36, 118, 40, 152, (30, 22, 16))
    rect(100, 118, 104, 152, (30, 22, 16))
    for k in range(14):
        rect(30 + k * 2, 118 - k, 110 - k * 2, 120 - k, (24, 18, 14))
    rect(64, 150, 76, 154, (0, 0, 0))
    # Rain.
    for _ in range(260):
        x, y = rng.range(0, w), rng.range(0, h - 8)
        for k in range(6):
            if 0 <= x - k // 3 < w:
                px[(y + k) * w + x - k // 3] = mix(px[(y + k) * w + x - k // 3], (150, 170, 200), 0.35)
    return u16s([rgb15(*c, a=1) for c in px])


def logo_bitmap():
    """256x256 8bpp extended-affine bitmap: the HUSHWELL wordmark from the
    game font, with dripping lower edges. Palette 0 transparent."""
    w = h = 256
    pix = blank(w, h)
    text = "HUSHWELL"
    scale = 5
    cw = 6 * scale
    x0 = (w - len(text) * cw) // 2 + scale
    y0 = 128 - 4 * scale
    rng = Rng(5)
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
                        edge = sx == 0 or sy == 0
                        shade = 1 + int(15 * (gy * scale + sy) / (8 * scale))
                        pix[y][x] = 17 if edge else shade
                        if y + 2 < h and x + 2 < w and pix[y + 2][x + 2] == 0:
                            pix[y + 2][x + 2] = 18
                if gy == 7 or not g[gy + 1][gx]:
                    # Drips under some lower edges.
                    if rng.next() % 3 == 0:
                        x = x0 + i * cw + (gx - 1) * scale + scale // 2
                        ln = rng.range(3, 12)
                        for k in range(ln):
                            y = y0 + (gy + 1) * scale + k
                            if y < h and pix[y][x] == 0:
                                pix[y][x] = 16
    return bytes(v for row in pix for v in row)


def logo_palette():
    cols = [(0, 0, 0)] + ramp((236, 228, 210), (150, 20, 24), 16) + [(255, 250, 240), (6, 4, 6)]
    cols += [(0, 0, 0)] * (256 - len(cols))
    return palette_bytes(cols)


def rain_layer():
    """256x256 4bpp text BG of slanted rain streaks, a 64-pixel pattern so
    the tile set stays small. Scrolled per scanline by HBlank DMA."""
    w = h = 256
    rng = Rng(808)
    pat = blank(64, 64)
    for _ in range(46):
        x, y = rng.range(0, 64), rng.range(0, 64)
        ln = rng.range(5, 11)
        for k in range(ln):
            xx, yy = (x - k // 4) % 64, (y + k) % 64
            pat[yy][xx] = 1 + min(3, k // 3)
    pix = [[pat[y % 64][x % 64] for x in range(w)] for y in range(h)]
    tiles = tiles_4bpp(pix, w, h)
    uniq, idx = dedupe_tiles(tiles, 32)
    pal = [(0, 0, 0), (200, 215, 240), (150, 170, 205), (110, 125, 160), (80, 90, 120)]
    pal += [(0, 0, 0)] * (16 - len(pal))
    return uniq, u16s(idx), palette_bytes(pal)


CAUSTIC_PAL_BASE = 200


def caustic_tiles():
    """64x64 8bpp light caustics (palette 200-215) as 64 tiles for an affine
    BG over the chapter 2 water. Index 0 stays transparent."""
    pix = blank(64, 64)
    for y in range(64):
        for x in range(64):
            v = (math.sin(x * 0.29 + math.sin(y * 0.21) * 2.1)
                 + math.sin(y * 0.33 + math.sin(x * 0.17) * 1.7)) * 0.5
            v = 1 - abs(v)
            if v > 0.78:
                pix[y][x] = CAUSTIC_PAL_BASE + min(15, int((v - 0.78) * 70))
    return tiles_8bpp(pix, 64, 64)


def caustic_palette():
    return palette_bytes(ramp((40, 90, 100), (190, 240, 240), 16))


# Map tiles for the bottom-screen map (8bpp affine BG, palette 128+).
MAP_TILES = ['blank', 'floor', 'wall', 'water', 'ground', 'fence', 'door', 'open',
             'shelf', 'exit', 'radio', 'well']
MAP_PAL_BASE = 128


def map_palette():
    cols = [(232, 222, 192), (214, 202, 168), (60, 54, 48), (110, 100, 88),
            (80, 120, 170), (200, 40, 30), (40, 150, 70), (150, 140, 120),
            (24, 22, 20), (190, 176, 140)]
    return palette_bytes(cols)


def map_tiles():
    """8bpp 8x8 tiles in MAP_TILES order, drawn like pencil on paper."""
    P, P2, INK, INK2, BLUE, RED, GREEN, GREY, BLACK, EDGE = range(MAP_PAL_BASE, MAP_PAL_BASE + 10)
    out = bytearray()
    for name in MAP_TILES:
        t = blank(8, 8, P)
        if name == 'blank':
            t = blank(8, 8, 0)
        elif name == 'floor':
            for y in range(8):
                for x in range(8):
                    if (x * 3 + y * 5) % 11 == 0:
                        t[y][x] = P2
        elif name == 'wall':
            for y in range(8):
                for x in range(8):
                    t[y][x] = INK if (x + y) % 3 else INK2
        elif name == 'water':
            for y in range(8):
                for x in range(8):
                    if (y % 4 == 1 and x % 4 != 3) or (y % 4 == 2 and x % 4 == 3):
                        t[y][x] = BLUE
        elif name == 'ground':
            for y in range(8):
                for x in range(8):
                    if (x * 7 + y * 3) % 9 == 0:
                        t[y][x] = GREY
        elif name == 'fence':
            for i in range(8):
                t[i][i] = t[i][7 - i] = INK2
        elif name == 'door':
            for y in range(8):
                for x in range(8):
                    if 2 <= y <= 5:
                        t[y][x] = RED if 3 <= x <= 4 and y in (3, 4) else INK
        elif name == 'open':
            for x in range(8):
                t[2][x] = t[5][x] = GREY if x % 2 else P
        elif name == 'shelf':
            for y in range(8):
                for x in range(8):
                    if y % 3 == 0 or x in (0, 7):
                        t[y][x] = INK2
        elif name == 'exit':
            for i in range(1, 7):
                t[i][i] = t[i][7 - i] = RED
                t[i][min(7, i + 1)] = RED
        elif name == 'radio':
            for y in range(1, 7):
                for x in range(1, 7):
                    t[y][x] = GREEN if (y in (1, 6) or x in (1, 6)) else P
            t[3][3] = t[3][4] = t[4][3] = t[4][4] = GREEN
        elif name == 'well':
            for y in range(8):
                for x in range(8):
                    d = math.hypot(x - 3.5, y - 3.5)
                    if 2.2 < d < 3.6:
                        t[y][x] = INK
                    elif d <= 2.2:
                        t[y][x] = BLACK
        out += tiles_8bpp(t, 8, 8)
    return bytes(out)


BRIEF_SCENES = ['road', 'lobby', 'hydro', 'archive', 'chapel', 'well']


def briefing_art(scene):
    """256x128 8bpp tiled illustration for a radio transmission and its
    256-colour palette (shown through an extended BG palette slot)."""
    w, h = 256, 128
    rng = Rng(500 + BRIEF_SCENES.index(scene))
    themes = {
        'road':    ((6, 8, 16), (40, 48, 70), (90, 90, 96)),
        'lobby':   ((8, 8, 8), (60, 64, 56), (130, 128, 110)),
        'hydro':   ((4, 10, 14), (40, 80, 90), (150, 170, 170)),
        'archive': ((10, 6, 2), (70, 50, 26), (120, 90, 60)),
        'chapel':  ((4, 4, 18), (40, 30, 90), (150, 60, 90)),
        'well':    ((2, 0, 0), (40, 10, 8), (80, 70, 66)),
    }
    sky0, sky1, solid = themes[scene]
    pal = [(0, 0, 0)] + ramp(sky0, sky1, 128) + ramp(mix(solid, (0, 0, 0), 0.7), solid, 64) \
        + ramp((255, 236, 170), (255, 120, 40), 32)
    pal += [(0, 0, 0)] * (256 - len(pal))
    pix = blank(w, h)
    for y in range(h):
        for x in range(w):
            pix[y][x] = 1 + int(127 * (y / h) ** 1.3)

    def rect(x0, y0, x1, y1, shade_top=0.2):
        for y in range(max(0, y0), min(h, y1)):
            for x in range(max(0, x0), min(w, x1)):
                t = (x - x0) / max(1, x1 - x0)
                pix[y][x] = 129 + int(63 * (shade_top + (1 - shade_top) * t))

    def disc(cx, cy, r, base, ry=None):
        ry = r if ry is None else ry
        for y in range(max(0, int(cy - ry)), min(h, int(cy + ry) + 1)):
            for x in range(max(0, int(cx - r)), min(w, int(cx + r) + 1)):
                d = math.hypot((x - cx) / r, (y - cy) / ry)
                if d <= 1:
                    pix[y][x] = base + int(31 * (1 - d))

    if scene == 'road':
        for y in range(64, h):
            half = (y - 64) * 1.6
            for x in range(w):
                if abs(x - 128) < half:
                    pix[y][x] = 129 + int(30 * (y - 64) / 64)
        disc(90, 112, 18, 193, 8)
        disc(166, 112, 18, 193, 8)
        for _ in range(300):
            x, y = rng.range(0, w), rng.range(0, h - 6)
            for k in range(5):
                pix[y + k][x] = 160
        rect(122, 20, 134, 64, 0.6)
    elif scene == 'lobby':
        for y in range(h):
            for x in range(w):
                dx, dy = abs(x - 128), abs(y - 60)
                if dx < 22 and dy < 30:
                    pix[y][x] = 2
                elif dy * 128 < dx * 70:
                    pix[y][x] = 129 + int(50 * dx / 128)
        rect(118, 34, 138, 88, 0.0)
        disc(128, 30, 6, 193)
    elif scene == 'hydro':
        for y in range(h):
            for x in range(w):
                if (x % 16 == 0 or y % 16 == 0) and y < 80:
                    pix[y][x] = 150
                if y >= 80:
                    pix[y][x] = 129 + int(20 + 10 * math.sin(x / 7 + y / 3))
        rect(40, 50, 90, 80, 0.4)
        rect(166, 50, 216, 80, 0.4)
    elif scene == 'archive':
        for k in range(7):
            rect(8 + k * 36, 10, 30 + k * 36, 120, 0.1)
            for s in range(5):
                rect(8 + k * 36, 20 + s * 20, 30 + k * 36, 22 + s * 20, 0.9)
        disc(128, 64, 20, 193, 14)
    elif scene == 'chapel':
        for y in range(10, 110):
            for x in range(88, 168):
                if math.hypot((x - 128) / 40, (y - 60) / 50) < 1 and y > 10:
                    seg = (int(math.atan2(y - 60, x - 128) * 3) + (int(math.hypot(x - 128, y - 60)) // 12)) % 3
                    pix[y][x] = (193 + 10 * seg) if seg else 140
        rect(0, 100, 256, 128, 0.1)
    else:
        disc(128, 64, 60, 129, 60)
        disc(128, 64, 40, 1, 40)
        disc(128, 64, 10, 193, 6)
    tiles = tiles_8bpp(pix, w, h)
    uniq, idx = dedupe_tiles(tiles, 64)
    return uniq, idx, palette_bytes(pal)


# ---------------------------------------------------------------------------
# 3D textures (all seven DS formats)

def textures():
    t = {}
    # A3I5 16x16: burning flare core.
    alpha, index = blank(16, 16), blank(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - 7.5, y - 7.5) / 8
            alpha[y][x] = max(0, min(7, int((1 - d) * 9)))
            index[y][x] = max(0, min(31, int((1 - d) * 31)))
    t['flame'] = (tex_a3i5(alpha, index), palette_bytes(ramp((180, 20, 10), (255, 250, 220), 32)), 16, 16)
    # A5I3 32x32: steam / smoke puff.
    alpha, index = blank(32, 32), blank(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x - 15.5, y - 15.5) / 16
            n = fbm(x / 7, y / 7, 3)
            a = max(0.0, (1 - d) * 1.3 * (0.5 + n))
            alpha[y][x] = max(0, min(31, int(a * 31)))
            index[y][x] = max(0, min(7, int(n * 8)))
    t['steam'] = (tex_a5i3(alpha, index), palette_bytes(ramp((70, 80, 84), (220, 228, 230), 8)), 32, 32)
    # A5I3 32x32: smooth glow for lamps and flare light.
    alpha, index = blank(32, 32), blank(32, 32)
    for y in range(32):
        for x in range(32):
            d = min(1.0, math.hypot(x - 15.5, y - 15.5) / 16)
            alpha[y][x] = max(0, min(31, int(31 * (1 - d) ** 1.6)))
            index[y][x] = max(0, min(7, int(8 * (1 - d) ** 2.2)))
    t['glow'] = (tex_a5i3(alpha, index), palette_bytes(ramp((120, 120, 120), (255, 255, 255), 8)), 32, 32)
    # 4-colour 32x32: bathroom tiles with grout and mould.
    idx = blank(32, 32)
    for y in range(32):
        for x in range(32):
            v = 1
            if x % 8 == 0 or y % 8 == 0:
                v = 0
            elif fbm(x / 6, y / 6, 21, 2) > 0.62:
                v = 2
            if y > 26 and fbm(x / 3, y / 3, 22, 2) > 0.45:
                v = 3
            idx[y][x] = v
    t['tiles'] = (tex_pal4(idx), palette_bytes([(70, 76, 72), (196, 210, 204), (150, 160, 140), (30, 40, 30)]), 32, 32)
    # 16-colour 64x64: the creature's skin, pale and veined.
    idx = blank(64, 64)
    for y in range(64):
        for x in range(64):
            n = fbm(x / 9, y / 9, 31, 3, period=7)
            v = 2 + int(n * 11)
            vein = abs(math.sin(x / 5.0 + math.sin(y / 7.0) * 2.0)) < 0.08
            if vein:
                v = 14
            if (x * 13 + y * 7) % 97 == 0:
                v = 15
            idx[y][x] = min(15, v)
    pal = [(0, 0, 0), (20, 22, 20)] + ramp((80, 86, 78), (200, 200, 186), 12) + [(90, 20, 30), (230, 220, 200)]
    t['flesh'] = (tex_pal16(idx), palette_bytes(pal), 64, 64)
    # 256-colour 64x64: old wood (doors, pews, cabinets).
    idx = blank(64, 64)
    for y in range(64):
        for x in range(64):
            g = math.sin((x + 6 * math.sin(y / 11.0)) / 2.4) * 0.5 + 0.5
            n = fbm(x / 13, y / 4, 41, 2)
            v = int(40 + 150 * (0.6 * g + 0.4 * n))
            if y % 32 == 0:
                v = 2
            idx[y][x] = max(1, min(255, v))
    pal = ramp((10, 6, 4), (60, 34, 18), 64) + ramp((60, 34, 18), (140, 90, 50), 128) + ramp((140, 90, 50), (190, 140, 90), 64)
    t['wood'] = (tex_pal256(idx), palette_bytes(pal), 64, 64)
    # Direct colour 32x32: framed photograph, opaque.
    rgba = []
    for y in range(32):
        row = []
        for x in range(32):
            if x < 3 or y < 3 or x > 28 or y > 28:
                c = (70, 44, 20)
            else:
                v = 0.6 - 0.3 * (math.hypot(x - 16, y - 13) < 7) + 0.25 * (y > 21)
                if math.hypot(x - 16, y - 12) < 2.2:
                    v = 0.05
                c = mix((50, 36, 26), (210, 190, 150), max(0, min(1, v)))
            row.append((*c, 1))
        rgba.append(row)
    t['photo'] = (tex_direct(rgba), b'', 32, 32)
    # Direct colour 32x32: bare twigs over a rusty rail (alpha test).
    rgba = [[(0, 0, 0, 0)] * 32 for _ in range(32)]
    rng = Rng(77)
    def branch(x, y, ang, ln):
        for k in range(ln):
            px = int(x + math.cos(ang) * k) % 32
            py = int(y - math.sin(ang) * k)
            if 0 <= py < 32:
                rgba[py][px] = (96, 84, 72, 1)
        if ln > 5:
            ex, ey = x + math.cos(ang) * ln, y - math.sin(ang) * ln
            branch(ex, ey, ang + 0.5 + rng.uniform() * 0.3, ln * 3 // 5)
            branch(ex, ey, ang - 0.4 - rng.uniform() * 0.3, ln * 3 // 5)
    for k in range(4):
        branch(3 + k * 8, 31, 1.35 + rng.uniform() * 0.4, 11)
    for x in range(32):
        rgba[24][x] = (120, 70, 40, 1)
        rgba[25][x] = (80, 46, 28, 1)
    for y in range(24, 32):
        rgba[y][0] = rgba[y][16] = (80, 46, 28, 1)
    t['branch'] = (tex_direct(rgba), b'', 32, 32)
    # 4x4 compressed 64x64: plaster wall over a hospital-green wainscot.
    rgb = []
    for y in range(64):
        row = []
        for x in range(64):
            n = fbm(x / 9, y / 9, 71, 3, period=7)
            stain = fbm(x / 20, y / 14, 73, 2, period=3)
            if y >= 38:
                c = mix((70, 92, 74), (100, 120, 98), n)
                if y in (38, 39):
                    c = (44, 54, 44)
            else:
                c = mix((150, 146, 128), (186, 180, 160), n)
                if stain > 0.6:
                    c = mix(c, (100, 84, 60), (stain - 0.6) * 2)
            row.append(c)
        rgb.append(row)
    tex, pidx, pal = tex_4x4(rgb, 64, 64)
    t['wall'] = (tex + pidx, pal, 64, 64)
    # 4x4 compressed 64x64: stained linoleum checkerboard.
    rgb = []
    for y in range(64):
        row = []
        for x in range(64):
            chk = ((x // 16) + (y // 16)) % 2
            n = fbm(x / 8, y / 8, 81, 3, period=8)
            c = mix((120, 118, 108), (150, 148, 136), n) if chk else mix((50, 48, 46), (70, 66, 60), n)
            if fbm(x / 16, y / 16, 82, 2, period=4) > 0.66:
                c = mix(c, (60, 40, 30), 0.5)
            row.append(c)
        rgb.append(row)
    tex, pidx, pal = tex_4x4(rgb, 64, 64)
    t['floor'] = (tex + pidx, pal, 64, 64)
    return t


def toon_table():
    """Stained-glass bands for the chapel: the lit red channel picks one of
    four glass colours (dark violet, blue, crimson, gold)."""
    out = []
    for i in range(32):
        if i < 8:
            c = (20, 10, 40)
        elif i < 16:
            c = (40, 60, 170)
        elif i < 24:
            c = (170, 30, 50)
        else:
            c = (255, 210, 90)
        out.append(rgb15(*c))
    return u16s(out)


def icon():
    pal = [(0, 0, 0), (6, 8, 20), (30, 34, 56), (220, 216, 180), (12, 12, 14),
           (230, 190, 90), (70, 68, 64), (40, 38, 36)] + [(0, 0, 0)] * 8
    pix = blank(32, 32, 1)
    for y in range(32):
        for x in range(32):
            if y < 20:
                pix[y][x] = 1 if y < 10 else 2
            if math.hypot(x - 8, y - 6) < 3.5:
                pix[y][x] = 3
            if 12 <= x <= 28 and 9 <= y <= 20:
                pix[y][x] = 4
            if y >= 20:
                pix[y][x] = 4
            if 18 < x < 22 and 4 <= y < 9:
                pix[y][x] = 4
            dx = (x - 16) / 9
            if abs(dx) <= 1 and 23 <= y <= 29:
                pix[y][x] = 6 if (x // 3 + y // 2) % 2 else 7
    pix[14][24] = pix[14][25] = pix[15][24] = pix[15][25] = 5
    return pix, pal
