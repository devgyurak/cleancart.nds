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

**Status.** Scaffold: title, window list, credits with the licence viewer and the hidden auto QA
run. The windows, the play scene and the judge are still to come. Nothing has been measured on real
hardware.

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

The judge arrives with the judge changes; see `docs/QA.md`.
