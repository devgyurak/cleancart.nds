# Pendulane

Original automaton-workshop game for the DS (work in progress). Pendulane is the third L3 game: target level 3, area
"CPU, bus and sound timing". It measures timings on the console itself and judges them against GBATEK in the ROM.
Human docs (English, each with a Korean twin `*-KO.md`): `README.md`, `docs/QA.md`, `docs/HW-QA.md`, `ASSETS.md`; plus
`THIRD_PARTY_NOTICES`. Repo-wide rules are in `../../AGENTS.md`. The plan is `out/WORKPLAN-pendulane.md` (not committed).
Created from paneveil (arm7, common, `arm9/source/core` and `gfx` are corvane's layers: keep fixes in sync).

Status (PR 3 of the stack): skeleton, the timing core (ITCM edge finding), the math bench, the ARM7 bench (SPI byte time, ARM7 memory timings) the sound bench and the cartridge bench; scenes: title (a text
scaffold, `TODO(title)`), the bench ledger, credits; the hidden QA run reaches the credits and runs the bench once.
Next (stack plan): the cartridge bench, the device game, the rehearsal and QA
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
- `arm7/source/bench7.c` + `arm9/source/judge/bench7.c`: the ARM7 bench (`common/cv_ipc.h`: `CV_FIFO_BENCH`, ops `CV_BENCH_SPI`, `CV_BENCH_MEM`).
  The ARM9 sends `op << 24 | scratch address`, parks in ITCM with IME off polling the IPCSYNC nibble (no main RAM access), and the ARM7
  runs the request inside its FIFO handler (nothing can interrupt it), sends 8 result words as a datamsg and raises the done nibble
  (0xC / 0xD alternating). SPI: the firmware chip is read with RDSR and chip select held, one data write starts a byte, a nop sled
  (`pd7_probe`, ARM7 WRAM) and one read of SPICNT bit 7; binary search for the smallest sled length at which busy is clear (monotone),
  per baud rate. Memory: unrolled `pd7_ldm12` / `pd7_ldm4` / `pd7_ldr` x64 timed with ARM7 timer 1 (bus cycles), the same code for every
  memory (only the base address changes), reported as a difference of two runs; code in main RAM runs from a 640 byte ARM9 scratch
  buffer the ARM7 fills with nops (the ARM9 invalidates its cache lines first and never touches it again); VRAM bank C is mapped to
  the ARM7 for the run. The ARM7 sound engine uses timers 0 and 3, the bench timer 1.
- `arm7/source/benchsnd.c` (op `CV_BENCH_SND`, 1 KB buffer layout in `cv_ipc.h`): one sample period P = 16384 bus cycles (timer reload 0x10000 - 8192).
  Busy: a one-shot channel is started and the time to the clear of SOUNDxCNT bit 31 is measured with a 32-bit stopwatch (ARM7 timers 1 + 2), reported
  in samples (rounded). Onset: the channel runs alone and capture 0 records the left mixer (capture 0 takes its rate from channel 1's timer, so channel 1
  plays silence at the same reload); the capture starts first and the channel 4.5 samples later, so every capture sample falls in the middle of a channel
  sample; the report is the index of the first non-zero sample, and the ARM9 subtracts two runs (the constant between index and time cancels). Hold
  restart: (old value still there << 8) | zero samples before the new data. Every wait is bounded by 80 sample periods (a stuck flag costs 40 ms).
  The ADPCM block is the asset `pd_snd_adpcm` (predictor 0, step index 50, nibbles 7). `snd-rate-slope` is informational (DeSmuME reads a different
  value on every run). The bench takes about 0.2 s with IME off on the ARM9.
- `arm9/source/judge/benchcart.c`: slot-1 gap1. Command B8 with **no data block** (block size 0: nothing has to be read, so the transfer ends by itself; with a
  4-byte block melonDS's transfer time stopped being linear in gap1 once the card DMA had to be re-armed between trials), ROMCTRL bit 28 set, the header's own
  control bits kept. The delay between the start write and the read of ROMCTRL is a countdown loop (`subs` + taken branch, ITCM) plus 0-7 nops: the loop is too
  long for a sled (the transfer is thousands of cycles). Its cycles per iteration are measured with the stopwatch over two long runs and must be within 1/16 of
  a whole number (melonDS 3, DeSmuME 4), else the bench stops. Binary search for the smallest delay at which bit 31 is clear; the report is edge(gap1 + 0x100)
  minus edge(gap1) in bus cycles at CLK/5 and CLK/8. An edge at 0 is a floor and is not reported (DeSmuME ends the transfer at once). Items have `env: "cart"`
  (`judge.py --hardware` marks them n/a: a flash card cannot be asked); whether B8 leaves the card's KEY2 state alone on hardware is not verified here.
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
