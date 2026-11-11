# Lightenette

Original lantern-and-marionette puzzle for the DS. It also self-checks three
emulator-accuracy cases (reversed texture coordinates, translucent OBJ with a
brightness effect, edge marking / line polygons) so other emulators can use the
ROM as a CI test item. Plan: stages 0-9 in the PR stack (each PR is one stage).
Human docs (English, each with a Korean twin `*-KO.md`): `README.md`,
`docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in
`../AGENTS.md`. Created from the corvane template: `arm7/`, `common/`, and
`arm9/source/core` and `gfx` are corvane's layers (keep fixes in sync when they apply
to both).

## Design principles

1. Playable to the end on any emulator. Accuracy differences are visible but never
   block progress (clues also shown on the bottom screen; strings carry vertex colour
   and an outline).
2. Judging happens in the ROM: display capture, count pixels of fixed RGB555 colours,
   write a result block at a fixed main-RAM address plus the same values and OK/NG on
   screen.
3. No cycle-timing-dependent items (if one is added, judge a range).
4. Judging and rehearsal modes work without NitroFS (some cart kernels cannot open it;
   see `nitro_ok()` in `core/nitro.c`).
5. No third-party trademarks in code or ROM strings (device names included).

## Build

```sh
./build.sh                  # lightenette.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=<name>      # one demo (names: Makefile DEMOS)
./build.sh demos            # all demos
./build.sh -j8 release      # build/release/{rom,demos} (tag lightenette-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and
`build/nitrofs` (cart files) on each build when `tools/` changes.

## Code map

- `arm9/source/main.c`: init, scene loop.
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset), `title.c`, `judge_scene.c` (judge and rehearsal scenes), `rehearsal.c` (act 1 from an embedded touch script, act 2 from a key script, then the judge; the result block is 32 words, `CvResult.values[29]` in `core/sys.h`; result block id `RHEL`), `act3.h`, `act3.c` (act 3: the stage is BG0 3D with BLDCNT 01C1; a semi-transparent 64x64 lantern OBJ, shared with the judge via `judge_lamp_load()`, is moved with the D-pad or a drag over three marks, found in software; after the clear it parks over empty backdrop and captures BLDCNT 0100/0140/01C1, reading the lantern pixel and a backdrop pixel; result block id `ACT3`; held-key script for the rehearsal, which fills `values[18..26]`), `act2.h`, `act2.c` (act 2: three paper dolls turned with reversed texcoords written in the vertices, never the TEXIMAGE_PARAM flip bits; the stitched third doll uses the plain 32>16 mirror; after the clear it captures its own picture and counts cut-line and edge pixels, expected 0/0/18; result block id `ACT2`; key script for the rehearsal), `act1.h`, `act1.c` (act 1: marionette rig, sliders, silhouette score;
  result block id `ACT1`).
- `arm9/source/judge/`: accuracy judge (`judge.h` documents the result block at
  `CV_RESULT_ADDR`). Needs bank D free as the capture target (`VL_JUDGE` in `gfx/vram.h`).
- `arm9/source/demo/`: `make DEMO=acc-stage` builds the judge, `DEMO=rehearsal` the rehearsal, each as its own ROM. Demo builds skip `nitro_init`, `cart_init` and the save chip.
- `expected.json` + `tools/judge.py`: expected result-block values per item and the stdlib-only grader (exit 0 = pass). Keep both in step with the C result block layout; unconfirmed items stay informational until a hardware value arrives.
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers.
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only).

## Controls (melonDS keys in brackets)

Start (Return) starts act 1: touch-drag the four sliders on the bottom screen; A (X) after a clear
goes on to act 2, B (Z) backs out. Act 2: Left/Right (arrows) or tap chooses a doll, L/R (Q/W) or a tap
on the lower half turns it; A (X) after a clear goes on to act 3. Act 3: D-pad (arrows) or drag moves the lantern over the marks; A (X) after a clear / B (Z) returns to the title. Title + Select (Backspace) opens the test menu (up/down, A (X), B (Z)); in the judge A (X) cycles BLDCNT,
B (Z) / Start returns. More actions are listed here as scenes land.
