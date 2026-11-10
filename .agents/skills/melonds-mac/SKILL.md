---
name: melonds-mac
description: Install, configure and launch melonDS on the user's Mac, set the project's keyboard mapping, and verify from the terminal that keys actually reach the DS (GDB stub + injected key events). Use when the user wants to play/QA in melonDS or reports that input doesn't work there.
---

# melonDS on macOS

## Install

The Homebrew cask is disabled (fails Gatekeeper). Use the official release:

```sh
curl -sSL -o /tmp/melonDS.zip https://github.com/melonDS-emu/melonDS/releases/download/1.1/melonDS-1.1-macOS-universal.zip
unzip -q -o /tmp/melonDS.zip -d /tmp/melonDS && cp -R /tmp/melonDS/melonDS.app ~/Applications/
open -a ~/Applications/melonDS.app <game>/<game>.nds
```

It is ad-hoc signed, not notarized; tell the user. No BIOS files are needed
(built-in FreeBIOS, direct boot). Saves go next to the ROM as `.sav` (ignored by git).

## Keyboard mapping

Config: `~/Library/Preferences/melonDS/melonDS.toml`. On this machine it started
with every `[Instance0.Keyboard]` entry `= -1` (no keys worked).

```sh
pkill -9 -f melonDS.app/Contents/MacOS   # melonDS rewrites the toml when it quits normally
python3 tools/melonds/set_keys.py        # writes the mapping in ../../AGENTS.md
open -a ~/Applications/melonDS.app <game>/<game>.nds
```

Facts behind the script: values are Qt key codes; **arrow keys need
`| 0x20000000`** (Qt sets KeypadModifier on macOS arrows and melonDS compares
modifiers too). If the user's Korean input source swallows letter keys, suggest
switching to ABC while playing. Remind them to click the window for focus.

## Verify input without seeing the screen

The terminal can't screenshot (no Screen Recording permission) and `osascript`
System Events hangs on an Automation prompt; it *does* have Accessibility trust.

```sh
pkill -9 -f melonDS.app/Contents/MacOS
python3 tools/melonds/set_keys.py --gdb on
open -a ~/Applications/melonDS.app <game>/<game>.nds; sleep 8
swiftc tools/melonds/keypid.swift -o /tmp/keypid
P=$(pgrep -f melonDS.app/Contents/MacOS)
/tmp/keypid $P 7 down; sleep 1.2
python3 tools/melonds/gdbread.py 04000130:2   # REG_KEYINPUT, bit clear = pressed
/tmp/keypid $P 7 up
```

- DS A/B/L/R/Start/Select/D-pad show in `REG_KEYINPUT` (0x04000130, little endian).
  X/Y come through the ARM7: read the game's ARM7 packet copy (`cv_arm7.keyxy`
  address from `nm`; bit0 X, bit1 Y, clear = pressed).
- Injected events to a background app are lossy: repeat each key several
  times; one clean press/release is the evidence. Map a key to two buttons only
  as a diagnostic, never as proof for one button.
- Keep GDB reads sparse (each one pauses emulation; too many backed up the ARM7
  FIFO once). Check that packets still flow (`cv_arm7_packets` increments).
- Ask the user before killing a melonDS session they might be playing.
- Afterwards: `pkill -9`, `python3 tools/melonds/set_keys.py --gdb off`, relaunch,
  and confirm `lsof -iTCP -sTCP:LISTEN -n -P | grep melon` shows nothing (the
  stub listens on all interfaces).
