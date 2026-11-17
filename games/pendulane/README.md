# Pendulane

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)

[한국어](README-KO.md)

An original automaton-workshop game for the Nintendo DS (work in progress). It is built to judge itself: a rehearsal ROM measures
CPU, bus and sound timings on the console (busy times of the divide and square-root unit now; SPI, ARM7 memory, cartridge gap and
sound start timing follow) and reports a result block, so other emulators can use it as a regression test.

**Hardware areas it tests (level 3, "CPU, bus and sound timing"):** so far the busy times of the hardware divider (modes 0, 1, 2) and
the square-root unit, from GBATEK "DS Maths". Every value is the difference of two edges found with a nop sled in ITCM, so the fixed
cost of the measurement cancels.

**Status.** Skeleton and the math bench. The device game is not written yet; the title screen is a scaffold. Nothing has been
measured on real hardware.

- Languages: C (game, ARM9 + own ARM7 core), Python 3 (build-time tools)
- Toolchain: BlocksDS v1.25.0 (Docker image pinned by digest)
- Licence: Apache-2.0 — third-party notices in `THIRD_PARTY_NOTICES`, asset sources in `ASSETS.md`

## Build

```sh
./build.sh                  # pendulane.nds
./build.sh DEMO=rehearsal   # the rehearsal ROM
./build.sh clean
```

## Using it as an emulator test

Boot `pendulane-rehearsal.nds`, run about 60 frames, and read the 64-word result block at `0x0237F000` (id `PDRH`). Grade it:

```sh
RESULT_WORDS=64 tools/emu/emu.sh run games/pendulane/pendulane-rehearsal.nds 300 out/x "" 299 \
  | python3 games/pendulane/tools/judge.py --profile desmume-pydesmume-0.0.9
tools/melonds/mds.sh run games/pendulane/pendulane-rehearsal.nds 300 out/x "" 299 \
  | python3 games/pendulane/tools/judge.py --profile melonds-1.1-software     # RENDERER=gl, compute
```

`judge.py` exits 0 when every gating item is OK (or the profile matches). Items are listed in `expected.json` with their GBATEK section
and difficulty level (see `docs/LEVELS.md` in the repository root).

## Controls

Title: Start opens the bench ledger (a live look at the measurement), X the credits. See the repository `AGENTS.md` for the melonDS key map.
