# Paneveil

Original stained-glass workshop game for the DS: each order names the glass, pattern and
rosette of a window; set the twelve settings to match the order and fire it in the kiln.
The top screen shows the window in 3D, lit from behind, and a 2D schematic mode shows the same
state as text so every order can be solved without 3D. Human docs (English, each with a Korean
twin `*-KO.md`): `README.md`, `docs/QA.md`, `docs/HW-QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`.
Repo-wide rules are in `../../AGENTS.md`. Created from elevane (which came from lightenette and the
corvane template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are corvane's layers (keep fixes
in sync when they apply to all games).

Status: all five chapters (30 windows), play scene, kiln check, light box, QA mode with the judge epilogue; v0.1.0 is next. Target level 1, area "3D geometry and texturing"
(GBATEK sections no other game judges: matrix stack, matrix load/multiply, geometry commands,
polygon attributes, texture attributes, texture coordinates).

## Design principles

1. Playable to the end on any emulator: the 2D schematic alone solves every order; a wrong 3D
   picture never blocks progress (a small kiln lamp shows the judge's verdict, nothing more).
2. Judging happens in the ROM, like lightenette and elevane: a rehearsal ROM (`make DEMO=rehearsal`)
   runs the kiln check (matrix and command bench against an integer model), the light box (3D
   pixels captured into bank D) and a scripted play of the first window of every chapter, and
   `tools/judge.py` grades its block against `expected.json`.
3. Judging and rehearsal modes work without NitroFS (see `nitro_ok()` in `core/nitro.c`).
4. No third-party trademarks in code or ROM strings.

## Build

```sh
./build.sh                  # paneveil.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=rehearsal   # the rehearsal ROM (Makefile DEMOS)
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag paneveil-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and `build/nitrofs`
(cart files) on each build when `tools/` changes.

## Code map

- `arm9/source/main.c`: init, scene loop.
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset), `title.c`, `select.c` (chapter and
  window list, every window open), `play.c` (one window: 3D top screen or the 2D schematic, the settings list, the kiln lamp, bake; `pv_play_hook` lets the rehearsal replace the input), `script.c/.h` (the scripted player shared by the rehearsal and the QA pilot: the first wrong setting, down to it, left or right the short way, A), `window.c/.h` (the `cv_windows` blob, the
  setting model, par arithmetic; pure logic), `credits.c` (credits and the licence viewer; NitroFS
  needed only here), `qa_game.c` + `qa_tune.h`, `rehearsal.c/.h`.
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them
  identical to the other games'). Title: hold **L+R+X+Y for 120 frames** (press L and R before X), or the
  SD marker `cleancart/paneveil/autoqa`. The run is title -> window list -> all 30 windows (`script_step()` through the
  live play scene; the list is driven with the D-pad and A) -> list -> title -> credits -> title; there `judge_epilogue()`
  (`game/qa_game.c`) runs the kiln check and the light box once and `qa_game_publish()` rewrites the block in the rehearsal's
  layout (`values[0..30]`) plus the QA words at `values[48..54]` (48 = verdict 1 PASS), so `tools/judge.py` grades it as
  boot `qa`. Nothing is tuned (`QaTune` is empty). A checkpoint per chapter, one for the judge and one at the credits.
- `arm9/source/judge/capture.*`: every display capture goes through `cap_start()` and `cap_done()`
  (bank D is the capture target).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers.
- `arm9/source/judge/bench.c/.h`: the kiln check. Command sequences go to the geometry engine through the
  direct ports and the packed GXFIFO; after GXSTAT bits 27/14/0 clear it reads GXSTAT stack levels,
  `CLIPMTX_RESULT` and `VECMTX_RESULT` and counts the words that differ from the integer model in the same
  file (row-major matrices, `C = M * C`, clip = position * projection, GBATEK "DS 3D Matrix Load/Multiply").
  Matrices only hold values that are exact in 1.19.12 (no rounding dependence). It draws nothing; the game
  runs it once for the kiln lamp, the rehearsal for `values[3..15]`. `reset()` first empties both stacks.
- `arm9/source/gfx/glass.c/.h`: the window from the twelve settings (`window.h` `F_*`): side -> polygon render bits,
  tint -> alpha 31/16/0, motif/repeat/flip/edge/colour/cutout -> TEXIMAGE_PARAM + PLTT_BASE (`g3_tex_set()`),
  petals and rings -> push / rotate / pop per petal and store / restore per ring, decal -> a second pass with depth test
  Equal, grain -> texture matrix + transformation mode 1; the lead frame is a display list built once with VTX_16, VTX_XY and
  VTX_10 vertices and sent through the packed GXFIFO. Textures are 8 bits per texel (indices 0-7) sharing one palette block of
  eight sets of eight colours (`tools/glassgen.py`): PLTT_BASE n picks set n; libnds uploads the texture, `g3_textures_load()`
  writes the palette into bank G and keeps libnds' address and size bits in each texture's parameter.
- `arm9/source/judge/lightbox.c/.h`: the light box, five still scenes with identity matrices (vertex = screen pixel;
  x exact, y exact for multiples of 3), fixed colours, captured into bank D; items count samples of the right colour at texel
  centres at least 3 px inside a polygon (edge rules never decide). Scenes: 1 vertex formats and culling, 2 alpha and wireframe,
  3 depth test, 4 repeat/flip/clamp, 5 colour 0, PLTT_BASE, coordinate mode 1. libnds leaves the last uploaded texture bound:
  the frame loop unbinds with `g3_tex_set(TEX_COUNT, 0, 0)` first. Result `values[16..30]`, capture bits in flags 8-12.
- `expected.json` + `tools/judge.py` (from lightenette: boots `rehearsal` and `qa`, exit 0 pass, 1 NG / profile
  mismatch, 2 no usable result) + `tools/tests/test_judge.py`. Items from GBATEK / `rom` gate, `unconfirmed` ones are
  informational. `known` has one profile per emulator version and renderer (DeSmuME, melonDS 1.1 software / GL Classic /
  GL Compute); re-measure all four after changing the ROM (`judge.py --profile NAME`). The generator of the file's items is
  not in the repo: edit the JSON (keep `values` indices in step with `game/rehearsal.h`).
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only).
- `tools/pvorder/` + `tools/orders/ch*.txt` + `tools/window_check.py`: the window orders. One line per
  window: `NAME: setting=VALUE ...` (settings and values in `pvorder/model.py` `FIELDS`; unnamed settings
  stay at their first value). Par is the sum of cyclic distances (`solve.py`; the checker compares it with
  a search per setting). `python3 tools/window_check.py [--chapter N] [--json] [--pack OUT]`. The build
  stops when an order fails the checker. Keep `FIELDS` and `game/window.h` (`F_*`) in step.
