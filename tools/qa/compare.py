#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Compare the checkpoints of two QA logs.

    compare.py A.log B.log [--info]

Checkpoints are matched by order and name; the state hash must be equal.
Lag frames are shown but never compared (they depend on the card and the
emulator). Exit status 0 identical, 1 different, 2 a log is invalid.
--info: the comparison is informational (emulator vs real hardware): print
the differences but exit 0.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import parse_log  # noqa: E402


def load(path):
    with open(path, encoding='ascii', errors='replace') as f:
        doc, errors = parse_log.parse(f.read())
    if errors:
        for e in errors:
            print('ERROR %s: %s' % (path, e), file=sys.stderr)
        sys.exit(2)
    return doc


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--info', action='store_true')
    args = ap.parse_args(argv)
    a, b = load(args.a), load(args.b)
    diffs = 0
    for k in ('game', 'build'):
        if a['header'].get(k) != b['header'].get(k):
            print('header %s differs: %s vs %s' % (k, a['header'].get(k), b['header'].get(k)))
    ca, cb = a['checkpoints'], b['checkpoints']
    for i in range(max(len(ca), len(cb))):
        x = ca[i] if i < len(ca) else None
        y = cb[i] if i < len(cb) else None
        if x is None or y is None:
            print('checkpoint %d only in %s' % (i, 'B' if x is None else 'A'))
            diffs += 1
        elif x['name'] != y['name'] or x['hash'] != y['hash']:
            print('checkpoint %d differs: %s %s (frame %d) vs %s %s (frame %d)' % (
                i, x['name'], x['hash'], x['frame'], y['name'], y['hash'], y['frame']))
            diffs += 1
    if a['result'] and b['result'] and a['result']['verdict'] != b['result']['verdict']:
        print('verdict differs: %s vs %s' % (a['result']['verdict'], b['result']['verdict']))
        diffs += 1
    print('%d checkpoints compared, %d differences' % (max(len(ca), len(cb)), diffs))
    if diffs and not args.info:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
