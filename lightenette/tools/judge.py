#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Judge a Lightenette result block against expected.json.

    judge.py [--expected expected.json] [--boot NAME] [LOGFILE ...]

Reads the run logs (stdin if no file) and uses the last result block found.
Two log formats are understood:

  RESULT w0 w1 ... w15      16 (or at least 8) hex words of the block at
                            0x0237F000: melonDS headless runner (mds.sh run),
                            DeSmuME harness (emu.sh run, RESULT_WORDS=16).
  PEEK 0237f000 b0 b1 ...   at least 64 hex bytes of the block, little endian
                            (melonDS runner with PEEK=0237f000:64).

Block words: w0 magic, w1 item id, w2 frame, w3.. values[0..12]. The item id
selects the boot ("rehearsal", "acc-stage") unless --boot is given.

Each expected item is graded OK / NG. Items whose source is "unconfirmed" are
informational: they print INFO and never fail the run.

Exit code: 0 all gating items OK, 1 at least one gating item NG,
2 no valid result block (or a value the log did not carry).
Standard library only.
"""

import argparse
import json
import os
import re
import sys

BLOCK_WORDS = 16


def parse_words(text):
    """Return the last result block in the text as a list of ints, or None."""
    words = None
    for line in text.splitlines():
        t = line.split()
        if not t:
            continue
        if t[0] == 'RESULT' and len(t) >= 9:
            try:
                words = [int(x, 16) for x in t[1:1 + BLOCK_WORDS]]
            except ValueError:
                pass
        elif t[0] == 'PEEK' and len(t) >= 2 and int(t[1], 16) == 0x0237F000:
            try:
                data = bytes(int(x, 16) for x in t[2:])
            except ValueError:
                continue
            n = len(data) // 4
            if n >= 8:
                words = [int.from_bytes(data[4 * i:4 * i + 4], 'little') for i in range(n)]
    return words


def grade(item, vals):
    """Return (ok, got_text, want_text). Raises IndexError when a value is missing."""
    got = [vals[i] for i in item['values']]
    tol = item.get('tolerance', 0)
    check = item['check']
    if check == 'eq':
        want = item['expect']
        shown = '%d' % got[0]
        if item.get('pack') == 'u8x3':
            shown = '%d/%d/%d' % (got[0] & 255, got[0] >> 8 & 255, got[0] >> 16 & 255)
        return abs(got[0] - want) <= tol, shown, item.get('display', '%d' % want)
    if check == 'gt':
        want = item['expect']
        return got[0] > want - tol, '%d' % got[0], '> %d' % want
    if check == 'range':
        lo, hi = item['min'], item['max']
        return lo - tol <= got[0] <= hi + tol, '%d' % got[0], '%d..%d' % (lo, hi)
    if check == 'all_equal':
        return max(got) - min(got) <= tol, '/'.join('%04x' % g for g in got), 'all equal'
    if check == 'differ':
        return got[0] != got[1], '/'.join('%04x' % g for g in got), 'differ'
    raise ValueError('unknown check %r' % check)


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--expected', default=os.path.join(here, '..', 'expected.json'))
    ap.add_argument('--boot', help='boot name; default: chosen by the item id in the block')
    ap.add_argument('logs', nargs='*')
    args = ap.parse_args(argv)

    with open(args.expected) as f:
        exp = json.load(f)
    text = ''
    if args.logs:
        for p in args.logs:
            with open(p, errors='replace') as f:
                text += f.read() + '\n'
    else:
        text = sys.stdin.read()

    words = parse_words(text)
    if words is None:
        print('NO RESULT BLOCK in the log')
        return 2
    if words[0] != exp['block']['magic']:
        print('BAD MAGIC %08x (the ROM did not reach its checkpoint)' % words[0])
        return 2
    boot = args.boot
    if boot is None:
        for name, b in sorted(exp['boots'].items()):
            if b['item_id'] == words[1]:
                boot = name
        if boot is None:
            print('UNKNOWN ITEM ID %08x' % words[1])
            return 2
    vals = words[3:]
    print('boot %s  item %08x  frame %d' % (boot, words[1], words[2]))

    failed = 0
    missing = 0
    for item in exp['items']:
        if boot not in item['boots']:
            continue
        info = item['source'] == 'unconfirmed'
        try:
            ok, got, want = grade(item, vals)
        except IndexError:
            print('%-5s %-22s value not in the log (use 16 result words)' % ('MISS', item['id']))
            missing += 1
            continue
        tag = ('INFO' if ok else 'INFO-NG') if info else ('OK' if ok else 'NG')
        print('%-7s %-22s got %-16s want %-12s [%s]' % (tag, item['id'], got, want, item['source']))
        if not ok and not info:
            failed += 1
    if missing:
        return 2
    print('RESULT: %s' % ('NG (%d gating item%s failed)' % (failed, 's' * (failed != 1)) if failed else 'OK'))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
