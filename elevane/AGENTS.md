# Elevane

Original voxel silhouette puzzle for the DS: stack blocks until the front and plan
views match the order. Both screens show the model in 3D; a 2D grid shows the same
state so the puzzles can be solved without 3D. Human docs (English, each with a Korean
twin `*-KO.md`): `README.md`, `docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`.
Repo-wide rules are in `../AGENTS.md`. Created from lightenette (which came from the
corvane template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are corvane's
layers (keep fixes in sync when they apply to all games).

Status: skeleton. The scope for v0.1.0 is five chapters with eight puzzles each,
a showcase mode (two presets) and the rehearsal/judge ROMs; those come in later changes.

## Design principles

1. Playable to the end on any emulator: the 2D grid alone solves every puzzle.
2. Judging happens in the ROM (display capture, fixed colours, a result block at a
   fixed main-RAM address), like lightenette. `tools/judge.py` and `expected.json`
   arrive with the rehearsal stage.
3. Judging and rehearsal modes work without NitroFS (see `nitro_ok()` in `core/nitro.c`).
4. No third-party trademarks in code or ROM strings.

## Build

```sh
./build.sh                  # elevane.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=<name>      # one demo (names: Makefile DEMOS; none yet)
./build.sh -j8 release      # build/release/{rom,demos} (tag elevane-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and
`build/nitrofs` (cart files) on each build when `tools/` changes.

## Code map

- `arm9/source/main.c`: init, scene loop.
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset), `title.c`, `credits.c`
  (credits and the licence viewer; NitroFS needed only here), `qa_game.c` + `qa_tune.h`.
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them
  identical to corvane's/lightenette's). Title: hold **L+R+X+Y for 120 frames** (press L and R
  before X). The run is title → credits → title → `RESULT PASS`.
- `arm9/source/judge/capture.*`: every display capture goes through `cap_start()` and
  `cap_done()` (bank D is the capture target).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers.
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only).

## Controls (melonDS keys in brackets)

Title: X (X) opens the credits; in the credits X (X) opens the licence viewer, B (Z) goes back.
Y (A) is free in this game and is meant for choosing the work layer in chapter 4 (Y + D-pad up/down).
More actions are listed here as scenes land.
