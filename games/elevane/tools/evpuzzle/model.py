# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Voxel structures, orders (silhouettes) and the support rule.

Coordinates: x runs left to right, y front to back (y = 0 is the front row,
the one the front view looks at first), z runs up (z = 0 is the floor).
A structure is a set of (x, y, z) cells inside a W x D x H grid, each at most 8.

Front view (x, z): filled when any cell exists along y for that x and z.
Plan view (x, y): filled when the column (x, y) holds any cell.
An order is that pair of views. The side view (L/R camera) is for looking only.

Support rule: every cell must reach z = 0 through face-adjacent cells. A
floating lump is not deliverable. An arch is two columns joined from above.

Puzzle text format (one file per chapter, blank lines and '#' comments ignored):

    [1-01] name=First Row chapter=1 mode=stack grid=4x4x2
    h = 1111/0000/0000/0000          # stack mode: heights, rows are y = 0..D-1

    [4-01] name=Gate chapter=4 mode=layers grid=5x1x3
    z0 = #.#
    z1 = ###
    part grid=...                    # chapter 5: next structure of the same puzzle

Lines starting with '#' are comments. In 'layers' mode there is one z<N> line per
layer, rows separated by '/', characters '#' (block) or '.'; in 'stack' mode the
rows are digits. Orders are derived from the maker's structure, so a
solution always exists; solve.py then finds the true minimum.
"""

MAX_DIM = 8

NEIGHBOURS = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))


class PuzzleError(ValueError):
    pass


class Structure:
    """A maker's structure plus the grid it lives in."""

    def __init__(self, w, d, h, cells):
        if not (1 <= w <= MAX_DIM and 1 <= d <= MAX_DIM and 1 <= h <= MAX_DIM):
            raise PuzzleError('grid %dx%dx%d outside 1..%d' % (w, d, h, MAX_DIM))
        self.w, self.d, self.h = w, d, h
        self.cells = frozenset(cells)
        for (x, y, z) in self.cells:
            if not (0 <= x < w and 0 <= y < d and 0 <= z < h):
                raise PuzzleError('cell %r outside the grid' % ((x, y, z),))

    def front(self):
        """Front view as w bytes: bit z of byte x is set when (x, z) is filled."""
        out = [0] * self.w
        for (x, y, z) in self.cells:
            out[x] |= 1 << z
        return tuple(out)

    def plan(self):
        """Plan view as d bytes: bit x of byte y is set when column (x, y) is filled."""
        out = [0] * self.d
        for (x, y, z) in self.cells:
            out[y] |= 1 << x
        return tuple(out)


class Order:
    """The two silhouettes of one structure."""

    def __init__(self, w, d, h, front, plan):
        self.w, self.d, self.h = w, d, h
        self.front = tuple(front)   # w bytes, bits over z
        self.plan = tuple(plan)     # d bytes, bits over x

    @classmethod
    def of(cls, s):
        return cls(s.w, s.d, s.h, s.front(), s.plan())

    def front_has(self, x, z):
        return bool(self.front[x] >> z & 1)

    def plan_has(self, x, y):
        return bool(self.plan[y] >> x & 1)

    def allowed(self):
        """Cells a deliverable structure may use: inside both silhouettes."""
        return [(x, y, z)
                for z in range(self.h) for y in range(self.d) for x in range(self.w)
                if self.front_has(x, z) and self.plan_has(x, y)]


def supported(cells):
    """True when every cell reaches z = 0 through face-adjacent cells."""
    cells = set(cells)
    seen = [c for c in cells if c[2] == 0]
    mark = set(seen)
    while seen:
        x, y, z = seen.pop()
        for dx, dy, dz in NEIGHBOURS:
            n = (x + dx, y + dy, z + dz)
            if n in cells and n not in mark:
                mark.add(n)
                seen.append(n)
    return len(mark) == len(cells)


def unsupported(cells):
    """The cells that do not reach the floor (sorted, for messages)."""
    cells = set(cells)
    seen = [c for c in cells if c[2] == 0]
    mark = set(seen)
    while seen:
        x, y, z = seen.pop()
        for dx, dy, dz in NEIGHBOURS:
            n = (x + dx, y + dy, z + dz)
            if n in cells and n not in mark:
                mark.add(n)
                seen.append(n)
    return sorted(cells - mark)


def check_structure(order, cells):
    """Return a list of problems with a delivered structure (empty = accepted)."""
    errs = []
    cells = set(cells)
    for (x, y, z) in sorted(cells):
        if not (0 <= x < order.w and 0 <= y < order.d and 0 <= z < order.h):
            errs.append('cell %r outside the grid' % ((x, y, z),))
    fv = Structure(order.w, order.d, order.h, [c for c in cells
                   if 0 <= c[0] < order.w and 0 <= c[1] < order.d and 0 <= c[2] < order.h])
    for x in range(order.w):
        if fv.front()[x] != order.front[x]:
            errs.append('front view differs at x=%d (%02x, want %02x)' % (x, fv.front()[x], order.front[x]))
    for y in range(order.d):
        if fv.plan()[y] != order.plan[y]:
            errs.append('plan view differs at y=%d (%02x, want %02x)' % (y, fv.plan()[y], order.plan[y]))
    if not supported(cells):
        errs.append('floating blocks: %r' % (unsupported(cells),))
    return errs


class Puzzle:
    def __init__(self, pid, name, chapter, mode, parts):
        self.id, self.name, self.chapter, self.mode = pid, name, chapter, mode
        self.parts = parts   # list of Structure (the maker's solutions)

    def orders(self):
        return [Order.of(p) for p in self.parts]


def _grid(text):
    try:
        w, d, h = (int(v) for v in text.lower().split('x'))
    except ValueError:
        raise PuzzleError('bad grid %r (want WxDxH)' % text)
    return w, d, h


def _fields(text):
    """key=value words; a value may be quoted with double quotes."""
    out = {}
    i, n = 0, len(text)
    while i < n:
        while i < n and text[i].isspace():
            i += 1
        j = text.find('=', i)
        if j < 0:
            if text[i:].strip():
                raise PuzzleError('bad field text %r' % text[i:])
            break
        key = text[i:j].strip()
        j += 1
        if j < n and text[j] == '"':
            k = text.index('"', j + 1)
            out[key] = text[j + 1:k]
            i = k + 1
        else:
            k = j
            while k < n and not text[k].isspace():
                k += 1
            out[key] = text[j:k]
            i = k
    return out


def _build_part(w, d, h, mode, rows, where):
    cells = set()
    if mode == 'stack':
        hm = rows.get('h')
        if hm is None:
            raise PuzzleError('%s: stack mode needs an h = line' % where)
        lines = hm.split('/')
        if len(lines) != d:
            raise PuzzleError('%s: h has %d rows, grid depth is %d' % (where, len(lines), d))
        for y, line in enumerate(lines):
            line = line.strip()
            if len(line) != w:
                raise PuzzleError('%s: h row %d has %d columns, grid width is %d' % (where, y, len(line), w))
            for x, ch in enumerate(line):
                if not ch.isdigit():
                    raise PuzzleError('%s: bad height %r' % (where, ch))
                for z in range(int(ch)):
                    cells.add((x, y, z))
    else:
        for key, val in rows.items():
            if not (key.startswith('z') and key[1:].isdigit()):
                raise PuzzleError('%s: unexpected line %r in layers mode' % (where, key))
            z = int(key[1:])
            lines = val.split('/')
            if len(lines) != d:
                raise PuzzleError('%s: %s has %d rows, grid depth is %d' % (where, key, len(lines), d))
            for y, line in enumerate(lines):
                line = line.strip()
                if len(line) != w:
                    raise PuzzleError('%s: %s row %d has %d columns, grid width is %d' % (where, key, y, len(line), w))
                for x, ch in enumerate(line):
                    if ch == '#':
                        cells.add((x, y, z))
                    elif ch != '.':
                        raise PuzzleError('%s: bad cell %r (use # or .)' % (where, ch))
    try:
        return Structure(w, d, h, cells)
    except PuzzleError as e:
        raise PuzzleError('%s: %s' % (where, e))


def parse(text, source='<text>'):
    """Parse a puzzle file into a list of Puzzle objects (file order)."""
    puzzles = []
    cur = None   # [pid, fields, [part dicts]]

    def finish():
        if cur is None:
            return
        pid, f, parts = cur
        mode = f.get('mode', 'stack')
        if mode not in ('stack', 'layers'):
            raise PuzzleError('%s: %s: mode must be stack or layers' % (source, pid))
        if 'chapter' not in f:
            raise PuzzleError('%s: %s: chapter missing' % (source, pid))
        built = []
        for k, (grid, rows) in enumerate(parts):
            w, d, h = _grid(grid)
            built.append(_build_part(w, d, h, mode, rows, '%s: %s part %d' % (source, pid, k + 1)))
        puzzles.append(Puzzle(pid, f.get('name', pid), int(f['chapter']), mode, built))

    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('['):
            finish()
            end = line.index(']')
            f = _fields(line[end + 1:])
            if 'grid' not in f:
                raise PuzzleError('%s:%d: grid missing' % (source, n))
            cur = [line[1:end], f, [[f['grid'], {}]]]
            continue
        if cur is None:
            raise PuzzleError('%s:%d: line before the first [id]' % (source, n))
        if line.startswith('part'):
            f = _fields(line[4:])
            if 'grid' not in f:
                raise PuzzleError('%s:%d: part needs grid=' % (source, n))
            cur[2].append([f['grid'], {}])
            continue
        if '=' not in line:
            raise PuzzleError('%s:%d: cannot read %r' % (source, n, line))
        key, _, val = line.partition('=')
        cur[2][-1][1][key.strip()] = val.strip()
    finish()
    ids = [p.id for p in puzzles]
    if len(set(ids)) != len(ids):
        raise PuzzleError('%s: duplicate puzzle ids' % source)
    return puzzles
