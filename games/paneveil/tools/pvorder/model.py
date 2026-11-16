# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The window model: twelve settings, each with a small cyclic list of values.

A window order names a target value for some settings; every other setting
has to stay at its first value. The player turns one setting one step at a time
(either direction), so the cost of an order is a sum of cyclic distances.
Keep FIELDS in step with the C side: mkassets.py writes it into assets.h.
"""

# (id, title, hardware feature it drives, [value labels])
FIELDS = [
    ('side',    'GLASS SIDE', 'polygon attribute: render front / back face', ['FRONT', 'BACK', 'BOTH']),
    ('tint',    'GLASS TINT', 'polygon alpha 31 / 16 / 0 (wireframe)',       ['CLEAR', 'TINT', 'SKETCH']),
    ('motif',   'MOTIF',      'texture image',                               ['CHEVRON', 'ARC', 'BARS']),
    ('repeat',  'REPEAT',     'texture repeat count (S and T)',              ['1X', '2X', '4X']),
    ('flip',    'MIRROR',     'texture flip S / T',                          ['NONE', 'S', 'T', 'ST']),
    ('edge',    'EDGE',       'texture repeat or clamp',                     ['REPEAT', 'CLAMP']),
    ('colour',  'COLOUR',     'texture palette base',                        ['AMBER', 'RUBY', 'SEA', 'VIOLET']),
    ('cutout',  'CUTOUT',     'texture colour 0 transparent',                ['SOLID', 'CUT']),
    ('petals',  'PETALS',     'push / rotate / pop per petal',               ['4', '5', '6', '7', '8']),
    ('rings',   'RINGS',      'matrix stack depth',                          ['1', '2', '3']),
    ('decal',   'DECAL',      'depth test equal overlay',                    ['OFF', 'ON']),
    ('grain',   'GRAIN',      'texture coordinate source mode 1',            ['0', '1', '2', '3']),
]
NFIELDS = len(FIELDS)
FIELD_INDEX = {f[0]: i for i, f in enumerate(FIELDS)}
COUNTS = [len(f[3]) for f in FIELDS]

# The chapters and what they teach (the five chapters of six windows each).
CHAPTERS = ['GLASS', 'PATTERN', 'ROSE', 'FINISH', 'PAVILION']
PER_CHAPTER = 6


class OrderError(Exception):
    pass


class Window:
    def __init__(self, chapter, index, name, target):
        self.chapter = chapter
        self.index = index
        self.name = name
        self.target = target          # list of NFIELDS value indices

    @property
    def id(self):
        return '%d-%02d' % (self.chapter, self.index)


def parse(text, filename='ch1.txt'):
    """Parse one chapter file: `name: field=VALUE field=VALUE` per line, # comments."""
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
            raise OrderError('%s:%d: expected "name: field=VALUE ..."' % (stem, n))
        name, rest = line.split(':', 1)
        name = name.strip().upper()
        if not name or len(name) > 15 or not all(c.isalnum() or c == ' ' for c in name):
            raise OrderError('%s:%d: bad window name %r (1-15 letters, digits, spaces)' % (stem, n, name))
        target = [0] * NFIELDS
        seen = set()
        for tok in rest.split():
            if '=' not in tok:
                raise OrderError('%s:%d: %r is not field=VALUE' % (stem, n, tok))
            key, val = tok.split('=', 1)
            if key not in FIELD_INDEX:
                raise OrderError('%s:%d: unknown setting %r' % (stem, n, key))
            fi = FIELD_INDEX[key]
            labels = FIELDS[fi][3]
            val = val.upper()
            if val not in labels:
                raise OrderError('%s:%d: %s has no value %r (%s)' % (stem, n, key, val, ' '.join(labels)))
            if key in seen:
                raise OrderError('%s:%d: %s given twice' % (stem, n, key))
            seen.add(key)
            target[fi] = labels.index(val)
        out.append(Window(chapter, len(out) + 1, name, target))
    return out


def distance(field, a, b):
    """Fewest single steps between two values of a setting (the cycle can be turned both ways)."""
    n = COUNTS[field]
    d = (b - a) % n
    return min(d, n - d)
