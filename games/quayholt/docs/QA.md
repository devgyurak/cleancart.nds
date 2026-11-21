# Quayholt QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`). Run melonDS with its JIT off: the judge
profiles record the interpreter.

| Scene | Check | Status |
|---|---|---|
| Title | the dusk harbour with two cranes, "QUAYHOLT" in stencil letters, the tagline and the X/Select hint on the top screen; licence, ARM7 and save lines on the bottom screen. Start and Select open the test menu, X the credits | headless run (screenshots looked at); not yet by hand in melonDS |
| Test menu | the rehearsal and the credits; B back | headless run only |
| Pier | pier name and progress on top, five jobs with `OK` (balanced) / `!!` (crooked box) marks; up/down choose, A open, L/R other pier, B title | headless run with the QA pilot (screenshot looked at); not yet by hand |
| Job | the order, the wanted chalk word, the quay with the hook, after a lift THE BOOK / THE CRANE words, `BALANCED` and `CROOKED BOX` lines; a job starts on a setup that is not yet the answer; bottom: crane (X, in the jobs of piers A and B that both cranes may work), setup (left/right, L/R by 5), A lift, B back | headless run with the QA pilot (screenshot looked at); not yet by hand |
| Rehearsal | six pier lines; DONE for the piers the build has (A to F), LATER for the others | headless run |
| Credits | scrolls; X opens the licence viewer (needs NitroFS), B back | not yet checked |

## Demo ROMs

`make DEMO=devcheck` is a development aid (not in `DEMOS`, not graded): it runs every device of the harbour on both cranes with every setup and lists the crooked boxes. melonDS 1.1: 93 setups run, 19 crooked (the documented differences above); DeSmuME: 32.

| Demo | What it reports | Status |
|---|---|---|
| `rehearsal` | `quayholt-rehearsal.nds`: every measurement the build has (no save chip, no NitroFS); block id `QYRH`, layout in `arm9/source/game/ledger.h`; graded by `python3 tools/judge.py` (boot `rehearsal`) | melonDS 1.1 soft / GL classic / GL compute, DeSmuME: profile MATCH |
| `l2-loads` | `quayholt-l2-loads.nds`: pier A (block id `QYLO`) | profile MATCH on all four |
| `l2-blocks` | `quayholt-l2-blocks.nds`: pier B (block id `QYBL`) | profile MATCH on all four |
| `l2-alu` | `quayholt-l2-alu.nds`: pier C (block id `QYAL`) | profile MATCH on all four |
| `l2-pu` | `quayholt-l2-pu.nds`: pier D, the control register, BKPT and the protection unit (block id `QYPU`) | profile MATCH on all four |
| `l2-tcm` | `quayholt-l2-tcm.nds`: pier E, tightly coupled memory (block id `QYTC`) | profile MATCH on all four |
| `l2-cache` | `quayholt-l2-cache.nds`: pier F, the caches (block id `QYCA`) | profile MATCH on all four |

## QA run and suite (2026-11-20)

| Check | Result |
|---|---|
| melonDS 1.1 soft + SD image, QA run | `RESULT PASS`: 141 log lines, 7 checkpoints (six piers and the rehearsal), all 30 jobs played by the pilot, the rehearsal's measurements, the credits, then the judge suite (`l2-loads` .. `l2-cache`) |
| `tools/qa/parse_log.py`, `tools/qa/lint.py` | pass |
| two runs, `tools/qa/compare.py` | 7 checkpoints, 0 differences |
| `tools/qa/suite_compare.py` against the stand-alone demos | 6 demos, 0 differ |
| DeSmuME | the QA run reaches the QARN block with verdict PASS and 30 jobs cleared |
| judge `qa` boot, profile `melonds-1.1-software` | MATCH |
| reproducible build | two clean builds, same SHA-256 |

## Judge profiles (2026-11-20, build of this commit)

| Emulator | NG items | Notes |
|---|---|---|
| melonDS 1.1 soft / GL classic / GL compute (interpreter) | `ldrh7-odd`, `ldrsh7-odd`, `thumb7-ldrh-odd`, `stm7-base-new`, `stm7-empty`, `thumb7-stmia-base-new`, `stm-pc12`, `alu7-rn-pc12`, `cp15-prev5-bit15`, `bkpt9-pabt` | the CPU core is shared, so the renderers agree. `ldm9-empty`, `dtcm-load-mode`, `itcm-load-mode` and the four cache items (`dcache-dma-stale`, `dcache-wb-dirty`, `dcache-inval-drop`, `icache-stale`) also differ (informational): melonDS ignores control bits 17 and 19 and keeps no data cache contents; both emulators show the coherent (no-cache) result for all four cache items |
| DeSmuME py-desmume 0.0.9 | the nine melonDS also fails (not `bkpt9-pabt`) plus `ldm7-base-nowb`, `ldm9-base-wb`, `pu-priority`, `pu-background`, `pu-user-ro`, `pu-code-xn` (no protection unit), `dtcm-mirror`, `itcm-size`, `tcm-dma-bus` | `ldm9-empty`, the two load modes and the four cache items also differ (informational) |

Hardware: not run yet. The first DS Lite run decides the `unconfirmed` items (`docs/HW-QA.md`: the plan).
