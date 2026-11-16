# Paneveil

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)

[한국어](README-KO.md)

An original stained-glass workshop game for the Nintendo DS. Each order describes a rose window:
the glass, the pattern, the colour and how many petals and rings it has. Set the window to the
order and fire it in the kiln; fewer turns earn more stars. The top screen shows the window in 3D,
lit from behind. It is also built to judge itself: a rehearsal ROM checks the 3D geometry engine
(matrix stack, matrix load/multiply, geometry commands, polygon and texture attributes) against
exact expected values and reports a result block, so other emulators can use it as a regression
test (see [Using it as an emulator test](#using-it-as-an-emulator-test)).

**Hardware areas it tests (level 1, "3D geometry and texturing"):** the matrix stack (push, pop,
store, restore, the projection stack and the GXSTAT stack level), matrix load/multiply
(CLIPMTX_RESULT, VECMTX_RESULT), geometry commands (packed GXFIFO and direct ports, the six vertex
formats), polygon attributes (front/back face, alpha, wireframe, depth test) and texture attributes
(repeat, flip, clamp, colour 0, palette base, texture coordinate source).

**Status.** Chapters 1 (glass) and 2 (pattern) are playable: twelve windows, the 3D window on the top
screen, the settings on the bottom screen, a 2D schematic mode (Select), the kiln lamp, the rehearsal and its judge
(kiln check and light box: 30 items) and the hidden auto QA run. Chapters 3 to 5 are still to come. Nothing has been
measured on real hardware.

- Languages: C (game, ARM9 + own ARM7 core), Python 3 (build-time tools)
- Toolchain: BlocksDS v1.25.0 (Docker image pinned by digest)
- Licence: Apache-2.0 — third-party notices in `THIRD_PARTY_NOTICES`, asset sources in `ASSETS.md`

## Build

```sh
./build.sh                  # paneveil.nds
./build.sh DEMO=rehearsal   # paneveil-rehearsal.nds, the ROM that judge.py grades
./build.sh -j8 release      # build/release/{rom,demos,judge}
```

The build runs in a Docker container (BlocksDS pinned by digest); nothing is installed on the host.
Two clean builds give the same SHA-256.

## Using it as an emulator test

`paneveil-rehearsal.nds` (`./build.sh DEMO=rehearsal`) runs the judge and leaves a 64-word result block
at 0x0237F000 (layout in `arm9/source/game/rehearsal.h`); `tools/judge.py` grades it against
`expected.json` (target level 1, area "3D geometry and texturing"). No save chip and no NitroFS are needed.

The **kiln check** sends fixed command sequences to the geometry engine (direct ports and packed GXFIFO),
waits for GXSTAT bit 27 to clear, and compares GXSTAT stack levels, `CLIPMTX_RESULT` (16 words) and
`VECMTX_RESULT` (9 words) with an integer model of GBATEK's rules. Every matrix element is exact in 1.19.12, so the
answer does not depend on how a product is rounded, and no renderer is involved.

```sh
tools/melonds/mds.sh run games/paneveil/paneveil-rehearsal.nds 400 out/pv "" 399 \
  | python3 games/paneveil/tools/judge.py --profile melonds-1.1-software
RESULT_WORDS=64 tools/emu/emu.sh run games/paneveil/paneveil-rehearsal.nds 400 out/pv "" 399 \
  | python3 games/paneveil/tools/judge.py --profile desmume-pydesmume-0.0.9
```

Exit code 0: every gating item passed (or the failing set equals the profile's). `known` in `expected.json`
holds one profile per emulator version and renderer.
