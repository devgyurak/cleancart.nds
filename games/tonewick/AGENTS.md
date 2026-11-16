# Tonewick

Original listening puzzle for the DS: the signal keeper of a moor listening post mends twelve broken signal lines in
four districts (the trestle, the splice, the echo room, the junction) with an oscilloscope; each line is a register
field of the console's sound block. Human docs (English, each with a Korean twin `*-KO.md`): `README.md`,
`ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in `../../AGENTS.md`. Created from the ledgerine folder
(which came from lightenette and the corvane template): `arm7/`, `common/`, and `arm9/source/core` and `gfx` are
corvane's layers (keep fixes in sync when they apply to all games). The plan is `out/WORKPLAN-tonewick.md` (not committed).

Status: the sound lab, the ear (the experiments), the twelve signal lines with their GBATEK models, the rehearsal and
its judge (`expected.json`, `tools/judge.py`), the QA run; the scope, the control panel and the morning log scene are
next. Target level 2, area "sound and I/O edges": the exceptions, boundary values and documented bugs of
the GBATEK sections corvane and hushwell judge at level 1 (DS Sound Channels, DS Sound Notes, DS Sound Capture,
DS Real-Time Clock, DS Power Management, DS Cartridge Backup, DS Firmware User Settings).

## Design principles

1. Playable to the end on any emulator. Whether a signal line clears is decided by the ROM's own GBATEK rule model,
   never by the hardware; the ear's answer is shown beside it, and a difference marks the line ("the ear disagrees")
   without stopping the game.
2. Judging happens in the ROM: every district has an audit of fixed experiments; the numbers they measure go raw
   into the result block (id `TWRH`, `game/lab.h`) and `tools/judge.py` compares them with the ranges in `expected.json`.
   A value a capture never reached stays 0xFFFF.
3. No cycle-timing-dependent items: windows are whole frames (waits count VBlank interrupts, not `swiWaitForVBlank()`
   returns: on the ARM7 that call can return sooner than a frame) or ratios between two measurements.
4. Judging and rehearsal modes work without NitroFS (see `nitro_ok()` in `core/nitro.c`) and without the save chip.
5. No third-party trademarks in code or ROM strings.

## Build

```sh
./build.sh                  # tonewick.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=rehearsal   # the rehearsal ROM (Makefile DEMOS)
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag tonewick-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and `build/nitrofs` (cart files) on each build
when `tools/` changes.

## Code map

- `arm9/source/main.c`: init, scene loop (`tw_room` is the district scene's argument; the type is still called `Room`).
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset), `title.c` (title picture on BG3 + test menu),
  `credits.c` (credits and the licence viewer), `rooms.h/.c` (the district table and the puzzle record: two dials, a
  hardware function `hw`, the GBATEK model `model`, the wanted value `want`), `room_scene.c` (the district scene;
  `room_pilot_key()` is what the QA pilot presses next), `lab.h/.c` (the result words `tw_res`, layout in `lab.h`),
  `ear.h/.c` (the ear: every experiment as a function that builds a lab script, listens and reduces the capture to numbers;
  `ear_lfsr()` is the GBATEK noise register), `rehearsal.c/.h`, `qa_game.c` + `qa_tune.h` (the QA run; its state hash covers only the completion bits and the lines
  solved: measured values move a little from run to run). One file per district (`trestle.c`, `splice.c`, `echo.c`,
  `junction.c`), each with its three signal lines (`hw` runs the experiment and returns a coarse class, `model` is the
  same class from the GBATEK rules, `want` the class that clears the line) and its `audit_*()` (the judged numbers).
- `arm9/source/core/sndlab.c/.h` + `arm7/source/lab7.c/.h` (`CvLabDesc` in `common/cv_ipc.h`): the sound lab. The ARM9
  builds a script of sound-register writes (SOUNDxCNT, the capture registers; the ARM7 accepts only 04000400h-0400051Fh
  and main-RAM pointers), sends its address on `CV_FIFO_SND`, and the ARM7 runs it with its sound engine stopped and the
  speaker amplifier off through power management (never SOUNDCNT), then reads the RTC, power management and firmware
  settings if the descriptor asks, and answers `CV_LAB_DONE | id` on `CV_FIFO_SYS`. Captures land in main RAM; the ARM9
  invalidates the cache before reading. `save_lab()` in `core/save.c` reads the save chip's status around a write.
- `expected.json` + `tools/judge.py` (tests in `tools/tests/test_judge.py`, the GBATEK models in `tools/tests/test_model.py`):
  items read one or two raw values (`values[1..36]`) and carry `done`, the district's completion bit in `values[0]`;
  `known` holds the per-emulator profiles. The three save items are n/a when the run has no save chip
  (`na_when [32, 65535]`).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers (no 3D engine code here).
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them identical to the other
  games'). Title: hold **L+R+X+Y for 120 frames** (press L and R before X), or the SD marker `cleancart/tonewick/autoqa`.
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only); `cvtools/twart.py` draws the title
  picture and the icon (a scaffold placeholder, `TODO(title)`).

## Controls (melonDS keys in brackets)

Start (Return) starts the shift at district 1; X (X) credits and licence viewer; Select (Backspace) the test menu
(up/down, A (X), B (Z)). In a district: up/down choose a line, left/right turn dial A, L/R (Q/W) or Y/X (A/S) dial B,
A (X) listens, Start (Return) ends the shift when every line is cleared, B (Z) goes back to the title.

## Result block on the SD card

`core/sdcard.c/.h`: when the folder `<drive>cleancart/tonewick/` already exists on the card, the rehearsal idle writes
its result block there as `res-<date>-<time>-tonewick-rehearsal.log` (the format `tools/judge.py` reads). The ROM never
creates the folder. QA marker start: `cleancart/tonewick/autoqa` (see skill `nds-qa-mode`).
