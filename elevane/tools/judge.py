#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Judge an Elevane result block against expected.json.

    judge.py [--expected expected.json] [--profile NAME] [--list-profiles]
             [--json] [--demo NAME] [LOGFILE ...]

Reads run logs (stdin without files) and grades the last result block found.
Log lines understood:

  RESULT w0 w1 ... w63   the 64 words of the block at 0x0237F000, as printed by the
                         melonDS headless runner (tools/melonds/mds.sh run) and the
                         DeSmuME harness (RESULT_WORDS=64 tools/emu/emu.sh run). A
                         leading ROM name ("<name> RESULT ...", the demos commands)
                         is accepted; --demo NAME keeps only that ROM's lines.

Words: w0 magic, w1 id, w2 frame, w3.. values[0..60]. The id must be the rehearsal's
("EVRH"). Each expected item is graded OK or NG. Items whose source is "unconfirmed"
are informational: they print INFO and never fail the run. An item can depend on a
flag bit ("requires": {"value": i, "bit": b}); when it is clear the item is n/a
(DeSmuME has no polygon RAM counters, so those items cannot be graded there).

Profiles: expected.json "known" lists, per emulator version and renderer, the items
(gating or not) expected to fail there. With --profile NAME the run passes (exit 0)
only when the set of failing items equals that list exactly; newly failing and newly
passing items are reported either way. Exit codes: 0 pass (all gating items OK, or the
profile matched), 1 a gating item failed / the profile did not match, 2 no usable
result (no or bad block, unknown profile, expected.json or a log unreadable).
Standard library only.
"""

import argparse
import json
import os
import re
import sys

BLOCK_WORDS = 64
MAGIC = 0x52455643


class Fatal(Exception):
    """A usage or input problem: exit code 2."""


def parse_block(text, demo=None):
    """The last RESULT block in the text as a list of ints, or None."""
    found = None
    for line in text.splitlines():
        m = re.match(r'^(?:(\S+)\s+)?RESULT((?:\s+[0-9a-fA-F]{1,8})+)\s*$', line.strip())
        if not m:
            continue
        if demo is not None and m.group(1) != demo:
            continue
        words = [int(w, 16) for w in m.group(2).split()]
        if len(words) >= 8:
            found = words
    return found


def grade(item, vals):
    """Return 'OK', 'NG' or 'NA' for one item."""
    req = item.get('requires')
    if req and not (vals[req['value']] >> req['bit'] & 1):
        return 'NA'
    v = vals[item['values'][0]]
    kind = item['check']
    if kind == 'bit':
        return 'OK' if v >> item['bit'] & 1 else 'NG'
    if kind == 'eq':
        return 'OK' if v == item['expect'] else 'NG'
    if kind == 'ge':
        return 'OK' if v >= item['min'] else 'NG'
    if kind == 'le':
        return 'OK' if v <= item['max'] else 'NG'
    if kind == 'range':
        return 'OK' if item['min'] <= v <= item['max'] else 'NG'
    raise Fatal('item %s: unknown check %r' % (item['id'], kind))


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
    except (OSError, ValueError) as e:
        raise Fatal('cannot read expected.json: %s' % e)
    if args.list_profiles:
        for name, p in sorted(exp.get('known', {}).items()):
            print('%-34s %s' % (name, p['emulator']))
        return 0
    text = ''
    try:
        if args.logs:
            for path in args.logs:
                with open(path, errors='replace') as f:
                    text += f.read() + '\n'
        else:
            text = sys.stdin.read()
    except OSError as e:
        raise Fatal('cannot read log: %s' % e)
    words = parse_block(text, args.demo)
    if words is None:
        raise Fatal('no RESULT line in the log')
    words = (words + [0] * BLOCK_WORDS)[:BLOCK_WORDS]
    if words[0] != MAGIC:
        raise Fatal('result block magic is %08x, not %08x (the ROM did not finish)' % (words[0], MAGIC))
    boot = exp['boot']
    if words[1] != boot['item_id']:
        raise Fatal('result block id %08x is not the rehearsal (%08x)' % (words[1], boot['item_id']))
    if words[2] < boot['frames_min']:
        raise Fatal('only %d frames ran, the rehearsal needs at least %d' % (words[2], boot['frames_min']))
    vals = words[3:]

    results = []
    for item in exp['items']:
        g = grade(item, vals)
        gating = item['source'] != 'unconfirmed'
        results.append((item, g, gating))
    failing = sorted(i['id'] for (i, g, _) in results if g == 'NG')
    gating_fail = sorted(i['id'] for (i, g, gt) in results if g == 'NG' and gt)

    rc = 0
    prof = None
    if args.profile:
        prof = exp.get('known', {}).get(args.profile)
        if prof is None:
            raise Fatal('unknown profile %r (see --list-profiles)' % args.profile)
        want = sorted(prof['ng'])
        rc = 0 if failing == want else 1
    else:
        rc = 1 if gating_fail else 0

    if args.json:
        print(json.dumps({'frame': words[2], 'failing': failing, 'gating_failing': gating_fail,
                          'items': {i['id']: g for (i, g, _) in results}, 'exit': rc},
                         indent=1, sort_keys=True))
        return rc
    print('frame %d, block id %08x' % (words[2], words[1]))
    for item, g, gating in results:
        tag = {'OK': 'OK  ', 'NG': 'NG  ' if gating else 'INFO', 'NA': 'n/a '}[g]
        print('%s %-24s %s' % (tag, item['id'], item['title']))
    if prof is not None:
        want = sorted(prof['ng'])
        new_fail = [i for i in failing if i not in want]
        new_pass = [i for i in want if i not in failing]
        print('profile %s: %s' % (args.profile, 'MATCH' if rc == 0 else 'MISMATCH'))
        for i in new_fail:
            print('  NEWLY FAILING %s' % i)
        for i in new_pass:
            print('  NEWLY PASSING %s' % i)
    else:
        print('%s (%d gating failing)' % ('PASS' if rc == 0 else 'FAIL', len(gating_fail)))
    return rc


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('logs', nargs='*')
    ap.add_argument('--expected')
    ap.add_argument('--profile')
    ap.add_argument('--list-profiles', action='store_true')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--demo')
    try:
        return run(ap.parse_args(argv))
    except Fatal as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
