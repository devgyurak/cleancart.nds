#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Compare a QA judge suite run with the stand-alone demo ROMs (the "same values" criterion).

    suite_compare.py STANDALONE.txt SD_DIR [--words N] [--ignore I ...]

STANDALONE.txt: `tools/melonds/mds.sh demos DIR` output (`<rom> RESULT w0 w1 ...` lines, one per ROM).
SD_DIR: the headless SD directory (or a copy of the card's cleancart/<kit>/) holding the QA run's
res-*.log files. For every res file the block words must equal the stand-alone ROM's block of the same
demo name; word 2 (the frame the block was taken at) and, with --ignore, any value index may differ.
Prints one line per demo, exit 1 when a demo differs or has no stand-alone counterpart.
Standard library only.
"""

import argparse
import glob
import os
import re
import sys


def read_standalone(path):
    out = {}
    with open(path) as f:
        for line in f:
            m = re.match(r'(\S+) RESULT((?: [0-9a-fA-F]{8})+)\s*$', line)
            if m:
                out[m.group(1)] = m.group(2).split()
    return out


def read_res(sd):
    out = {}
    for p in sorted(glob.glob(os.path.join(sd, '**', 'res-*.log'), recursive=True)):
        with open(p) as f:
            for line in f:
                m = re.match(r'(\S+) RESULT((?: [0-9a-fA-F]{8})+)\s*$', line)
                if m:
                    out[m.group(1)] = m.group(2).split()   # the newest file wins
    return out


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('standalone')
    ap.add_argument('sd')
    ap.add_argument('--words', type=int, default=16, help='block words compared (default 16: magic..values[12])')
    ap.add_argument('--ignore', type=int, nargs='*', default=[], help='block word indexes that may differ (word 2 always)')
    args = ap.parse_args(argv)
    solo = read_standalone(args.standalone)
    suite = read_res(args.sd)
    skip = set(args.ignore) | {2}
    bad = 0
    for name, words in sorted(suite.items()):
        if name.endswith('-qa'):
            continue
        ref = solo.get(name)
        if ref is None:
            print('NO-REF  %s (no stand-alone result named that)' % name)
            bad += 1
            continue
        diff = [i for i in range(min(args.words, len(words), len(ref))) if i not in skip and words[i] != ref[i]]
        if diff:
            bad += 1
            print('DIFF    %s: %s' % (name, ', '.join('w%d %s vs %s' % (i, words[i], ref[i]) for i in diff)))
        else:
            print('SAME    %s' % name)
    missing = sorted(set(solo) - set(suite))
    for name in missing:
        print('MISSING %s (stand-alone only)' % name)
    print('%d demos compared, %d differ' % (len(suite) - sum(1 for n in suite if n.endswith('-qa')), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
