# Ledgerine hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/ledgerine-hw-support.nds` on the card instead of `ledgerine.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block and `press-min` as `res-*-ledgerine-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

**Runs 1 and 2 on a DS Lite (2026-11-17), recorded at the end of this page:** QA run PASS both times. The one gating NG item (`ipc-empty-cleared`) was a finding about our expected value; the expectation was set from the console afterwards (see "Expected values from the runs").

Before run 1 nothing had been measured on a console. This is the checklist for the run, so the first values can
be recorded and the items that only an emulator or a reading of GBATEK stands behind can be settled. The console is
operated by a person; the steps below are what to do and what to write down.

**Automatic route (skill `nds-hw-qa`):** create the empty folder `cleancart/ledgerine/` on the card and the rehearsal and
the `press-min` ROM write their result blocks there (`res-*-ledgerine-rehearsal.log`, `res-*-ledgerine-press-min.log`);
an empty file `cleancart/ledgerine/autoqa` starts the QA run without a button hold. `python3 tools/qa/ingest.py
/Volumes/<card> --game ledgerine` then grades everything with `judge.py --hardware --partial`. Hardware NG items are
findings, not profiles. Old `res-*` logs on the card are cleared first.

## What the hardware has to answer

| Question | Item / where it shows |
|---|---|
| Does an empty FIFO read return the last word received (GBATEK) or the next cell (melonDS) or 0 (DeSmuME)? | `ipc-empty-last`, rehearsal `values[41]` (the word read) |
| Does a disabled FIFO ignore a write without error and show the oldest word when read? (DeSmuME says no) | `ipc-disabled`, `values[2]` bits 5 and 6 |
| Does the CPU keep running ITCM code while a long DMA runs? (melonDS halts it; the ratio of the two timer readings is 1.5 when serialized, about 1.0 when it runs on) | `dma-itcm-runs`, `values[30]` and `values[31]` (DMA alone, DMA plus loop in timer ticks) |
| Does the ARM7 CpuSet refuse a source in the BIOS area? (melonDS's replacement BIOS copies) | `bios-arm7-guard`, `values[6]` bit 4 |
| What does the 32-bit divider return for a divide by zero in the upper result word, and does the DIV0 flag follow the whole 64-bit denominator? | `math-div0`, `math-div0-flag-upper`, `values[8..11]` |
| Does a reload written while a timer runs wait for the overflow? Do SAD, DAD and CNT_L keep their initial values after a transfer? (DeSmuME says no to both) | `timer-reload-deferred`, `dma-regs-unchanged` |
| Is `-2^63 / -1` in mode 2 `8000000000000000h` with no DIV0? (run `ledgerine-press-min.nds`: it hangs DeSmuME) | `math-overflow-min64` |
| Do ARM9 byte writes to LCDC VRAM really get ignored? Does the whole game run to the credits (auto QA run)? | `vram-byte-write`, game ROM, hidden QA mode |

## Steps

1. Build: `cd games/ledgerine && ./build.sh -j8 release`. Note `shasum -a 256` of `build/release/rom/ledgerine.nds` and of
   `build/release/judge/ledgerine-rehearsal.nds` and `ledgerine-press-min.nds`.
2. Copy the three ROMs to the R4 card and create `cleancart/ledgerine/`.
3. **Rehearsal**: start `ledgerine-rehearsal.nds`, touch nothing, wait about five seconds until the six rooms read DONE.
4. **press-min**: start `ledgerine-press-min.nds` (nothing is drawn; wait two seconds).
5. **Auto QA run**: leave an empty `cleancart/ledgerine/autoqa` on the card (or hold L+R+X+Y for two seconds on the title). The
   game plays itself to the credits (about 30 seconds) and writes `cleancart/ledgerine/qa-*.log`. Room 2 pauses input and sound for
   about a second per lever pull: nothing else should be off.
   Then `python3 tools/qa/ingest.py /Volumes/<card> --game ledgerine` and compare with an emulator run
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## Recording

- `docs/QA.md` and `docs/QA-KO.md` (twins): ROM SHA-256s, console and card, the failing items, the QA log header.
- `expected.json`: for every item whose hardware value matches the ROM's expectation change `source` to `hw:dslite-ntr`. If the
  hardware disagrees the item is a finding: record the value and decide whether the ROM or the expectation is wrong (a BIOS
  item may only differ with the BIOS; note which BIOS the console runs). Add a profile `hw-ds-lite` (the failing items) to `known`.
- Never commit anything under `build/qa/`.

## Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256 | `ledgerine-hw-support.nds` v0.2.1 `8e2fbdfad675ebfa14246c44d99eab59aa9eacf88c2d52d6fb9b2ca25c7de2dc` (455,020 bytes, same as the release); melonDS 1.1 soft was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 27 lines, 6 checkpoints, build `8821063cd048`, started with `start=marker`. Suite: QA block and the `press-min` demo `saved=1`; `suite end demos=1 saved=1`, all 12 probes `ok=1`, no `suite done` line (see the note). Judge: QA block L1 11/11, L2 28/29 (**1 gating NG: `ipc-empty-cleared`**); `press-min` L2 1/1. 40 of 41 OK, no n/a |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | room2 (room1 equal): room2-room6 hashes differ from melonDS 1.1 soft |
| Lag frames | 3 / 11 / 54 / 63 / 13 / 2 (room1-room6); melonDS soft 0 / 10 / 53 / 62 / 11 / 1 |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0` everywhere; `rdl=63` on the console (no 3D in this game), melonDS 46 |

| Item | Hardware | melonDS 1.1 soft | Differs? |
|---|---|---|---|
| `ipc-empty-cleared` (L2, GBATEK, mask 0x8) | **0x0** (NG) | 0x8 | yes: the expectation was wrong, now mask 0xC / expect 0x4, source `hw:dslite-ntr` (see below) |
| `ipc-empty-last` | 0x3 (OK) | 0x2 (NG, profile) | the melonDS profile NG is a real emulator difference |
| `bios-arm7-guard` | 0x10 (OK) | 0x0 (NG, profile) | as above |
| `dma-itcm-runs` | 0x20 (OK) | 0x0 (NG, profile) | as above |
| QA block values[2], [4], [6], [30], [31] (not judged) | 247, 63, 31, 590170, 590170 | 254, 31, 15, 589864, 884598 | yes, not judged |
| the other judged items | as melonDS soft | | no |

Note: the suite now writes `suite end` before its final reset (#186) and it is on the card, as are the 12 probe lines after the reset up to `wait60`. Only the closing `suite done` line, the last write of the run, is missing (melonDS writes it). So the reset is not what loses the line: the last line written to the card is lost. This is still open.

The NG item was left alone in the PR that recorded run 1; it is dealt with in "Expected values from the runs" below.

## Run 2: QA run and judge suite, 2026-11-17 (console clock 2026-01-15 12:04)

The same console, card and ROM as run 1 (`ledgerine-hw-support.nds`, build `8821063cd048`), run again; the console clock now read 2026-01-15, 12:04, so the files say `res-20260115-*` (run 1: `res-20020115-*`).

| Field | Value |
|---|---|
| `judge.py --hardware` | QA block and `press-min`: the same verdicts as run 1 (before the expected values were changed: 1 gating NG, `ipc-empty-cleared`, `values[2]` = 0xF7 again; after: QA block 40 of 40 and `press-min` 1 of 1 OK on all four logs of both runs, NG 0) |
| Difference from run 1 | judged values equal. Not judged: `values[19]` and `[20]` 58437 -> 58438, `[21]` 16397 -> 16398, total lag frames 146 -> 145; the `press-min` block frame 1382 -> 1383 |

## Expected values from the runs

| Item | Change | Why |
|---|---|---|
| `ipc-empty-cleared` | `mask 0xC`, `expect 0x4`, source `hw:dslite-ntr`; title: an empty read after the remote side cleared its send FIFO does not return 0 | GBATEK says 0; both runs read `values[2]` = 0xF7 (bit 3, "returned 0", clear; bit 2, the error acknowledge, set). The raw word of that read was not recorded in these runs, only that it is not 0; from v0.2.2 it is in `values[44]` (and IPCFIFOCNT right after it in `values[45]`), to be read on the next run. melonDS 1.1 soft and DeSmuME both read `values[44]` = 0 and `values[45]` = 0xC101 (receive FIFO empty, error bit set). The mask keeps bit 2 in the check, so the item also fails if the acknowledge broke |
| `ipc-empty-last`, `dma-itcm-runs`, `bios-arm7-guard` | `level_basis` gets `hw:games/ledgerine/docs/HW-QA.md` | the console agrees with GBATEK; melonDS (and DeSmuME) do not |
| Profiles | `ipc-empty-cleared` is in `ng` of all four (melonDS soft / GL classic / GL compute, DeSmuME) | every emulator returns 0 there, the console does not |
