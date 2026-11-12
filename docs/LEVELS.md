[한국어](LEVELS-KO.md)

# Difficulty levels

Every judged item of every game (each item of a game's `expected.json`, each demo that is
judged) has a level from L1 to L4. The level says how hard the behaviour is for an emulator to get right.

| Level | Name | To pass | Examples |
|---|---|---|---|
| L1 | Function | Only the documented basic behaviour of registers and instructions | DS maths basic division and square root, plain DMA copy, sprite display |
| L2 | Edge | Documented but easy-to-miss boundary values and special cases | empty IPCFIFO read, LDM/STM edges, division by zero, cart address wrap, SQRT of large 64-bit values |
| L3 | Timing | Cycle- or scanline-exact timing | DMA non-sequential timing, start-of-line DMA, polygon RAM bank swap point, VCOUNT writes |
| L4 | Research | Undocumented or not yet known for certain; needs hardware experiments | melonDS CHECKME areas (shadow polygon edge rules, clipping oddities, texture matrix stack) |

## Rules

- **The highest level wins.** If an item spans several levels, it gets the highest. A
  boundary-value problem that only passes with exact timing is L3.
- **Documentation decides.** Described exactly in GBATEK: L1 or L2. Timing figures given: L3.
  Not documented, or documented wrongly: L4.
- **L1 or L2.** Inside the same GBATEK entry, the main behaviour is L1; an exception condition,
  a boundary value or an error path is L2.
- **Levels move down with knowledge.** When an L4 behaviour is confirmed on hardware and
  documented, it moves to L2 or L3. A level reflects what is known now, so it can change over time.
- **What counts as confirmed on hardware.** A public, reproducible hardware result: this repository's
  `HW-QA.md` records, or a public issue that carries hardware values. We do not wait for GBATEK to be corrected.
- **Levels are metadata.** They do not change a verdict (OK/NG), the exit code or the known
  profiles. When a level changes, the PR description gives the reason.
- **Items decided by the ROM's own logic** (`source` is `rom`) are levelled by the hardware
  feature their verdict relies on.

## Fields in `expected.json`

| Field | Meaning |
|---|---|
| `"level"` | integer 1 to 4 |
| `"level_basis"` | where the level comes from: `"gbatek:<section>"` (for example `"gbatek:DS Maths"`), `"report:<ref>"` (for example `"report:melonDS#2008"`), `"hw:<file>"` (for example `"hw:games/elevane/docs/HW-QA.md"`), or `"none"` when no source exists (usually L4) |

`judge.py` of each game rejects an item without a valid `level` (exit code 2, input error) and
prints a per-level summary. `python3 scripts/mklevels.py` lists the items by level in the root
README, and `--check` fails when a level is missing or the README is out of date.
