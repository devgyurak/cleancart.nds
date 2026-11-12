#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Grade Hushwell's feature demos against expected.json.

    judge.py [--profile NAME] [--demo NAME] [--partial] [--json] LOG...
    judge.py --list-profiles

Every demo ROM writes a result block at 0x0237F000 at frame 120 (u32 'CVER',
u32 FNV-1a of the demo name, u32 frame, u32 values[13]). Feed this script the
RESULT lines of the emulator, one per demo:

    tools/melonds/mds.sh demos <folder with the demo ROMs> > demos.log
    RESULT_WORDS=16 tools/emu/emu.sh demos games/hushwell > demos.log   # DeSmuME
    python3 tools/judge.py --profile melonds-1.1-software demos.log

A line is `[<rom name>] RESULT <hex words>`; the demo is identified by the id in
the block, so the name prefix is optional. The auto QA run of hushwell.nds leaves a block with id QARN; its item is `qa-verdict`
(demo `qa`, optional: `--demo qa` grades a log of the main ROM's QA run).

Exit 0: every gating item passes (or, with --profile, the failing gating items and the n/a
items match the profile exactly; informational items that changed are only reported). Exit 1: a gating item fails or the profile differs.
Exit 2: no usable input (a demo is missing, too few words, a bad expected.json;
--partial or --demo grade only what is in the log).
Items with source `unconfirmed` are informational: they print INFO but never fail.
An item with `na_when` [i, v] is n/a when values[i] == v (no polygon counters in
DeSmuME; without --profile an n/a item is not a failure, so a real run on hardware should use a profile or look at the n/a list).
Checks: eq, range, mask, nonzero, same (all values equal), ne (two values differ); `not_value` makes an item fail when a value
equals it (0xFFFF = a capture that did not finish). The level of an item (docs/LEVELS.md) never changes a verdict.
"""

import argparse
import json
import os
import re
import sys

MIN_WORDS = 16


class Fatal(Exception):
    """A usage or input problem: exit code 2."""


def check_levels(items):
    """Every item needs "level" 1-4 (docs/LEVELS.md); anything else is an input error (exit 2)."""
    for it in items:
        lv = it.get('level')
        if isinstance(lv, bool) or not isinstance(lv, int) or not 1 <= lv <= 4:
            raise Fatal('item %r has no valid "level" (1-4, see docs/LEVELS.md)' % it.get('id'))


def level_summary(rows):
    """rows: (level, ok, informational). One line: L1 a/b OK . L2 ... (levels never change the verdict)."""
    parts = []
    for lv in (1, 2, 3, 4):
        gate = [r for r in rows if r[0] == lv and not r[2]]
        info = [r for r in rows if r[0] == lv and r[2]]
        if not gate and not info:
            continue
        s = 'L%d' % lv
        if gate:
            s += ' %d/%d OK' % (sum(1 for r in gate if r[1]), len(gate))
        if info:
            s += (' informational %d' if not gate else ' +%d informational') % len(info)
        parts.append(s)
    return ' · '.join(parts)


def parse_blocks(text):
    """{demo id: words}: every `RESULT` line (the last one of an id wins)."""
    blocks = {}
    zero = 0
    for line in text.splitlines():
        m = re.search(r'\bRESULT((?: [0-9a-fA-F]{8})+)\s*$', line)
        if not m:
            continue
        words = [int(w, 16) for w in m.group(1).split()]
        if len(words) >= 3:
            blocks[words[1]] = words
            zero += words[0] == 0
    if zero:
        print('warning: %d RESULT line(s) are all zero (the ROM did not reach its checkpoint, or it is not a demo)' % zero,
              file=sys.stderr)
    return blocks


def grade(item, vals):
    """'OK', 'NG' or 'NA'."""
    try:
        na = item.get('na_when')
        if na and vals[na[0]] == na[1]:
            return 'NA'
        c = item['check']
        v = [vals[i] for i in item['values']]
        if 'not_value' in item and any(x == item['not_value'] for x in v):
            return 'NG'    # e.g. 0xFFFF: a capture that did not finish
        if c == 'eq':
            return 'OK' if v[0] == item['expect'] else 'NG'
        if c == 'range':
            return 'OK' if item['min'] <= v[0] <= item['max'] else 'NG'
        if c == 'mask':
            return 'OK' if (v[0] & item['mask']) == item['expect'] else 'NG'
        if c == 'nonzero':
            return 'OK' if (v[0] & item['mask']) != 0 else 'NG'
        if c == 'same':
            return 'OK' if all(x == v[0] for x in v) else 'NG'
        if c == 'ne':
            return 'OK' if v[0] != v[1] else 'NG'
    except (KeyError, IndexError, TypeError) as e:
        raise Fatal('item %r is malformed in expected.json: %r' % (item.get('id'), e))
    raise Fatal('item %r: unknown check %r' % (item.get('id'), c))


def find_expected(arg):
    if arg:
        return arg
    here = os.path.dirname(os.path.abspath(__file__))
    for cand in (os.path.join(here, 'expected.json'), os.path.join(here, '..', 'expected.json')):
        if os.path.exists(cand):
            return cand
    raise Fatal('expected.json not found next to judge.py or one folder up')


def run(args):
    try:
        with open(find_expected(args.expected)) as f:
            exp = json.load(f)
        magic = exp['block']['magic']
        demos = exp['demos']
        items = exp['items']
    except (OSError, ValueError, KeyError) as e:
        raise Fatal('cannot read expected.json: %s' % e)
    check_levels(items)
    if args.list_profiles:
        for name, p in sorted(exp.get('known', {}).items()):
            print('%-30s %s (%d known NG, %d n/a)' % (name, p.get('emulator', ''), len(p['ng']), len(p.get('na', []))))
        return 0
    profile = None
    if args.profile:
        profile = exp.get('known', {}).get(args.profile)
        if profile is None:
            raise Fatal('unknown profile %r (see --list-profiles)' % args.profile)
    text = ''
    try:
        if args.logs:
            for p in args.logs:
                with open(p, errors='replace') as f:
                    text += f.read() + '\n'
        else:
            text = sys.stdin.read()
    except OSError as e:
        raise Fatal('cannot read log: %s' % e)
    blocks = parse_blocks(text)
    if not blocks:
        raise Fatal('no RESULT line in the log')

    by_id = {v: k for k, v in demos.items()}
    present = {}
    for bid, words in blocks.items():
        name = by_id.get(bid)
        if name is None:
            if words[0] != 0:
                print('warning: result block with unknown id %08x ignored' % bid, file=sys.stderr)
            continue
        if words[0] != magic:
            raise Fatal('%s: block magic %08x is not %08x (the ROM did not reach its checkpoint)' % (name, words[0], magic))
        if len(words) < MIN_WORDS:
            raise Fatal('%s: the RESULT line carries %d words, at least %d are needed (RESULT_WORDS=16 for emu.sh)'
                        % (name, len(words), MIN_WORDS))
        present[name] = words[3:]
    if args.demo:
        name = args.demo[len('hushwell-'):] if args.demo.startswith('hushwell-') else args.demo
        if name not in present:
            raise Fatal('demo %r is not in the log' % args.demo)
        present = {name: present[name]}
    elif not args.partial:
        missing = sorted(set(demos) - set(present) - set(exp.get('optional', [])))
        if missing:
            raise Fatal('%d demo(s) have no RESULT line (first: %s); use --partial to grade only what is in the log'
                        % (len(missing), missing[0]))

    results = []
    for item in items:
        vals = present.get(item['demo'])
        if vals is None:
            continue
        results.append((item, grade(item, vals), item['source'] != 'unconfirmed'))
    failing = sorted(i['id'] for (i, g, _) in results if g == 'NG')
    na = sorted(i['id'] for (i, g, _) in results if g == 'NA')
    gating_fail = sorted(i['id'] for (i, g, gt) in results if g == 'NG' and gt)

    gating_ids = {i['id'] for (i, _, gt) in results if gt}
    if profile is not None:
        want, want_na = sorted(profile['ng']), sorted(profile.get('na', []))
        if args.partial or args.demo:
            seen = {i['id'] for (i, _, _) in results}
            want = [w for w in want if w in seen]
            want_na = [w for w in want_na if w in seen]
        # Only gating items decide the verdict; informational ones are reported when they change.
        want_gate = [w for w in want if w in gating_ids]
        rc = 0 if gating_fail == want_gate and na == want_na else 1
    else:
        rc = 1 if gating_fail else 0

    if args.json:
        print(json.dumps({'demos': sorted(present), 'failing': failing, 'gating_failing': gating_fail, 'na': na,
                          'items': {i['id']: g for (i, g, _) in results}, 'exit': rc}, indent=1, sort_keys=True))
        return rc
    for item, g, gating in results:
        tag = {'OK': 'OK  ', 'NG': 'NG  ' if gating else 'INFO', 'NA': 'n/a '}[g]
        if not args.quiet or g != 'OK':
            print('%s L%d %-28s %s' % (tag, item['level'], item['id'], item['title']))
    print('LEVELS ' + level_summary([(i['level'], g == 'OK', not gt) for (i, g, gt) in results if g != 'NA']))
    if profile is not None:
        new_fail = [i for i in failing if i not in want]
        new_pass = [i for i in want if i not in failing]
        print('profile %s: %s' % (args.profile, 'MATCH' if rc == 0 else 'MISMATCH'))
        for i in new_fail:
            print('  NEWLY FAILING %s%s' % (i, '' if i in gating_ids else ' (informational, not part of the verdict)'))
        for i in new_pass:
            print('  NEWLY PASSING %s%s' % (i, '' if i in gating_ids else ' (informational, not part of the verdict)'))
        for i in na:
            if i not in want_na:
                print('  NEWLY N/A %s' % i)
        for i in want_na:
            if i not in na:
                print('  NO LONGER N/A %s' % i)
    else:
        print('%s (%d gating failing, %d demos)' % ('PASS' if rc == 0 else 'FAIL', len(gating_fail), len(present)))
    return rc


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('logs', nargs='*')
    ap.add_argument('--expected')
    ap.add_argument('--profile')
    ap.add_argument('--list-profiles', action='store_true')
    ap.add_argument('--demo', help='grade one demo (name with or without the hushwell- prefix)')
    ap.add_argument('--partial', action='store_true', help='grade only the demos that are in the log')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--quiet', action='store_true', help='print only items that are not OK')
    try:
        return run(ap.parse_args(argv))
    except Fatal as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
