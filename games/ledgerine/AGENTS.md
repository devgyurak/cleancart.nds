# Ledgerine

Original machine-room ledger puzzle for the DS: a night clerk in an old counting house balances the books
by setting the dials of six machines (the press, the tube, the clock wall, the belt, the vault, the rule
book), each one a real hardware device of the console. The counting house stands in two towers (the two
screens) joined by a pneumatic tube. Human docs (English, each with a Korean twin `*-KO.md`): `README.md`,
`docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in `../../AGENTS.md`. Created from
the lightenette folder (which came from the corvane template): `arm7/`, `common/`, and `arm9/source/core`
and `gfx` are corvane's layers (keep fixes in sync when they apply to all games). The plan is
`out/WORKPLAN-ledgerine.md` (not committed).

Status: scaffold (title, test menu, credits and licence viewer, room scene stub, rehearsal stub, QA run that
walks the six rooms). Target level 2, area "system edges": the exceptions, boundary values and error paths of
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
  the puzzle record and its model/hardware pair), `room_scene.c` (the room scene), `ledger.h/.c` (the result
  words `lg_res` and the block publisher; layout in `ledger.h`), `rehearsal.c/.h` (every room's audit in order,
  one block with id `LDRH`), `qa_game.c` + `qa_tune.h` (the QA run).
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`; keep them identical to the
  other games'). Title: hold **L+R+X+Y for 120 frames** (press L and R before X), or the SD marker
  `cleancart/ledgerine/autoqa`. The run is title -> rooms 1 to 6 -> credits -> title; `qa_game_publish()`
  rewrites the block in the rehearsal's layout plus the QA words at `values[48..54]` (48 = verdict 1 PASS), so
  `tools/judge.py` grades it as boot `qa`. Nothing is tuned (`QaTune` is empty).
- `arm9/source/core`, `gfx`, `arm7/`, `common/`: corvane layers (no 3D engine code here: `gfx/gl3d.*` is not
  copied).
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only); `cvtools/ledart.py` draws the
  title picture and the icon.

## Controls (melonDS keys in brackets)

Start (Return) starts the night shift at room 1; X (X) credits and licence viewer; Select (Backspace) the test
menu (up/down, A (X), B (Z)). In a room: the dials and the lever are listed as the room scene grows (PR 2 on).
