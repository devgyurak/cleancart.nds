#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Write the project's keyboard mapping into melonDS's config (macOS).

    set_keys.py [--gdb on|off] [CONFIG]

melonDS rewrites its config when it exits, so this refuses to run while
melonDS is open. A backup is written next to the file (.bak).

Mapping (matches melonDS's own defaults):
  DS A/B/X/Y = X/Z/S/A, L/R = Q/W, Start = Return, Select = Backspace,
  D-pad = arrow keys, lid = C, pause = P, fast-forward = Tab (hold),
  frame step = N.
On macOS Qt reports arrow keys with KeypadModifier (0x20000000) set, and
melonDS compares the key value *including* modifiers, so the arrow entries
must carry that flag or they never match.
"""

import os
import re
import shutil
import subprocess
import sys

KEYPAD = 0x20000000
KEYS = {
    'A': 0x58, 'B': 0x5A, 'X': 0x53, 'Y': 0x41,          # X Z S A
    'L': 0x51, 'R': 0x57,                               # Q W
    'Start': 0x01000004, 'Select': 0x01000003,          # Return, Backspace
    'Left': 0x01000012 | KEYPAD, 'Up': 0x01000013 | KEYPAD,
    'Right': 0x01000014 | KEYPAD, 'Down': 0x01000015 | KEYPAD,
    'HK_Lid': 0x43, 'HK_Pause': 0x50,                   # C, P
    'HK_FastForward': 0x01000001, 'HK_FrameStep': 0x4E,  # Tab, N
}
DEFAULT = os.path.expanduser('~/Library/Preferences/melonDS/melonDS.toml')


def section(s, name):
    i = s.index('[%s]' % name)
    j = s.find('\n[', i + 1)
    return i, (len(s) if j < 0 else j)


def main(argv):
    gdb = None
    args = argv[1:]
    if args[:1] == ['--gdb']:
        gdb, args = args[1], args[2:]
    path = args[0] if args else DEFAULT
    if path == DEFAULT and subprocess.run(['pgrep', '-f', 'melonDS.app/Contents/MacOS'],
                                          capture_output=True).returncode == 0:
        sys.exit('melonDS is running; quit it first (it rewrites the config on exit)')
    s = open(path).read()
    shutil.copyfile(path, path + '.bak')
    i, j = section(s, 'Instance0.Keyboard')
    block = s[i:j]
    for k, v in KEYS.items():
        block, n = re.subn(r'(?m)^%s = -?\d+$' % re.escape(k), '%s = %d' % (k, v), block)
        if n != 1:
            sys.exit('key %s not found in [Instance0.Keyboard]' % k)
    s = s[:i] + block + s[j:]
    if gdb:
        i, j = section(s, 'Instance0.Gdb')
        want = 'true' if gdb == 'on' else 'false'
        s = s[:i] + re.sub(r'Enabled = \w+', 'Enabled = ' + want, s[i:j], count=1) + s[j:]
    open(path, 'w').write(s)
    print('updated', path)


if __name__ == '__main__':
    main(sys.argv)
