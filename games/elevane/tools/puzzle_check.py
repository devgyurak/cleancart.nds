#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Check every Elevane puzzle: the maker's structure is deliverable, the order has
a solution, and the minimum block count ("par").

    puzzle_check.py [--chapter N] [--json] [--pack OUT.bin] [FILE ...]

With no FILE the chapter files tools/puzzles/ch*.txt are read in name order.
Output is deterministic (no clock, no randomness). Exit 0 when every puzzle passes,
1 when one fails. A puzzle whose search hit the node limit passes but is marked
"reference" (par is the maker's count, not proven minimal).
Standard library only.
"""

import argparse
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from evpuzzle import model, pack, solve  # noqa: E402


def load(files):
    out = []
    for f in files:
        with open(f) as fh:
            out += model.parse(fh.read(), os.path.basename(f))
    return out


def check(puzzles):
    """Return (rows, entries, ok). rows are plain dicts, in puzzle order."""
    rows, entries, ok = [], [], True
    for p in puzzles:
        parts = []
        for k, maker in enumerate(p.parts):
            order = model.Order.of(maker)
            row = {'id': p.id, 'part': k + 1, 'name': p.name, 'chapter': p.chapter,
                   'mode': p.mode, 'grid': '%dx%dx%d' % (maker.w, maker.d, maker.h),
                   'maker': len(maker.cells), 'par': None, 'exact': None, 'problems': []}
            row['problems'] += solve.validate(order, p.mode, maker.cells)
            if not maker.cells:
                row['problems'].append('empty structure')
            try:
                res = solve.solve(order, p.mode, maker)
                row['par'], row['exact'] = res.count, res.exact
                row['problems'] += ['solution: ' + e for e in solve.validate(order, p.mode, res.cells)]
                parts.append((order, res, maker))
            except model.PuzzleError as e:
                row['problems'].append(str(e))
            if row['par'] is not None and row['par'] > row['maker']:
                row['problems'].append('par %d above the maker count %d' % (row['par'], row['maker']))
            if row['exact'] is False and p.mode == 'stack':
                row['problems'].append('a stack puzzle must be solved exactly (closed form)')
            ok = ok and not row['problems']
            rows.append(row)
        if len(parts) == len(p.parts):
            entries.append((p, parts))
    return rows, entries, ok


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('files', nargs='*')
    ap.add_argument('--chapter', type=int)
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--pack', metavar='OUT')
    args = ap.parse_args(argv)
    here = os.path.dirname(os.path.abspath(__file__))
    files = args.files or sorted(glob.glob(os.path.join(here, 'puzzles', 'ch*.txt')))
    try:
        puzzles = load(files)
    except (OSError, model.PuzzleError) as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2
    if args.chapter is not None:
        puzzles = [p for p in puzzles if p.chapter == args.chapter]
    rows, entries, ok = check(puzzles)
    if args.json:
        print(json.dumps(rows, indent=1, sort_keys=True))
    else:
        for r in rows:
            tag = 'FAIL' if r['problems'] else ('ok' if r['exact'] else 'reference')
            print('%-6s p%d %-22s %-6s %s par %s (maker %d)%s' % (
                r['id'], r['part'], r['name'], r['grid'], tag, r['par'], r['maker'],
                '' if r['exact'] in (True, None) else ' [not proven minimal]'))
            for m in r['problems']:
                print('       ! %s' % m)
        print('%d puzzles, %d parts: %s' % (len(puzzles), len(rows), 'all pass' if ok else 'FAILED'))
    if args.pack and ok:
        with open(args.pack, 'wb') as f:
            f.write(pack.pack(entries))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
