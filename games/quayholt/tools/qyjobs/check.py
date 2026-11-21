# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Proves from the model alone that every job can be solved (the game must end on any emulator) and that no job
is a gimme. Returns a list of problems; empty means fine.

    python3 tools/qyjobs/check.py            (run from the game folder)
"""

import os
import sys

if __name__ == '__main__':
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from qyjobs.jobs import JOBS, PIERS, job_list  # noqa: E402
from qyjobs.model import MAX_SETUPS, OPS, model  # noqa: E402


def solutions(op, mask, goal):
    return [(c, p) for c in (0, 1) if mask >> c & 1 for p in range(OPS[op][3]) if model(op, c, p) == goal]


def start_crane(op, mask, goal):
    """The crane a job starts on (the same rule as game/harbour.c): an allowed crane that cannot deliver the goal
    with any setup if there is one, else the first allowed crane."""
    allowed = [c for c in (0, 1) if mask >> c & 1]
    for c in allowed:
        if all(model(op, c, q) != goal for q in range(OPS[op][3])):
            return c
    return allowed[0]


def check():
    problems = []
    jl = job_list()
    if len(jl) != 30:
        problems.append('%d jobs, 30 wanted' % len(jl))
    for pier in range(len(PIERS)):
        n = sum(1 for j in jl if j[0] == pier)
        if n != 5:
            problems.append('pier %s has %d jobs, 5 wanted' % (PIERS[pier], n))
    seen = set()
    goals = {}
    for i, (pier, title, op, mask, crane, p, text, goal) in enumerate(jl):
        name = '%s: %s' % (PIERS[pier], title)
        if OPS[op][1] != pier:
            problems.append('%s: device %s belongs to pier %s' % (name, OPS[op][0], PIERS[OPS[op][1]]))
        if len(title) > 22 or len(text) > 26:
            problems.append('%s: title (22) or order text (26) too long for the pier list' % name)
        if mask & ~OPS[op][2]:
            problems.append('%s: the job allows a crane the device cannot run on' % name)
        key = (op, goal)
        if key in goals:
            problems.append('%s: the same device and goal as %s' % (name, goals[key]))
        goals[key] = name
        if (title) in seen:
            problems.append('%s: duplicate title' % name)
        seen.add(title)
        if goal is None:
            problems.append('%s: the intended crane/setup is not valid' % name)
            continue
        if not mask >> crane & 1:
            problems.append('%s: the intended crane is not allowed' % name)
        sols = solutions(op, mask, goal)
        if (crane, p) not in sols:
            problems.append('%s: the intended setup does not deliver the goal' % name)
        total = sum(OPS[op][3] for c in (0, 1) if mask >> c & 1 and OPS[op][2] >> c & 1)
        if len(sols) >= total:
            problems.append('%s: every setup delivers the goal (no puzzle)' % name)
        # a job both cranes may work is worked out by the intended crane only (so choosing the crane matters)
        if mask == 3 and any(c != crane for c, _ in sols):
            problems.append('%s: the other crane delivers the goal too' % name)
        start = start_crane(op, mask, goal)
        if all(model(op, start, q) == goal for q in range(OPS[op][3])):
            problems.append('%s: the starting crane delivers the goal with every setup' % name)
    if any(o[3] > MAX_SETUPS for o in OPS):
        problems.append('a device has more setups than MAX_SETUPS')
    return problems


def main():
    problems = check()
    for p in problems:
        print('PROBLEM', p)
    if not problems:
        print('all 30 jobs solvable by the model, none trivial')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
