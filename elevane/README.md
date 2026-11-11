# Elevane

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)

[한국어](README-KO.md)

An original voxel silhouette puzzle for the Nintendo DS: stack blocks until the
front and plan views match the order. Both screens show the model in 3D.

**Status.** Skeleton only: an empty title screen, credits with the licence viewer
and the hidden auto QA run. The puzzle tools, chapters, showcase mode and the
accuracy rehearsal come in later changes. Nothing is measured on real hardware yet.

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
Auto QA run: on the title hold L+R+X+Y for 120 frames (see [docs/QA.md](docs/QA.md)).
