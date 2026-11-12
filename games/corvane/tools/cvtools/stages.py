# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Stage layouts: terrain heightmaps and enemy spawn scripts.

Binary format (little endian), consumed by arm9/source/game/stage_data.c:

  0  u32  magic 'CVST'
  4  u16  version (1)
  6  u16  stage number (1-5)
  8  u16  rows (terrain rows, 1 world unit apart along -Z)
 10  u16  cols (vertices per row, 17)
 12  u16  spawn count
 14  u16  flags (bit0 terrain, bit1 corridor, bit2 space, bit3 sea)
 16  u16  boss type
 18  u16  song id
 20  u16  fog colour (BGR555)
 22  u16  clear colour (BGR555)
 24  u8[rows*cols] heights (1/32 world unit) or corridor half-width
 ..  spawns: 8 bytes each
        u16 row, u8 type, s8 x (1/4 unit), s8 y (1/4 unit),
        u8 pattern, u8 count, u8 param
"""

import json
import math
import struct

from .gfx import Rng, fbm, rgb15

MAGIC = 0x54535643  # 'CVST'
COLS = 17

FLAG_TERRAIN, FLAG_CORRIDOR, FLAG_SPACE, FLAG_SEA = 1, 2, 4, 8

# Enemy types (arm9/source/game/enemy.h)
E_TURRET, E_DRONE, E_AIRSHIP, E_METEOR, E_STATION, E_WALLGUN, E_FIGHTER, E_BOSS = range(1, 9)
TYPE_NAMES = {E_TURRET: 'turret', E_DRONE: 'drone', E_AIRSHIP: 'airship',
              E_METEOR: 'meteor', E_STATION: 'station', E_WALLGUN: 'wallgun',
              E_FIGHTER: 'fighter', E_BOSS: 'boss'}
NAME_TYPES = {v: k for k, v in TYPE_NAMES.items()}

# Bullet patterns
P_AIMED, P_SPREAD3, P_RING, P_SPIRAL, P_NONE, P_RAIN = range(6)

STAGES = [
    dict(n=1, rows=180, flags=FLAG_TERRAIN | FLAG_SEA, song=2, boss=1,
         fog=(170, 200, 230), clear=(140, 190, 240)),
    dict(n=2, rows=190, flags=0, song=3, boss=2,
         fog=(200, 220, 255), clear=(90, 140, 230)),
    dict(n=3, rows=170, flags=FLAG_SPACE, song=4, boss=3,
         fog=(0, 0, 0), clear=(0, 0, 0)),
    dict(n=4, rows=200, flags=FLAG_CORRIDOR, song=5, boss=4,
         fog=(20, 10, 10), clear=(10, 5, 5)),
    dict(n=5, rows=80, flags=FLAG_SPACE, song=6, boss=5,
         fog=(40, 0, 20), clear=(16, 0, 10)),
]


def heights_for(st):
    rows = st['rows']
    rng = Rng(st['n'] * 1000 + 7)
    h = []
    if st['flags'] & FLAG_TERRAIN:
        for r in range(rows):
            for c in range(COLS):
                x = c - 8
                n = fbm(c / 5.0, r / 7.0, 300 + st['n'], 4)
                # A shallow valley the player flies along, ridges at the sides.
                valley = min(1.0, (abs(x) / 8.0) ** 1.6)
                coast = max(0.0, 1.0 - r / 25.0)  # starts at the shoreline
                v = (0.2 + n * 0.6 + valley * 1.6) * (1 - coast * 0.8)
                h.append(max(0, min(255, int(v * 32))))
    elif st['flags'] & FLAG_CORRIDOR:
        for r in range(rows):
            hub = (r % 40) >= 30
            width = 6.5 if hub else 3.5 + 0.5 * math.sin(r / 6)
            for c in range(COLS):
                h.append(int(width * 32))
    else:
        h = [0] * (rows * COLS)
    return h


def spawns_for(st):
    rng = Rng(st['n'] * 77 + 1)
    rows = st['rows']
    sp = []
    n = st['n']
    def add(row, t, x, y, pat=P_AIMED, count=1, param=0):
        sp.append((row, t, int(x * 4), int(y * 4), pat, count, param))
    if n == 1:
        for r in range(14, rows - 20, 7):
            add(r, E_TURRET, rng.range(-5, 6), 0, P_AIMED if r < 80 else P_SPREAD3)
            if r % 21 == 0:
                add(r + 2, E_DRONE, rng.range(-4, 5), 3, P_SPREAD3, 3, 20)
    elif n == 2:
        for r in range(12, rows - 20, 9):
            add(r, E_DRONE, rng.range(-5, 6), rng.range(2, 5), P_AIMED, 3, 16)
            if r % 27 == 3:
                add(r + 4, E_AIRSHIP, rng.range(-3, 4), 4, P_RING, 1, 0)
            if r % 18 == 12:
                add(r, E_FIGHTER, rng.range(-4, 5), 3, P_AIMED, 2, 12)
    elif n == 3:
        for r in range(10, rows - 20, 4):
            add(r, E_METEOR, rng.range(-6, 7), rng.range(1, 6), P_NONE, 1, rng.range(0, 4))
            if r % 16 == 2:
                add(r + 1, E_STATION, rng.range(-4, 5), 3, P_SPIRAL, 1, 0)
    elif n == 4:
        for r in range(8, rows - 20, 5):
            side = -1 if (r // 5) % 2 else 1
            add(r, E_WALLGUN, side * 3, rng.range(1, 4), P_AIMED if r < 100 else P_SPREAD3)
            if r % 40 == 32:
                add(r, E_FIGHTER, 0, 3, P_RING, 3, 10)
    else:
        for r in range(6, 40, 6):
            add(r, E_DRONE, rng.range(-5, 6), rng.range(2, 5), P_RAIN, 2, 14)
    # Boss at the end of the run.
    add(rows - 12, E_BOSS, 0, 3, P_NONE, 1, st['boss'])
    sp.sort()
    return sp


def build(st):
    h = heights_for(st)
    sp = spawns_for(st)
    hdr = struct.pack('<IHHHHHHHHHH', MAGIC, 1, st['n'], st['rows'], COLS,
                      len(sp), st['flags'], st['boss'], st['song'],
                      rgb15(*st['fog']), rgb15(*st['clear']))
    body = bytes(h)
    spb = b''.join(struct.pack('<HBbbBBB', *s) for s in sp)
    blob = hdr + body + spb
    while len(blob) % 4:
        blob += b'\0'
    return blob


def to_json(blob):
    """Decode a stage blob into an editable JSON document (stage editor)."""
    (magic, ver, n, rows, cols, nsp, flags, boss, song, fog, clear) = \
        struct.unpack_from('<IHHHHHHHHHH', blob, 0)
    assert magic == MAGIC
    off = 24
    heights = list(blob[off:off + rows * cols])
    off += rows * cols
    spawns = []
    for i in range(nsp):
        row, t, x, y, pat, cnt, param = struct.unpack_from('<HBbbBBB', blob, off + i * 8)
        spawns.append(dict(row=row, type=TYPE_NAMES.get(t, t), x=x / 4, y=y / 4,
                           pattern=pat, count=cnt, param=param))
    return dict(stage=n, rows=rows, cols=cols, flags=flags, boss=boss, song=song,
                fog=fog, clear=clear, heights=heights, spawns=spawns)


def from_json(doc):
    sp = sorted((s['row'], NAME_TYPES.get(s['type'], s['type']), int(s['x'] * 4),
                 int(s['y'] * 4), s['pattern'], s['count'], s['param'])
                for s in doc['spawns'])
    hdr = struct.pack('<IHHHHHHHHHH', MAGIC, 1, doc['stage'], doc['rows'],
                      doc['cols'], len(sp), doc['flags'], doc['boss'],
                      doc['song'], doc['fog'], doc['clear'])
    blob = hdr + bytes(doc['heights']) + b''.join(struct.pack('<HBbbBBB', *s) for s in sp)
    while len(blob) % 4:
        blob += b'\0'
    return blob


# ---------------------------------------------------------------------------
# Replay logs. Format consumed by arm9/source/core/replay.c:
#   u32 magic 'CVRP', u32 seed, u16 count, u8 stage, u8 rtc hour,
#   u8 lives, u8 bombs, u16 reserved, then runs of
#   { u16 frames, u16 keys, u8 touch_x, u8 touch_y, u8 flags, u8 pad }

REPLAY_MAGIC = 0x50525643  # 'CVRP'
K_A, K_B, K_SELECT, K_START, K_RIGHT, K_LEFT, K_UP, K_DOWN, K_R, K_L, K_X, K_Y = \
    [1 << i for i in range(12)]
RF_TOUCH, RF_LID = 1, 2


REPLAY_HEADER = '<IIHBBBBH'


def replay_blob(seed, stage, runs, hour=12, lives=3, bombs=2):
    out = struct.pack(REPLAY_HEADER, REPLAY_MAGIC, seed, len(runs), stage, hour,
                      lives, bombs, 0)
    for (frames, keys, tx, ty, fl) in runs:
        out += struct.pack('<HHBBBB', frames, keys, tx, ty, fl, 0)
    while len(out) % 4:
        out += b'\0'
    return out


def demo_replays():
    """Attract-mode logs: one per stage, about 20 s each, weaving and firing."""
    logs = []
    for n in range(1, 6):
        rng = Rng(9000 + n)
        runs = []
        total = 0
        while total < 1200:
            frames = rng.range(20, 70)
            k = K_A
            r = rng.next() % 6
            k |= (K_LEFT, K_RIGHT, K_UP, K_DOWN, 0, K_LEFT | K_UP)[r]
            if rng.next() % 9 == 0:
                k |= K_B
            if n == 2 and rng.next() % 5 == 0:
                runs.append((2, 0, 128 + rng.range(-40, 40), 96 + rng.range(-40, 40), RF_TOUCH))
            if n == 4 and rng.next() % 3 == 0:
                runs.append((frames, K_A, rng.range(40, 216), rng.range(40, 150), RF_TOUCH))
                total += frames
                continue
            runs.append((frames, k, 0, 0, 0))
            total += frames
        # Stage 2's attract run is recorded at night to show the searchlights.
        logs.append(replay_blob(0x1234 + n, n, runs, 22 if n == 2 else 12))
    return logs
