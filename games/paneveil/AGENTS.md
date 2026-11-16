# Paneveil

Original stained-glass workshop game for the DS: each order names the glass, pattern and
rosette of a window; set the twelve settings to match the order and fire it in the kiln.
The top screen shows the window in 3D, lit from behind, and a 2D schematic mode shows the same
state as text so every order can be solved without 3D. Human docs (English, each with a Korean
twin `*-KO.md`): `README.md`, `docs/QA.md`, `docs/HW-QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`.
Repo-wide rules are in `../../AGENTS.md`. Created from elevane (which came from lightenette and the
corvane template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are corvane's layers (keep fixes
in sync when they apply to all games).

Status: scaffold (title, window list, credits with the licence viewer, QA mode). Not yet: the
windows, the play scene, the judge. Target level 1, area "3D geometry and texturing"
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
  window list, every window open), `play.c` (one window), `window.c/.h` (the `cv_windows` blob, the
  setting model, par arithmetic; pure logic), `credits.c` (credits and the licence viewer; NitroFS
  needed only here), `qa_game.c` + `qa_tune.h`, `rehearsal.c/.h`.
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them
  identical to the other games'). Title: hold **L+R+X+Y for 120 frames** (press L and R before X), or the
  SD marker `cleancart/paneveil/autoqa`.
- `arm9/source/judge/capture.*`: every display capture goes through `cap_start()` and `cap_done()`
  (bank D is the capture target).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers.
- `expected.json` + `tools/judge.py`: the grader (added with the judge changes).
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only).
- `tools/pvorder/` + `tools/orders/ch*.txt` + `tools/window_check.py`: the window orders. One line per
  window: `NAME: setting=VALUE ...` (settings and values in `pvorder/model.py` `FIELDS`; unnamed settings
  stay at their first value). Par is the sum of cyclic distances (`solve.py`; the checker compares it with
  a search per setting). `python3 tools/window_check.py [--chapter N] [--json] [--pack OUT]`. The build
  stops when an order fails the checker. Keep `FIELDS` and `game/window.h` (`F_*`) in step.
