#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Judge an Elevane result block against expected.json.

    judge.py [--expected expected.json] [--profile NAME] [--list-profiles]
             [--json] [--demo NAME] [--values HEX x18 CRC] [LOGFILE ...]

Reads run logs (stdin without files) and grades the last result block found.
Log lines understood:

  RESULT w0 w1 ... w63   the 64 words of the block at 0x0237F000, as printed by the
                         melonDS headless runner (tools/melonds/mds.sh run) and the
                         DeSmuME harness (RESULT_WORDS=64 tools/emu/emu.sh run). A
                         leading ROM name ("<name> RESULT ...", the demos commands)
                         is accepted; --demo NAME keeps only that ROM's lines.

--values grades the numbers a console showed on its screen (the rehearsal prints
values[0..17] in hex and a CRC when it ends) instead of a log: `judge.py --values v0 ... v17 CRC`,
all 18 values and the CRC, hex only. A missing value, a character that is not a hex digit or a
CRC that does not match is exit code 2 (never a pass built from numbers nobody typed).

Words: w0 magic, w1 id, w2 frame, w3.. values[0..60]. The id must be the rehearsal's
("EVRH"). Each expected item is graded OK or NG. Items whose source is "unconfirmed"
are informational: they print INFO and never fail the run. An item can depend on a
flag bit ("requires": {"value": i, "bit": b}); when it is clear the item is n/a
(DeSmuME has no polygon RAM counters, so those items cannot be graded there).

Profiles: expected.json "known" lists, per emulator version and renderer, the items
(gating or not) expected to fail there ("ng") and the ones that cannot be graded there
("na"). With --profile NAME the run passes (exit 0) only when both sets equal the lists
exactly (an item silently turning n/a is a regression too); newly failing and newly
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
SHOWN_VALUES = 18   # values[0..17] are what the rehearsal prints (rehearsal.h)


def crc16(data):
    """The BIOS GetCRC16 (poly 0xA001 reflected, start 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


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


def check_levels(items):
    """Every item needs "level" 1-4 (docs/LEVELS.md); anything else is an input error (exit 2)."""
    for it in items:
        lv = it.get('level')
        if isinstance(lv, bool) or not isinstance(lv, int) or not 1 <= lv <= 4:
            raise Fatal('item %r has no valid "level" (1-4, see docs/LEVELS.md)' % it.get('id'))


ROLES = ('target', 'prerequisite', 'relocate')


def check_roles(exp):
    """target_level and every item's role (docs/LEVELS.md); a violation is an input error (exit 2).

    target: level == target_level. prerequisite: level <= target_level. relocate: temporary, the item is
    informational and moves to the game named in "to" ("probes" or a game folder).
    """
    tl = exp.get('target_level')
    if isinstance(tl, bool) or not isinstance(tl, int) or not 1 <= tl <= 4:
        raise Fatal('expected.json has no valid "target_level" (1-4, see docs/LEVELS.md)')
    for it in exp['items']:
        role, lv, name = it.get('role'), it['level'], it.get('id')
        if role not in ROLES:
            raise Fatal('item %r: "role" must be one of %s' % (name, ', '.join(ROLES)))
        if role == 'target' and lv != tl:
            raise Fatal('item %r: role target needs level %d (the target level), not %d' % (name, tl, lv))
        if role == 'prerequisite' and lv > tl:
            raise Fatal('item %r: role prerequisite needs level <= %d, not %d' % (name, tl, lv))
        if lv > tl and role != 'relocate':
            raise Fatal('item %r: level %d is above the target level %d, only role relocate is allowed' % (name, lv, tl))
        if role == 'relocate' and not it.get('to'):
            raise Fatal('item %r: role relocate needs "to"' % name)


def is_gating(item):
    """Gating: not unconfirmed and not waiting to be relocated."""
    return item['source'] != 'unconfirmed' and item['role'] != 'relocate'


def role_summary(rows):
    """rows: (role, ok). target a/b OK . prerequisite a/b OK . relocate n informational."""
    parts = []
    for role in ('target', 'prerequisite'):
        sel = [r for r in rows if r[0] == role]
        if sel:
            parts.append('%s %d/%d OK' % (role, sum(1 for r in sel if r[1]), len(sel)))
    n = sum(1 for r in rows if r[0] == 'relocate')
    if n:
        parts.append('relocate %d informational' % n)
    return ' \u00b7 '.join(parts)


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
    return ' \u00b7 '.join(parts)


def run(args):
    try:
        with open(find_expected(args.expected)) as f:
            exp = json.load(f)
    except (OSError, ValueError) as e:
        raise Fatal('cannot read expected.json: %s' % e)
    check_levels(exp.get('items', []))
    check_roles(exp)
    if args.list_profiles:
        for name, p in sorted(exp.get('known', {}).items()):
            print('%-34s %s' % (name, p['emulator']))
        return 0
    text = ''
    try:
        if args.values is not None:
            if len(args.values) != SHOWN_VALUES + 1:
                raise Fatal('--values wants %d hex numbers and the CRC (got %d)' % (SHOWN_VALUES, len(args.values)))
            bad = [v for v in args.values if not re.fullmatch(r'[0-9a-fA-F]{1,8}', v)]
            if bad:
                raise Fatal('--values: not hex digits: %s' % ' '.join(bad))
            vals_in = [int(v, 16) for v in args.values[:SHOWN_VALUES]]
            want = crc16(b''.join(v.to_bytes(4, 'little') for v in vals_in))
            if int(args.values[SHOWN_VALUES], 16) != want:
                raise Fatal('--values: the CRC %s does not match the numbers (%04x): a digit was misread'
                            % (args.values[SHOWN_VALUES], want))
            words = [MAGIC, exp["boot"]["item_id"], exp["boot"]["frames_min"]] + vals_in + [0] * (61 - len(vals_in))
            text = 'RESULT ' + ' '.join('%08x' % w for w in words) + '\n'
        elif args.logs:
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
    if len(words) < BLOCK_WORDS:
        raise Fatal('the RESULT line carries %d words, not %d (RESULT_WORDS=64 missing for emu.sh?)'
                    % (len(words), BLOCK_WORDS))
    words = words[:BLOCK_WORDS]
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
        gating = is_gating(item)
        results.append((item, g, gating))
    failing = sorted(i['id'] for (i, g, _) in results if g == 'NG')
    na = sorted(i['id'] for (i, g, _) in results if g == 'NA')
    gating_fail = sorted(i['id'] for (i, g, gt) in results if g == 'NG' and gt)

    rc = 0
    prof = None
    if args.profile:
        prof = exp.get('known', {}).get(args.profile)
        if prof is None:
            raise Fatal('unknown profile %r (see --list-profiles)' % args.profile)
        want = sorted(prof['ng'])
        want_na = sorted(prof.get('na', []))
        rc = 0 if failing == want and na == want_na else 1
    else:
        rc = 1 if gating_fail else 0

    if args.json:
        print(json.dumps({'frame': words[2], 'failing': failing, 'gating_failing': gating_fail,
                          'items': {i['id']: g for (i, g, _) in results}, 'exit': rc},
                         indent=1, sort_keys=True))
        return rc
    print('frame %d, block id %08x' % (words[2], words[1]))
    prereq_ng = sorted(i['id'] for (i, g, _) in results if g == 'NG' and i['role'] == 'prerequisite'
                       and i['id'] not in (prof.get('ng', []) if prof else []))
    if prereq_ng:
        print('PREREQ NG %s: the target-level verdicts of this game cannot be trusted' % ', '.join(prereq_ng))
    for item, g, gating in results:
        tag = {'OK': 'OK  ', 'NG': 'NG  ' if gating else 'INFO', 'NA': 'n/a '}[g]
        print('%s %-24s %s' % (tag, item['id'], item['title']))
    print('LEVELS ' + level_summary([(i['level'], g == 'OK', not gt) for (i, g, gt) in results if g != 'NA']))
    print('ROLES ' + role_summary([(i['role'], g == 'OK') for (i, g, _) in results if g != 'NA']))
    if prof is not None:
        want = sorted(prof['ng'])
        new_fail = [i for i in failing if i not in want]
        new_pass = [i for i in want if i not in failing]
        want_na = sorted(prof.get('na', []))
        print('profile %s: %s' % (args.profile, 'MATCH' if rc == 0 else 'MISMATCH'))
        for i in new_fail:
            print('  NEWLY FAILING %s' % i)
        for i in new_pass:
            print('  NEWLY PASSING %s' % i)
        for i in na:
            if i not in want_na:
                print('  NEWLY N/A %s (the ROM could not grade it)' % i)
        for i in want_na:
            if i not in na:
                print('  NO LONGER N/A %s' % i)
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
    ap.add_argument('--values', nargs='+', metavar='HEX', help='the 18 values and the CRC the console showed')
    try:
        return run(ap.parse_args(argv))
    except Fatal as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
