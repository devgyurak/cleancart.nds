# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Route files (tools/routes/ch1.txt ... ch5.txt), the checker and the packer.

File format (text, one chapter per file, '#' starts a comment):

    chapter 3 FOG BANK
    approach "Slow Bell Passage"
    gate <view 0..2> <true type 0..5> <option a> <option b> <option c>
    gate ...
    gate ...

An approach has exactly three gates. The ROM draws the light of the true type in the given view; the chart
offers three candidate types (a, b, c: distinct, one of them the true type) and the player names the one
with the look the 3D view shows. The checker proves every approach solvable from the almanac alone
(tools/byroute/model.py): at every gate exactly one option has the look of the true type in that view.

Packed blob (little endian, 4-byte aligned), read by arm9/source/game/model.c:
    u32 magic 'BYRT', u32 chapters (5), u32 approaches per chapter (6), u32 gates per approach (3),
    then chapters * approaches * gates records of 5 bytes (view, true, a, b, c) padded to a multiple of 4.
"""

import os
import struct

from . import model

APPROACHES = 6
GATES = 3
MAGIC = 0x54524259   # 'BYRT'


class RouteError(ValueError):
    pass


def parse(path):
    """-> (chapter number, chapter title, [(approach name, [(view, true, (a, b, c))])])."""
    chapter = title = None
    approaches = []
    with open(path, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            where = '%s:%d' % (os.path.basename(path), n)
            if line.startswith('chapter '):
                parts = line.split(None, 2)
                chapter, title = int(parts[1]), parts[2]
            elif line.startswith('approach '):
                name = line.split('"')[1]
                approaches.append((name, []))
            elif line.startswith('gate '):
                if not approaches:
                    raise RouteError('%s: gate before approach' % where)
                v = [int(x) for x in line.split()[1:]]
                if len(v) != 5:
                    raise RouteError('%s: a gate needs view, true, a, b, c' % where)
                approaches[-1][1].append((v[0], v[1], tuple(v[2:5])))
            else:
                raise RouteError('%s: unknown line %r' % (where, line))
    if chapter is None:
        raise RouteError('%s: no chapter line' % path)
    return chapter, title, approaches


def check(chapter, title, approaches):
    """Raises RouteError unless every approach is well formed and solvable from the almanac."""
    problems = []
    if title != model.CHAPTERS[chapter - 1]:
        problems.append('chapter %d is called %s, not %s' % (chapter, model.CHAPTERS[chapter - 1], title))
    if len(approaches) != APPROACHES:
        problems.append('chapter %d has %d approaches, not %d' % (chapter, len(approaches), APPROACHES))
    names = [a[0] for a in approaches]
    if len(set(names)) != len(names):
        problems.append('chapter %d repeats an approach name' % chapter)
    for ai, (name, gates) in enumerate(approaches, 1):
        if len(gates) != GATES:
            problems.append('%s %d: %d gates, not %d' % (name, ai, len(gates), GATES))
        for gi, (view, true, opts) in enumerate(gates, 1):
            where = 'chapter %d approach %d gate %d' % (chapter, ai, gi)
            if not 0 <= view < model.NVIEWS or any(not 0 <= t < model.NTYPES for t in (true,) + opts):
                problems.append('%s: view or type out of range' % where)
                continue
            if len(set(opts)) != 3 or true not in opts:
                problems.append('%s: options must be three distinct types including the true one' % where)
                continue
            want = model.look(chapter, true, view)
            match = [t for t in opts if model.look(chapter, t, view) == want]
            if match != [true]:
                problems.append('%s: %d options look like %d in view %d (types %s), the almanac cannot decide'
                                % (where, len(match), want, view, match))
    if problems:
        raise RouteError('\n'.join(problems))


def load_all(folder):
    chapters = {}
    for ch in range(1, model.NCHAPTERS + 1):
        path = os.path.join(folder, 'ch%d.txt' % ch)
        number, title, approaches = parse(path)
        if number != ch:
            raise RouteError('%s: chapter number %d' % (path, number))
        check(number, title, approaches)
        chapters[ch] = approaches
    return chapters


def pack(chapters):
    out = bytearray(struct.pack('<5I', MAGIC, model.NCHAPTERS, APPROACHES, GATES, 0))
    del out[16:]
    recs = bytearray()
    for ch in range(1, model.NCHAPTERS + 1):
        for _, gates in chapters[ch]:
            for view, true, opts in gates:
                recs += bytes((view, true) + opts)
    while len(recs) % 4:
        recs.append(0)
    return bytes(out + recs)


def plan_hash(blob):
    h = 2166136261
    for b in blob:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def approach_names(chapters):
    return [[a[0] for a in chapters[ch]] for ch in range(1, model.NCHAPTERS + 1)]
