# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Par for a window: the fewest single steps from the start (all first values)
to the target. Settings do not interact, so it is a sum of cyclic distances;
`bfs_par` searches the state space instead and the tests compare the two."""

from collections import deque

from . import model


def par(target):
    return sum(model.distance(f, 0, v) for f, v in enumerate(target))


def moves(target):
    """A shortest sequence of (field, +1 / -1) steps, fields in order."""
    out = []
    for f, v in enumerate(target):
        n = model.COUNTS[f]
        d = v % n
        if d <= n - d:
            out += [(f, 1)] * d
        else:
            out += [(f, -1)] * (n - d)
    return out


def bfs_par(target, fields=None):
    """Breadth-first search over the settings named in `fields` (default: those that differ);
    small enough for the checker to run on every window."""
    fields = [f for f, v in enumerate(target) if v] if fields is None else fields
    start = tuple(0 for _ in fields)
    goal = tuple(target[f] for f in fields)
    seen = {start: 0}
    queue = deque([start])
    while queue:
        s = queue.popleft()
        if s == goal:
            return seen[s]
        for i, f in enumerate(fields):
            for step in (1, -1):
                t = list(s)
                t[i] = (t[i] + step) % model.COUNTS[f]
                t = tuple(t)
                if t not in seen:
                    seen[t] = seen[s] + 1
                    queue.append(t)
    return None
