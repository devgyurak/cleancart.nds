#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Compare a QA judge suite run with the stand-alone demo ROMs (the "same values" criterion).

    suite_compare.py STANDALONE.txt SD_DIR [--words N] [--ignore I ...]

STANDALONE.txt: `tools/melonds/mds.sh demos DIR` output (`<rom> RESULT w0 w1 ...` lines, one per ROM).
SD_DIR: the headless SD directory (or a copy of the card's cleancart/<kit>/) holding the QA run's
res-*.log files. For every res file the block words must equal the stand-alone ROM's block of the same
demo name; word 2 (the frame the block was taken at) and, with --ignore, any value index may differ.
Prints one line per demo, exit 1 when a demo differs, has no stand-alone counterpart, was run by the suite (suite-*.log) but has no
res file, or nothing was compared. Stand-alone ROMs the suite does not run are listed as INFO only. Of several runs of a demo the newest (file name clock) is used.
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


def run_key(path):
    """(stamp, k) from res-<stamp>-<kit>-<demo>[-2..9].log: the run's clock, then the collision suffix as a number."""
    m = re.match(r'res-(\d{8}-\d{6})-.+?(?:-([2-9]))?\.log$', os.path.basename(path))
    return (m.group(1), int(m.group(2) or 1)) if m else ('', 1)


def read_res(sd):
    out = {}
    newest = {}
    for p in glob.glob(os.path.join(sd, '**', 'res-*.log'), recursive=True):
        with open(p) as f:
            for line in f:
                m = re.match(r'(\S+) RESULT((?: [0-9a-fA-F]{8})+)\s*$', line)
                if m and (m.group(1) not in newest or run_key(p) >= newest[m.group(1)]):
                    newest[m.group(1)] = run_key(p)   # the newest run wins (not the name order: -a-2 sorts before -a)
                    out[m.group(1)] = m.group(2).split()
    return out


def read_suite_names(sd):
    """Demo names the suite logged (`suite demo demo=<name> ...` in suite-*.log), newest log only."""
    logs = sorted(glob.glob(os.path.join(sd, '**', 'suite-*.log'), recursive=True), key=run_key_suite)
    names = []
    if logs:
        with open(logs[-1]) as f:
            for line in f:
                m = re.search(r' suite demo demo=(\S+) ', line + ' ')
                if m:
                    names.append(m.group(1))
    return names


def run_key_suite(path):
    m = re.match(r'suite-(\d{8}-\d{6})(?:-([2-9]))?\.log$', os.path.basename(path))
    return (m.group(1), int(m.group(2) or 1)) if m else ('', 1)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('standalone')
    ap.add_argument('sd')
    ap.add_argument('--words', type=int, default=64, help='block words compared (default 64: the whole block; the stand-alone run may carry fewer)')
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
    # Missing = a demo the suite ran (named in suite-*.log: saved, timed out or not saved) that has no res file.
    # Stand-alone ROMs the suite does not run at all (not judged demos, io-save-flash/fram, ...) only get an INFO line.
    ran = read_suite_names(args.sd)
    missing = sorted(d for d in ran if not any(k.endswith('-' + d) for k in suite))
    for name in missing:
        print('MISSING %s (the suite ran it, no res file)' % name)
    for name in sorted(set(solo) - set(suite)):
        if not any(name.endswith('-' + d) for d in ran):
            print('INFO    %s (not in the suite)' % name)
    compared = len(suite) - sum(1 for n in suite if n.endswith('-qa'))
    print('%d demos compared, %d differ, %d missing' % (compared, bad, len(missing)))
    if compared == 0:
        print('nothing to compare: no res files in %s' % args.sd)
        return 1
    return 1 if bad or missing else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
