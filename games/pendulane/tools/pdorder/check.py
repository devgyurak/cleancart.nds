# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Parser and checker of the contraption files.

    python3 tools/pdorder/check.py [--chapter N] [--json] [files...]

For every contraption: the format is valid, every letter belongs to the chapter, no slot offers a letter twice, and the plan
that starts every slot at its first letter does not already hit the target. The checker finds every plan that adds up to the
target (a brute force over at most 4^6 plans) and reports how many there are and the par: the fewest slots a player has to
change from the starting plan. A contraption with no plan is an error. The build runs this (tools/mkassets.py).
"""

import itertools
import os
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from pdorder import model  # noqa: E402

LINE = re.compile(r'^([A-Z0-9][A-Z0-9 ]*[A-Z0-9]):\s*(\d+)\s*\|\s*(.+)$')


def load(paths):
    out = []
    for path in paths:
        name = os.path.basename(path)
        m = re.match(r'ch(\d)\.txt$', name)
        if not m:
            raise ValueError('%s: file names are ch<N>.txt' % path)
        chapter = int(m.group(1))
        with open(path) as f:
            for no, raw in enumerate(f, 1):
                line = raw.split('#', 1)[0].strip()
                if not line:
                    continue
                mm = LINE.match(line)
                if not mm:
                    raise ValueError('%s:%d: not "NAME: TARGET | SLOT | SLOT"' % (name, no))
                slots = [s.strip() for s in mm.group(3).split('|')]
                out.append(model.Contraption(chapter, mm.group(1), int(mm.group(2)), slots, no))
    return out


def plans(c):
    """Every plan (tuple of letters) that adds up to the target."""
    for combo in itertools.product(*c.slots):
        if sum(c.cost(x) for x in combo) == c.target:
            yield combo


def par(c, found):
    start = tuple(s[0] for s in c.slots)
    return min(sum(1 for a, b in zip(start, p) if a != b) for p in found)


def check(contraptions):
    rows, ok = [], True
    seen = set()
    for c in contraptions:
        problems = []
        if c.chapter not in model.CHAPTERS:
            problems.append('unknown chapter %d' % c.chapter)
        else:
            if len(c.name) > model.NAME_MAX:
                problems.append('name longer than %d' % model.NAME_MAX)
            if (c.chapter, c.name) in seen:
                problems.append('duplicate name')
            seen.add((c.chapter, c.name))
            if not 1 <= len(c.slots) <= model.MAX_SLOTS:
                problems.append('%d slots (1..%d)' % (len(c.slots), model.MAX_SLOTS))
            for i, s in enumerate(c.slots):
                if not 2 <= len(s) <= model.MAX_OPTS:
                    problems.append('slot %d offers %d stations (2..%d)' % (i + 1, len(s), model.MAX_OPTS))
                if len(set(s)) != len(s):
                    problems.append('slot %d repeats a station' % (i + 1))
                for ch in s:
                    if ch not in c.stations:
                        problems.append('slot %d: no station %r in chapter %d' % (i + 1, ch, c.chapter))
        found = []
        if not problems:
            found = list(plans(c))
            start = tuple(s[0] for s in c.slots)
            if not found:
                problems.append('no plan adds up to %d' % c.target)
            elif start in found:
                problems.append('the starting plan already adds up to the target')
        rows.append({'id': '%d-%s' % (c.chapter, c.name), 'plans': len(found),
                     'par': par(c, found) if found else None, 'problems': problems})
        ok = ok and not problems
    return rows, ok


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    paths = [a for a in argv if not a.startswith('-')]
    chapter = None
    if '--chapter' in argv:
        chapter = int(argv[argv.index('--chapter') + 1])
        paths = [p for p in paths if p != str(chapter)]
    if not paths:
        d = os.path.join(here, '..', 'contraptions')
        paths = sorted(os.path.join(d, n) for n in os.listdir(d) if n.endswith('.txt'))
    cs = load(paths)
    if chapter is not None:
        cs = [c for c in cs if c.chapter == chapter]
    rows, ok = check(cs)
    for r in rows:
        print('%-26s plans %3d  par %s  %s' % (r['id'], r['plans'], r['par'], '; '.join(r['problems']) or 'ok'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
