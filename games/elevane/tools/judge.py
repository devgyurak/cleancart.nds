#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Judge an Elevane result block against expected.json.

    judge.py [--expected expected.json] [--profile NAME] [--list-profiles]
             [--json] [--demo NAME] [--partial] [--values HEX x18 CRC] [LOGFILE ...]

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

Words: w0 magic, w1 id, w2 frame, w3.. values[0..60]. The id names the ROM: the rehearsal's
("EVRH", boot.item_id) or one of the level 3 demos listed under "demos" in expected.json
(id = FNV-1a 32 of the demo name, e.g. "l3-acc-dual"; those blocks carry values[0..12], 16 words
are enough). An item without "demo" is graded from the rehearsal block, one with "demo" from that
demo's block. Blocks of several ROMs may be in one log (one `<rom> RESULT ...` line each, e.g. the
output of `tools/melonds/mds.sh demos`). A full run (no --partial, --demo or --values) must contain
the rehearsal block and the block of every demo that has an item, and must grade at least one
`role: target` item (not n/a), otherwise exit 2; --partial / --demo / --values grade only what is
present. A block whose magic is wrong (all zero: the ROM never finished) is exit 2, also for a demo.
A malformed expected.json (missing keys) is exit 2. Each expected item is graded OK or NG. Items whose source is "unconfirmed"
are informational: they print INFO and never fail the run. An item can depend on a
flag bit ("requires": {"value": i, "bit": b}); when it is clear the item is n/a
(DeSmuME has no polygon RAM counters, so those items cannot be graded there).

Profiles: expected.json "known" lists, per emulator version and renderer, the items
(gating or not) expected to fail there ("ng") and the ones that cannot be graded there
("na"). With --profile NAME the run passes (exit 0) only when the GATING failing set and the
gating n/a set equal the lists exactly (an item silently turning n/a is a regression too);
informational items that changed are only reported (INFO CHANGED). A failing gating
prerequisite prints PREREQ NG (exit 1); a failing informational one prints PREREQ INFO and
never changes the exit code. ROLES counts gating and informational items separately. Exit codes: 0 pass (all gating items OK, or the
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
DEMO_WORDS = 16      # a demo block: magic, id, frame, values[0..12]
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


def parse_lines(text, demo=None):
    """[(rom name or None, words)]: every RESULT line in log order."""
    out = []
    for line in text.splitlines():
        m = re.match(r'^(?:(\S+)\s+)?RESULT((?:\s+[0-9a-fA-F]{1,8})+)\s*$', line.strip())
        if not m:
            continue
        if demo is not None and m.group(1) != demo:
            continue
        words = [int(w, 16) for w in m.group(2).split()]
        if len(words) >= 8:
            out.append((m.group(1), words))
    return out


def grade(item, vals, hardware=False):
    """Return 'OK', 'NG' or 'NA' for one item. hardware: items with an "env" are n/a on a real console."""
    if hardware and item.get('env'):
        return 'NA'
    req = item.get('requires')
    if req and not (vals[req['value']] >> req['bit'] & 1):
        return 'NA'
    na = item.get('na_when')
    if na and vals[na[0]] == na[1]:
        return 'NA'
    vs = [vals[i] for i in item['values']]
    v = vs[0]
    kind = item['check']
    if 'not_value' in item and any(x == item['not_value'] for x in vs):
        return 'NG'
    if kind == 'mask':
        return 'OK' if (v & item['mask']) == item['expect'] else 'NG'
    if kind == 'nonzero':
        return 'OK' if (v & item['mask']) != 0 else 'NG'
    if kind == 'same':
        return 'OK' if all(x == v for x in vs) else 'NG'
    if kind == 'ne':
        return 'OK' if vs[0] != vs[1] else 'NG'
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


def check_schema(exp):
    """expected.json must have the keys this judge reads; a missing one is an input error (exit 2)."""
    if not isinstance(exp, dict):
        raise Fatal('expected.json must be a JSON object')
    for key, typ in (('items', list), ('boot', dict), ('known', dict)):
        if not isinstance(exp.get(key), typ):
            raise Fatal('expected.json: "%s" is missing or not a %s' % (key, typ.__name__))
    for key in ('item_id', 'frames_min'):
        if not isinstance(exp['boot'].get(key), int):
            raise Fatal('expected.json: boot.%s is missing or not a number' % key)
    if not isinstance(exp.get('demos', {}), dict):
        raise Fatal('expected.json: "demos" must be an object')
    need = {'mask': ('mask', 'expect'), 'nonzero': ('mask',), 'bit': ('bit',), 'eq': ('expect',),
            'ge': ('min',), 'le': ('max',), 'range': ('min', 'max'), 'same': (), 'ne': ()}
    for it in exp['items']:
        if not isinstance(it, dict):
            raise Fatal('"items" must be a list of objects')
        for key in ('id', 'title', 'values', 'check', 'source'):
            if key not in it:
                raise Fatal('item %r: key "%s" is missing' % (it.get('id'), key))
        if not isinstance(it['values'], list) or not it['values'] or not all(isinstance(x, int) for x in it['values']):
            raise Fatal('item %r: "values" must be a non-empty list of indices' % it['id'])
        if it['check'] not in need:
            raise Fatal('item %r: unknown check %r' % (it['id'], it['check']))
        for key in need[it['check']]:
            if key not in it:
                raise Fatal('item %r: check %s needs "%s"' % (it['id'], it['check'], key))
        if it.get('demo') is not None and it['demo'] not in exp.get('demos', {}):
            raise Fatal('item %r: demo %r is not listed under "demos"' % (it['id'], it['demo']))
    for name, p in exp['known'].items():
        if not isinstance(p, dict) or not isinstance(p.get('ng'), list):
            raise Fatal('profile %r: "ng" must be a list' % name)


def check_levels(items):
    """Every item needs "level" 1-4 (docs/LEVELS.md); anything else is an input error (exit 2)."""
    if not isinstance(items, list) or not all(isinstance(i, dict) for i in items):
        raise Fatal('"items" must be a list of objects')
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
    """rows: (role, ok, informational). target a/b OK . prerequisite a/b OK +n informational . relocate n informational."""
    parts = []
    for role in ('target', 'prerequisite', 'relocate'):
        gate = [r for r in rows if r[0] == role and not r[2]]
        info = [r for r in rows if r[0] == role and r[2]]
        if not gate and not info:
            continue
        sx = role
        if gate:
            sx += ' %d/%d OK' % (sum(1 for r in gate if r[1]), len(gate))
        if info:
            sx += (' %d informational' if not gate else ' +%d informational') % len(info)
        parts.append(sx)
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


def known_rom(rom, exp):
    """A ROM name in a log line that is ours (the rehearsal or one of the demos), or no name at all."""
    return rom is None or 'rehearsal' in rom or any(d in rom for d in exp.get('demos', {}))


def expected_id(rom, exp):
    """The block id the named ROM should leave (the rehearsal's or the demo's), or None when the name is not ours."""
    if rom is None:
        return None
    best = max((d for d in exp.get('demos', {}) if d in rom), key=len, default=None)
    if best is not None:
        return exp['demos'][best]
    return exp['boot']['item_id'] if 'rehearsal' in rom else None


def run(args):
    try:
        with open(find_expected(args.expected)) as f:
            exp = json.load(f)
    except (OSError, ValueError) as e:
        raise Fatal('cannot read expected.json: %s' % e)
    check_schema(exp)
    check_levels(exp['items'])
    check_roles(exp)
    if args.list_profiles:
        for name, p in sorted(exp['known'].items()):
            print('%-34s %s' % (name, p.get('emulator', '')))
        return 0
    # --demo / --values grade only what they name; so does --partial.
    partial = bool(args.partial) or args.demo is not None or args.values is not None
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
    parsed = parse_lines(text, args.demo)
    if not parsed:
        raise Fatal('no RESULT line in the log')
    boot = exp['boot']
    ids = {boot['item_id']: None}          # block id -> demo name (None: the rehearsal)
    for name, bid in exp.get('demos', {}).items():
        ids[bid] = name
    present = {}                            # demo name (None: the rehearsal) -> values
    frames = {}
    for rom, words in parsed:
        if words[0] != MAGIC:
            # all zero (the ROM never finished) or garbage: never graded, never skipped
            want_id = expected_id(rom, exp)
            if want_id is not None and any(w[0] == MAGIC and w[1] == want_id for (_, w) in parsed):
                # a batch run (mds.sh demos) stops the rehearsal ROM early; the finished block came from its own run
                print('warning: unfinished block of %s ignored (a finished block of it is in the log)' % rom, file=sys.stderr)
                continue
            if known_rom(rom, exp):
                raise Fatal('%s: result block magic is %08x, not %08x (the ROM did not finish)'
                            % (rom or 'a RESULT line', words[0], MAGIC))
            print('warning: result block of %s without magic ignored' % rom, file=sys.stderr)
            continue
        bid = words[1]
        if bid not in ids:
            if len(parsed) == 1:
                raise Fatal('result block id %08x is not the rehearsal (%08x) nor one of the level 3 demos'
                            % (bid, boot['item_id']))
            print('warning: result block with unknown id %08x ignored' % bid, file=sys.stderr)
            continue
        name = ids[bid]
        label = 'the rehearsal' if name is None else name
        need = BLOCK_WORDS if name is None else DEMO_WORDS
        if len(words) < need:
            raise Fatal('%s: the RESULT line carries %d words, not %d (RESULT_WORDS=%d missing for emu.sh?)'
                        % (label, len(words), need, need))
        words = words[:BLOCK_WORDS]
        # --hardware: the block was saved to the SD card the moment the rehearsal finished, so its frame
        # counter is the finish frame, not the 4000-frame snapshot the emulator runs read.
        if name is None and words[2] < boot['frames_min'] and not getattr(args, 'hardware', False):
            raise Fatal('only %d frames ran, the rehearsal needs at least %d' % (words[2], boot['frames_min']))
        present[name] = words[3:] + [0] * (61 - len(words[3:]))
        frames[name] = words[2]
    if not present:
        raise Fatal('no result block of this game in the log')
    if not partial:
        want_roms = {None} | {i['demo'] for i in exp['items'] if i.get('demo')}
        missing = sorted(('rehearsal' if n is None else n) for n in want_roms if n not in present)
        if missing:
            raise Fatal('missing blocks in a full run: %s (use --partial or --demo to grade only what is present)'
                        % ', '.join(missing))
    words = [0, boot['item_id'] if None in present else next(b for b, n in ids.items() if n in present),
             frames.get(None, next(iter(frames.values())))]

    results = []
    for item in exp['items']:
        vals = present.get(item.get('demo'))
        if vals is None:
            continue      # the log has no block of this item's ROM
        g = grade(item, vals, getattr(args, 'hardware', False))
        gating = is_gating(item)
        results.append((item, g, gating))
    if not partial and not any(it['role'] == 'target' and g != 'NA' for (it, g, _) in results):
        raise Fatal('no target item was graded (all n/a): this run says nothing about the target level')
    seen = {i['id'] for (i, _, _) in results}
    failing = sorted(i['id'] for (i, g, _) in results if g == 'NG')
    na = sorted(i['id'] for (i, g, _) in results if g == 'NA')
    gating_fail = sorted(i['id'] for (i, g, gt) in results if g == 'NG' and gt)
    gating_na = sorted(i['id'] for (i, g, gt) in results if g == 'NA' and gt)
    gating_ids = {i['id'] for (i, _, gt) in results if gt}

    rc = 0
    prof = None
    want = want_na = []
    if args.profile and getattr(args, 'hardware', False):
        raise Fatal('--hardware does not compare with a profile (a console value is not an emulator profile)')
    if args.profile:
        prof = exp['known'].get(args.profile)
        if prof is None:
            raise Fatal('unknown profile %r (see --list-profiles)' % args.profile)
        # Only gating items decide the match; informational ones that changed are reported below.
        want = sorted(w for w in prof['ng'] if w in gating_ids)
        want_na = sorted(w for w in prof.get('na', []) if w in gating_ids)
        rc = 0 if gating_fail == want and gating_na == want_na else 1
    else:
        rc = 1 if gating_fail else 0

    ungraded = sorted(i['id'] for (i, gr, gt) in results
                      if gr == 'NA' and gt and not (getattr(args, 'hardware', False) and i.get('env')))
    if not args.profile and ungraded and not args.allow_na:
        rc = 1    # a PASS must not be built from items nobody graded
        print('NOT GRADED %d gating item(s) were n/a without a profile (first: %s); use --profile or --allow-na'
              % (len(ungraded), ungraded[0]), file=sys.stderr)
    if args.json:
        print(json.dumps({'frame': words[2], 'demos': sorted(n for n in present if n), 'failing': failing, 'gating_failing': gating_fail,
                          'items': {i['id']: g for (i, g, _) in results}, 'exit': rc},
                         indent=1, sort_keys=True))
        return rc
    print('frame %d, block id %08x%s' % (words[2], words[1], ''.join(', ' + n for n in sorted(n for n in present if n))))
    known_ng = prof['ng'] if prof else []
    prereq_ng = sorted(i['id'] for (i, g, gt) in results if g == 'NG' and gt and i['role'] == 'prerequisite'
                       and i['id'] not in known_ng)
    prereq_info = sorted(i['id'] for (i, g, gt) in results if g == 'NG' and not gt and i['role'] == 'prerequisite'
                         and i['id'] not in known_ng)
    if prereq_ng:
        print('PREREQ NG %s: the target-level verdicts of this game cannot be trusted' % ', '.join(prereq_ng))
    if prereq_info:
        print('PREREQ INFO %s: informational prerequisite(s) failed (unconfirmed, the exit code is unchanged)'
              % ', '.join(prereq_info))
    for item, g, gating in results:
        tag = {'OK': 'OK  ', 'NG': 'NG  ' if gating else 'INFO', 'NA': 'n/a '}[g]
        print('%s %-24s %s' % (tag, item['id'], item['title']))
    print('LEVELS ' + level_summary([(i['level'], g == 'OK', not gt) for (i, g, gt) in results if g != 'NA']))
    print('ROLES ' + role_summary([(i['role'], g == 'OK', not gt) for (i, g, gt) in results if g != 'NA']))
    if prof is not None:
        print('profile %s: %s' % (args.profile, 'MATCH' if rc == 0 else 'MISMATCH'))
        for i in gating_fail:
            if i not in want:
                print('  NEWLY FAILING %s' % i)
        for i in want:
            if i not in gating_fail:
                print('  NEWLY PASSING %s' % i)
        for i in gating_na:
            if i not in want_na:
                print('  NEWLY N/A %s (the ROM could not grade it)' % i)
        for i in want_na:
            if i not in gating_na:
                print('  NO LONGER N/A %s' % i)
        # informational items: reported, never part of the match
        info_ids = seen - gating_ids
        for i in failing:
            if i in info_ids and i not in prof['ng']:
                print('  INFO CHANGED %s now fails (informational, not part of the match)' % i)
        for i in sorted(w for w in prof['ng'] if w in info_ids):
            if i not in failing:
                print('  INFO CHANGED %s now passes (informational, not part of the match)' % i)
        info_na_want = {w for w in prof.get('na', []) if w in info_ids}
        for i in sorted(info_ids & (set(na) ^ info_na_want)):
            print('  INFO CHANGED %s n/a state differs (informational)' % i)
    else:
        print('%s (%d gating failing)' % ('PASS' if rc == 0 else 'FAIL', len(gating_fail)))
    return rc


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('logs', nargs='*')
    ap.add_argument('--expected')
    ap.add_argument('--profile')
    ap.add_argument('--list-profiles', action='store_true')
    ap.add_argument('--allow-na', action='store_true', help='without --profile, do not fail on gating items that were n/a')
    ap.add_argument('--hardware', action='store_true',
                    help='the log is from a real console: items with an "env" are n/a, no profile comparison')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--demo')
    ap.add_argument('--partial', action='store_true',
                    help='grade only the blocks present (a full run needs the rehearsal and every demo)')
    ap.add_argument('--values', nargs='+', metavar='HEX', help='the 18 values and the CRC the console showed')
    try:
        return run(ap.parse_args(argv))
    except Fatal as e:
        print('ERROR %s' % e, file=sys.stderr)
        return 2
    except (KeyError, TypeError, IndexError, AttributeError, ValueError) as e:
        print('ERROR malformed expected.json or log (%s: %s)' % (type(e).__name__, e), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
