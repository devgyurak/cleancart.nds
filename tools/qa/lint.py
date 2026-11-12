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
"""

import os
import re
import sys

FORBIDDEN_DIRS = ('gfx', 'judge')
FORBIDDEN_FILES = ('save.c', 'capture.c', 'judge_scene.c', 'rehearsal.c')
FORBIDDEN_PATTERN = re.compile(r'^act\d+\.[ch]$')   # lightenette: acts capture and grade
PATTERN = re.compile(r'\b(qa_\w+|qalog_\w+|QA_\w+|QAR_\w+)')
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def forbidden(rel):
    parts = rel.split(os.sep)
    return (any(p in FORBIDDEN_DIRS for p in parts[:-1])
            or parts[-1] in FORBIDDEN_FILES
            or bool(FORBIDDEN_PATTERN.match(parts[-1])))


def lint_game(game):
    src = os.path.join(game, 'arm9', 'source')
    bad = []
    for dirpath, _, files in sorted(os.walk(src)):
        for name in sorted(files):
            if not name.endswith(('.c', '.h')):
                continue
            path = os.path.join(dirpath, name)
            if not forbidden(os.path.relpath(path, src)):
                continue
            with open(path, encoding='utf-8', errors='replace') as f:
                for n, line in enumerate(f, 1):
                    m = PATTERN.search(line)
                    if m:
                        bad.append('%s:%d: %s' % (os.path.relpath(path, ROOT), n, m.group(1)))
    return bad


def main(argv):
    games = argv or [os.path.join(ROOT, 'games', d) for d in sorted(os.listdir(os.path.join(ROOT, 'games')))
                     if os.path.isdir(os.path.join(ROOT, 'games', d, 'arm9', 'source', 'core'))]
    bad = []
    for g in games:
        bad += lint_game(g)
    for b in bad:
        print('QA symbol in a forbidden path:', b, file=sys.stderr)
    if bad:
        return 1
    print('qa lint ok (%s)' % ', '.join(os.path.basename(os.path.abspath(g)) for g in games))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
