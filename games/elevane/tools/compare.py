#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Compare 1x screenshots of one emulator renderer with the same emulator's software renderer.

    compare.py --ref PREFIX --test PREFIX ([--frames 380,381,382,383] --phase dual|showcase | --frames ... --max-diff N)
               [--tolerance 8] [--max-diff N] [--json] [--expected expected.json]

PREFIX is the screenshot name the runner wrote up to the frame number: the files
are PREFIX_<frame as %05d>.png (tools/melonds/mds.sh run ... "<keys>" 380,381,382,383
with out path PREFIX). Both screens are in one 256x384 picture. A pixel differs when
any channel is more than --tolerance levels off. Renderers rasterise a little
differently, so the verdict is a count of differing pixels per frame against
--max-diff, not an exact match; run consecutive frames (two in the dual screen
phase, four in the showcase) so one-frame flicker shows. --phase takes the limit, the tolerance and the default frames from expected.json
("screens"); --frames overrides the frames.
Exit 0 when every frame is
within the limit, 1 when a frame is over it, 2 for unreadable input.
PNG decoding is done here (zlib only), no third-party packages.
"""

import argparse
import json
import os
import struct
import sys
import zlib


def read_png(path):
    with open(path, 'rb') as f:
        d = f.read()
    if d[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('%s: not a PNG' % path)
    i, idat = 8, b''
    w = h = ct = bd = None
    while i < len(d):
        n, = struct.unpack('>I', d[i:i + 4])
        t, c = d[i + 4:i + 8], d[i + 8:i + 8 + n]
        i += 12 + n
        if t == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', c[:10])
        elif t == b'IDAT':
            idat += c
    if bd != 8 or ct not in (2, 6):
        raise ValueError('%s: only 8-bit RGB/RGBA PNGs are supported' % path)
    bpp = 3 if ct == 2 else 4
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, p = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(bytes(line))
        prev = line
    return w, h, bpp, rows


def diff_pixels(a, b, tol):
    w, h, bpp, ra = a
    if (w, h, bpp) != b[:3]:
        raise ValueError('picture sizes differ (%dx%d vs %dx%d): compare 1x output only' % (w, h, b[0], b[1]))
    rb = b[3]
    n = 0
    for y in range(h):
        la, lb = ra[y], rb[y]
        if la == lb:
            continue
        for x in range(w):
            o = x * bpp
            if abs(la[o] - lb[o]) > tol or abs(la[o + 1] - lb[o + 1]) > tol or abs(la[o + 2] - lb[o + 2]) > tol:
                n += 1
    return n


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--ref', required=True)
    ap.add_argument('--test', required=True)
    ap.add_argument('--frames')
    ap.add_argument('--phase', choices=('dual', 'showcase'))
    ap.add_argument('--expected')
    ap.add_argument('--tolerance', type=int)
    ap.add_argument('--max-diff', type=int)
    ap.add_argument('--json', action='store_true')
    args = ap.parse_args(argv)
    tol, limit = args.tolerance, args.max_diff
    if args.phase:
        here = os.path.dirname(os.path.abspath(__file__))
        path = args.expected or next((c for c in (os.path.join(here, 'expected.json'),
                                                  os.path.join(here, '..', 'expected.json')) if os.path.exists(c)), None)
        try:
            with open(path) as f:
                scr = json.load(f)['screens']
        except (OSError, TypeError, KeyError, ValueError) as e:
            print('ERROR cannot read the screens section of expected.json: %s' % e, file=sys.stderr)
            return 2
        tol = scr['tolerance'] if tol is None else tol
        limit = scr['phases'][args.phase]['max_diff'] if limit is None else limit
        if not args.frames:
            args.frames = ','.join(str(n) for n in scr['phases'][args.phase]['frame_list'])
    tol = 8 if tol is None else tol
    if limit is None or not args.frames:
        print('ERROR give --frames and --max-diff, or --phase', file=sys.stderr)
        return 2
    args.tolerance, args.max_diff = tol, limit
    out, worst = [], 0
    try:
        for fr in args.frames.split(','):
            n = int(fr)
            a = read_png('%s_%05d.png' % (args.ref, n))
            b = read_png('%s_%05d.png' % (args.test, n))
            d = diff_pixels(a, b, args.tolerance)
            out.append({'frame': n, 'differing': d, 'total': a[0] * a[1]})
            worst = max(worst, d)
    except (OSError, ValueError, zlib.error) as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2
    ok = worst <= args.max_diff
    if args.json:
        print(json.dumps({'frames': out, 'worst': worst, 'max_diff': args.max_diff, 'ok': ok}, indent=1))
    else:
        for r in out:
            print('frame %d: %d of %d pixels differ (> %d levels)' % (r['frame'], r['differing'], r['total'], args.tolerance))
        print('%s (worst %d, limit %d)' % ('OK' if ok else 'OVER THE LIMIT', worst, args.max_diff))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
