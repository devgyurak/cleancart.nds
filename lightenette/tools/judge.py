#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
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

Profiles: expected.json "known" lists, per emulator version and renderer, the
gating items that are expected to fail there. With --profile NAME the run passes
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


def parse_words(text, demo=None, warn=None):
    """Return the last result block in the text as a list of ints, or None."""
    words = None
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
        except (ValueError, IndexError) as e:
            if warn:
                warn('skipping malformed %s line %d: %s' % (t[0], n, e))
    return words


def channels(value, pack):
    if pack == 'u8x3':
        return [value & 255, value >> 8 & 255, value >> 16 & 255]
    return [value]


def grade(item, vals):
    """Return (ok, got_text, want_text). Raises IndexError when a value is missing."""
    got = [vals[i] for i in item['values']]
    tol = item.get('tolerance', 0)
    check = item['check']
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

    if args.list_profiles:
        for name, p in sorted(exp.get('known', {}).items()):
            print('%-28s %s  (%d known NG)' % (name, p.get('emulator', ''), len(p.get('ng', []))))
        return 0

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
    print('boot %s  item %08x  frame %d' % (boot, words[1], words[2]))

    failed = []
    missing = 0
    for item in items:
        if boot not in item['boots']:
            continue
        info = item['source'] == 'unconfirmed'
        try:
            ok, got, want = grade(item, vals)
        except IndexError:
            print('%-5s %-22s value not in the log (use 64 result words)' % ('MISS', item['id']))
            missing += 1
            continue
        tag = ('INFO' if ok else 'INFO-NG') if info else ('OK' if ok else 'NG')
        print('%-7s %-22s got %-16s want %-12s [%s]' % (tag, item['id'], got, want, item['source']))
        if not ok and not info:
            failed.append(item['id'])
    if missing:
        return 2

    if profile is not None:
        known_ng = set(profile.get('ng', []))
        now_ng = set(failed)
        new_fail = sorted(now_ng - known_ng)
        new_pass = sorted(known_ng - now_ng)
        for i in new_fail:
            print('NEWLY FAILING  %s (not in the profile)' % i)
        for i in new_pass:
            print('NEWLY PASSING  %s (the profile expects NG; the emulator may have been fixed)' % i)
        same = not new_fail and not new_pass
        print('PROFILE %s (%s): %s' % (args.profile, profile.get('emulator', ''),
                                       'MATCH' if same else 'MISMATCH'))
        return 0 if same else 1

    print('RESULT: %s' % ('NG (%d gating item%s failed)' % (len(failed), 's' * (len(failed) != 1))
                          if failed else 'OK'))
    return 1 if failed else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--expected', help='expected.json (default: next to this script, then one folder up)')
    ap.add_argument('--boot', help='boot name; must agree with the item id in the block')
    ap.add_argument('--profile', help='emulator profile from expected.json "known"; exit 0 only if the NG set matches')
    ap.add_argument('--demo', help='ROM name prefix to keep in logs of several ROMs (mds.sh demos)')
    ap.add_argument('--list-profiles', action='store_true')
    ap.add_argument('logs', nargs='*')
    args = ap.parse_args(argv)
    try:
        return run(args)
    except Fatal as e:
        print('ERROR: %s' % e)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
