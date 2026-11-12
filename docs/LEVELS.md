[한국어](LEVELS-KO.md)

# Difficulty levels

Every judged item of every game (each item of a game's `expected.json`, each demo that is
judged) has a level from L1 to L4. The level says how hard the behaviour is for an emulator to get right.

| Level | Name | To pass | Examples |
|---|---|---|---|
| L1 | Function | Only the documented basic behaviour of registers and instructions | DS maths basic division and square root, plain DMA copy, sprite display |
| L2 | Edge | Documented but easy-to-miss boundary values and special cases | empty IPCFIFO read, LDM/STM edges, division by zero, cart address wrap, SQRT of large 64-bit values |
| L3 | Timing | Cycle- or scanline-exact timing | DMA non-sequential timing, start-of-line DMA, polygon RAM bank swap point, VCOUNT writes |
| L4 | Research | Undocumented or not yet known for certain; needs hardware experiments | melonDS source CHECKME areas (shadow polygon edge rules, clipping oddities, texture matrix stack) |

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
  `HW-QA.md` records ("documented" includes this repository's own hardware record), or a public issue that carries hardware values. We do not wait for GBATEK to be corrected.
- **Tolerance counts.** An item whose window is wide enough that exact cycle or scanline timing is not needed to pass
  (a count within a few percent, a frame-sized window) is not L3: it is levelled by the documented feature it checks.
- **Levels are metadata.** They do not change a verdict (OK/NG), the exit code or the known
  profiles. When a level changes, the PR description gives the reason.
- **Items decided by the ROM's own logic** (`source` is `rom`) are levelled by the hardware
  feature their verdict relies on.

## Fields in `expected.json`

| Field | Meaning |
|---|---|
| `"level"` | integer 1 to 4 |
| `"level_basis"` | where the level comes from: `"gbatek:<section>"` (for example `"gbatek:DS Maths"`), `"report:<ref>"` (for example `"report:melonDS#2008"`), `"hw:<file>"` (for example `"hw:games/elevane/docs/HW-QA.md"`), or `"none"` when no source exists: L4 for behaviour nobody has confirmed; items decided by the ROM's own logic (`source` `rom`) may use `none` at the level of the hardware feature they rely on |

`judge.py` of each game rejects an item without a valid `level` (exit code 2, input error) and
prints a per-level summary. `python3 scripts/mklevels.py` lists the items by level in the root
README, and `--check` fails when a level is missing or the README is out of date.

### Environment-dependent items and n/a

- `"env"` (Corvane, Hushwell): `"harness"` (the value depends on input the test harness injects, such as touch coordinates) or `"cart"`
  (save chip and cart behaviour, which differ between emulated and flash carts). `judge.py --hardware` grades such items n/a
  because a real console run cannot reproduce the harness or the cart.
- Without `--profile`, a gating item that is n/a (`na_when`, `requires`) is not a pass: the run exits 1 and prints `NOT GRADED`.
  Use a profile (which lists the n/a items) or `--allow-na`.
- A log line whose ROM name names another game (`hushwell-...` fed to Corvane's judge) is ignored with a warning: the demos of
  two games share block ids.

## Target level and roles

Each game has one **target level** (`"target_level"` in `expected.json`, 1 to 4) and a one-line `"area"`
(for example `"graphics and system"`). The target-level items of a game decide whether it passes; lower
items are prerequisites; higher items belong to the game that owns that level. Games hold no L4 item:
L4 lives in a separate probe collection (`probes/`), which is not a game.

| Level | Game (area) |
|---|---|
| L1 | Corvane (graphics and system), Hushwell (sound and I/O) |
| L2 | Lightenette |
| L3 | Elevane |
| L4 | `probes/`: informational only, recorded per emulator as a profile |

Every item has a `"role"`:

| role | Meaning | Verdict |
|---|---|---|
| `target` | `level == target_level` and inside the game's area | gating |
| `prerequisite` | `level < target_level`, or the same level in another area that a target item relies on (for example Hushwell's `sys-ipc-packets`, the ARM7 to ARM9 packet) | checked and counted apart; a failure is exit code 1 and `PREREQ NG` is printed first: the game's target verdicts cannot be trusted |
| `relocate` | **temporary.** Above the target level or outside the area, due to move; `"to"` names the game or `"probes"` | informational |

Rules (`judge.py` of every game and `scripts/mklevels.py --check` enforce them; a violation is exit code 2 / a failed check):

- a `target` item has `level == target_level`; a `prerequisite` has `level <= target_level`;
- an item above the target level can only be `relocate`; `relocate` needs `"to"`;
- the final state has no `relocate` item (until then `--check` only warns).

Exit codes stay 0/1/2. The output adds `ROLES target a/b OK · prerequisite a/b OK · relocate n informational`.
Split an item rather than move it whole when only part of it is above the target (a loose "about once per line" range is L1
and stays; the exact window is L2 and moves). `python3 scripts/mklevels.py --map` prints a GBATEK section x level x game table.
