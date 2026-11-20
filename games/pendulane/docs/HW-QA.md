# Pendulane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/pendulane-hw-support.nds` on the card instead of `pendulane.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block (it carries the rehearsal items) as `res-*-pendulane-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

**Runs 1 and 2 on a DS Lite (2026-11-17), recorded at the end of this page:** QA run PASS both times. The two gating NG items (`mem7-main-nonseq`, `mem7-code-main-seq`) were findings about our expected values; the expectations were set from the console afterwards (see "Expected values from the runs").

Before run 1 nothing had been measured on a console. This is the checklist for the run, so the first values can be recorded and the
`unconfirmed` items in `expected.json` can be promoted. The console is operated by a person (skill `nds-hw-qa`): create the empty folder
`cleancart/pendulane/` on the card and the rehearsal writes its result block there; `python3 tools/qa/ingest.py /Volumes/<card> --game
pendulane` grades it with `judge.py --hardware --partial`. Hardware NG items are findings, not profiles.

**Order for the first R4 run:** put the empty file `cleancart/pendulane/nocart` on the card first and check that the SD log is written; then run again without the marker to see whether the card kernel takes the cartridge bench. When grading rehearsal and QA logs together, grade each log on its own: `judge.py --partial` with both blocks grades only the last one.

## What the hardware has to answer

| Question | Where it shows |
|---|---|
| Does DIV mode 0 take 18 bus clocks, modes 1 and 2 34, SQRT 13 (GBATEK "DS Maths")? | rehearsal values 3 to 5 (10, 32, 0 half cycles; window ±2) |
| Is the firmware SPI byte 8 x (64 - 8) = 448 cycles longer at 512 kHz than at 4 MHz (dividers /8 and /64 are inferred from the nominal rates)? | rehearsal value 6 (448, window ±8); raw edges 28 to 31 |
| Is the ARM7 main RAM non-sequential 32-bit access 10 cycles (GBATEK's text) or 5 ("reportedly")? Both emulators disagree with 10. | rehearsal value 16 (576 for 10; 256 for 5) |
| Do the other ARM7 memory timings (main RAM S32 = 2, code from main RAM, VRAM S32 = 2) match? | values 15, 17, 18 |
| Does a one-shot channel's Busy clear 18 samples after the start for 16 PCM16 samples and 42 for 32 ADPCM samples (GBATEK: Busy drops at the start of the last sample)? Both emulators clear one sample later. | rehearsal values 10 and 11 |
| Do PSG, PCM16 and ADPCM start 1, 3 and 11 samples after the start write, and does a Hold restart play the old value for one sample and then two zero samples? | values 12, 13, 14 (2, 8, 258) |
| Does Busy clear at a constant rate (the slope of 32 extra samples is 0 cycles within 16)? | value 20 (informational; the value differs between boot paths on both emulators) |
| (If a card kernel does not take the cartridge bench's slot-1 command, put an empty file `cleancart/pendulane/nocart` on the card: the bench is skipped and the two cart items stay unmeasured. The item is `env: "cart"` and n/a on a console anyway.) Does +0x100 gap1 clock pulses lengthen a slot-1 B8 command by 1280 bus cycles at CLK/5 and 2048 at CLK/8? The items are `env: "cart"`: a flash card kernel cannot be asked, but record what the card reads. Does B8 leave the KEY2 state of the card alone (NitroFS reads afterwards)? | values 8, 9; raw edges 19 and 22 |
| Does the ARM9 write buffer move the edge (the fixed path is not the same on the console)? | the three differences stay in their windows; the raw edges in values 24 to 27 are information |

## Record

| Date | Console | Card | Result |
|---|---|---|---|
| 2026-11-17 | DS Lite (NTR) | R4 Plus (Wood) | QA run PASS; 2 gating NG, see run 1 below |
| 2026-11-17 (console clock 2026-01-15 12:06) | DS Lite (NTR) | R4 Plus (Wood) | QA run PASS; the same 2 gating NG, see run 2 below |

## Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256 | `pendulane-hw-support.nds` v0.2.1 `3ab56cf90dcd375c0fb8e990b20b2f5a4a3ef1f9a392b1263d062db4bc7e6a70` (439,660 bytes, same as the release); melonDS 1.1 soft was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 53 lines, 7 checkpoints, build `dc770b923005`, started with `start=marker`. Suite: QA block only (`suite block block=qa saved=1`; no suite demos, so no `suite end` line by design). Judge: L1 7/7, L3 11/13 (**2 gating NG: `mem7-main-nonseq`, `mem7-code-main-seq`**) +1 informational; n/a 3 (env: `cart-gap1-clk5`, `cart-gap1-clk8`, `pre-cart-busy`) |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=0 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: ch1-ch5, judge and credits hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 5 / 6 / 6 / 6 / 5 / 2 / 0 (ch1-ch5, judge, credits); melonDS soft 0 everywhere |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0` everywhere; `rdl=63` on the console (no 3D), melonDS 46 |

| Item | Hardware | melonDS 1.1 soft | Differs? |
|---|---|---|---|
| `mem7-main-nonseq` (L3, GBATEK, want 574..578) | **383** (NG) | 384 (NG, profile) | the console agrees with melonDS, not with GBATEK's 9 x 64: the expectation was wrong, now 376..392, source `hw:dslite-ntr` (see below) |
| `mem7-code-main-seq` (L3, GBATEK, want 126..130) | **142** (NG) | 128 | yes: sequential ARM7 code fetch from main RAM is slower on the console: the expectation was wrong, now 139..145, source `hw:dslite-ntr` (see below) |
| `snd-hold-restart` / `snd-pcm-busy` / `snd-adpcm-busy` | 258 / 18 / 42 (OK) | 7 / 19 / 43 (NG, profile) | the melonDS profile NGs are real emulator differences |
| `snd-rate-slope` (unconfirmed) | 13 | 830 | inside -16..16 on the console |
| `math-div32-vs-sqrt`, `spi-512k-minus-4m` | 10, 448 | 9, 449 | both OK |
| timing words values[24..31], [36..39] (raw edges, not judged) | 2-3 higher than melonDS | | yes, not judged |
| the other judged items | as melonDS soft | | no |

The NG items were left alone in the PR that recorded run 1; they are dealt with in "Expected values from the runs" below.

## Run 2: QA run and judge suite, 2026-11-17 (console clock 2026-01-15 12:06)

The same console, card and ROM as run 1 (`pendulane-hw-support.nds`, build `dc770b923005`), run again; the console clock read 2026-01-15, 12:06, so the files say `res-20260115-*` (run 1: `res-20020115-*`).

| Field | Value |
|---|---|
| `judge.py --hardware` | QA block: the same verdicts as run 1 (before the expected values were changed: L3 11/13, the same 2 gating NG, 383 and 142 again). n/a 3 (env: `cart-gap1-clk5`, `cart-gap1-clk8`, `pre-cart-busy`) |
| Difference from run 1 | `snd-rate-slope` (`values[20]`, raw `[46]`) 13 -> -10, total lag frames 30 -> 32, block frame 4292 -> 4294. Every other word is equal |

## Expected values from the runs

| Item | Change | Why |
|---|---|---|
| `mem7-main-nonseq` | range `376..392`, source `hw:dslite-ntr`; title: main RAM N32 effective 7 against WRAM 1: 6x64 = 384; GBATEK's table 10 is not the instruction cost | both runs read 383. melonDS reads 384 and now passes; DeSmuME reads 64 and stays NG |
| `mem7-code-main-seq` | range `139..145`, source `hw:dslite-ntr`; title: about 14 cycles over 64 words more than GBATEK's 2 per word; cause undocumented | both runs read 142 (GBATEK 128). melonDS and DeSmuME read 128 and are NG now |
| `snd-rate-slope` | unchanged: `unconfirmed`, -16..16, informational | the console value moved between runs (13, then -10), so it is not a value to expect |
| `snd-pcm-busy`, `snd-adpcm-busy`, `snd-onset-pcm-psg`, `snd-onset-adpcm-pcm`, `snd-hold-restart`, `spi-512k-minus-4m`, `mem7-main-seq`, `mem7-vram-seq`, the three `math-*` items and the five `pre-*` items that are not `env` | `level_basis` gets `hw:games/pendulane/docs/HW-QA.md` | the console agrees with GBATEK (both runs); source stays `GBATEK` |
| In-game ledger | from v0.2.2 the ledger screen (`game/ledger.c`) uses the same windows: MAIN NONSEQ 384 +- 8, CODE IN MAIN 142 +- 3. Only the colours and the seal mark of chapter 4 depend on them; the contraptions' station costs (`pdorder/model.py`) are unchanged. melonDS reads MAIN NONSEQ 384 (in the window) and CODE IN MAIN 128 (outside it), so its chapter 4 still shows NO SEAL |
| Profiles | `mem7-main-nonseq` leaves the three melonDS profiles' `ng` (DeSmuME keeps it); `mem7-code-main-seq` joins `ng` of all four | |
