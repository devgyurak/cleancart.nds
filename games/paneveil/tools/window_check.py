#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Check every Paneveil window order and print its par.

    window_check.py [--chapter N] [--json] [--pack OUT.bin] [FILE ...]

With no FILE the chapter files tools/orders/ch*.txt are read in name order.
A window passes when its order parses, its name is unique, it asks for at least one
change (par > 0), the par agrees with a breadth-first search over the settings
it changes (one setting at a time: the settings do not interact), and the chapter holds six windows. Output is deterministic.
Exit 0 when every window passes, 1 otherwise. Standard library only.
"""

import argparse
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pvorder import model, pack, solve  # noqa: E402


def load(files):
    out = []
    for f in files:
        with open(f) as fh:
            out += model.parse(fh.read(), os.path.basename(f))
    return out


def check(windows):
    """Return (rows, ok). Rows are plain dicts in window order."""
    rows, ok = [], True
    names = set()
    per = {}
    for w in windows:
        p = solve.par(w.target)
        row = {'id': w.id, 'name': w.name, 'chapter': w.chapter, 'par': p, 'problems': []}
        per[w.chapter] = per.get(w.chapter, 0) + 1
        if w.name in names:
            row['problems'].append('duplicate name')
        names.add(w.name)
        if p == 0:
            row['problems'].append('order asks for no change (par 0)')
        bp = sum(solve.bfs_par(w.target, [f]) for f, v in enumerate(w.target) if v)
        if bp != p:
            row['problems'].append('par %d differs from the search (%s)' % (p, bp))
        ok = ok and not row['problems']
        rows.append(row)
    for ch, n in per.items():
        if n != model.PER_CHAPTER:
            rows.append({'id': 'ch%d' % ch, 'name': '', 'chapter': ch, 'par': None,
                         'problems': ['chapter %d has %d windows, expected %d' % (ch, n, model.PER_CHAPTER)]})
            ok = False
    return rows, ok


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('files', nargs='*')
    ap.add_argument('--chapter', type=int)
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--pack', metavar='OUT')
    args = ap.parse_args(argv)
    files = args.files or sorted(glob.glob(os.path.join(here, 'orders', 'ch*.txt')))
    try:
        windows = load(files)
    except model.OrderError as e:
        print('window_check: %s' % e, file=sys.stderr)
        return 1
    if args.chapter:
        windows = [w for w in windows if w.chapter == args.chapter]
    rows, ok = check(windows)
    if args.pack:
        with open(args.pack, 'wb') as f:
            f.write(pack.pack(windows))
    if args.json:
        json.dump(rows, sys.stdout, indent=1)
        print()
    else:
        for r in rows:
            print('%-6s %-16s par %s %s' % (r['id'], r['name'], r['par'], '; '.join(r['problems']) or 'ok'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
