# Buoyane

Original night-harbour approach game for the DS (design: `out/WORKPLAN-buoyane.md`, not committed): steer a small
boat into a made-up harbour by reading how the buoy and sector lights look. The top screen is a 3D view from the
bow, the bottom screen a 2D chart. The game judges itself: a rehearsal ROM and five check ROMs capture pixel-exact
3D scenes into bank D and `tools/judge.py` grades the result block against `expected.json`. Human docs (English,
each with a Korean twin `*-KO.md`): `README.md`, `ASSETS.md`, `docs/QA.md`; plus `THIRD_PARTY_NOTICES`.
Repo-wide rules are in `../../AGENTS.md`. Created from paneveil (which came from elevane, lightenette and the corvane
template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are corvane's layers (keep fixes in sync when they apply to all games).

Status: playable core: title, chart, voyage (30 approaches in five chapters), almanac, credits; the light check (five scenes), the
rehearsal and the five check ROMs, `expected.json` + `judge.py`, the QA mode (title -> chart -> all 30 approaches -> credits -> light check ->
judge suite). The title is still a placeholder; docs (QA, HW-QA) and the title art follow. Target level 2, area "3D rendering edges: polygon rules, texture blending and the
fog/edge/anti-aliasing post-pass".

## Build

```sh
./build.sh                  # buoyane.nds
./build.sh DEMO=rehearsal   # or DEMO=check-dot|fog|edge|depth|blend (Makefile DEMOS)
./build.sh demos            # every demo ROM
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag buoyane-vX.Y.Z runs this in CI)
```

## Code map

- `arm9/source/judge/lightcheck.c/.h`: the five scenes (dot, fog, edge, depth, blend). Each draws with an exact
  projection (`px_proj`: pixel = vertex * 16, W 3; `ident_proj`; `dot_proj`: W = -z), captures with `capture()` (frame 4 start,
  frame 8 read) and reads fixed pixels. Colour items use source A = 3D only (`cap_start(true)`), alpha items the blended
  screen (`cap_start(false)`). All 3D registers are written by the scene; nothing depends on what a loader left. DISP_1DOT_DEPTH
  is written only after GXSTAT bit 27 clears. VRAM takes 16-bit writes only (the textures are written as halfwords).
- `arm9/source/judge/capture.*`: `cap_start(only_3d)`, `cap_done()`, `cap_pixel()` (bank D).
- `arm9/source/game/rehearsal.c/.h`: block layout (`values[0..20]`, QA words at 48..54), `rehearsal_run()` and `check_run(scene)`.
- `arm9/source/demo/demo.c`: `rehearsal`, `check-<scene>`; check ROM ids are `ck` + the first two letters of the scene.
- `arm9/source/game/qa_game.c`: QA hooks; after the credits the light check runs once, `qa_pass`, then `qa_suite_run("buoyane", ...)` runs the
  five check scenes as `res-*-check-<scene>.log` (`tools/qa/suite_compare.py` must say SAME for each).
- `arm9/source/game/model.c/.h`: routes and almanac accessors (`by_gate`, `by_look_of`, `by_almanac_answer`). `voyage.c`: the chart and the voyage
  (`by_pilot_keys()` is the scripted player shared by the rehearsal and the QA pilot). `lamps.c`: the lights, one function per chapter
  (1 DISP_1DOT_DEPTH, 2 mixed-W dots, 3 fog density, 4 edge marking, 5 depth Equal), drawn with the same rules the light check judges.
- `tools/byroute/model.py` is the almanac (single source: mkassets generates the C tables), `routes.py` parses, checks and packs `tools/routes/ch*.txt`.
  The checker proves at every gate that exactly one option has the look of the true light in that view, so every approach can be finished
  from the almanac alone (Select) on any emulator. `tools/tests/test_routes.py`, `test_judge.py`.
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers. `core/qa*.c`, `qalog*`, `qasuite*` stay identical across games.

## Judging

- `expected.json`: 23 target items (level 2), 11 prerequisites. `source: unconfirmed` is informational until a
  hardware run: only `fog-alpha-glitch` (GBATEK itself holds back). Every other item is a GBATEK statement that gates; an emulator that
  gets one wrong is recorded in `known`.
- `known` holds the measured failing set per emulator profile (DeSmuME, melonDS software, GL Classic, GL Compute). Re-measure after every scene change.
- The 5-bit capture cannot tell fog density 126 from 127, nor 127 from 128 (a difference of less than one level), so `fog-density-127-full` (a level 1 prerequisite) only checks that a high density reaches the fog colour and 120 does not.
- Texture/palette setup is in `textures_load()`: VRAM A (textures) and G (palette) are mapped as LCDC while written.

## Rules

Everything the repo-wide rules say applies: SPDX headers, generated assets listed in `ASSETS.md` and `ASSETS-KO.md`, no
third-party names in ROM strings, QA code only through the hooks in `core/`. The title screen is a placeholder (`TODO(title)`)
until the voyage PRs replace it.
