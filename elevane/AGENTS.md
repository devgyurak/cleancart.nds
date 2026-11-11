# Elevane

Original voxel silhouette puzzle for the DS: stack blocks until the front and plan
views match the order. Both screens show the model in 3D; a 2D grid shows the same
state so the puzzles can be solved without 3D. Human docs (English, each with a Korean
twin `*-KO.md`): `README.md`, `docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`.
Repo-wide rules are in `../AGENTS.md`. Created from lightenette (which came from the
corvane template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are corvane's
layers (keep fixes in sync when they apply to all games).

Status: chapter 1 is playable (title, puzzle list, play). The scope for v0.1.0 is five chapters with eight puzzles each,
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
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset), `title.c`, `select.c` (puzzle list),
  `play.c` (one puzzle: input, judge, both 3D views, the text overlay and the 2D-grid mode), `puzzle.c/.h` (the
  `cv_puzzles` blob, silhouettes, judge, stars; pure logic), `credits.c` (credits and the licence viewer; NitroFS
  needed only here), `qa_game.c` + `qa_tune.h`.
- `arm9/source/gfx/dual.c`: 3D on both screens (see below).
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them
  identical to corvane's/lightenette's). Title: hold **L+R+X+Y for 120 frames** (press L and R
  before X). The run is title → credits → title → `RESULT PASS`.
- `arm9/source/judge/capture.*`: every display capture goes through `cap_start()` and
  `cap_done()` (bank D is the capture target).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers.
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only).
- `tools/evpuzzle/` + `tools/puzzles/ch*.txt` + `tools/puzzle_check.py`: puzzle tools. A puzzle is the maker's
  structure (voxel cells in a W x D x H grid, each at most 8) in a text file; the order is derived from it
  (`model.py`: front view (x, z) = any block along y, plan view (x, y) = any block in the column; support rule:
  every block reaches z = 0 through face-adjacent blocks). `solve.py` finds the minimum block count ("par"):
  `stack` mode (chapters 1-3, piles from the floor) in closed form, `layers` mode (chapters 4-5, arches) by
  iterative deepening search with a node limit instead of a clock; past the limit the maker's count becomes the
  reference and the puzzle is flagged. `pack.py` writes the ROM blob `cv_puzzles` (layout in its docstring).
  `python3 tools/puzzle_check.py [--chapter N] [--json] [--pack OUT]`; unit tests:
  `python3 -m unittest discover -s tools/tests`. The build stops when a puzzle fails the checker. Output is
  deterministic. To add a puzzle: append it to the chapter file, run the checker, read par.

## Frame structure (normal play)

One `dual_begin_call()` per VBlank draws one view: even calls the front view (top screen), odd calls the plan view
(bottom screen), so each screen refreshes at 30 fps. Engine A renders the view and a display capture copies the
**whole engine A output** (3D + the BG1 text layer, so the 2D overlay is baked in) into bank D (front) or bank C
(plan); engine B shows the other bank on the other screen (C as a 16bpp bitmap BG, D as twelve 64x64 bitmap OBJs).
The routing for a frame is written one call after its geometry (latency 1, measured by corvane `acc-dual`).
Banks (VL_DUAL3D): A, B texture (unused), C, D alternate LCDC / engine B, E main BG (font tiles at 0, text maps at
2 KB blocks 28 = front view, 29 = plan view; `REG_BG1CNT` picks the map per frame), F OBJ, G texture palette.
Nothing else may live in C or D. Both views are orthographic elevations (`ortho_camera`), so what is drawn is exactly
what the silhouette check sees: the top screen has the order as a blue blueprint wall behind the blocks, the bottom
screen has it as lit floor tiles; blocks outside the order are red. Touch hit testing is linear in that projection.

**2D grid mode** (Select, any time in a puzzle): `scene_text_screens()` text on both screens, no 3D, no capture, same
state and rules. Every puzzle must stay solvable in it. In 3D mode the same grid is drawn small in the corners.
Mode changes tear the screens down and rebuild them (`enter_3d`/`leave_3d`/`enter_2d`). 
**Showcase mode** (`game/showcase.c`, X in a puzzle): the finished structure repeated as a plaza of copies, about 2300
polygons, more than one frame holds (2048 polygons / 6144 vertices), so it is drawn in two passes (left half, right
half, fixed split x = 128 so each pass holds about 1140 polygons) one per frame; the screen shows the measured counts
and overflow flags. Preset 1 (A switches): HBlank DMA, the path of melonDS #2618 (acc-split-hdma): engine A at the
bottom only captures into bank D, the top screen is engine B's 16bpp bitmap BG2 in bank C filled one line per HBlank
(BG2Y = -1); even calls draw the right pass alone, odd calls the left pass blended with bank D. The screen counts sub BG
holes (transparent or black pixels, every 4th line) per kind of frame. Preset 2: rear plane (acc-split-rear): engine A
on top, banks A and C alternate as capture target and rear plane colour (texture slot 2), bank B the far depth plane
(slot 3), text on engine B (bank H). Both are optional extras: a wrong picture there never blocks play.
Bank hand-over: entering the showcase leaves the dual 3D layout (`leave_3d`), `setup()` remaps every bank for the
preset (`vram_layout(VL_DUAL3D)` first, then the preset's changes), leaving stops HBlank DMA channel 1, switches capture
off and clears `GL_CLEAR_BMP`; the puzzle then re-enters dual 3D from scratch (`enter_3d` -> `dual_begin`, call counter
0, sub bitmap re-set up) and its 20-frame fade-in covers the first two frames. The pile is kept (`resume`). The routing
latency (1 call) is a constant: it is what every emulator and renderer measured in the corvane demos; hardware
unconfirmed. Expected: preset 1 hole count 0 on melonDS 1.1 (all renderers) and master software; master GL shows the
left half black, master Compute grey (`BLEND 6144`, the #2618 defect); preset 2 is wrong on one side on GL and
Compute (1.1 and master), a separate defect.

## Controls (melonDS keys in brackets)

Title: Start (Return) opens the puzzle list, X (X) the credits. List: up/down, A (X) starts, B (Z) back. Puzzle: D-pad (arrows) moves the cursor, A (X) +1, B (Z) -1 (A delivers when both views match), touch a plan cell +1, hold ~0.5 s empties it, Select (Backspace) 2D grid / 3D, X (S) showcase (A switches preset, B back), Start (Return) back to the list; after a delivery A goes on, B returns to the list. Title: X (X) opens the credits; in the credits X (X) opens the licence viewer, B (Z) goes back.
Y (A) is free in this game and is meant for choosing the work layer in chapter 4 (Y + D-pad up/down).
More actions are listed here as scenes land.
