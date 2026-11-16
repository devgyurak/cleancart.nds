# Ledgerine

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)

[한국어](README-KO.md)

An original machine-room ledger puzzle for the Nintendo DS. You are the night clerk of an old counting
house that stands in two towers joined by a pneumatic tube (the two screens). Each room holds a machine
with dials and a lever; pull the lever and a real hardware device of the console runs, and its answer
is written into the ledger. The boundary values are the intended solutions: seven coins shared among
no clerks come back as seven, an empty tube returns its last capsule as an echo. It is also built to
judge itself: a rehearsal ROM runs the exceptions, boundary values and error paths of the system
hardware (maths unit, FIFO, timers, DMA, WRAM and VRAM control, BIOS) against exact expected values and
reports a result block, so other emulators can use it as a regression test.

**Hardware areas it tests (level 2, "system edges"):** the exceptions and edges of the DS maths unit
(divide by zero, overflow, the reserved mode, 64-bit root), the IPC FIFO (empty reads, error bit, full,
disabled, edge-triggered IRQ), timers (cascade, reload, frozen reads), DMA (repeat reload rules, register
state after a transfer, ITCM code during DMA), WRAM and VRAM control (half swaps, mirrors, byte writes,
bank offsets) and BIOS (CpuFastSet, LZ77, BitUnPack, RLUnComp, the ARM7 CpuSet guard).

**Status.** Scaffold: title, test menu, credits with the licence viewer, a room scene stub, a rehearsal
stub and the hidden auto QA run through six empty rooms. The six rooms and their judge arrive in the
following changes. Nothing has been measured on real hardware.

- Languages: C (game, ARM9 + own ARM7 core), Python 3 (build-time tools)
- Toolchain: BlocksDS v1.25.0 (Docker image pinned by digest)
- Licence: Apache-2.0 — third-party notices in `THIRD_PARTY_NOTICES`, asset sources in `ASSETS.md`

## Build

```sh
./build.sh                  # ledgerine.nds
./build.sh DEMO=rehearsal   # the rehearsal ROM
./build.sh demos            # every demo ROM
./build.sh -j8 release      # build/release/{rom,demos,judge}
```

Everything builds in the pinned BlocksDS container; two clean builds give the same SHA-256.
Docs: [QA checklist](docs/QA.md) · [Assets](ASSETS.md).
