#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Fail when QA code leaks into paths QA must never change.

    lint.py [GAME_DIR ...]      (default: every game folder with core/qa*.c)

QA mode may only change numbers (qa_tune) and feed input; it must not branch
inside rendering, the save code, judging or capture. Any `qa_` / `qalog_` /
`QA_` / `QAR_` identifier in those files is an error. Files checked, inside
games/<game>/arm9/source: everything under gfx/ and judge/, and the files named
save.c, capture.c, judge_scene.c, rehearsal.c, actN.c/h (lightenette).

The same paths (without rehearsal.c, which writes its result block to the SD card once the block is
complete) must not use the SD card helpers either: no `sd_` identifier (core/sdcard.h). A result
is written after judging, from the demo `finish()` or the rehearsal idle, never from inside it.
`probes/` is checked for the `sd_` rule only (it has no QA mode).
"""

import os
import re
import sys

FORBIDDEN_DIRS = ('gfx', 'judge')
FORBIDDEN_FILES = ('save.c', 'capture.c', 'judge_scene.c', 'rehearsal.c')
FORBIDDEN_PATTERN = re.compile(r'^act\d+\.[ch]$')   # lightenette: acts capture and grade
PATTERN = re.compile(r'\b(qa_\w+|qalog_\w+|QA_\w+|QAR_\w+)')
SD_PATTERN = re.compile(r'\b(sd_\w+)')
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def forbidden(rel, sd=False):
    parts = rel.split(os.sep)
    files = tuple(f for f in FORBIDDEN_FILES if not (sd and f == 'rehearsal.c'))
    return (any(p in FORBIDDEN_DIRS for p in parts[:-1])
            or parts[-1] in files
            or bool(FORBIDDEN_PATTERN.match(parts[-1])))


def lint_game(game, qa=True):
    src = os.path.join(game, 'arm9', 'source')
    bad = []
    for dirpath, _, files in sorted(os.walk(src)):
        for name in sorted(files):
            if not name.endswith(('.c', '.h')):
                continue
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, src)
            check_qa = qa and forbidden(rel)
            check_sd = forbidden(rel, sd=True)
            if not (check_qa or check_sd):
                continue
            with open(path, encoding='utf-8', errors='replace') as f:
                for n, line in enumerate(f, 1):
                    for use, pat in ((check_qa, PATTERN), (check_sd, SD_PATTERN)):
                        m = pat.search(line) if use else None
                        if m:
                            bad.append('%s:%d: %s' % (os.path.relpath(path, ROOT), n, m.group(1)))
    return bad


def main(argv):
    games = argv or [os.path.join(ROOT, 'games', d) for d in sorted(os.listdir(os.path.join(ROOT, 'games')))
                     if os.path.isdir(os.path.join(ROOT, 'games', d, 'arm9', 'source', 'core'))]
    bad = []
    for g in games:
        bad += lint_game(g)
    probes = os.path.join(ROOT, 'probes')
    if not argv and os.path.isdir(os.path.join(probes, 'arm9', 'source')):
        bad += lint_game(probes, qa=False)
        games = games + [probes]
    for b in bad:
        print('QA or SD symbol in a forbidden path:', b, file=sys.stderr)
    if bad:
        return 1
    print('qa lint ok (%s)' % ', '.join(os.path.basename(os.path.abspath(g)) for g in games))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
