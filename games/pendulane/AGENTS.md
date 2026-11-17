# Pendulane

Original automaton-workshop game for the DS (work in progress). Pendulane is the third L3 game: target level 3, area
"CPU, bus and sound timing". It measures timings on the console itself and judges them against GBATEK in the ROM.
Human docs (English, each with a Korean twin `*-KO.md`): `README.md`, `docs/QA.md`, `docs/HW-QA.md`, `ASSETS.md`; plus
`THIRD_PARTY_NOTICES`. Repo-wide rules are in `../../AGENTS.md`. The plan is `out/WORKPLAN-pendulane.md` (not committed).
Created from paneveil (arm7, common, `arm9/source/core` and `gfx` are corvane's layers: keep fixes in sync).

Status (PR 1 of the stack): skeleton, the timing core (ITCM edge finding) and the math bench; scenes: title (a text
scaffold, `TODO(title)`), the bench ledger, credits; the hidden QA run reaches the credits and runs the bench once.
Next: the ARM7 benches (SPI, memory), the sound bench, the cartridge bench, the device game, the rehearsal and QA
epilogue in full, hardware.

## Build

```sh
./build.sh                  # pendulane.nds
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=rehearsal   # the rehearsal ROM (Makefile DEMOS)
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag pendulane-vX.Y.Z runs this in CI)
./build.sh clean
```

## Code map

- `arm9/source/main.c`: init, scene loop. `game/`: scenes (`scenes.h`), `title.c`, `bench.c` (the bench ledger, a live look), `credits.c`,
  `display.c`, `qa_game.c` + `qa_tune.h`, `rehearsal.c/.h` (the block layout lives in `rehearsal.h`).
- `arm9/source/judge/bench.c/.h`: the measurement core. **Edge finding**: the device is started by one store, the CPU runs a sled of n ARM
  nops in ITCM (one ARM9 cycle = half a bus cycle each) and reads the busy register once; the smallest n at which it reads clear is the
  busy time plus a fixed path (the store, the jump into the sled, the load). `pd_probe` (ITCM, ARM, one routine) does store + sled + load, so
  that path is as short as possible. Every judged value is the difference of two edges, so the path cancels; a value of 0 as an edge is a
  floor, not a measurement, and fails the bench. IME is 0 during a bench. The bench never polls, so there is no poll jitter.
  `bench_math()`: DIV modes 0, 1, 2 and SQRT, plus a check of their results; `bench_timer_cascade()`: timer 1 counts timer 0's overflows.
- `expected.json` + `tools/judge.py` (from paneveil, plus `"signed": true` items compared as s32) + `tools/tests/test_judge.py`.
  Boots `rehearsal` (id `PDRH`) and `qa` (`QARN`, rehearsal layout plus the QA words at `values[48..54]`). `known` has the four emulator
  profiles (DeSmuME py-desmume 0.0.9, melonDS 1.1 software / GL Classic / GL Compute); re-measure all four after changing the ROM.
- `tools/mkassets.py`, `tools/cvtools/`: asset generators (Python 3, stdlib only); the song `pendulum` is composed by `cvtools/audio.py`.
- `arm9/source/core/qa.*`, `qalog.*`: the hidden auto QA mode (skill `nds-qa-mode`, identical to the other games'); title: hold
  **L+R+X+Y for 120 frames** or the SD marker `cleancart/pendulane/autoqa`.

## Rules of this game

1. Playable to the end on any emulator: nothing a bench measures blocks progress.
2. Every timing item is a difference of two measurements (no absolute cycle counts) and is given in the unit of the smallest step the
   hardware can show (ARM9 cycle = half bus cycle for the math unit).
3. Items whose GBATEK figure is uncertain are `unconfirmed` (informational); a hardware value that differs from GBATEK replaces the
   expected value with a `hw:` source (see the plan, decision 5).
4. No third-party trademarks in code or ROM strings.
