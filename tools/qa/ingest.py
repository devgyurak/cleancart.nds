#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Copy QA logs, result logs and save mirrors from a real SD card and parse/judge them.

    ingest.py SD_MOUNT [--game NAME ...] [--no-judge]

Looks for <SD_MOUNT>/cleancart/<kit>/ (every kit unless --game is given; the kits are the
games and `probes`), copies qa-*.log, res-*.log and save-*.bin to
<repo>/games/<kit>/build/qa/ (probes: <repo>/probes/build/qa/; not committed; the file names
carry the run's date and time, so runs never overwrite each other), runs parse_log on every
QA log and prints one line per log. The res-*.log files hold the `RESULT` lines of demo,
rehearsal and probe ROMs (core/sdcard.c); they are graded with the kit's
`tools/judge.py --hardware --partial` (Lightenette: one run per file, it grades one block at a time).
Hardware values are never compared with an emulator profile; the exit code is the
worst of parse_log and judge.
"""

import argparse
import glob
import os
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
        for path in sorted(glob.glob(os.path.join(src, 'qa-*.log')) + glob.glob(os.path.join(src, 'save-*.bin'))
                           + glob.glob(os.path.join(src, 'res-*.log'))):
            out = os.path.join(dst, os.path.basename(path))
            shutil.copyfile(path, out)
        for log in sorted(glob.glob(os.path.join(dst, 'qa-*.log'))):
            rc = parse_log.main([log, '--json', log[:-4] + '.json'])
            status = max(status, rc)
        if not args.no_judge:
            status = max(status, judge_results(kit, game, sorted(glob.glob(os.path.join(dst, 'res-*.log')))))
    return status


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
