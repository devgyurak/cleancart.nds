# Elevane

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)

[한국어](README-KO.md)

An original voxel silhouette puzzle for the Nintendo DS: stack blocks until the
front and plan views match the order. Both screens show the model in 3D.

**Status.** All five chapters (40 puzzles) are playable with both screens in 3D, a 2D grid mode, a showcase mode, the
accuracy rehearsal, the credits with the licence viewer and the hidden auto QA run, which plays the whole game. Nothing is measured on real hardware yet.

- Languages: C (game, ARM9 + own ARM7 core), Python 3 (build-time tools)
- Toolchain: BlocksDS v1.25.0 (Docker image pinned by digest)
- Licence: Apache-2.0 — third-party notices in `THIRD_PARTY_NOTICES`, asset sources in `ASSETS.md`

## Build

```sh
./build.sh                  # elevane.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh -j8 release      # build/release/{rom,demos} (tag elevane-vX.Y.Z runs this in CI)
./build.sh clean
```

Everything builds in the pinned Docker image; two clean builds give the same SHA-256.
Play: Start on the title, choose an order, build with the D-pad and A/B or by touching the bottom screen;
Select switches to the 2D grid, X opens the showcase mode (the work drawn bigger than one 3D frame holds). Auto QA run: on the title hold L+R+X+Y for 120 frames (see [docs/QA.md](docs/QA.md)).
