# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Pixel helpers and DS graphics encoders (tiles, maps, textures)."""

import math
import struct


def rgb15(r, g, b, a=0):
    """8-bit RGB -> BGR555 (bit 15 = alpha/opaque flag where relevant)."""
    r, g, b = (max(0, min(255, int(v))) >> 3 for v in (r, g, b))
    return r | (g << 5) | (b << 10) | (0x8000 if a else 0)


def lerp(a, b, t):
    return a + (b - a) * t


def mix(c0, c1, t):
    t = max(0.0, min(1.0, t))
    return tuple(lerp(x, y, t) for x, y in zip(c0, c1))


class Rng:
    """Deterministic xorshift32 so assets never depend on Python's random."""

    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF or 1

    def next(self):
        s = self.s
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        self.s = s
        return s

    def uniform(self):
        return self.next() / 4294967296.0

    def range(self, lo, hi):
        return lo + self.next() % (hi - lo)


def value_noise(x, y, seed, period=None):
    """Smooth 2D value noise in [0,1)."""
    def h(ix, iy):
        if period:
            ix %= period
            iy %= period
        v = (ix * 374761393 + iy * 668265263 + seed * 2246822519) & 0xFFFFFFFF
        v = ((v ^ (v >> 13)) * 1274126177) & 0xFFFFFFFF
        return (v ^ (v >> 16)) / 4294967296.0
    x0, y0 = math.floor(x), math.floor(y)
    fx, fy = x - x0, y - y0
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = lerp(h(x0, y0), h(x0 + 1, y0), sx)
    b = lerp(h(x0, y0 + 1), h(x0 + 1, y0 + 1), sx)
    return lerp(a, b, sy)


def fbm(x, y, seed, octaves=4, period=None):
    total, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        p = period * (1 << o) if period else None
        total += value_noise(x, y, seed + o * 17, p) * amp
        norm += amp
        amp *= 0.5
        x *= 2
        y *= 2
    return total / norm


# ---------------------------------------------------------------------------
# Tiles and maps

def tiles_4bpp(pix, w, h):
    """pix: list of rows of palette indices (0-15). Row-major 8x8 tiles."""
    out = bytearray()
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            for y in range(8):
                for x in range(0, 8, 2):
                    lo = pix[ty + y][tx + x] & 15
                    hi = pix[ty + y][tx + x + 1] & 15
                    out.append(lo | (hi << 4))
    return bytes(out)


def tiles_8bpp(pix, w, h):
    out = bytearray()
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            for y in range(8):
                out.extend(pix[ty + y][tx + x] & 255 for x in range(8))
    return bytes(out)


def obj_tiles_4bpp(pix, w, h):
    """Sprite in 1D mapping: tiles of the sprite in row-major order."""
    return tiles_4bpp(pix, w, h)


def dedupe_tiles(tile_bytes, tile_size):
    """Remove duplicate tiles. Returns (unique_tiles, map_indices)."""
    seen = {}
    uniq = bytearray()
    idx = []
    for i in range(0, len(tile_bytes), tile_size):
        t = bytes(tile_bytes[i:i + tile_size])
        if t not in seen:
            seen[t] = len(seen)
            uniq.extend(t)
        idx.append(seen[t])
    return bytes(uniq), idx


def u16s(values):
    return struct.pack('<%dH' % len(values), *[v & 0xFFFF for v in values])


def palette_bytes(colors):
    return u16s([rgb15(*c) for c in colors])


def quantize(rgb_pix, palette):
    """Nearest palette index for each RGB pixel."""
    cache = {}
    out = []
    for row in rgb_pix:
        orow = []
        for c in row:
            key = tuple(int(v) >> 2 for v in c)
            if key not in cache:
                best, bd = 0, 1e18
                for i, p in enumerate(palette):
                    d = sum((a - b) ** 2 for a, b in zip(c, p))
                    if d < bd:
                        best, bd = i, d
                cache[key] = best
            orow.append(cache[key])
        out.append(orow)
    return out


def ramp(c0, c1, n):
    return [mix(c0, c1, i / max(1, n - 1)) for i in range(n)]


# ---------------------------------------------------------------------------
# 3D texture encoders

def tex_a3i5(alpha, index):
    """alpha 0..7, index 0..31."""
    out = bytearray()
    for arow, irow in zip(alpha, index):
        out.extend(((a & 7) << 5) | (i & 31) for a, i in zip(arow, irow))
    return bytes(out)


def tex_a5i3(alpha, index):
    out = bytearray()
    for arow, irow in zip(alpha, index):
        out.extend(((a & 31) << 3) | (i & 7) for a, i in zip(arow, irow))
    return bytes(out)


def tex_pal4(index):
    out = bytearray()
    for row in index:
        for x in range(0, len(row), 4):
            out.append(row[x] | (row[x + 1] << 2) | (row[x + 2] << 4) | (row[x + 3] << 6))
    return bytes(out)


def tex_pal16(index):
    out = bytearray()
    for row in index:
        for x in range(0, len(row), 2):
            out.append((row[x] & 15) | ((row[x + 1] & 15) << 4))
    return bytes(out)


def tex_pal256(index):
    return bytes(v & 255 for row in index for v in row)


def tex_direct(rgba):
    """rgba rows of (r,g,b,a01)."""
    return u16s([rgb15(r, g, b, a) for row in rgba for (r, g, b, a) in row])


def tex_4x4(rgb, w, h):
    """4x4 compressed texture. Every block uses mode 2 (four opaque colours:
    two stored, two interpolated) with its own palette pair. Returns
    (texels, palette_indices, palette_colors)."""
    texels = bytearray()
    pidx = bytearray()
    palette = []
    pal_lookup = {}
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            block = [rgb[by + y][bx + x] for y in range(4) for x in range(4)]
            lum = [0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2] for c in block]
            c0 = block[lum.index(min(lum))]
            c1 = block[lum.index(max(lum))]
            p0, p1 = rgb15(*c0), rgb15(*c1)
            key = (p0, p1)
            if key not in pal_lookup:
                pal_lookup[key] = len(palette)
                palette.extend([p0, p1])
            slot = pal_lookup[key]  # in colours, always even
            # Mode 2 colours: 0 = c0, 1 = c1, 2 = (5c0+3c1)/8, 3 = (3c0+5c1)/8
            cols = [c0, c1, mix(c0, c1, 3 / 8), mix(c0, c1, 5 / 8)]
            word = 0
            for i, c in enumerate(block):
                best = min(range(4), key=lambda k: sum((a - b) ** 2 for a, b in zip(c, cols[k])))
                word |= best << (2 * i)
            texels.extend(struct.pack('<I', word))
            # Palette offset is in units of 4 bytes (2 colours); mode bits 14-15.
            pidx.extend(struct.pack('<H', (slot // 2) | (2 << 14)))
    return bytes(texels), bytes(pidx), u16s(palette)


def bmp_4bpp(path, pix, pal):
    """Write a 4bpp Windows BMP (used for the ndstool banner icon)."""
    w, h = len(pix[0]), len(pix)
    row_bytes = ((w // 2) + 3) & ~3
    img = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(0, w, 2):
            row.append(((pix[y][x] & 15) << 4) | (pix[y][x + 1] & 15))
        row.extend(b'\0' * (row_bytes - len(row)))
        img.extend(row)
    pal_bytes = bytearray()
    for (r, g, b) in pal:
        pal_bytes.extend(bytes((int(b), int(g), int(r), 0)))
    off = 14 + 40 + len(pal_bytes)
    with open(path, 'wb') as f:
        f.write(b'BM' + struct.pack('<IHHI', off + len(img), 0, 0, off))
        f.write(struct.pack('<IiiHHIIiiII', 40, w, h, 1, 4, 0, len(img), 2835, 2835, 16, 16))
        f.write(pal_bytes)
        f.write(img)
