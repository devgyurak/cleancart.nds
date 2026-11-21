# Quayholt

Original two-crane harbour puzzle for the DS: an old steam crane (the ARM7) and a new electric crane (the
ARM9) work the same cargo jobs on a dusk quay, and the game shows them handling one job differently. Under
the story every device is a real instruction or CP15 register of the console. Human docs (English, each with a
Korean twin `*-KO.md`): `README.md`, `docs/QA.md`, `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules
are in `../../AGENTS.md`. Created from the ledgerine folder (which came from the corvane template): `arm7/`,
`common/`, and `arm9/source/core` and `gfx` are corvane's layers (keep fixes in sync when they apply to all
games). The plan is `out/WORKPLAN-quayholt.md` (not committed).

Status: the measurement harness (piers A loads, B blocks, C tally), the rehearsal, the per-pier demos and
their judge (`expected.json`, `tools/judge.py`) and a QA run through the rehearsal and the credits. The
protection unit, TCM and cache piers (D, E, F), the harbour game (30 jobs) and the title polish come in later
changes. Target level 2, area "ARM core edges": ARM9/ARM7 instruction edges and the ARM9 CP15.

## Design principles

1. Playable to the end on any emulator. A job's goal is set by the ROM's own GBATEK model, never by the
   hardware; the hardware's answer is shown beside it and a difference marks the box ("crooked box") without
   stopping the game.
2. Judging happens in the ROM: each pier runs fixed instruction stubs on fixed test data and turns the raw
   registers and memory words into a result code (`common/qy_class.h`: 1 = GBATEK, 2/3 = alternatives, 0xFF
   other). Expected values are constants worked out by hand from GBATEK, never another instruction's output.
3. Nothing is timing dependent; every measurement runs with IRQs masked on the ARM9 (the ARM7 stubs run in the
   FIFO handler).
4. Judging and rehearsal modes work without NitroFS and without a save chip.
5. No third-party trademarks and no CPU or emulator names in ROM strings (the workshop panel may show
   mnemonics such as LDM and STM).

## Code map

| Path | What |
|---|---|
| `arm9/source/judge/stubs9.s`, `stubs9.h` | hand-written ARM9 stubs (`.word` encodings for what the assembler would refuse) |
| `arm9/source/judge/measure.c` | runs the stubs and the ARM7 steps, writes the codes into `qy_res` |
| `arm7/source/stubs7.s`, `step7.c` | the ARM7 stubs and the step dispatcher; steps are `CV_QY_*` in `common/cv_ipc.h`; raw words go to the raw page `CV_QY_RAW` (0x0237F800) |
| `arm9/source/game/ledger.[ch]` | the result block layout (`QYRH`, values[0..33], QA words at 48..54) |
| `arm9/source/game/rehearsal.c` | rehearsal scene and `rehearsal_measure(mask)` |
| `arm9/source/demo/demo.c` | demo ROMs: `rehearsal` and one per pier (`l2-loads`, `l2-blocks`, `l2-alu`; block ids `QYLO`, `QYBL`, `QYAL`) |
| `tools/judge.py`, `expected.json` | the judge and its items, boots and per-emulator profiles |
| `tools/cvtools/qyart.py` | title picture and icon |

## Adding a measurement

1. Write the stub (`stubs9.s` or `stubs7.s`), classify its raw words in `measure.c` / `step7.c` with constants
   from GBATEK. Store the raw words in the raw page.
2. Give the item a slot in `game/ledger.h`, an entry in `expected.json` (`level` 2, `role` target, `done` bit of its
   pier, `source` `unconfirmed` until a hardware run agrees) and add it to the profiles you measured.
3. `python3 scripts/mklevels.py` from the repository root, then `--check`.

## Build

```sh
./build.sh                  # quayholt.nds
./build.sh DEMO=rehearsal   # the rehearsal ROM (Makefile DEMOS)
./build.sh demos            # rehearsal and every pier demo
./build.sh -j8 release      # build/release/{rom,demos,judge} (tag quayholt-vX.Y.Z runs this in CI)
./build.sh clean
```

melonDS profiles are interpreter profiles (the headless runner has no JIT); run the app with the JIT off when
you compare by hand.
