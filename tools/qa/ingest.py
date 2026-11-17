#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Copy QA logs, result logs and save mirrors from a real SD card and parse/judge them.

    ingest.py SD_MOUNT [--game NAME ...] [--no-judge]

Looks for <SD_MOUNT>/cleancart/<kit>/ (every kit unless --game is given; the kits are the
games and `probes`), copies qa-*.log, res-*.log, save-*.bin and shot-*.bmp (checkpoint screenshots, see
core/qashot.c) to
<repo>/games/<kit>/build/qa/ (probes: <repo>/probes/build/qa/; not committed; the file names
carry the run's date and time, so runs never overwrite each other), runs parse_log on every
QA log and prints one line per log. The res-*.log files hold the `RESULT` lines of demo,
rehearsal and probe ROMs (core/sdcard.c); they are graded with the kit's
`tools/judge.py --hardware --partial` (Lightenette: one run per file, it grades one block at a time).
Only the res-*.log files copied from the card in this call are graded, one per ROM: the newest
(file name stamp, then the -2..-9 suffix as a number). Older runs of the same ROM are listed
as superseded, with a warning when their values or build differ.
Hardware values are never compared with an emulator profile; the exit code is the
worst of parse_log and judge.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import parse_log  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('sd')
    ap.add_argument('--game', action='append')
    ap.add_argument('--no-judge', action='store_true', help='only copy and parse, do not run judge.py')
    args = ap.parse_args(argv)
    base = os.path.join(args.sd, 'cleancart')
    if not os.path.isdir(base):
        print('no cleancart/ folder under', args.sd, file=sys.stderr)
        return 2
    games = args.game or sorted(d for d in os.listdir(base) if os.path.isdir(os.path.join(base, d)))
    status = 0
    for game in games:
        src = os.path.join(base, game)
        kit = os.path.join(ROOT, 'probes') if game == 'probes' else os.path.join(ROOT, 'games', game)
        dst = os.path.join(kit, 'build', 'qa')
        os.makedirs(dst, exist_ok=True)
        fresh = []
        for path in sorted(glob.glob(os.path.join(src, 'qa-*.log')) + glob.glob(os.path.join(src, 'save-*.bin'))
                           + glob.glob(os.path.join(src, 'shot-*.bmp'))
                           + glob.glob(os.path.join(src, 'res-*.log'))):
            out = os.path.join(dst, os.path.basename(path))
            shutil.copyfile(path, out)
            if os.path.basename(path).startswith('res-'):
                fresh.append(out)
        for log in sorted(glob.glob(os.path.join(dst, 'qa-*.log'))):
            rc = parse_log.main([log, '--json', log[:-4] + '.json'])
            status = max(status, rc)
        if not args.no_judge:
            status = max(status, judge_results(kit, game, pick_latest(fresh)))
    return status


def result_info(path):
    """(rom, sort key, build, RESULT words) of one res-*.log (the name carries the run's clock)."""
    base = os.path.basename(path)
    m = re.match(r'res-(\d{8}-\d{6})-(.+?)(?:-([2-9]))?\.log$', base)
    stamp, rom, k = (m.group(1), m.group(2), int(m.group(3) or 1)) if m else ('', base, 1)
    build, words = '?', ''
    with open(path, errors='replace') as f:
        for line in f:
            b = re.search(r'\bbuild=(\S+)', line)
            if line.startswith('#') and b:
                build = b.group(1)
            r = re.search(r'\bRESULT((?: [0-9a-fA-F]{8})+)\s*$', line)
            if r:
                words = r.group(1).strip()
    return rom, (stamp, k), build, words


def pick_latest(paths):
    """One file per ROM: the newest; says what it left out and warns when that differs."""
    groups = {}
    for p in paths:
        rom, key, build, words = result_info(p)
        groups.setdefault(rom, []).append((key, p, build, words))
    chosen = []
    for rom, runs in sorted(groups.items()):
        runs.sort()
        key, p, build, words = runs[-1]
        chosen.append(p)
        for _, q, qb, qw in runs[:-1]:
            diff = qw != words or qb != build
            print('%s %s: superseded by %s%s' % ('warning:' if diff else 'note:', os.path.basename(q),
                                                 os.path.basename(p),
                                                 ' (values or build differ: %s vs %s)' % (qb, build) if diff else ''),
                  file=sys.stderr)
        print('grading %s (build=%s)' % (os.path.basename(p), build), flush=True)
    builds = {result_info(p)[2] for p in chosen}
    if len(builds) > 1:
        print('warning: the graded files come from different builds (%s): a ROM that was not run again after a rebuild is graded with its old result; '
              'clear the old res-*.log from the card and run it again' % ', '.join(sorted(builds)), file=sys.stderr)
    return chosen


def judge_results(kit, name, logs):
    """Runs the kit's judge on the result logs; returns its worst exit code (0 when there are none)."""
    if not logs:
        return 0
    judge = os.path.join(kit, 'tools', 'judge.py')
    # Lightenette grades one block per run; the others take every block in one call.
    runs = [[l] for l in logs] if name == 'lightenette' else [logs]
    worst = 0
    for group in runs:
        extra = [] if name == 'lightenette' else ['--partial']
        print('judge %s: %s' % (name, ' '.join(os.path.basename(g) for g in group)), flush=True)
        rc = subprocess.call([sys.executable, '-I', judge, '--hardware'] + extra + group)
        worst = max(worst, rc)
    return worst


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
