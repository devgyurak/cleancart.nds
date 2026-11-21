# Quayholt hardware QA (DS Lite + R4) — plan

[한국어](HW-QA-KO.md)

Status: **not run yet.** Nothing in this game has been measured on a console. Every expectation comes from GBATEK and
from reading emulator sources, and the emulator profiles in `expected.json` only record what the emulators do. The
run decides the seven `unconfirmed` items and settles the items that rest on a reading of GBATEK alone. The console
is operated by a person; the steps are what to do and what to write down. See the skill `nds-hw-qa`.

**Wood-style R4 kernels:** put `build/release/rom/quayholt-hw-support.nds` on the card instead of `quayholt.nds` (the same
ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, otherwise the ROM sees no SD card).
The demo and rehearsal ROMs keep their own code.

## Risk, and the order to run

The ARM9 measurements of piers D and E (`l2-pu`, `l2-tcm`) move the exception vectors, replace protection regions and
relocate the DTCM for a few microseconds with IRQs masked. They restore everything, but a console that behaves
differently from the emulators could hang there (a power cycle fixes it) or lose the SD log. So run the safe ROMs first:

1. `quayholt-rehearsal.nds` is **not** the first: it includes D and E. Start with `quayholt-l2-loads.nds`, then
   `l2-alu`, `l2-cache` (plain user-mode code), and `l2-blocks` after them: its `ldm9-empty` makes the ARM9 execute an
   empty-list LDM, and a console that reads R15 from an empty list would jump to the fill word and hang (a power cycle
   fixes it; the other six items of the ROM are plain).
2. Then `quayholt-l2-tcm.nds`, then `quayholt-l2-pu.nds` (the exception harness).
3. Then the rehearsal and the auto QA run (`cleancart/quayholt/autoqa`), which also plays all 30 jobs and runs the
   judge suite. Wait for `suite end` in `suite-<stamp>.log` before powering off.

Create the empty folder `cleancart/quayholt/` on the card first: the demos write `res-*-quayholt-<demo>.log` there.
Then `python3 tools/qa/ingest.py /Volumes/<card> --game quayholt` grades every file with `judge.py --hardware --partial`.
Hardware NG items are findings, not profiles.

## What the hardware has to answer (first priority)

| Question | Item |
|---|---|
| Does the ARM9 data cache keep a stale word after a DMA, hold a write until cleaned, drop a dirty line on invalidate, and does the instruction cache keep old code? (all four are unconfirmed; the emulators show the coherent result) | `dcache-dma-stale`, `dcache-wb-dirty`, `dcache-inval-drop`, `icache-stale` |
| Are DTCM reads sent to the bus behind with control bit 17, ITCM reads with bit 19? (melonDS ignores both bits) | `dtcm-load-mode`, `itcm-load-mode` |
| Does an empty-list LDM on the ARM9 move the base by 40h and load nothing? | `ldm9-empty` |
| Does STM {..PC} store the STM's address + 12 on both CPUs? | `stm-pc12` (report melonDS#2312 gives the console value) |
| Does ALU with Rn = PC and a register shift read PC + 12 on the ARM7? | `alu7-rn-pc12` |
| Is the TCM checked against the protection regions (the tests rely on the ITCM mirror at 01000000h being covered)? | the whole of pier E; a data abort in `l2-tcm` would show as a hang |
| Does BKPT take the prefetch abort vector with LR = BKPT + 4 in abort mode? | `bkpt9-pabt` |

For each item whose hardware value matches the ROM's expectation change `source` to `hw:dslite-ntr` in `expected.json`
(an `unconfirmed` item becomes a gating target). If the hardware disagrees the item is a finding: record the value and
decide whether the ROM or the expectation is wrong; an item the emulators cannot be made to agree with moves to
`probes/`. Add a profile `hw-ds-lite` (the failing items) to `known`.

## Steps

1. Build: `cd games/quayholt && ./build.sh -j8 release`. Note `shasum -a 256` of `build/release/rom/quayholt.nds`,
   `quayholt-hw-support.nds` and the demo ROMs.
2. Copy the ROMs to the card, create `cleancart/quayholt/`, run them in the order above and note anything the screen shows.
3. `ingest.py` the card, compare with an emulator run (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
4. Record in this file (a "Run 1" section), `docs/QA.md` and `docs/QA-KO.md` (ROM SHA-256s, console and card, failing items).

Never commit anything under `build/qa/`.
