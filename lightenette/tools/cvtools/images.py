# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Every bitmap, tile set, sprite and texture in Corvane, drawn in code."""

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
# Sprites (4bpp sheet + 8bpp meteor)

SPRITE_PALETTE = [
    (0, 0, 0), (255, 255, 255), (255, 120, 200), (255, 40, 120),
    (120, 200, 255), (40, 120, 255), (255, 220, 80), (255, 140, 30),
    (200, 50, 20), (90, 255, 230), (60, 60, 70), (140, 140, 160),
    (255, 80, 80), (120, 255, 120), (40, 40, 40), (255, 255, 160)]


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


def sprite_sheet():
    """Returns (tile bytes, dict name -> (tile index, w, h)). Tile index is in
    32-byte units (1D mapping, 32-byte boundary)."""
    items = []
    items.append(('bullet', 8, 8, _circle(8, 1.6, 3.4, 1, 3)))
    items.append(('needle', 8, 8, _circle(8, 1.2, 2.6, 1, 5)))
    big = _circle(16, 3.5, 7.2, 15, 7)
    for y in range(16):
        for x in range(16):
            if big[y][x] == 7 and math.hypot(x - 7.5, y - 7.5) > 6.2:
                big[y][x] = 8
    items.append(('bigshot', 16, 16, big))
    shot = blank(8, 8)
    for y in range(8):
        for x in range(3, 5):
            shot[y][x] = 1 if 1 <= y <= 6 else 9
        if 2 <= y <= 5:
            shot[y][2] = shot[y][5] = 9
    items.append(('pshot', 8, 8, shot))
    mis = blank(8, 8)
    for y in range(8):
        for x in range(8):
            if abs(x - 3.5) < 1.5 and y < 6:
                mis[y][x] = 11 if y > 0 else 12
            if y >= 6 and abs(x - 3.5) < 1:
                mis[y][x] = 6
    items.append(('missile', 8, 8, mis))
    rng = Rng(77)
    for f in range(4):
        e = blank(16, 16)
        r = 2.5 + f * 1.6
        for y in range(16):
            for x in range(16):
                d = math.hypot(x - 7.5, y - 7.5) + (rng.uniform() - 0.5) * 2.0
                if d < r * 0.45:
                    e[y][x] = 15 if f < 2 else 6
                elif d < r * 0.8:
                    e[y][x] = 6 if f < 3 else 7
                elif d < r:
                    e[y][x] = 7 if f < 3 else 10
        items.append(('explode%d' % f, 16, 16, e))
    items.append(('spark', 8, 8, _circle(8, 0.8, 2.0, 1, 6)))
    items.append(('blip_enemy', 8, 8, _circle(8, 1.0, 2.5, 12, 3)))
    items.append(('blip_player', 8, 8, _circle(8, 1.0, 2.5, 13, 9)))
    items.append(('blip_boss', 8, 8, _circle(8, 2.0, 3.6, 6, 8)))
    lock = blank(16, 16)
    for i in range(16):
        for (x, y) in ((i, 0), (i, 15), (0, i), (15, i)):
            if i < 5 or i > 10:
                lock[y][x] = 12
        if 6 <= i <= 9:
            lock[7][i] = lock[i][7] = 6
    items.append(('lock', 16, 16, lock))
    cross = blank(16, 16)
    for i in range(16):
        if i < 6 or i > 9:
            cross[7][i] = cross[8][i] = 13
            cross[i][7] = cross[i][8] = 13
    for y in range(16):
        for x in range(16):
            if abs(math.hypot(x - 7.5, y - 7.5) - 6.5) < 0.6:
                cross[y][x] = 9
    items.append(('cross', 16, 16, cross))
    # Radar mask: a filled disc. Only its shape matters (OBJ window).
    items.append(('radar_mask', 64, 64, _circle(64, 31.5, 31.9, 1, 1)))
    ship = blank(16, 16)
    for y in range(16):
        for x in range(16):
            if abs(x - 7.5) < (y / 2.2) and y < 14 and y > 1:
                ship[y][x] = 9 if abs(x - 7.5) < 1.5 else 5
    items.append(('life', 16, 16, ship))

    out = bytearray()
    index = {}
    for name, w, h, pix in items:
        index[name] = (len(out) // 32, w, h)
        # 1D mapping: tiles row-major within the sprite.
        out += tiles_4bpp(pix, w, h)
    return bytes(out), index


def meteor_8bpp():
    """32x32 8bpp meteor. Indices 1..63 encode shading so the extended OBJ
    palettes can recolour it (rock, magma, ice, gold)."""
    rng = Rng(1234)
    pix = blank(32, 32)
    for y in range(32):
        for x in range(32):
            ang = math.atan2(y - 15.5, x - 15.5)
            r = 13 + 2.0 * math.sin(ang * 3 + 0.5) + 1.2 * math.sin(ang * 7)
            d = math.hypot(x - 15.5, y - 15.5)
            if d < r:
                light = 0.55 + 0.45 * (-(x - 15.5) - (y - 15.5)) / 22
                n = fbm(x / 6, y / 6, 9)
                crater = 0.0
                for (cx, cy, cr) in ((10, 12, 3.5), (19, 20, 4.5), (20, 9, 2.5)):
                    dd = math.hypot(x - cx, y - cy)
                    if dd < cr:
                        crater = -0.35 * (1 - dd / cr)
                v = max(0.0, min(1.0, light * 0.7 + n * 0.4 + crater))
                pix[y][x] = 1 + int(v * 62)
    return tiles_8bpp(pix, 32, 32)


def meteor_palettes():
    sets = [((40, 30, 28), (200, 180, 160)),
            ((90, 10, 0), (255, 220, 80)),
            ((20, 40, 80), (220, 250, 255)),
            ((70, 50, 0), (255, 240, 140))]
    pals = []
    for lo, hi in sets:
        cols = [(0, 0, 0)] + ramp(lo, hi, 63) + [(0, 0, 0)] * 192
        pals.append(palette_bytes(cols))
    return pals


def cutin_bitmap():
    """64x64 16bpp bitmap OBJ: pilot cut-in (helmet silhouette, visor)."""
    out = []
    for y in range(64):
        for x in range(64):
            dx, dy = x - 32, y - 30
            a = 1
            bg = mix((10, 20, 60), (40, 90, 160), y / 64)
            c = bg
            if dx * dx / 22 ** 2 + dy * dy / 24 ** 2 < 1:
                c = mix((200, 205, 215), (90, 95, 110), (dx + dy + 40) / 80)
                if -14 < dx < 16 and -6 < dy < 6 - abs(dx) * 0.15:
                    c = mix((255, 160, 40), (120, 30, 10), (dy + 6) / 12)
                    if abs(dx - 6) < 3 and dy < -1:
                        c = (255, 240, 200)
            if y > 52 and abs(dx) < 26:
                c = (60, 70, 90)
            if x in (0, 63) or y in (0, 63):
                c = (255, 200, 60)
            out.append(rgb15(*c, a=a))
    return u16s(out)


# ---------------------------------------------------------------------------
# Backgrounds

def title_bitmap():
    """256x192 16bpp title illustration."""
    rng = Rng(42)
    w, h = 256, 192
    hz = 118
    lo = [[fbm(x / 10, y / 10, 3) for x in range(64)] for y in range(48)]
    clouds = upscale(lo, 64, 48, 4)
    ridge = [hz - 18 - 22 * fbm(x / 40, 0.5, 7, 4) for x in range(w)]
    ridge2 = [hz - 6 - 12 * fbm(x / 25, 3.5, 8, 4) for x in range(w)]
    stars = {(rng.range(0, w), rng.range(0, 90)) for _ in range(70)}
    px = []
    for y in range(h):
        for x in range(w):
            if y < hz:
                c = mix((8, 10, 40), (220, 90, 70), (y / hz) ** 1.6)
                d = math.hypot(x - 186, y - 96)
                if d < 26:
                    c = mix((255, 240, 190), (255, 170, 90), d / 26)
                elif d < 60:
                    c = mix(c, (255, 140, 80), (1 - (d - 26) / 34) * 0.5)
                cl = clouds[y][x]
                if cl > 0.58 and y < hz - 10:
                    c = mix(c, (255, 180, 150), min(1, (cl - 0.58) * 4) * 0.6)
                if (x, y) in stars and y < 60:
                    c = (255, 255, 255)
                if y > ridge[x]:
                    c = mix((40, 30, 60), (70, 40, 70), (y - ridge[x]) / 30)
                if y > ridge2[x]:
                    c = (24, 18, 40)
            else:
                t = (y - hz) / (h - hz)
                c = mix((80, 50, 90), (10, 20, 50), t ** 0.7)
                if abs(x - 186) < 30 - t * 10 and math.sin(y * 1.7 + x * 0.05) > 0.2:
                    c = mix(c, (255, 190, 120), 0.6 * (1 - t))
            px.append(rgb15(*c, a=1))
    # A hover fighter silhouette crossing the sky.
    def tri(ax, ay, bx, by, cx, cy, col):
        minx, maxx = int(min(ax, bx, cx)), int(max(ax, bx, cx)) + 1
        miny, maxy = int(min(ay, by, cy)), int(max(ay, by, cy)) + 1
        for yy in range(max(0, miny), min(h, maxy)):
            for xx in range(max(0, minx), min(w, maxx)):
                d1 = (xx - bx) * (ay - by) - (ax - bx) * (yy - by)
                d2 = (xx - cx) * (by - cy) - (bx - cx) * (yy - cy)
                d3 = (xx - ax) * (cy - ay) - (cx - ax) * (yy - ay)
                neg = d1 < 0 or d2 < 0 or d3 < 0
                pos = d1 > 0 or d2 > 0 or d3 > 0
                if not (neg and pos):
                    px[yy * w + xx] = rgb15(*col, a=1)
    tri(60, 70, 120, 84, 40, 92, (20, 16, 30))
    tri(60, 70, 96, 78, 74, 74, (90, 200, 255))
    tri(40, 92, 120, 84, 70, 96, (12, 10, 20))
    return u16s(px)


def logo_bitmap():
    """256x256 8bpp extended-affine bitmap: the CORVANE wordmark, built from
    the game font scaled 4x with bevelled edges. Palette 0 transparent."""
    w = h = 256
    pix = blank(w, h)
    text = "CORVANE"
    scale = 5
    cw = 6 * scale
    x0 = (w - len(text) * cw) // 2
    y0 = 128 - 4 * scale
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
    return bytes(v for row in pix for v in row)


def logo_palette():
    cols = [(0, 0, 0)] + ramp((255, 240, 160), (255, 90, 40), 16) + [(255, 255, 255), (20, 10, 30)]
    cols += [(0, 0, 0)] * (256 - len(cols))
    return palette_bytes(cols)


def nebula():
    """1024x384 8bpp content for the 1024x512 large bitmap of stage 3. Only
    rows 0-383 (banks A-C) are ever shown; bank D is the capture buffer."""
    lw, lh, f = 256, 96, 4
    lo = [[fbm(x / 24, y / 24, 31, 5, period=None) for x in range(lw)] for y in range(lh)]
    lo2 = [[fbm(x / 12 + 7, y / 12, 37, 3) for x in range(lw)] for y in range(lh)]
    a = upscale(lo, lw, lh, f)
    b = upscale(lo2, lw, lh, f)
    rng = Rng(99)
    out = bytearray()
    for y in range(lh * f):
        ra, rb = a[y], b[y]
        for x in range(lw * f):
            v = ra[x] * 0.75 + rb[x] * 0.35
            v = max(0.0, min(1.0, (v - 0.32) * 1.9))
            idx = 1 + int(v * 199)
            out.append(idx)
    for _ in range(1400):
        x, y = rng.range(0, lw * f), rng.range(0, lh * f)
        out[y * lw * f + x] = 200 + rng.range(0, 56)
    return bytes(out)


def nebula_palette():
    cols = [(0, 0, 8)]
    cols += ramp((0, 0, 10), (40, 10, 70), 60)
    cols += ramp((40, 10, 70), (170, 40, 140), 70)
    cols += ramp((170, 40, 140), (255, 190, 220), 70)
    cols += ramp((140, 160, 255), (255, 255, 255), 56)
    return palette_bytes(cols[:256])


def sky_affine():
    """256x256 8bpp tiles for the rotating sky (stage 2): radial bands with
    cloud streaks. Returns (tiles, map bytes 32x32, palette)."""
    w = h = 256
    pal_cols = ramp((20, 40, 120), (120, 180, 255), 32) + ramp((255, 255, 255), (170, 200, 255), 16)
    pal_cols = [(0, 0, 0)] + pal_cols + [(0, 0, 0)] * (256 - 1 - len(pal_cols))
    # Designed on the tile grid (an affine BG holds at most 256 tiles):
    # every tile is a solid band colour or one of a few cloud-streak tiles.
    pix = blank(w, h)
    for ty in range(32):
        for tx in range(32):
            d = math.hypot(tx - 15.5, ty - 15.5) / 22.6
            ang = math.atan2(ty - 15.5, tx - 15.5)
            band = 1 + min(31, int(d * 32))
            streak = math.sin(ang * 6 + d * 9) > 0.8 and d > 0.25
            kind = (tx * 3 + ty) % 4
            for y in range(8):
                for x in range(8):
                    v = band
                    if streak and ((x + y + kind * 2) % 8 < 3 or (x - y) % 8 == kind):
                        v = 33 + (kind * 3 + y) % 16
                    pix[ty * 8 + y][tx * 8 + x] = v
    tiles = tiles_8bpp(pix, w, h)
    uniq, idx = dedupe_tiles(tiles, 64)
    if len(idx) and max(idx) > 255:
        raise SystemExit('sky_affine: too many tiles (%d)' % (max(idx) + 1))
    return uniq, bytes(idx), palette_bytes(pal_cols)


def cloud_layer():
    """256x256 4bpp text BG of clouds for alpha blending over 3D."""
    w = h = 256
    lo = [[fbm(x / 6, y / 6, 51, 4, period=8) for x in range(64)] for y in range(64)]
    g = upscale(lo, 64, 64, 4)
    pix = blank(w, h)
    for y in range(h):
        for x in range(w):
            v = g[y][x]
            if v > 0.55:
                pix[y][x] = min(15, 1 + int((v - 0.55) * 40))
    tiles = tiles_4bpp(pix, w, h)
    uniq, idx = dedupe_tiles(tiles, 32)
    pal = [(0, 0, 0)] + ramp((170, 180, 210), (255, 255, 255), 15)
    return uniq, u16s(idx), palette_bytes(pal)


def sea_layer():
    """256x256 4bpp backdrop behind the 3D coast (stage 1): sky down to the
    horizon at line 64, open sea with wave crests below it."""
    w = h = 256
    horizon = 64
    pix = blank(w, h)
    for y in range(h):
        for x in range(w):
            if y < horizon:
                pix[y][x] = 1 + min(4, y * 5 // horizon)
                continue
            wave = math.sin(x * 0.19 + y * 0.9) + math.sin(x * 0.07 - y * 0.4)
            base = 6 + min(6, (y - horizon) // 22)
            pix[y][x] = 13 if wave > 1.55 else (14 if wave > 1.3 else base)
    tiles = tiles_4bpp(pix, w, h)
    uniq, idx = dedupe_tiles(tiles, 32)
    pal = ([(0, 0, 0)] + ramp((110, 160, 235), (225, 235, 245), 5)
           + ramp((120, 180, 220), (20, 60, 130), 7)
           + [(240, 250, 255), (170, 220, 250), (0, 0, 0)])
    return uniq, u16s(idx), palette_bytes(pal)


def radar_grid():
    """64x64 4bpp scrolling grid for the radar (tiles a 256x256 map)."""
    pix = blank(64, 64)
    for y in range(64):
        for x in range(64):
            if x % 16 == 0 or y % 16 == 0:
                pix[y][x] = 13
            elif (x + y) % 32 == 0:
                pix[y][x] = 6
            else:
                pix[y][x] = 3
    tiles = tiles_4bpp(pix, 64, 64)
    return tiles


BRIEF_SCENES = ['coast', 'fleet', 'station', 'fortress', 'reactor']


def briefing_art(scene):
    """256x128 8bpp tiled illustration and a 256-colour palette. Each scene
    is displayed through its own extended BG palette slot."""
    w, h = 256, 128
    rng = Rng(500 + BRIEF_SCENES.index(scene))
    themes = {
        'coast':    ((20, 50, 110), (255, 200, 140), (60, 70, 60)),
        'fleet':    ((50, 90, 170), (230, 240, 255), (90, 80, 100)),
        'station':  ((5, 5, 20), (150, 80, 200), (180, 190, 210)),
        'fortress': ((25, 15, 15), (120, 40, 30), (90, 90, 100)),
        'reactor':  ((10, 0, 20), (255, 90, 30), (200, 200, 220)),
    }
    sky0, sky1, solid = themes[scene]
    pal = [(0, 0, 0)] + ramp(sky0, sky1, 128) + ramp(mix(solid, (0, 0, 0), 0.7), solid, 64) \
        + ramp((255, 255, 255), (255, 220, 120), 32)
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
    def disc(cx, cy, r, base):
        for y in range(max(0, int(cy - r)), min(h, int(cy + r) + 1)):
            for x in range(max(0, int(cx - r)), min(w, int(cx + r) + 1)):
                d = math.hypot(x - cx, y - cy)
                if d <= r:
                    pix[y][x] = base + int(31 * (1 - d / r))
    if scene == 'coast':
        for x in range(w):
            top = int(80 + 14 * math.sin(x / 30) + 6 * math.sin(x / 9))
            for y in range(top, h):
                pix[y][x] = 129 + int(40 * (y - top) / (h - top))
        for k in range(5):
            bx = 20 + k * 48
            rect(bx, 60 - k % 2 * 8, bx + 18, 84)
            rect(bx + 6, 48 - k % 2 * 8, bx + 10, 60 - k % 2 * 8)
    elif scene == 'fleet':
        for k in range(4):
            cx, cy = 40 + k * 60, 40 + (k % 2) * 30
            for y in range(cy - 10, cy + 10):
                for x in range(cx - 30, cx + 30):
                    if ((x - cx) / 30) ** 2 + ((y - cy) / 10) ** 2 < 1:
                        pix[y][x] = 129 + int(50 * (y - cy + 10) / 20)
            rect(cx - 6, cy + 9, cx + 6, cy + 15)
    elif scene == 'station':
        for _ in range(120):
            pix[rng.range(0, h)][rng.range(0, w)] = 193 + rng.range(0, 31)
        disc(128, 64, 40, 193)
        rect(40, 60, 216, 68)
        rect(124, 10, 132, 118)
        disc(128, 64, 16, 129)
    elif scene == 'fortress':
        rect(0, 70, 256, 128, 0.1)
        for k in range(8):
            rect(k * 34, 30 + (k * 7) % 25, k * 34 + 22, 128)
        for k in range(6):
            disc(20 + k * 44, 100, 5, 193)
    else:
        disc(128, 70, 46, 193)
        disc(128, 70, 26, 129)
        for k in range(12):
            a = k * math.tau / 12
            for r in range(48, 64):
                x, y = int(128 + math.cos(a) * r), int(70 + math.sin(a) * r * 0.8)
                if 0 <= x < w and 0 <= y < h:
                    pix[y][x] = 200
    tiles = tiles_8bpp(pix, w, h)
    uniq, idx = dedupe_tiles(tiles, 64)
    return uniq, idx, palette_bytes(pal)


def hud_radar_ring():
    """Ring overlay for the radar, 128x128 4bpp tiles (map built in C)."""
    pix = blank(128, 128)
    for y in range(128):
        for x in range(128):
            d = math.hypot(x - 63.5, y - 63.5)
            if 62 < d < 64:
                pix[y][x] = 4
            elif 60.5 < d <= 62:
                pix[y][x] = 13
            elif abs(d - 32) < 0.5 or (abs(x - 63.5) < 0.6 or abs(y - 63.5) < 0.6) and d < 60:
                pix[y][x] = 6
    return tiles_4bpp(pix, 128, 128)


# ---------------------------------------------------------------------------
# 3D textures (all seven DS formats)

def textures():
    t = {}
    # A3I5 16x16: glowing bullet core.
    alpha, index = blank(16, 16), blank(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - 7.5, y - 7.5) / 8
            alpha[y][x] = max(0, min(7, int((1 - d) * 9)))
            index[y][x] = max(0, min(31, int((1 - d) * 31)))
    t['glow'] = (tex_a3i5(alpha, index), palette_bytes(ramp((0, 60, 255), (255, 255, 255), 32)), 16, 16)
    # A5I3 32x32: smoke puff.
    alpha, index = blank(32, 32), blank(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x - 15.5, y - 15.5) / 16
            n = fbm(x / 7, y / 7, 3)
            a = max(0.0, (1 - d) * 1.4 * (0.5 + n))
            alpha[y][x] = max(0, min(31, int(a * 31)))
            index[y][x] = max(0, min(7, int(n * 8)))
    t['smoke'] = (tex_a5i3(alpha, index), palette_bytes(ramp((60, 60, 70), (230, 230, 230), 8)), 32, 32)
    # A5I3 32x32: smooth flare for large flashes (32 alpha levels, hot core).
    alpha, index = blank(32, 32), blank(32, 32)
    for y in range(32):
        for x in range(32):
            d = min(1.0, math.hypot(x - 15.5, y - 15.5) / 16)
            alpha[y][x] = max(0, min(31, int(31 * (1 - d) ** 1.6)))
            index[y][x] = max(0, min(7, int(8 * (1 - d) ** 2.2)))
    t['flare'] = (tex_a5i3(alpha, index), palette_bytes(ramp((120, 120, 120), (255, 255, 255), 8)), 32, 32)
    # 4-colour 32x32: hazard stripes.
    idx = [[(1 if ((x + y) // 6) % 2 else 2) if (y % 16) < 12 else 3 for x in range(32)] for y in range(32)]
    t['stripes'] = (tex_pal4(idx), palette_bytes([(0, 0, 0), (255, 200, 40), (40, 40, 40), (120, 120, 130)]), 32, 32)
    # 16-colour 64x64: hull plates.
    idx = blank(64, 64)
    for y in range(64):
        for x in range(64):
            n = fbm(x / 10, y / 10, 13, 3)
            v = 3 + int(n * 10)
            if x % 16 == 0 or y % 16 == 0:
                v = 1
            if (x % 16 == 3 and y % 16 == 3):
                v = 15
            idx[y][x] = v
    t['hull'] = (tex_pal16(idx), palette_bytes([(0, 0, 0), (30, 30, 40)] + ramp((70, 80, 100), (170, 180, 200), 13) + [(255, 230, 120)]), 64, 64)
    # 256-colour 64x64: boss armour gradient with glyph-like etchings.
    idx = blank(64, 64)
    for y in range(64):
        for x in range(64):
            v = int(127 + 100 * math.sin(x / 9.0) * math.cos(y / 7.0))
            if (x // 8 + y // 8) % 5 == 0 and x % 8 in (2, 5):
                v = 250
            idx[y][x] = max(1, min(255, v))
    pal = ramp((40, 10, 30), (200, 60, 90), 128) + ramp((200, 60, 90), (255, 220, 200), 122) + [(255, 255, 160)] * 6
    t['armor'] = (tex_pal256(idx), palette_bytes(pal), 64, 64)
    # Direct colour 32x32: reactor panel, opaque.
    rgba = []
    for y in range(32):
        row = []
        for x in range(32):
            on = (x % 8 == 0) or (y % 8 == 0) or ((x + y) % 11 == 0 and x % 8 > 3)
            c = (90, 255, 220) if on else mix((30, 30, 60), (60, 60, 100), y / 32)
            row.append((*c, 1))
        rgba.append(row)
    t['panel'] = (tex_direct(rgba), b'', 32, 32)
    # Direct colour 32x32 mesh with holes (alpha test).
    rgba = []
    for y in range(32):
        row = []
        for x in range(32):
            wire = (x + y) % 8 < 2 or (x - y) % 8 < 2
            row.append((160, 170, 180, 1) if wire else (0, 0, 0, 0))
        rgba.append(row)
    t['mesh'] = (tex_direct(rgba), b'', 32, 32)
    # Direct colour 32x32: paper for the marionette cut-outs (cream, fibres,
    # a darker rim like a hand-cut edge).
    rgba = []
    for y in range(32):
        row = []
        for x in range(32):
            n = fbm(x / 5, y / 5, 91, 3)
            fib = fbm(x / 1.5, y / 9, 92, 2)
            c = mix((214, 196, 160), (240, 226, 190), n)
            c = mix(c, (190, 170, 130), max(0.0, fib - 0.55))
            if x in (0, 31) or y in (0, 31):
                c = mix(c, (120, 95, 70), 0.6)
            row.append((*c, 1))
        rgba.append(row)
    t['paper'] = (tex_direct(rgba), b'', 32, 32)
    # 4x4 compressed 128x128 terrain.
    rgb = []
    for y in range(128):
        row = []
        for x in range(128):
            n = fbm(x / 16, y / 16, 71, 4, period=8)
            m = fbm(x / 5, y / 5, 72, 2, period=26)
            if n < 0.42:
                c = mix((200, 180, 120), (230, 210, 150), m)
            elif n < 0.6:
                c = mix((60, 120, 40), (110, 160, 60), m)
            else:
                c = mix((110, 100, 90), (160, 150, 140), m)
            row.append(c)
        rgb.append(row)
    tex, pidx, pal = tex_4x4(rgb, 128, 128)
    t['terrain'] = (tex + pidx, pal, 128, 128)
    return t


def toon_table():
    """Three cel bands plus a rim highlight, indexed by the lit red channel."""
    out = []
    for i in range(32):
        if i < 9:
            c = (40, 48, 110)
        elif i < 17:
            c = (90, 120, 200)
        elif i < 27:
            c = (170, 200, 245)
        else:
            c = (255, 255, 255)
        out.append(rgb15(*c))
    return u16s(out)


def icon():
    pal = [(0, 0, 0), (10, 14, 40), (40, 80, 160), (90, 200, 255), (255, 200, 60),
           (255, 120, 40), (240, 240, 255), (30, 30, 60)] + [(0, 0, 0)] * 8
    pix = blank(32, 32, 1)
    for y in range(32):
        for x in range(32):
            if y > 22:
                pix[y][x] = 7
            d = abs(x - 15.5)
            if 4 < y < 26 and d < (y - 4) * 0.55:
                pix[y][x] = 2 if d > 2 else 3
            if 18 < y < 24 and d < 2:
                pix[y][x] = 4
            if y > 24 and d < 3 and y < 28:
                pix[y][x] = 5
    return pix, pal
