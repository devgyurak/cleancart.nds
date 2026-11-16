# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The loom model: an order is a cloth of ROWS rows; every row is punched with a card of up to
five cells (columns), each cell cycling through a few values. A chapter switches columns on:

  chapter 1-2   colour, weave        (the loom picks the heddle program itself)
  chapter 3     + program            (the motor must finish inside the order's budget)
  chapter 4     + lever              (VCOUNT lever: LEVER_LINES more lines for that row, `slack` uses)
  chapter 5     + dye                (the shuttle colour changes only in the quiet lines 192..213)

Costs are the GBATEK figures of the geometry engine (cycles at 33.51 MHz, DS 3D I/O Map): MTX_IDENTITY 19,
MTX_PUSH 17, MTX_POP 36, MTX_TRANS 22, MTX_SCALE 22, and MTX_TRANS +30 in matrix mode 2.
Keep this file, tools/hmorder/pack.py and arm9/source/game/order.c in step.
"""

ROWS = 4
COLS = 5
COL_COLOUR, COL_WEAVE, COL_PROGRAM, COL_LEVER, COL_DYE = range(COLS)

# (id, title, [value labels])
COLUMNS = [
    ('colour',  'COLOUR',  ['RED', 'AMBER', 'GOLD', 'GREEN', 'BLUE', 'VIOLET']),
    ('weave',   'WEAVE',   ['PLAIN', 'TWILL', 'HERRING', 'DIAMOND']),
    ('program', 'PROGRAM', ['REST', 'LIFT', 'TWIST', 'FULL']),
    ('lever',   'LEVER',   ['OFF', 'ON']),
    ('dye',     'DYE',     ['NONE', 'L190', 'L192', 'L200', 'L213', 'L214']),
]
COUNTS = [len(c[2]) for c in COLUMNS]
NFIELDS = ROWS * COLS
CHAPTERS = ['PLAIN WEAVE', 'TWILL', 'MOTOR', 'SLACK', 'DYE HOUSE']
PER_CHAPTER = 8
# Columns a chapter lets the player turn.
ACTIVE = {1: (0, 1), 2: (0, 1), 3: (0, 1, 2), 4: (0, 1, 2, 3), 5: (0, 1, 2, 3, 4)}

# GX command cycles and the four heddle programs built from them.
GX_IDENTITY, GX_PUSH, GX_POP, GX_TRANS, GX_SCALE, GX_TRANS_MODE2 = 19, 17, 36, 22, 22, 22 + 30
PROGRAM_OPS = [
    ('IDENTITY',),
    ('PUSH', 'TRANS', 'POP'),
    ('PUSH', 'TRANS2', 'POP'),
    ('IDENTITY', 'PUSH', 'TRANS2', 'SCALE', 'POP'),
]
OP_COST = {'IDENTITY': GX_IDENTITY, 'PUSH': GX_PUSH, 'POP': GX_POP, 'TRANS': GX_TRANS,
           'SCALE': GX_SCALE, 'TRANS2': GX_TRANS_MODE2}
TICKS = [sum(OP_COST[o] for o in ops) for ops in PROGRAM_OPS]      # 19, 75, 105, 146
LINE_CYCLES = 2130        # 355 dots x 6 (DS Video Stuff)
LEVER_LINES = 10          # a VCOUNT write moves the frame on by 10 or 11 lines: the lever counts the sure 10
DYE_FIRST, DYE_LAST = 192, 213      # the quiet lines of the 3D engine (DS 3D Status)
DYE_LINES = [0, 190, 192, 200, 213, 214]
DYE_VALID = [i for i, v in enumerate(DYE_LINES) if DYE_FIRST <= v <= DYE_LAST]
FNV_INIT, FNV_PRIME = 0x811C9DC5, 0x01000193


class OrderError(Exception):
    pass


class Order:
    def __init__(self, chapter, index, name, budget, passes, slack, rows):
        self.chapter, self.index, self.name = chapter, index, name
        self.budget, self.passes, self.slack = budget, passes, slack
        self.rows = rows        # ROWS tuples (colour, weave, dye_wanted)

    @property
    def id(self):
        return '%d-%02d' % (self.chapter, self.index)


def distance(col, a, b):
    n = COUNTS[col]
    d = (b - a) % n
    return min(d, n - d)


def ticks(program, passes):
    return TICKS[program] * passes


def valid(weave, program, lever, budget, passes):
    """A program lifts the heddles the weave needs when its index is at least the weave's, and fits."""
    return program >= weave and ticks(program, passes) <= (budget + LEVER_LINES * lever) * LINE_CYCLES


def lever_needed(weave, budget, passes):
    return not valid(weave, weave, 0, budget, passes)


def row_plan(order, r):
    """The cheapest way to finish row r from the start state: (steps, program, lever, dye).
    Ties go to the lowest program and dye, which is what the scripted player and the checker use."""
    colour, weave, dye_wanted = order.rows[r]
    active = ACTIVE[order.chapter]
    steps = distance(COL_COLOUR, 0, colour) + distance(COL_WEAVE, 0, weave)
    lever = 1 if lever_needed(weave, order.budget, order.passes) else 0
    program = weave
    if COL_PROGRAM in active:
        best = None
        for m in range(COUNTS[COL_PROGRAM]):
            if valid(weave, m, lever, order.budget, order.passes):
                d = distance(COL_PROGRAM, 0, m)
                if best is None or d < best[0]:
                    best = (d, m)
        if best is None:
            raise OrderError('%s row %d: no valid program' % (order.id, r + 1))
        steps += best[0]
        program = best[1]
    if lever:
        if COL_LEVER not in active:
            raise OrderError('%s row %d needs the lever, which chapter %d does not have' % (order.id, r + 1, order.chapter))
        steps += 1
    dye = 0
    if dye_wanted:
        if COL_DYE not in active:
            raise OrderError('%s row %d wants a dye, which chapter %d does not have' % (order.id, r + 1, order.chapter))
        dye, dd = min(((v, distance(COL_DYE, 0, v)) for v in DYE_VALID), key=lambda t: (t[1], t[0]))
        steps += dd
    return steps, program, lever, dye


def par(order):
    return sum(row_plan(order, r)[0] for r in range(ROWS))


def plan_ticks(order):
    """Ticks the planned programs run per row (what the scripted player's motor does)."""
    return [ticks(row_plan(order, r)[1], order.passes) for r in range(ROWS)]


def fnv(words, h=FNV_INIT):
    for w in words:
        for k in range(4):
            h = ((h ^ (w >> (8 * k) & 255)) * FNV_PRIME) & 0xFFFFFFFF
    return h


def plan_hash(orders):
    """Hash of the planned ticks of the first order of each chapter (the rehearsal's plan-ticks value)."""
    words = []
    for ch in range(1, len(CHAPTERS) + 1):
        first = next(o for o in orders if o.chapter == ch and o.index == 1)
        words += plan_ticks(first)
    return fnv(words)


def parse(text, filename='ch1.txt'):
    """One chapter file; a line is `NAME: budget=N passes=N slack=N rows=c.w[.d],c.w[.d],...` (# comments).
    A row is colour and weave by value name, and a trailing `+` when the row wants a dye."""
    stem = filename.rsplit('/', 1)[-1]
    if not (stem.startswith('ch') and stem.endswith('.txt') and stem[2:-4].isdigit()):
        raise OrderError('chapter files are named ch<N>.txt, not %r' % filename)
    chapter = int(stem[2:-4])
    out = []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        if ':' not in line:
            raise OrderError('%s:%d: expected "NAME: key=value ..."' % (stem, n))
        name, rest = line.split(':', 1)
        name = name.strip().upper()
        if not name or len(name) > 15 or not all(c.isalnum() or c == ' ' for c in name):
            raise OrderError('%s:%d: bad order name %r (1-15 letters, digits, spaces)' % (stem, n, name))
        kv = {}
        for tok in rest.split():
            if '=' not in tok:
                raise OrderError('%s:%d: %r is not key=value' % (stem, n, tok))
            k, v = tok.split('=', 1)
            kv[k] = v
        for k in ('budget', 'passes', 'slack', 'rows'):
            if k not in kv:
                raise OrderError('%s:%d: missing %s=' % (stem, n, k))
        rows = []
        for spec in kv['rows'].split(','):
            dye = spec.endswith('+')
            parts = spec.rstrip('+').upper().split('.')
            if len(parts) != 2 or parts[0] not in COLUMNS[0][2] or parts[1] not in COLUMNS[1][2]:
                raise OrderError('%s:%d: bad row %r (COLOUR.WEAVE[+])' % (stem, n, spec))
            rows.append((COLUMNS[0][2].index(parts[0]), COLUMNS[1][2].index(parts[1]), 1 if dye else 0))
        if len(rows) != ROWS:
            raise OrderError('%s:%d: an order has %d rows, not %d' % (stem, n, ROWS, len(rows)))
        out.append(Order(chapter, len(out) + 1, name, int(kv['budget']), int(kv['passes']), int(kv['slack']), rows))
    return out
