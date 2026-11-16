# Tonewick

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)

[한국어](README-KO.md)

An original listening puzzle for the Nintendo DS. You are the signal keeper of a listening post on a moor,
mending twelve broken signal lines in four districts with an oscilloscope. Each line is a register field of the
console's sound block, and the boundary values are the intended solutions: a square wave whose duty is 7 never
leaves the floor, a tape shorter than four words keeps its line busy and says nothing, a capture length of 0 writes
one word. The ROM listens to its own sound block with the capture units, so the game also judges itself: a rehearsal
ROM runs the exceptions, boundary values and documented bugs of the sound channels, the capture units and the
neighbouring devices (RTC, power management, save chip, firmware settings) against exact expected values and reports
a result block, so other emulators can use it as a regression test.

**Hardware areas it tests (level 2, "sound and I/O edges"):** the edges of the sound channels (duty 7, the divider,
volume register 127, tapes shorter than four words, the hold bit, the ADPCM loop reload, the noise shift register),
the sound capture units (length 0, the channel source, the add overflow and both-negative bugs) and the I/O devices
next to them (the RTC AM/PM flag in 24 hour mode, power management register mirrors, EEPROM status bits, the newest
firmware user-settings copy).

**Status.** Scaffold: the ARM7 sound lab (a register script the ARM9 hands over, with the capture buffer in main RAM),
the title, test menu and credits, the QA run through four empty districts. The experiments, the judge and the
twelve lines follow in the next changes. Progress is not saved.

- Languages: C (game, ARM9 + own ARM7 core), Python 3 (build-time tools)
- Toolchain: BlocksDS v1.25.0 (Docker image pinned by digest)
- Licence: Apache-2.0 — third-party notices in `THIRD_PARTY_NOTICES`, asset sources in `ASSETS.md`

## Build

```sh
./build.sh                  # tonewick.nds
./build.sh DEMO=rehearsal   # tonewick-rehearsal.nds, the ROM that judge.py grades
./build.sh -j8 release      # build/release/{rom,demos,judge}
```

The build runs in a Docker container (BlocksDS pinned by digest); nothing is installed on the host.
Two clean builds give the same SHA-256.
