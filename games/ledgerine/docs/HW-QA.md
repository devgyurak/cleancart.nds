# Ledgerine hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/ledgerine-hw-support.nds` on the card instead of `ledgerine.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Nothing in Ledgerine has been measured on a console yet. This is the checklist for the run, so the first values can
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
- `expected.json`: for every item whose hardware value matches the ROM's expectation change `source` to `hw:ds-lite`. If the
  hardware disagrees the item is a finding: record the value and decide whether the ROM or the expectation is wrong (a BIOS
  item may only differ with the BIOS; note which BIOS the console runs). Add a profile `hw-ds-lite` (the failing items) to `known`.
- Never commit anything under `build/qa/`.
