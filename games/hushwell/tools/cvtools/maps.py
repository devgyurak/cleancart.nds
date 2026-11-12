# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Chapter maps: hand-drawn ASCII grids compiled to the cart format, plus
the attract-mode input logs.

Binary format (little endian), read by arm9/source/game/world.c:

  0  u32  magic 'HWMP'
  4  u16  version (1)
  6  u16  chapter (0 prologue, 1-5)
  8  u16  width (cells, <= 32)
 10  u16  height (cells, <= 32)
 12  u16  object count
 14  u16  flags (see FLAG_*)
 16  u16  song id
 18  u16  fog colour (BGR555)
 20  u16  clear colour (BGR555)
 22  u8   ambient light (0-31)
 23  u8   start facing (0 north, 1 east, 2 south, 3 west)
 24  u8[w*h] cells (CELL_*)
 ..  objects, 4 bytes each: u8 type, u8 x, u8 y, u8 param

One cell is one world unit; cell (x, y) spans world x..x+1, z..z+1.

Legend of the ASCII maps:
  #  wall            .  floor             ~  flooded floor   ,  open ground
  %  fence/branches  (alpha-tested wall)  =  shelf (wall with shelf texture)
  S  start           X  exit              A B C Z  locked doors (keys a b c z)
  K  code-locked door (chapter 3)         R  emergency radio (save)
  t  tape            f  flares            e  battery         M  monster
  L  emergency lamp  P  projector         u  car             p  pew
  +  bed             Y  tree              w  the well        v  night visitor
  q  bug swarm       o  altar
Picking up key z starts the chapter's chase.
"""

import struct

from .gfx import Rng, rgb15

MAGIC = 0x504D5748  # 'HWMP'

CELL_FLOOR, CELL_WALL, CELL_WATER, CELL_GROUND, CELL_FENCE, CELL_DOOR, CELL_SHELF = range(7)

# Object types (arm9/source/game/world.h)
(O_START, O_EXIT, O_DOOR, O_KEY, O_RADIO, O_TAPE, O_FLARES, O_BATTERY,
 O_MONSTER, O_LAMP, O_PROJECTOR, O_CAR, O_PEW, O_BED, O_TREE, O_WELL,
 O_VISITOR, O_SWARM, O_ALTAR, O_CODEDOOR) = range(20)

FLAG_OUTDOOR = 1    # no ceiling, sky behind the 3D
FLAG_RAIN = 2       # HBlank-DMA rain layer
FLAG_WATER = 4      # flooded floors, steam, afterimage
FLAG_FILM = 8       # projector / damaged film
FLAG_NIGHT = 16     # RTC night visitor
FLAG_WELL = 32      # dual-screen 3D, bug swarm
FLAG_PHANTOM = 64   # the monster only passes by (prologue)

KEY_IDS = {'a': 0, 'b': 1, 'c': 2, 'z': 3}
DOOR_IDS = {'A': 0, 'B': 1, 'C': 2, 'Z': 3}


def c15(r, g, b):
    return rgb15(r, g, b) & 0x7FFF


CHAPTERS = [
    dict(n=0, name='prologue', song=2, flags=FLAG_OUTDOOR | FLAG_RAIN | FLAG_PHANTOM,
         fog=(30, 36, 48), clear=(14, 18, 28), ambient=5, facing=0, rows=[
             '############',
             '#YY%%XX%%YY#',
             '#Y%,,,,,,%Y#',
             '#Y%,,,,,,%Y#',
             '#Y,,,,,,,,M#',
             '#Y%,,,,,,%Y#',
             '#YY%,,,,%YY#',
             '#Y%%,,,,%%Y#',
             '#Y%,,,,,,%Y#',
             '#Y%,,,,,,%Y#',
             '#YY,,,,,,YY#',
             '#Y%,,,,,,%Y#',
             '#Y%,,u,,,%Y#',
             '#Y%,,,,R,%Y#',
             '#Y%,,S,f,%Y#',
             '############',
         ]),
    dict(n=1, name='lobby', song=3, flags=0,
         fog=(4, 4, 6), clear=(2, 2, 3), ambient=3, facing=0, rows=[
             '####################',
             '#+..+.#X#.L..+..+..#',
             '#...b.#Z#..........#',
             '#..t..#.#..+..+..z.#',
             '#.....#.####B#######',
             '##A####.....L.....e#',
             '#...L..........###.#',
             '#.....###.####.#M..#',
             '#.f...#.....a#.#...#',
             '#.....#.###..#.....#',
             '#..R..#L..#..#####.#',
             '#.....#...#........#',
             '#..S..#####.####L..#',
             '#..................#',
             '####################',
         ]),
    dict(n=2, name='hydro', song=4, flags=FLAG_WATER,
         fog=(10, 14, 16), clear=(4, 6, 8), ambient=4, facing=0, rows=[
             '####################',
             '#~~~~#.X.#~~~~~~~~~#',
             '#~t~~#.Z.#~~~~~~~~~#',
             '#~~~~#...#~~#####~~#',
             '#~~~~#~~~~~~#...#~~#',
             '###A######~~#.z.B~~#',
             '#....L...#~~#...#~~#',
             '#.R.....f#~~#####~~#',
             '#......~~~~~~~~~~~~#',
             '#.####~~~~###~~~~~~#',
             '#.#b.#~~~~#a#~~~M~~#',
             '#.#..#~~~~#.#~~~~~~#',
             '#....L~~~~#...L~~~~#',
             '#.S..#~~~~~~~~~~~e~#',
             '####################',
         ]),
    dict(n=3, name='archive', song=5, flags=FLAG_FILM,
         fog=(8, 6, 4), clear=(3, 2, 1), ambient=3, facing=0, rows=[
             '####################',
             '#..=..=..=.#X#..t..#',
             '#..=..=..=.#Z#.....#',
             '#..=..=..=.#.#..L..#',
             '#..........#K#.....#',
             '#..=..=..=.......=.#',
             '#..=..=..=.L.....=.#',
             '#..=..=..=.......=.#',
             '#....M......########',
             '#########...#P....z#',
             '#.......#...#......#',
             '#.R..f..#...#..L...#',
             '#.......#...####.###',
             '#..S..L.......e....#',
             '####################',
         ]),
    dict(n=4, name='chapel', song=6, flags=FLAG_OUTDOOR | FLAG_NIGHT,
         fog=(6, 8, 18), clear=(2, 3, 10), ambient=4, facing=0, rows=[
             '####################',
             '#YY%%%%%%#####X#####',
             '#Y,,,,,,%#ppp.Zppp.#',
             '#Y,,Y,b,,#.........#',
             '#Y,,,,,v,#ppp.o.ppp#',
             '#%,,,Y,,,#.........#',
             '#%,,,,,,,B....z....#',
             '#Y,Y,,,,,#ppp...ppp#',
             '#Y,,,,Y,,###########',
             '#%,,,,,,,,,,,,,,,,Y#',
             '#%,,Y,,,,,Y,,,,,,,Y#',
             '#Y,R,,,e,,,,,Y,,M,%#',
             '#Y,,,,,,,,Y,,,,,,,%#',
             '#YY,S,f,,,,,,,t,,%%#',
             '####################',
         ]),
    dict(n=5, name='well', song=7, flags=FLAG_WELL,
         fog=(2, 0, 0), clear=(1, 0, 0), ambient=2, facing=1, rows=[
             '####################',
             '#S..#....q...#....t#',
             '#.R.#.######.#.###.#',
             '#...#.#...f#.#.#...#',
             '#.###.#.##.#...#.###',
             '#.....#..#.#####.#.#',
             '#####.##.#.......#.#',
             '#.....#..#####.###.#',
             '#.#####.e#...#.....#',
             '#.#.....##.#.#####.#',
             '#.#.###.#..#.....#.#',
             '#.....#.#.##.###.#.#',
             '#####.#...#..#zM.#.#',
             '#w....#####..#####.#',
             '####################',
         ]),
]

CHAR_CELL = {'#': CELL_WALL, '.': CELL_FLOOR, '~': CELL_WATER, ',': CELL_GROUND,
             '%': CELL_FENCE, '=': CELL_SHELF}
CHAR_OBJ = {'S': O_START, 'X': O_EXIT, 'R': O_RADIO, 't': O_TAPE, 'f': O_FLARES,
            'e': O_BATTERY, 'M': O_MONSTER, 'L': O_LAMP, 'P': O_PROJECTOR,
            'u': O_CAR, 'p': O_PEW, '+': O_BED, 'Y': O_TREE, 'w': O_WELL,
            'v': O_VISITOR, 'q': O_SWARM, 'o': O_ALTAR, 'K': O_CODEDOOR}
# Objects that stand on an otherwise walkable cell but block it.
BLOCKERS = {O_CAR, O_PEW, O_BED, O_TREE, O_ALTAR, O_PROJECTOR}


def floor_for(ch):
    return CELL_GROUND if ch['flags'] & FLAG_OUTDOOR else CELL_FLOOR


def parse(ch):
    rows = ch['rows']
    h = len(rows)
    w = len(rows[0])
    assert all(len(r) == w for r in rows), 'chapter %d: ragged map' % ch['n']
    assert w <= 32 and h <= 32
    cells = []
    objs = []
    base = floor_for(ch)
    tapes = 0
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c in CHAR_CELL:
                cells.append(CHAR_CELL[c])
                continue
            if c in DOOR_IDS:
                cells.append(CELL_DOOR)
                objs.append((O_DOOR, x, y, DOOR_IDS[c]))
                continue
            if c in KEY_IDS:
                cells.append(base)
                objs.append((O_KEY, x, y, KEY_IDS[c]))
                continue
            if c not in CHAR_OBJ:
                raise SystemExit('chapter %d: unknown map char %r' % (ch['n'], c))
            t = CHAR_OBJ[c]
            if t == O_CODEDOOR:
                cells.append(CELL_DOOR)
                objs.append((t, x, y, 0))
                continue
            if t == O_TREE:
                cells.append(CELL_GROUND)
            elif t == O_WELL:
                cells.append(CELL_FLOOR)
            else:
                # Water rooms keep their water under objects next to water.
                under = base
                if ch['flags'] & FLAG_WATER and x > 0 and row[x - 1] == '~':
                    under = CELL_WATER
                cells.append(under)
            param = 0
            if t == O_TAPE:
                param = ch['n']
                tapes += 1
            objs.append((t, x, y, param))
    return w, h, cells, objs


def check(ch, w, h, cells, objs):
    """Every chapter must be finishable: the exit is reachable from the
    start once each key behind reachable cells is collected."""
    start = [(x, y) for (t, x, y, p) in objs if t == O_START]
    exits = [(x, y) for (t, x, y, p) in objs if t in (O_EXIT, O_WELL)]
    assert len(start) == 1 and exits, 'chapter %d: needs one S and an X or w' % ch['n']
    blocked = {(x, y) for (t, x, y, p) in objs if t in BLOCKERS}
    doors = {(x, y): p for (t, x, y, p) in objs if t == O_DOOR}
    codedoors = {(x, y) for (t, x, y, p) in objs if t == O_CODEDOOR}
    keys = {(x, y): p for (t, x, y, p) in objs if t == O_KEY}
    have = set()
    while True:
        seen = {start[0]}
        todo = [start[0]]
        while todo:
            x, y = todo.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < w and 0 <= ny < h) or (nx, ny) in seen:
                    continue
                c = cells[ny * w + nx]
                if (nx, ny) in blocked:
                    continue
                if c == CELL_DOOR:
                    # The code door opens with the code from the tape.
                    if (nx, ny) not in codedoors and doors.get((nx, ny)) not in have:
                        continue
                elif c not in (CELL_FLOOR, CELL_WATER, CELL_GROUND):
                    continue
                seen.add((nx, ny))
                todo.append((nx, ny))
        got = {keys[p] for p in seen if p in keys}
        if got <= have:
            break
        have |= got
    for e in exits:
        assert e in seen, 'chapter %d: exit %s unreachable' % (ch['n'], e)
    for (t, x, y, p) in objs:
        if t in (O_TAPE, O_RADIO, O_KEY):
            assert (x, y) in seen, 'chapter %d: object %d at %d,%d unreachable' % (ch['n'], t, x, y)
    assert 3 in have or ch['n'] == 0, 'chapter %d: needs the chase key z' % ch['n']


def build(ch):
    w, h, cells, objs = parse(ch)
    check(ch, w, h, cells, objs)
    # Objects sorted by type then position: the game addresses them by index.
    objs.sort()
    out = struct.pack('<IHHHHHHHHHBB', MAGIC, 1, ch['n'], w, h, len(objs), ch['flags'],
                      ch['song'], c15(*ch['fog']), c15(*ch['clear']), ch['ambient'],
                      ch['facing'])
    out += bytes(cells)
    for o in objs:
        out += struct.pack('<BBBB', *o)
    while len(out) % 4:
        out += b'\0'
    return out


# ---------------------------------------------------------------------------
# Replay logs, read by arm9/source/core/input.c (32-byte header):
#   u32 magic 'CVRP', u32 seed, u16 run count, u16 reserved, then the start
#   state: u8 chapter, u8 RTC hour, u8 health, u8 flares, u8 radio (0xFF =
#   chapter start), u8 keys, u8 battery, u8 tapes, u32 taken lo, u32 taken hi,
#   u8 options (bit 0 touch look, bits 1-2 night visit mode, bit 3 mic), 3 pad bytes;
#   then runs of { u16 frames, u16 keys, u8 touch_x, u8 touch_y, u8 flags, u8 mic }

REPLAY_MAGIC = 0x50525643  # 'CVRP'
K_A, K_B, K_SELECT, K_START, K_RIGHT, K_LEFT, K_UP, K_DOWN, K_R, K_L, K_X, K_Y = \
    [1 << i for i in range(12)]
RF_TOUCH, RF_LID = 1, 2

REPLAY_HEADER = '<IIHHBBBBBBBBIIBBBB'


def replay_blob(seed, chapter, runs, hour=12, health=3, flares=3):
    out = struct.pack(REPLAY_HEADER, REPLAY_MAGIC, seed, len(runs), 0, chapter, hour,
                      health, flares, 0xFF, 0, 100, 0, 0, 0, 0, 0, 0, 0)
    for (frames, keys, tx, ty, fl) in runs:
        out += struct.pack('<HHBBBB', frames, keys, tx, ty, fl, 0)
    while len(out) % 4:
        out += b'\0'
    return out


def demo_replays():
    """Attract-mode logs, one per chapter (about 18 s): walk, look around,
    toggle the flashlight and the recorder, sometimes run or hush."""
    logs = []
    for n in range(len(CHAPTERS)):
        rng = Rng(7100 + n)
        runs = [(40, 0, 0, 0, 0)]
        total = 40
        while total < 1100:
            r = rng.next() % 10
            if r < 5:
                frames, k = rng.range(30, 90), K_UP | (K_B if rng.next() % 4 == 0 else 0)
            elif r < 8:
                frames, k = rng.range(10, 40), K_LEFT if rng.next() % 2 else K_RIGHT
            elif r == 8:
                frames, k = rng.range(20, 50), K_UP | K_L
            else:
                runs.append((4, K_X, 0, 0, 0))
                total += 4
                frames, k = rng.range(20, 40), 0
            runs.append((frames, k, 0, 0, 0))
            total += frames
        # Chapter 4's run is recorded at 2 a.m. so the night visitor appears.
        logs.append(replay_blob(0x4057 + n, n, runs, 2 if n == 4 else 12))
    return logs
