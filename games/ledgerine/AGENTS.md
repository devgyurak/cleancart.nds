# Ledgerine

Original machine-room ledger puzzle for the DS: a night clerk in an old counting house balances the books
by setting the dials of six machines (the press, the tube, the clock wall, the belt, the vault, the rule
book), each one a real hardware device of the console. The counting house stands in two towers (the two
screens) joined by a pneumatic tube. Human docs (English, each with a Korean twin `*-KO.md`): `README.md`,
`docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in `../../AGENTS.md`. Created from
the lightenette folder (which came from the corvane template): `arm7/`, `common/`, and `arm9/source/core`
and `gfx` are corvane's layers (keep fixes in sync when they apply to all games). The plan is
`out/WORKPLAN-ledgerine.md` (not committed).

Status: all six rooms (31 ledger lines), the rehearsal and its judge (`expected.json`, `tools/judge.py`), the QA run;
v0.1.0 is next. Target level 2, area "system edges": the exceptions, boundary values and error paths of
the GBATEK sections corvane judges at level 1 (DS Maths, IPC, timers, DMA, WRAM and VRAM control, BIOS).

## Design principles

1. Playable to the end on any emulator. Whether a ledger line balances is decided by the ROM's own GBATEK rule
   model, never by the hardware; the hardware's answer is shown beside it, and a difference stains the line
   ("machine disagrees") without stopping the game.
2. Judging happens in the ROM: every room has an audit of fixed experiments whose results are compared with
   GBATEK constants and written as flags into the result block (id `LDRH`, `game/ledger.h`). `tools/judge.py`
   grades it against `expected.json`.
3. No cycle-timing-dependent items: judging windows are a frame or more, or a ratio between two counters.
4. Judging and rehearsal modes work without NitroFS (see `nitro_ok()` in `core/nitro.c`).
5. No third-party trademarks in code or ROM strings.

## Build

```sh
./build.sh                  # ledgerine.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=rehearsal   # the rehearsal ROM (Makefile DEMOS)
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag ledgerine-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and `build/nitrofs` (cart files) on
each build when `tools/` changes.

## Code map

- `arm9/source/main.c`: init, scene loop (`lg_room` is the room scene's argument).
- `arm9/source/game/`: scenes (`scenes.h`), `display.c` (engine reset), `title.c` (title picture on BG3 + test
  menu), `credits.c` (credits and the licence viewer; NitroFS needed only here), `rooms.h/.c` (the room table,
  the puzzle record: two dials, a hardware function `hw`, the GBATEK model `model`, the wanted value `want`),
  `room_scene.c` (the room scene; `room_pilot_key()` is what the QA pilot presses next), `ledger.h/.c` (the result
  words `lg_res` and the block publisher; layout in `ledger.h`), `rehearsal.c/.h` (every room's audit in order, one
  block with id `LDRH`), `qa_game.c` + `qa_tune.h` (the QA run). One file per room, each with its puzzles and its
  `audit_*()` (the judged experiments, bit meanings in the file header): `press.c` (maths unit), `tube.c` (IPC FIFO,
  the ARM9 half of the barrier script), `clock.c` (timers 0-3), `belt.c` (DMA channels 0, 1, 3; ITCM spin
  function), `vault.c` (WRAMCNT, VRAMCNT, STRB; takes over the VRAM map while it runs and ends with
  `vram_layout(VL_MENU)`), `rules.c` (BIOS calls with sentinel buffers).
- `arm7/source/vault7.c`, `tube7.c`: the ARM7 halves (the only change to the corvane ARM7 core). The vault steps run in the
  FIFO value handler (`CV_VAULT_STEP | step` on `CV_FIFO_SYS`, reply `CV_VAULT_REPLY | result`; `core/sys.c`
  `sys_vault_step()`). The tube script is requested by `CV_SYS_TUBE_START`, run from the ARM7 main loop with
  `tube7_busy` set (no input packets, libnds FIFO IRQ off), uses 15 numbered barriers on the IPCSYNC nibbles (a CPU
  shows n and waits for the other to show n or n+1) and puts the FIFO back before it sends `CV_SYS_TUBE_DONE`. Keep
  both scripts in step with the barrier table in `tube.c`.
- `tools/ledger_model.py` + `tools/tests/test_model.py`: the GBATEK divider/root model in Python with the GBATEK
  constants as tests; `press.c` carries the same model in C.
- `expected.json` + `tools/judge.py`: items read one flag word (`values[1..7]`) and carry `done`, the room's
  completion bit in `values[0]`; tests in `tools/tests/test_judge.py` (`python3 tools/tests/test_judge.py`). `known`
  holds the per-emulator profiles; the rule book items depend on the emulator's BIOS.
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them identical to the
  other games'). Title: hold **L+R+X+Y for 120 frames** (press L and R before X), or the SD marker
  `cleancart/ledgerine/autoqa`. The run is title -> rooms 1 to 6 (the pilot solves every line with `puzzle_solution()` and
  pulls the levers) -> credits -> title; `qa_game_publish()`
  rewrites the block in the rehearsal's layout plus the QA words at `values[48..54]` (48 = verdict 1 PASS), so
  `tools/judge.py` grades it as boot `qa`. Nothing is tuned (`QaTune` is empty).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers (no 3D engine code here: `gfx/gl3d.*` is not
  copied).
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only); `cvtools/ledart.py` draws the
  title picture and the icon.

## Controls (melonDS keys in brackets)

Start (Return) starts the night shift at room 1; X (X) credits and licence viewer; Select (Backspace) the test
menu (up/down, A (X), B (Z)). In a room: up/down choose a line, left/right turn dial A, L/R (Q/W) or Y/X (A/S) dial B, A (X) pulls the lever,
Start (Return) closes the books when every line balances, B (Z) goes back to the title.

## Result block on the SD card

`core/sdcard.c/.h`: when the folder `<drive>cleancart/ledgerine/` already exists on the card, the rehearsal idle writes its
result block there as `res-<date>-<time>-ledgerine-rehearsal.log` (the format `tools/judge.py` reads). The ROM never creates
the folder. QA marker start: `cleancart/ledgerine/autoqa` (see skill `nds-qa-mode`).
