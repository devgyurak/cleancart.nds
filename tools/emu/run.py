# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Run a ROM headless in DeSmuME, inject input, save screenshots.

    run.py ROM FRAMES OUTPREFIX [EVENTS] [SHOT_FRAMES]

EVENTS: comma list of FRAME:KEY[:HOLD] (KEY = A B X Y L R START SELECT UP
DOWN LEFT RIGHT LID) or FRAME:Tx/y[:HOLD] for a touch. Do not press
anything before frame ~30: a key held at power-on freezes DeSmuME's boot.
SHOT_FRAMES: comma list of frames to screenshot (OUTPREFIX_NNNNN.png);
OUTPREFIX_final.png is always written.

Environment:
  SAVETYPE  DeSmuME save type (3 EEPROM 64 KB default, 4 FRAM, 5 FLASH)
  RESULT_WORDS  words of the result block to print (default 8, 16 for the whole block)
  POKE      ADDR:VALUE (hex addr) written as a u32 every 30 frames,
            e.g. to keep a lives counter up during long runs

Prints the demo result block at 0x0237F000 and the CPU PCs; resolve a stuck
PC with arm-none-eabi-addr2line -i -f -e build/<variant>/arm9.elf <pc>.
"""

import os
import sys

from desmume.controls import Keys, keymask
from desmume.emulator import DeSmuME

RESULT_ADDR = 0x0237F000


def main(argv):
    rom, frames, out = argv[1], int(argv[2]), argv[3]
    script = argv[4] if len(argv) > 4 else ''
    shots = {int(x) for x in argv[5].split(',')} if len(argv) > 5 and argv[5] else set()
    events = []
    for item in filter(None, script.split(',')):
        p = item.split(':')
        events.append((int(p[0]), p[1], int(p[2]) if len(p) > 2 else 4))

    e = DeSmuME()
    e.volume_set(0)
    e.set_savetype(int(os.environ.get('SAVETYPE', '3')))  # before open()
    e.open(rom)
    poke = os.environ.get('POKE')
    held = {}
    for f in range(frames):
        for (ef, key, hold) in events:
            if ef != f:
                continue
            if key.startswith('T'):
                x, y = key[1:].split('/')
                e.input.touch_set_pos(int(x), int(y))
                held['touch'] = f + hold
            else:
                e.input.keypad_add_key(keymask(getattr(Keys, 'KEY_' + key)))
                held[key] = f + hold
        for k, until in list(held.items()):
            if f == until:
                if k == 'touch':
                    e.input.touch_release()
                else:
                    e.input.keypad_rm_key(keymask(getattr(Keys, 'KEY_' + k)))
                del held[k]
        if poke and f % 30 == 0:
            a, v = poke.split(':')
            e.memory.write_long(int(a, 16), int(v))
        e.cycle(with_joystick=False)
        if f in shots:
            e.screenshot().save('%s_%05d.png' % (out, f))
    e.screenshot().save('%s_final.png' % out)
    m = e.memory.unsigned
    words = [m.read_long(RESULT_ADDR + 4 * i) for i in range(int(os.environ.get('RESULT_WORDS', '8')))]
    print('RESULT', ' '.join('%08x' % w for w in words))
    print('PC9 %x PC7 %x' % (e.memory.register_arm9.pc, e.memory.register_arm7.pc))


if __name__ == '__main__':
    main(sys.argv)
