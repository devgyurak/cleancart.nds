# Heddlemere

Original punch-card loom puzzle for the DS: five chapters of eight orders; every order is a four-row tapestry
whose cards (colour, weave, and in later chapters program, lever, dye) the player punches. Human docs (English, each
with a Korean twin `*-KO.md`): `README.md`, `docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`.
Repo-wide rules are in `../../AGENTS.md`. Created from paneveil (which came from elevane, lightenette and the corvane
template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are corvane's layers (keep fixes in sync when they apply to all games).

Status: scaffold (stack of PRs onto develop: scaffold, measurement core, raster weave and blueprint, motor/lever/dye,
QA and docs). Target level 3, area "3D engine and scanline timing" (GBATEK sections `DS Video Stuff`, `DS 3D I/O Map`,
`LCD I/O Interrupts and Status`).

## Design principles

1. Playable to the end on any emulator: puzzle verdicts and stars come from the documented costs only (`rom` logic, `game/order.c`);
   a wrong or missing raster/3D effect never blocks progress. The blueprint view (Select) draws the cloth in software.
2. Judging happens in the ROM, like paneveil and elevane: a rehearsal ROM (`make DEMO=rehearsal`) measures and plays the first
   order of every chapter, and `tools/judge.py` grades its block against `expected.json`.
3. Rehearsal and judging work without NitroFS (see `nitro_ok()` in `core/nitro.c`). Every measurement runs with IME=0 and polls;
   only differences of two edges or slopes of two run lengths are used, so IRQ latency and fixed offsets cancel.
4. No third-party trademarks in code or ROM strings.

## Build

```sh
./build.sh                  # heddlemere.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=rehearsal   # the rehearsal ROM (Makefile DEMOS)
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag heddlemere-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and `build/nitrofs` on each build when `tools/` changes.

## Code map

- `arm9/source/main.c`: init, scene loop.
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset, text screens, `display_3d_top()`), `title.c` (the woven name on BG2 over a raster-woven cloth), `select.c` (chapter and order list), `play.c` (one order: the cell table, the cloth, deliver; `hm_play_hook` lets
  the rehearsal replace the input), `script.c/.h` (the scripted player shared by the rehearsal and the QA pilot: follows `order_plan()`),
  `order.c/.h` (the `cv_orders` blob and the rules: pure logic, mirrors `tools/hmorder/model.py`), `credits.c`, `qa_game.c` + `qa_tune.h`,
  `rehearsal.c/.h` (block layout `HMRH`, `rehearsal_measure()`).
- `arm9/source/gfx/loom.c/.h`: the cloth on BG1 (char base 1, map block 29, palette line 9). Raster: one checker tile, the HBlank IRQ writes `BG1HOFS` (weave shift of the
  line: plain 0, twill k, herringbone and diamond triangle waves) and palette entry 1 (thread colour) for the next line. Blueprint (Select): the same pixels computed by the
  CPU into unique tiles, no IRQ, so a wrong raster effect never blocks the game; the two renderings are pixel-identical in melonDS software (checked with screenshots).
- `arm9/source/gfx/loom3d.c/.h`: the heddle frame in 3D on BG0 (screen lines 144..191, the rest transparent): one bar per row, moved by the row's program with the real
  push / translate / scale / pop / identity commands (matrix mode 2 where the program says so). `display_3d_top()` puts the text on BG3 and leaves BG1 to the cloth.
  `game/play.c` `run_motor()` runs the programs through the engine at delivery (`timing_motor_run()`) and lights the motor lamp (information only);
  every lever pulled costs one real VCOUNT write (`timing_lever_kick()`). Dye is model logic in the ROM (the quiet-line hardware behaviour is judged by the rehearsal).
- `arm9/source/judge/timing.c/.h`: the measurements (values[1..20]). Polling loops in ITCM with the 33.51 MHz stopwatch (timers 0+1), IME=0: line and
  frame length (means over 2000 lines / 32 frames), HBlank-low span (the delay before the fall poll sweeps, so the poll grid averages out),
  VBlank flag lines and 9-bit LYC from one frame of lines, VCOUNT write (frame with and without a write at line 212, medians), geometry
  command slopes (stream after SWAP_BUFFERS at line 100 so the engine runs from the VBlank; time from VBlank to GXSTAT bit 27 clear for two
  run lengths), full-FIFO stall, half-full IRQ (IF bit 21), RAM_COUNT at lines 191/192, and the texture quiet lines (a palette rewritten in
  lines 192..213 must show in the captured render; `judge/capture.*`). The ARM7 measures its own HBlank span on request (`CV_SYS_HM_HBLANK`, timers 2+3).
- `expected.json` + `tools/judge.py` (from paneveil, plus `"signed"` and a list `"expect"`) + `tools/tests/test_judge.py`: boots `rehearsal` and `qa`; GBATEK
  figures gate, `unconfirmed` items (ARM7 HBlank, swap tail, TRANS, BOX_TEST: GBATEK and melonDS disagree) are informational until hardware. `known` has one profile
  per emulator and renderer; re-measure all four after changing the ROM (`judge.py --profile NAME`).
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep identical to the other games'). Title: hold
  **L+R+X+Y for 120 frames**, or the SD marker `cleancart/heddlemere/autoqa`. The run is title -> order list -> all 40 orders (`script_step()`
  through the live play scene) -> list -> title -> credits -> title; there `judge_epilogue()` (`game/qa_game.c`) publishes the block in the
  rehearsal's layout plus the QA words at `values[48..54]`. Nothing is tuned. A checkpoint per chapter, one for the judge and one at the credits.
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers.
- `tools/hmorder/` (`model.py` rules, `pack.py` blob, shared by `tools/order_check.py`), `tools/orders/ch*.txt`: one line per order
  `NAME: budget=<lines> passes=<n> slack=<levers> rows=COLOUR.WEAVE[+],...` (`+` = the row wants a dye). `python3 tools/order_check.py
  [--chapter N] [--json] [--plan-hash] [--pack OUT]`; the build stops when an order fails (par formula against a search per row, levers within
  the slack, a valid program for every row). Keep `model.py`, `pack.py` and `game/order.[ch]` in step; the rehearsal's `plan-ticks` value
  (`order_plan_hash()`) must equal `order_check.py --plan-hash`.
- `tools/mkassets.py`, `tools/cvtools/` (incl. `hmart.py`, the title lettering): asset generators (Python 3, stdlib only).

## Model (see `tools/hmorder/model.py`)

Cells: colour (6), weave (4), program (4), lever (2), dye (6). Chapter 1-2 turn colour and weave; 3 adds program, 4 the lever, 5 the dye.
A program runs `passes` times per beat and costs GX cycles (IDENTITY 19, PUSH 17, POP 36, TRANS 22, SCALE 22, TRANS in matrix mode 2 +30);
it is valid when its index is at least the weave's and `ticks <= (budget + 10 * lever) * 2130`. A dye is valid on lines 192..213.
