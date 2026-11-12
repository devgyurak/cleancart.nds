#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Judge a Lightenette result block against expected.json.

    judge.py [--expected expected.json] [--boot NAME] [--profile NAME]
             [--demo NAME] [--list-profiles] [LOGFILE ...]

Reads the run logs (stdin if no file) and uses the last result block found.
Log formats understood:

  RESULT w0 w1 ... w63      64 (or at least 8) hex words of the block at
                            0x0237F000: melonDS headless runner (mds.sh run),
                            DeSmuME harness (emu.sh run, RESULT_WORDS=64).
                            A leading ROM name (mds.sh demos, emu.sh demos:
                            "<name> RESULT ...") is accepted; --demo NAME then
                            keeps only the lines of that ROM.
  PEEK 0237f000 b0 b1 ...   at least 256 hex bytes of the block, little endian
                            (melonDS runner with PEEK=0237f000:256).
Malformed lines are skipped (with a warning on stderr).

Block words: w0 magic, w1 item id, w2 frame, w3.. values[0..60]. The item id
selects the boot ("rehearsal", "acc-stage", "qa" = the release ROM's auto QA run,
which uses the rehearsal's layout); --boot must agree with it.

Each expected item is graded OK / NG. Items whose source is "unconfirmed" are
informational: they print INFO and never fail the run. An item can name the
captures it depends on ("done": {"value": i, "mask": m}); it is NG when one of
those capture bits is clear (the capture did not finish), and an item that reads
pixels ("pixels": true) is NG when a value is above 0x7FFF (the ROM's "not
captured" marker 0xFFFF).

Items can also be n/a: "na_when": [i, v] makes an item n/a when values[i] == v (no polygon counters
in DeSmuME). Extra checks for the level 2 demo ROMs (boots "l2-<name>", ported from Corvane's judge):
mask ((v & mask) == expect), nonzero ((v & mask) != 0), same (all values equal), ne (two values
differ); "not_value" fails an item when a value equals it. Without --profile a gating item that is n/a is
not a pass: the run exits 1 and prints NOT GRADED (use a profile, which lists the n/a items, or --allow-na).

Profiles: expected.json "known" lists, per emulator version and renderer, the
gating items that are expected to fail there ("ng") and the items that are n/a there ("na"); only items of
the boot being graded are compared. With --profile NAME the run passes
(exit 0) only when the set of failing gating items equals that list exactly;
newly failing and newly passing items are reported either way.

Exit code: 0 pass (all gating items OK, or the profile matched), 1 a gating
item failed / the profile did not match / the rehearsal did not finish,
2 no usable result (no or bad block, unknown or mismatching --boot, values the
log did not carry, expected.json or a log unreadable).
Standard library only.
"""

import argparse
import json
import os
import sys

BLOCK_WORDS = 64
BLOCK_ADDR = 0x0237F000
PIXEL_MAX = 0x7FFF


class Fatal(Exception):
    """A usage or input problem: exit code 2."""


def check_levels(items):
    """Every item needs "level" 1-4 (docs/LEVELS.md); anything else is an input error (exit 2)."""
    if not isinstance(items, list) or not all(isinstance(i, dict) for i in items):
        raise Fatal('"items" must be a list of objects')
    for it in items:
        for key in ('id', 'source', 'check', 'values', 'boots'):
            if key not in it:
                raise Fatal('item %r has no "%s" in expected.json' % (it.get('id'), key))
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
    """rows: (role, ok, gating). target a/b OK . prerequisite a/b OK +n informational . relocate n informational."""
    parts = []
    for role in ('target', 'prerequisite'):
        gate = [r for r in rows if r[0] == role and r[2]]
        info = [r for r in rows if r[0] == role and not r[2]]
        if not gate and not info:
            continue
        s = role
        if gate:
            s += ' %d/%d OK' % (sum(1 for r in gate if r[1]), len(gate))
        if info:
            s += (' informational %d' if not gate else ' +%d informational') % len(info)
        parts.append(s)
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


def parse_words(text, demo=None, warn=None):
    """Return the last result block in the text as a list of ints, or None."""
    words = None
    prev = None
    roms = set()
    for n, line in enumerate(text.splitlines(), 1):
        t = line.split()
        if not t:
            continue
        name = None
        if len(t) > 1 and t[1] in ('RESULT', 'PEEK'):
            name, t = t[0], t[1:]
        if t[0] not in ('RESULT', 'PEEK'):
            continue
        if demo is not None and name != demo:
            continue
        if name:
            roms.add(name)
        try:
            if t[0] == 'RESULT':
                if len(t) < 9:
                    raise ValueError('fewer than 8 words')
                words = [int(x, 16) for x in t[1:1 + BLOCK_WORDS]]
            else:
                if int(t[1], 16) != BLOCK_ADDR:
                    continue
                data = bytes(int(x, 16) for x in t[2:])
                if len(data) < 32:
                    raise ValueError('fewer than 8 words')
                words = [int.from_bytes(data[4 * i:4 * i + 4], 'little') for i in range(len(data) // 4)]
            if words is not None and not any(words) and prev is not None and any(prev):
                # a batch run (mds.sh demos) stops some ROMs early: an unfinished block never replaces a finished one
                if warn:
                    warn('unfinished (all zero) block at line %d ignored' % n)
                words = prev
            prev = words
        except (ValueError, IndexError) as e:
            if warn:
                warn('skipping malformed %s line %d: %s' % (t[0], n, e))
    if demo is None and len(roms) > 1 and warn:
        warn('the log holds blocks of %d ROMs; only the last block is graded (use --demo NAME)' % len(roms))
    return words


def channels(value, pack):
    if pack == 'u8x3':
        return [value & 255, value >> 8 & 255, value >> 16 & 255]
    return [value]


def is_na(item, vals):
    """na_when [i, v]: the item is n/a when values[i] == v. Raises IndexError when the value is missing."""
    na = item.get('na_when')
    return bool(na) and vals[na[0]] == na[1]


def is_env_na(item, hardware):
    """--hardware: an item with an "env" (harness input) cannot be graded on a real console."""
    return hardware and bool(item.get('env'))


def grade(item, vals):
    """Return (ok, got_text, want_text). Raises IndexError when a value is missing."""
    got = [vals[i] for i in item['values']]
    tol = item.get('tolerance', 0)
    check = item['check']
    if 'not_value' in item and any(g == item['not_value'] for g in got):
        return False, '/'.join('%d' % g for g in got), 'not %d' % item['not_value']
    done = item.get('done')
    if done is not None:
        mask = done['mask']
        if vals[done['value']] & mask != mask:
            return False, 'capture %#x/%#x' % (vals[done['value']] & mask, mask), 'capture finished'
    if item.get('pixels') and any(g > PIXEL_MAX for g in got):
        return False, '/'.join('%04x' % g for g in got), 'captured pixels'
    if check == 'eq':
        want = item['expect']
        pack = item.get('pack')
        ok = all(abs(g - w) <= tol for g, w in zip(channels(got[0], pack), channels(want, pack)))
        shown = '/'.join('%d' % c for c in channels(got[0], pack))
        return ok, shown, item.get('display', '%d' % want)
    if check == 'seam_rows':
        # Rows 0 and 1 (clean mirrors) show no cut line, row 2 (plain mirror) may.
        r = channels(got[0], 'u8x3')
        return r[0] == 0 and r[1] == 0 and r[2] > 0, '/'.join('%d' % c for c in r), '0/0/>0'
    if check == 'gt':
        want = item['expect']
        return got[0] > want - tol, '%d' % got[0], '> %d' % want
    if check == 'all_gt':
        want = item['expect']
        return min(got) > want - tol, '/'.join('%d' % g for g in got), 'all > %d' % want
    if check == 'range':
        lo, hi = item['min'], item['max']
        return lo - tol <= got[0] <= hi + tol, '%d' % got[0], '%d..%d' % (lo, hi)
    if check == 'all_equal':
        shown = '/'.join('%04x' % g for g in got)
        if 'unlike' in item:
            # The lantern must really be drawn: three identical backdrop
            # pixels would otherwise count as "all equal".
            if got[0] == vals[item['unlike']]:
                return False, shown + ' (= backdrop)', 'all equal, not the backdrop'
            return max(got) - min(got) <= tol, shown, 'all equal, not the backdrop'
        return max(got) - min(got) <= tol, shown, 'all equal'
    if check == 'differ':
        return got[0] != got[1], '/'.join('%04x' % g for g in got), 'differ'
    if check == 'mask':
        return got[0] & item['mask'] == item['expect'], '%#x' % (got[0] & item['mask']), '%#x (mask %#x)' % (item['expect'], item['mask'])
    if check == 'nonzero':
        return got[0] & item['mask'] != 0, '%#x' % (got[0] & item['mask']), 'non-zero (mask %#x)' % item['mask']
    if check == 'same':
        return all(g == got[0] for g in got), '/'.join('%d' % g for g in got), 'all equal'
    if check == 'ne':
        return got[0] != got[1], '/'.join('%d' % g for g in got), 'differ'
    raise Fatal('expected.json: unknown check %r in item %s' % (check, item.get('id')))


def find_expected(arg):
    if arg:
        return arg
    here = os.path.dirname(os.path.abspath(__file__))
    # The release kit keeps judge.py and expected.json in one folder; in the
    # source tree judge.py is in tools/ and expected.json one level up.
    for p in (os.path.join(here, 'expected.json'), os.path.join(here, '..', 'expected.json')):
        if os.path.isfile(p):
            return p
    raise Fatal('expected.json not found next to judge.py or one folder up (use --expected)')


def read_text(path):
    try:
        with open(path, errors='replace') as f:
            return f.read()
    except OSError as e:
        raise Fatal('cannot read %s: %s' % (path, e))


def run(args):
    exp_path = find_expected(args.expected)
    try:
        exp = json.loads(read_text(exp_path))
        magic = exp['block']['magic']
        boots = exp['boots']
        items = exp['items']
    except (ValueError, KeyError, TypeError) as e:
        raise Fatal('%s is not a valid expected.json: %s' % (exp_path, e))

    check_levels(items)
    check_roles(exp)
    if args.list_profiles:
        for name, p in sorted(exp.get('known', {}).items()):
            print('%-28s %s  (%d known NG, %d n/a)' % (name, p.get('emulator', ''), len(p.get('ng', [])), len(p.get('na', []))))
        return 0

    if args.hardware and args.profile is not None:
        raise Fatal('--hardware does not compare with a profile (a console value is not an emulator profile)')
    if args.boot is not None and args.boot not in boots:
        raise Fatal('unknown --boot %r (known: %s)' % (args.boot, ', '.join(sorted(boots))))
    profile = None
    if args.profile is not None:
        known = exp.get('known', {})
        if args.profile not in known:
            raise Fatal('unknown --profile %r (known: %s)' % (args.profile, ', '.join(sorted(known)) or 'none'))
        profile = known[args.profile]

    text = ''
    if args.logs:
        for p in args.logs:
            text += read_text(p) + '\n'
    else:
        text = sys.stdin.read()

    words = parse_words(text, args.demo, lambda m: print('warning: ' + m, file=sys.stderr))
    if words is None:
        print('NO RESULT BLOCK in the log')
        return 2
    if words[0] != magic:
        print('BAD MAGIC %08x (the ROM did not reach its checkpoint)' % words[0])
        return 2

    by_id = {b['item_id']: name for name, b in boots.items()}
    in_progress = {v: k for k, v in exp['block'].get('act_ids', {}).items()}
    if words[1] in in_progress:
        print('NG: REHEARSAL INCOMPLETE - the block is from the %s loop (the run ended before the '
              'rehearsal finished: run at least %d frames, or that act hung)'
              % (in_progress[words[1]], boots.get('rehearsal', {}).get('frames', 0)))
        return 1
    block_boot = by_id.get(words[1])
    if block_boot is None:
        print('UNKNOWN ITEM ID %08x' % words[1])
        return 2
    boot = args.boot or block_boot
    if boot != block_boot:
        print('--boot %s does not match the result block (item id %08x belongs to the %s ROM)'
              % (boot, words[1], block_boot))
        return 2

    vals = words[3:]

    failed = []
    rows = []
    roles = []
    lines = []
    prereq_ng = []
    prereq_info = []
    na = []          # ids of n/a items
    ungraded = []    # gating items that were n/a
    graded_ids = set()
    missing = 0
    for item in items:
        if boot not in item['boots']:
            continue
        info = not is_gating(item)
        graded_ids.add(item['id'])
        try:
            if is_env_na(item, args.hardware):
                lines.append('%-7s %-22s %s [%s]' % ('N/A', item['id'], 'needs the harness (env)', item['source']))
                na.append(item['id'])
                continue
            if is_na(item, vals):
                lines.append('%-7s %-22s %s [%s]' % ('N/A', item['id'], 'not measurable (values[%d] == %d)' % tuple(item['na_when']),
                                                      item['source']))
                na.append(item['id'])
                if not info:
                    ungraded.append(item['id'])
                continue
            ok, got, want = grade(item, vals)
        except IndexError:
            lines.append('%-5s %-22s value not in the log (use 64 result words)' % ('MISS', item['id']))
            missing += 1
            continue
        tag = ('INFO' if ok else 'INFO-NG') if info else ('OK' if ok else 'NG')
        lines.append('%-7s %-22s got %-16s want %-12s [%s]' % (tag, item['id'], got, want, item['source']))
        rows.append((item['level'], ok, info))
        roles.append((item['role'], ok, not info))
        if not ok and item['role'] == 'prerequisite' and not info and item['id'] not in (profile or {}).get('ng', []):
            prereq_ng.append(item['id'])
        if not ok and item['role'] == 'prerequisite' and info:
            prereq_info.append(item['id'])
        if not ok and not info:
            failed.append(item['id'])
    if prereq_ng:
        print('PREREQ NG %s: the target-level verdicts of this game cannot be trusted' % ', '.join(sorted(prereq_ng)))
    if prereq_info:
        print('PREREQ INFO %s: informational prerequisite(s) differ (no effect on the verdict)' % ', '.join(sorted(prereq_info)))
    print('boot %s  item %08x  frame %d' % (boot, words[1], words[2]))
    for line in lines:
        print(line)
    if missing:
        return 2
    print('LEVELS ' + level_summary(rows))
    print('ROLES ' + role_summary(roles))

    if profile is not None:
        # Only the items of this boot take part (a profile covers every boot of the game).
        known_ng = set(profile.get('ng', [])) & graded_ids
        known_na = set(profile.get('na', [])) & graded_ids
        now_ng = set(failed)
        new_fail = sorted(now_ng - known_ng)
        new_pass = sorted(known_ng - now_ng)
        for i in sorted(set(na) - known_na):
            print('NEWLY N/A  %s (not in the profile)' % i)
        for i in sorted(known_na - set(na)):
            print('NO LONGER N/A  %s' % i)
        for i in new_fail:
            print('NEWLY FAILING  %s (not in the profile)' % i)
        for i in new_pass:
            print('NEWLY PASSING  %s (the profile expects NG; the emulator may have been fixed)' % i)
        same = not new_fail and not new_pass and set(na) == known_na
        print('PROFILE %s (%s): %s' % (args.profile, profile.get('emulator', ''),
                                       'MATCH' if same else 'MISMATCH'))
        return 0 if same else 1

    if ungraded and not args.allow_na:
        print('NOT GRADED %d gating item(s) were n/a without a profile (first: %s); use --profile or --allow-na'
              % (len(ungraded), ungraded[0]))
        return 1    # a PASS must not be built from items nobody graded
    print('RESULT: %s' % ('NG (%d gating item%s failed)' % (len(failed), 's' * (len(failed) != 1))
                          if failed else 'OK'))
    return 1 if failed else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--expected', help='expected.json (default: next to this script, then one folder up)')
    ap.add_argument('--boot', help='boot name; must agree with the item id in the block')
    ap.add_argument('--profile', help='emulator profile from expected.json "known"; exit 0 only if the NG set matches')
    ap.add_argument('--demo', help='ROM name prefix to keep in logs of several ROMs (mds.sh demos)')
    ap.add_argument('--allow-na', action='store_true', help='without --profile, do not fail on gating items that were n/a')
    ap.add_argument('--list-profiles', action='store_true')
    ap.add_argument('--hardware', action='store_true',
                    help='the log is from a real console: items with an "env" are n/a, no profile comparison')
    ap.add_argument('logs', nargs='*')
    args = ap.parse_args(argv)
    try:
        return run(args)
    except Fatal as e:
        print('ERROR: %s' % e)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
