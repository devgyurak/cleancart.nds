# Lightenette

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#build)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#build)
[![Accuracy items: 23](https://img.shields.io/badge/accuracy%20items-23-orange)](expected.json)

[한국어](README-KO.md)

<p align="center"><img src="docs/images/title.png" width="256" alt="Lightenette title screen"> <img src="docs/images/act1.png" width="256" alt="Act 1, cleared"> <img src="docs/images/act2.png" width="256" alt="Act 2, cleared"> <img src="docs/images/act3.png" width="256" alt="Act 3, cleared"> <img src="docs/images/act4.png" width="256" alt="Act 4, cleared"> <img src="docs/images/act5.png" width="256" alt="Act 5, cleared"></p>

An original puzzle-action game for the Nintendo DS: you carry a lantern and work a
marionette show with the touch screen. It is also built to judge itself: the ROM
captures its own display, counts pixels of fixed colours and reports pass/fail for
cases where emulators have disagreed with hardware, so other emulators can use it as
a regression test (see [Using it as an emulator test](#using-it-as-an-emulator-test)).

**Status.** Five acts, the accuracy judge, a rehearsal mode, `expected.json` and the
judging script are done and run headless in DeSmuME and in melonDS (software, GL and
compute renderers). **Nothing has been measured on real hardware yet**: items whose
expected value comes from the documentation (GBATEK) or a third-party report are
gating; the rest are informational until hardware values arrive. Hands-on feel in a
desktop emulator has not been reviewed either.

- Languages: C (game, ARM9 + own ARM7 core), Python 3 (build-time tools)
- Toolchain: BlocksDS v1.25.0 (Docker image pinned by digest)
- Licence: Apache-2.0 — third-party notices in `THIRD_PARTY_NOTICES`, asset sources in `ASSETS.md`

## Playing

Title: Start plays from act 1, X shows the credits and the licence viewer, Select opens
the test menu (any act, the accuracy judge, the rehearsal). Each cleared act leads to
the next.

1. **Act 1.** Drag the four touch-screen sliders to swing the paper marionette's limbs until they cover the shadow on the top screen (85% overlap held for 20 frames).
2. **Act 2.** Choose a doll (left/right or tap) and turn it with L/R until each of the three paper dolls faces the way the touch screen asks. The turn mirrors the card with reversed texture coordinates written into the vertices; the third (stitched) doll's back may show a seam.
3. **Act 3.** The stage is nearly black (3D layer with the brightness-decrease effect). Move the translucent lantern (D-pad or drag) over three hidden marks until each lights up; the bottom screen is a map.
4. **Act 4.** Four line-polygon strings are tangled: pick two pegs to swap their strings until none cross.
5. **Act 5.** The finale: light each doll with the lantern and turn it to the side the touch screen asks for, with three edge-marking colours on the plinths.

Accuracy differences are visible but never block progress: every clue is also on the
bottom screen, and strings carry vertex colour plus an outline.

## Build

Docker is all you need.

```sh
./build.sh                    # lightenette.nds
./build.sh SAVE=flash         # lightenette-save-flash.nds
./build.sh SAVE=fram          # lightenette-save-fram.nds
./build.sh demos              # lightenette-acc-stage.nds, lightenette-rehearsal.nds
./build.sh -j8 release        # build/release/{rom,demos,judge}: what a lightenette-vX.Y.Z tag ships
./build.sh clean
```

Two clean builds give the same SHA-256.

## Using it as an emulator test

The ROM decides by itself; the emulator only has to run it and expose main RAM.

1. Run `lightenette-rehearsal.nds` (boots straight into the rehearsal: act 1 from a touch script, acts 2 to 5 from key scripts, then the judge). It needs no save chip, no pen input and no NitroFS, so it works from any cart kernel. `lightenette-acc-stage.nds` runs only the static judge scene.
2. After 1100 frames read the result block at `0x0237F000` (64 words; layout in `arm9/source/game/rehearsal.h` and `arm9/source/judge/judge.h`: magic `0x52455643`, item id, frame, values). The same numbers and OK/NG are drawn on screen, so a screenshot works too.
3. Grade it: `python3 tools/judge.py <log>` (Python 3 stdlib only) compares the block with `expected.json` and exits 0 when every gating item is OK, 1 if one is NG, 2 if there was no valid block. It reads the `RESULT` line of the melonDS headless runner and of the DeSmuME harness, or a `PEEK` dump.

```sh
tools/melonds/mds.sh run lightenette/lightenette-rehearsal.nds 1100 out/mds/reh "" 1099 > reh.log   # RENDERER=gl|compute
python3 lightenette/tools/judge.py reh.log
```

`expected.json` lists every item: frame, expected value, tolerance, source, boot path and hardware
model. Sources: `GBATEK` (documented behaviour), `report:<ref>` (a third-party hardware report),
`hw:<model>` (measured by us on hardware), `rom` (the ROM's own deterministic logic),
`unconfirmed`. Items from `unconfirmed` sources only print INFO and never fail a run; when a
hardware value arrives the item moves up to gating. The file carries a `schema` number and each
release ships the `expected.json` that matches its ROM, so pin the pair (ROM + `expected.json` +
`judge.py`) from the same `lightenette-vX.Y.Z` release. The software renderer of melonDS 1.1 is the
current all-OK reference; the other renderers' known differences are listed in [docs/QA.md](docs/QA.md).

## Layout

```
arm9/source/game/   scenes: title, acts 1-5, credits and licence viewer, rehearsal
arm9/source/judge/  accuracy judge (capture, pixel counts, result block)
arm9/source/demo/   demo ROM entry (acc-stage, rehearsal)
arm9/source/core/   system, input, DMA, save, sound (corvane layers)
arm9/source/gfx/    2D/3D layers (corvane layers)
arm7/               own ARM7 core
tools/              asset generators and judge.py (Python 3, stdlib only)
expected.json       expected values for the judge
```
