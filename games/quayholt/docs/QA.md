# Quayholt QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`). Run melonDS with its JIT off: the judge
profiles record the interpreter.

| Scene | Check | Status |
|---|---|---|
| Title | the dusk harbour with two cranes, "QUAYHOLT" in stencil letters, the tagline and the X/Select hint on the top screen; licence, ARM7 and save lines on the bottom screen. Start and Select open the test menu, X the credits | headless run (screenshots looked at); not yet by hand in melonDS |
| Test menu | the rehearsal and the credits; B back | headless run only |
| Rehearsal | six pier lines; DONE for the piers the build has (A, B, C), LATER for the others | headless run |
| Credits | scrolls; X opens the licence viewer (needs NitroFS), B back | not yet checked |

## Demo ROMs

| Demo | What it reports | Status |
|---|---|---|
| `rehearsal` | `quayholt-rehearsal.nds`: every measurement the build has (no save chip, no NitroFS); block id `QYRH`, layout in `arm9/source/game/ledger.h`; graded by `python3 tools/judge.py` (boot `rehearsal`) | melonDS 1.1 soft / GL classic / GL compute, DeSmuME: profile MATCH |
| `l2-loads` | `quayholt-l2-loads.nds`: pier A (block id `QYLO`) | profile MATCH on all four |
| `l2-blocks` | `quayholt-l2-blocks.nds`: pier B (block id `QYBL`) | profile MATCH on all four |
| `l2-alu` | `quayholt-l2-alu.nds`: pier C (block id `QYAL`) | profile MATCH on all four |

## Judge profiles (2026-11-20, build of this commit)

| Emulator | NG items | Notes |
|---|---|---|
| melonDS 1.1 soft / GL classic / GL compute (interpreter) | `ldrh7-odd`, `ldrsh7-odd`, `thumb7-ldrh-odd`, `stm7-base-new`, `stm7-empty`, `thumb7-stmia-base-new`, `stm-pc12`, `alu7-rn-pc12`, `cp15-prev5-bit15` | the CPU core is shared, so the renderers agree. `ldm9-empty` also differs (informational) |
| DeSmuME py-desmume 0.0.9 | the nine above plus `ldm7-base-nowb`, `ldm9-base-wb` | `ldm9-empty` also differs (informational) |

Hardware: not run yet. The first DS Lite run decides the `unconfirmed` items (`docs/HW-QA.md`, to be written).
