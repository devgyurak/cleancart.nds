#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Corvane input-log (replay) tool.

  replay.py dump FILE                 print a .rpl log, or the log stored in a
                                      .sav (EEPROM/FLASH/FRAM area is found
                                      automatically)
  replay.py extract FILE.sav OUT.rpl  pull the last run out of a save file
  replay.py make OUT.rpl --stage N [--seed S] [--hour H] RUN...
                                      write a log; RUN = frames:KEYS[@x,y]
                                      KEYS = '+'-joined names (A+LEFT) or -

Log format (little endian): u32 'CVRP', u32 seed, u16 run count, u8 stage,
u8 RTC hour, u8 lives, u8 bombs, u16 reserved, then runs of
{u16 frames, u16 keys, u8 touch x, u8 touch y, u8 flags, u8 pad}.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from cvtools import stages  # noqa: E402

KEYS = ['A', 'B', 'SELECT', 'START', 'RIGHT', 'LEFT', 'UP', 'DOWN', 'R', 'L', 'X', 'Y']
HDR = stages.REPLAY_HEADER
HDR_SIZE = struct.calcsize(HDR)
SAVE_AREAS = [0x8000, 0x20000, 0x0800]   # EEPROM, FLASH, FRAM


def keys_to_str(k):
    names = [n for i, n in enumerate(KEYS) if k & (1 << i)]
    return '+'.join(names) if names else '-'


def parse(blob):
    magic, seed, count, stage, hour, lives, bombs, _ = struct.unpack_from(HDR, blob, 0)
    if magic != stages.REPLAY_MAGIC:
        raise ValueError('not a Corvane input log')
    runs = []
    for i in range(count):
        runs.append(struct.unpack_from('<HHBBBB', blob, HDR_SIZE + 8 * i)[:5])
    return dict(seed=seed, stage=stage, hour=hour, lives=lives, bombs=bombs, runs=runs)


def from_save(data):
    for at in SAVE_AREAS:
        if at + 4 + HDR_SIZE > len(data):
            continue
        size = struct.unpack_from('<I', data, at)[0]
        blob = data[at + 4:at + 4 + size]
        if 0 < size <= 30000 and blob[:4] == struct.pack('<I', stages.REPLAY_MAGIC):
            return blob
    raise ValueError('no input log in this save file')


def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] == struct.pack('<I', stages.REPLAY_MAGIC):
        return data
    return from_save(data)


def cmd_dump(path):
    log = parse(load(path))
    total = sum(r[0] for r in log['runs'])
    print('stage %d  seed %08x  rtc hour %d  lives %d  bombs %d  %d runs, %d frames (%.1f s)'
          % (log['stage'], log['seed'], log['hour'], log['lives'], log['bombs'],
             len(log['runs']), total, total / 59.83))
    frame = 0
    for frames, keys, tx, ty, flags in log['runs']:
        touch = ' touch %d,%d' % (tx, ty) if flags & stages.RF_TOUCH else ''
        lid = ' lid closed' if flags & stages.RF_LID else ''
        print('%6d +%5d  %-20s%s%s' % (frame, frames, keys_to_str(keys), touch, lid))
        frame += frames


def cmd_make(out, args):
    stage, seed, hour, runs = 1, 0x1234, 12, []
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--stage':
            stage = int(args[i + 1]); i += 2; continue
        if a == '--seed':
            seed = int(args[i + 1], 0); i += 2; continue
        if a == '--hour':
            hour = int(args[i + 1]); i += 2; continue
        frames, rest = a.split(':', 1)
        touch = None
        if '@' in rest:
            rest, pos = rest.split('@')
            touch = tuple(int(v) for v in pos.split(','))
        keys = 0
        if rest != '-':
            for name in rest.split('+'):
                keys |= 1 << KEYS.index(name.upper())
        if touch:
            runs.append((int(frames), keys, touch[0], touch[1], stages.RF_TOUCH))
        else:
            runs.append((int(frames), keys, 0, 0, 0))
        i += 1
    with open(out, 'wb') as f:
        f.write(stages.replay_blob(seed, stage, runs, hour))
    print('wrote %s: %d runs' % (out, len(runs)))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    if argv[1] == 'dump':
        cmd_dump(argv[2])
    elif argv[1] == 'extract':
        with open(argv[3], 'wb') as f:
            f.write(load(argv[2]))
        print('wrote', argv[3])
    elif argv[1] == 'make':
        cmd_make(argv[2], argv[3:])
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
