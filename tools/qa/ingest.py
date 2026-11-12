#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Copy QA logs and save mirrors from a real SD card and parse them.

    ingest.py SD_MOUNT [--game NAME ...]

Looks for <SD_MOUNT>/cleancart/<game>/ (every game unless --game is given),
copies qa-*.log and save-*.bin to <repo>/games/<game>/build/qa/ (not committed; the
file names carry the run's date and time, so runs never overwrite each other), runs parse_log on every log and prints one line per log.
"""

import argparse
import glob
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import parse_log  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('sd')
    ap.add_argument('--game', action='append')
    args = ap.parse_args(argv)
    base = os.path.join(args.sd, 'cleancart')
    if not os.path.isdir(base):
        print('no cleancart/ folder under', args.sd, file=sys.stderr)
        return 2
    games = args.game or sorted(d for d in os.listdir(base) if os.path.isdir(os.path.join(base, d)))
    status = 0
    for game in games:
        src = os.path.join(base, game)
        dst = os.path.join(ROOT, 'games', game, 'build', 'qa')
        os.makedirs(dst, exist_ok=True)
        for path in sorted(glob.glob(os.path.join(src, 'qa-*.log')) + glob.glob(os.path.join(src, 'save-*.bin'))):
            out = os.path.join(dst, os.path.basename(path))
            shutil.copyfile(path, out)
        for log in sorted(glob.glob(os.path.join(dst, 'qa-*.log'))):
            rc = parse_log.main([log, '--json', log[:-4] + '.json'])
            status = max(status, rc)
    return status


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
