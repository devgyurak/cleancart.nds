#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Check the loom orders (tools/orders/ch*.txt) against the model in tools/hmorder.

    order_check.py [--chapter N] [--json] [--plan-hash] [--pack OUT] [FILE ...]

Every order must parse, fit the columns of its chapter, need no more levers than its slack, have a
valid program for every row, and its par (the formula in model.py) must equal a breadth-first search over
the row's cells. --plan-hash prints the plan hash the rehearsal's `plan-ticks` item expects.
Exit code 0 when every order passes. Standard library only."""

import argparse
import glob
import json
import os
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from hmorder import model, pack  # noqa: E402


def load(files):
    orders = []
    for f in sorted(files):
        with open(f) as fh:
            orders += model.parse(fh.read(), f)
    return orders


def bfs_row(order, r):
    """Fewest single steps from the start state to any state in which row r is finished."""
    colour, weave, dye_wanted = order.rows[r]
    active = model.ACTIVE[order.chapter]
    cols = [c for c in range(model.COLS) if c in active]
    start = tuple(0 for _ in cols)

    def done(s):
        v = dict(zip(cols, s))
        if v.get(model.COL_COLOUR) != colour or v.get(model.COL_WEAVE) != weave:
            return False
        lever = v.get(model.COL_LEVER, 0)
        if lever and not model.lever_needed(weave, order.budget, order.passes):
            return False   # a pulled lever the row does not need only wastes a use
        program = v.get(model.COL_PROGRAM, weave)
        if not model.valid(weave, program, lever, order.budget, order.passes):
            return False
        dye = v.get(model.COL_DYE, 0)
        return dye in model.DYE_VALID if dye_wanted else dye == 0

    seen = {start: 0}
    queue = deque([start])
    while queue:
        s = queue.popleft()
        if done(s):
            return seen[s]
        for i, c in enumerate(cols):
            for step in (1, -1):
                t = list(s)
                t[i] = (t[i] + step) % model.COUNTS[c]
                t = tuple(t)
                if t not in seen:
                    seen[t] = seen[s] + 1
                    queue.append(t)
    return None


def check(orders):
    rows, ok = [], True
    for o in orders:
        problems = []
        try:
            par = model.par(o)
            levers = sum(model.row_plan(o, r)[2] for r in range(model.ROWS))
            if levers > o.slack:
                problems.append('needs %d levers, slack is %d' % (levers, o.slack))
            if o.slack and model.COL_LEVER not in model.ACTIVE[o.chapter]:
                problems.append('slack in a chapter without the lever')
            for r in range(model.ROWS):
                b = bfs_row(o, r)
                if b != model.row_plan(o, r)[0]:
                    problems.append('row %d: formula %s, search %s' % (r + 1, model.row_plan(o, r)[0], b))
            if not 1 <= o.budget <= 15 or not 1 <= o.passes <= 255:
                problems.append('budget or passes out of range')
        except model.OrderError as e:
            par = None
            problems.append(str(e))
        rows.append({'id': o.id, 'name': o.name, 'par': par, 'problems': problems})
        ok = ok and not problems
    for ch in range(1, len(model.CHAPTERS) + 1):
        n = sum(1 for o in orders if o.chapter == ch)
        if n != model.PER_CHAPTER:
            rows.append({'id': 'ch%d' % ch, 'name': '', 'par': None, 'problems': ['%d orders, not %d' % (n, model.PER_CHAPTER)]})
            ok = False
    return rows, ok


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--chapter', type=int)
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--plan-hash', action='store_true')
    ap.add_argument('--pack', metavar='OUT')
    ap.add_argument('files', nargs='*')
    args = ap.parse_args(argv)
    files = args.files or sorted(glob.glob(os.path.join(here, 'orders', 'ch*.txt')))
    orders = load(files)
    rows, ok = check(orders)
    if args.chapter:
        rows = [r for r in rows if r['id'].startswith('%d-' % args.chapter)]
    if args.json:
        print(json.dumps(rows, indent=1))
    else:
        for r in rows:
            print('%-6s %-16s par %-3s %s' % (r['id'], r['name'], r['par'], '; '.join(r['problems']) or 'ok'))
    if args.plan_hash:
        print('plan hash 0x%08x' % model.plan_hash(orders))
    if args.pack:
        with open(args.pack, 'wb') as f:
            f.write(pack.pack(orders))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
