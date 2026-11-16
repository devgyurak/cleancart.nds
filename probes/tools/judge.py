#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Record and compare what an emulator does in the research probes (level L4).

    judge.py [--profile NAME] [--partial] [--json] [--observed] LOG...
    judge.py --list-profiles

Every probe ROM writes a result block at 0x0237F000 at frame 120 (u32 'CVER',
u32 FNV-1a of the demo name, u32 frame, u32 values[13]). Feed this script the
RESULT lines of the emulator, one per ROM:

    tools/melonds/mds.sh demos probes/build/release/demos > probes.log
    RESULT_WORDS=16 tools/emu/emu.sh demos probes > probes.log          # DeSmuME
    python3 probes/tools/judge.py --profile melonds-1.1-software probes.log

The probes are not a pass/fail test. Their items are L4 (undocumented or unconfirmed
on hardware), every item has source `unconfirmed` and role `target`, and no item can
fail the run by its value.

Without --profile: print every item with the value(s) the emulator gave and a verdict
wording (informational): "reference" is the value or range in the item's check, an observation
recorded when the probe was written (what the emulators gave, or the value the behaviour was
expected to have), never a hardware value. SAME AS REFERENCE / DIFFERS FROM REFERENCE / n/a only
tell where an emulator stands against it, never which one is right. The per-emulator record that
`--profile` compares is `known.<profile>.observed`, not the reference. Exit 0.

With --profile: expected.json `known.<profile>.observed` records what that emulator gave
per item. Exit 0 only if every observed value equals the record; otherwise exit 1 and
the changed items are listed (a change is news: the emulator moved).

Exit 2: no usable input (a bad expected.json, a role or level rule broken, no RESULT
line, a probe missing without --partial). --observed prints the observed values as JSON
(the form to paste into expected.json `known`).

An item records `values` (indexes into the block's values[]); for a check with a `mask` the
recorded number is the value AND the mask. Checks: eq, range, mask, nonzero, same, ne;
`na_when` [i, v]: n/a when values[i] == v (an emulator without that counter).
"""

import argparse
import json
import os
import re
import sys

MIN_WORDS = 16
TARGET_LEVEL = 4
OTHER_GAMES = ('corvane', 'hushwell', 'lightenette', 'elevane')


class Fatal(Exception):
    """A usage or input problem: exit code 2."""


def check_exp(exp):
    """Schema and role rules of the probe collection (docs/LEVELS.md); a violation is exit code 2."""
    try:
        items = exp['items']
        exp['block']['magic']
        exp['demos']
    except (KeyError, TypeError) as e:
        raise Fatal('expected.json is missing %s' % e)
    if exp.get('target_level') != TARGET_LEVEL or isinstance(exp.get('target_level'), bool):
        raise Fatal('expected.json: target_level must be %d' % TARGET_LEVEL)
    if not isinstance(items, list) or not all(isinstance(i, dict) for i in items):
        raise Fatal('"items" must be a list of objects')
    seen = set()
    for it in items:
        name = it.get('id')
        if name in seen:
            raise Fatal('item %r appears twice' % name)
        seen.add(name)
        lv = it.get('level')
        if isinstance(lv, bool) or not isinstance(lv, int) or not 1 <= lv <= 4:
            raise Fatal('item %r has no valid "level" (1-4, see docs/LEVELS.md)' % name)
        if lv != TARGET_LEVEL:
            raise Fatal('item %r: every probe item is level %d, not %r' % (name, TARGET_LEVEL, lv))
        if it.get('role') == 'relocate':
            raise Fatal('item %r: the probes are the destination, role relocate is not allowed' % name)
        if it.get('role') != 'target':
            raise Fatal('item %r: role must be target' % name)
        if it.get('source') != 'unconfirmed':
            raise Fatal('item %r: source must be unconfirmed (no hardware value exists for an L4 probe)' % name)
        if it.get('demo') not in exp['demos']:
            raise Fatal('item %r: unknown demo %r' % (name, it.get('demo')))


def parse_blocks(text):
    """{demo id: words}: every `RESULT` line (the last one of an id wins)."""
    blocks = {}
    zero = 0
    for line in text.splitlines():
        m = re.search(r'\bRESULT((?: [0-9a-fA-F]{8})+)\s*$', line)
        if not m:
            continue
        other = [x for x in OTHER_GAMES if x in line[:m.start()].lower()]
        if other:    # the demos of two games share block ids: never grade another game's ROM
            print('warning: RESULT line of %s ignored (this is the probes judge)' % other[0], file=sys.stderr)
            continue
        words = [int(w, 16) for w in m.group(1).split()]
        if len(words) >= 3:
            blocks[words[1]] = words
            zero += words[0] == 0
    if zero:
        print('warning: %d RESULT line(s) are all zero (the ROM did not reach its checkpoint)' % zero, file=sys.stderr)
    return blocks


def observe(item, vals):
    """The number(s) an item records: values[i] for its indexes, ANDed with `mask` when it has one."""
    try:
        v = [vals[i] for i in item['values']]
    except (KeyError, IndexError, TypeError) as e:
        raise Fatal('item %r is malformed in expected.json: %r' % (item.get('id'), e))
    if 'mask' in item:
        v = [x & item['mask'] for x in v]
    return v


def verdict(item, vals):
    """'SAME', 'DIFFERS' or 'NA' against the reference value of the item (informational only)."""
    try:
        na = item.get('na_when')
        if na and vals[na[0]] == na[1]:
            return 'NA'
        c = item['check']
        v = [vals[i] for i in item['values']]
        if 'not_value' in item and any(x == item['not_value'] for x in v):
            return 'DIFFERS'
        if c == 'eq':
            ok = v[0] == item['expect']
        elif c == 'range':
            ok = item['min'] <= v[0] <= item['max']
        elif c == 'mask':
            ok = (v[0] & item['mask']) == item['expect']
        elif c == 'nonzero':
            ok = (v[0] & item['mask']) != 0
        elif c == 'same':
            ok = all(x == v[0] for x in v)
        elif c == 'ne':
            ok = v[0] != v[1]
        else:
            raise Fatal('item %r: unknown check %r' % (item.get('id'), c))
    except (KeyError, IndexError, TypeError) as e:
        raise Fatal('item %r is malformed in expected.json: %r' % (item.get('id'), e))
    return 'SAME' if ok else 'DIFFERS'


WORDING = {
    'SAME': 'same as reference',
    'DIFFERS': 'differs from reference',
    'NA': 'n/a (the emulator has no such counter)',
}


def find_expected(arg):
    if arg:
        return arg
    here = os.path.dirname(os.path.abspath(__file__))
    for cand in (os.path.join(here, 'expected.json'), os.path.join(here, '..', 'expected.json')):
        if os.path.exists(cand):
            return cand
    raise Fatal('expected.json not found next to judge.py or one folder up')


def load_blocks(exp, args):
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
    by_id = {v: k for k, v in exp['demos'].items()}
    present = {}
    for bid, words in blocks.items():
        name = by_id.get(bid)
        if name is None:
            if words[0] != 0:
                print('warning: result block with unknown id %08x ignored' % bid, file=sys.stderr)
            continue
        if words[0] != exp['block']['magic']:
            raise Fatal('%s: block magic %08x is not %08x (the ROM did not reach its checkpoint)'
                        % (name, words[0], exp['block']['magic']))
        if len(words) < MIN_WORDS:
            raise Fatal('%s: the RESULT line carries %d words, at least %d are needed (RESULT_WORDS=16 for emu.sh)'
                        % (name, len(words), MIN_WORDS))
        present[name] = words[3:]
    if not present:
        raise Fatal('no RESULT line of a probe in the log (unknown block ids only)')
    if not args.partial:
        missing = sorted(set(exp['demos']) - set(present))
        if missing:
            raise Fatal('%d probe(s) have no RESULT line (first: %s); use --partial to record only what is in the log'
                        % (len(missing), missing[0]))
    return present


def run(args):
    try:
        with open(find_expected(args.expected)) as f:
            exp = json.load(f)
    except (OSError, ValueError) as e:
        raise Fatal('cannot read expected.json: %s' % e)
    check_exp(exp)
    known = exp.get('known', {})
    if args.hardware and args.profile:
        raise Fatal('--hardware does not compare with a profile (a console value is not an emulator profile)')
    if args.list_profiles:
        for name, p in sorted(known.items()):
            print('%-30s %s (%d items recorded)' % (name, p.get('emulator', ''), len(p.get('observed', {}))))
        return 0
    profile = None
    if args.profile:
        profile = known.get(args.profile)
        if profile is None:
            raise Fatal('unknown profile %r (see --list-profiles)' % args.profile)
        if not isinstance(profile.get('observed'), dict):
            raise Fatal('profile %r has no "observed" record' % args.profile)
    present = load_blocks(exp, args)

    rows = []    # (item, observed, verdict)
    for item in exp['items']:
        vals = present.get(item['demo'])
        if vals is None:
            continue
        rows.append((item, observe(item, vals), verdict(item, vals)))
    observed = {i['id']: o for (i, o, _) in rows}

    changed = []
    if profile is not None:
        rec = profile['observed']
        for item, o, _ in rows:
            if item['id'] not in rec:
                changed.append((item['id'], None, o))
            elif rec[item['id']] != o:
                changed.append((item['id'], rec[item['id']], o))
        if not args.partial:
            for name in sorted(set(rec) - set(observed)):
                changed.append((name, rec[name], None))
    rc = 1 if changed else 0

    if args.observed:
        print(json.dumps(observed, indent=1, sort_keys=True))
        return rc
    if args.json:
        print(json.dumps({'probes': sorted(present), 'observed': observed,
                          'verdicts': {i['id']: v for (i, _, v) in rows},
                          'changed': [c[0] for c in changed], 'exit': rc}, indent=1, sort_keys=True))
        return rc
    for item, o, v in rows:
        print('INFO L%d %-26s %s  [%s]  %s' % (item['level'], item['id'], o, WORDING[v], item['title']))
    n = {k: sum(1 for r in rows if r[2] == k) for k in WORDING}
    print('LEVELS L4 informational %d' % len(rows))
    print('ROLES target %d informational (%d same as reference, %d differ, %d n/a)'
          % (len(rows), n['SAME'], n['DIFFERS'], n['NA']))
    if profile is not None:
        print('profile %s: %s' % (args.profile, 'MATCH' if rc == 0 else 'CHANGED'))
        for name, old, new in changed:
            print('  CHANGED %s: recorded %s, now %s' % (name, old, new))
    else:
        print('INFORMATIONAL (%d probes, nothing graded)' % len(present))
    return rc


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('logs', nargs='*')
    ap.add_argument('--expected')
    ap.add_argument('--profile')
    ap.add_argument('--list-profiles', action='store_true')
    ap.add_argument('--partial', action='store_true', help='use only the probes that are in the log')
    ap.add_argument('--observed', action='store_true', help='print the observed values as JSON (for expected.json known)')
    ap.add_argument('--hardware', action='store_true',
                    help='the log is from a real console: observations only, no profile comparison '
                         '(record them with --observed under known.hardware-dslite-r4)')
    ap.add_argument('--json', action='store_true')
    try:
        return run(ap.parse_args(argv))
    except Fatal as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
