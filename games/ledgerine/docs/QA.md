# Ledgerine QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | the tower picture, "LEDGERINE", the tagline and the X/Select hint on the top screen; licence, ARM7 and save lines on the bottom screen. Start opens room 1, X the credits, Select the test menu | headless run (screenshots looked at); not yet by hand in melonDS |
| Test menu | six rooms, the rehearsal, the credits; B back | headless run only |
| Room | top: room name, tower, hardware and the ledger lines (`[X]` balanced); bottom: the chosen line, its wanted value, the dials, `LEDGER` (model) and `MACHINE` (hardware) words after a lever pull, `BALANCED` or `DOES NOT BALANCE`, `MACHINE DISAGREES` when the two words differ. Up/down choose a line, left/right dial A, L/R or Y/X dial B, A pulls the lever, Start closes the books when every line balances, B back | headless run with the QA pilot (screenshots of room 1 looked at); not yet by hand |
| Credits | scrolls; X opens the licence viewer (needs NitroFS), B back | not yet checked |

Room 2 (the tube) stops the libnds FIFO handling on both CPUs for about a second per lever pull and the ARM7 sends no
input packets meanwhile: input and sound pause for that moment, then everything resumes (checked only by the QA run, which
plays rooms 3 to 6 and the credits after it).

## Demo ROMs

| Demo | What it reports | Status |
|---|---|---|
| `rehearsal` | `ledgerine-rehearsal.nds` boots straight into the rehearsal (no save chip, no NitroFS): every room's audit; result block id `0x4852444C` (`LDRH`), layout in `game/ledger.h`; graded by `python3 tools/judge.py` (boot `rehearsal`) | melonDS 1.1 soft / GL classic / GL compute: profile MATCH, see below |

Hardware: not run yet.

## Judge profiles (2026-11-16, build of this commit)

| Emulator | NG items | Notes |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute (headless, built-in replacement BIOS) | `ipc-empty-last` (empty read returns the next cell: word 7A000001h, not the last word B), `dma-itcm-runs` (the CPU halts for the whole DMA: 000d7f6b ticks against 00090027 for the DMA alone), `bios-arm7-guard` (the replacement BIOS copies from the BIOS area) | profiles `melonds-1.1-software`, `-gl-classic`, `-gl-compute` MATCH; identical on the three renderers |
| DeSmuME (py-desmume 0.0.9) | not profiled | the headless harness did not finish the rehearsal in 10+ minutes (the ARM7 IPC experiment is the suspect: with the old ROM without rooms 2, 4 and 5 it finished); no profile yet, `expected.json` has none for DeSmuME |

The three NG items are results to check against hardware (items stay gating because GBATEK states them); the hardware run
(`docs/HW-QA.md` when it exists) would tell whether the emulator or the reading of GBATEK is at fault.

## Auto QA run

Hidden mode of the release ROM (`ledgerine.nds`, SHA-256 identical to what ships): on the title screen press L and R, then X and Y, and
hold all four for 120 frames. The game plays the normal flow by itself: title, rooms 1 to 6 (the pilot finds each line's
dials with the GBATEK model, turns them with the keys and pulls the lever; Start closes the books), the credits and the title
again, then ends with `RESULT PASS`. Log on the SD card (`cleancart/ledgerine/qa-<date>-<time>.log`), result block `QARN` in the
rehearsal's layout, graded with `python3 games/ledgerine/tools/judge.py --profile <profile> <log>` (boot `qa`, extra item
`qa-verdict`). Headless: `SDDIR=out/lg/x tools/melonds/mds.sh run games/ledgerine/ledgerine.nds 3000 out/lg/x "300:L:140,300:R:140,300:X:140,300:Y:140"`
(create `out/lg/x/cleancart/ledgerine/` first; `RENDERER=gl|compute` for the GL renderers).

| Renderer | QA run (2026-11-16) | `judge.py --profile` |
|---|---|---|
| melonDS 1.1 software | `RESULT PASS` at frame 1770, 6 checkpoints, `parse_log.py` ok | `melonds-1.1-software` MATCH |
| melonDS 1.1 GL classic | PASS | `melonds-1.1-gl-classic` MATCH |
| melonDS 1.1 GL compute | PASS | `melonds-1.1-gl-compute` MATCH |

Two software runs give identical checkpoint hashes (`tools/qa/compare.py`, 6 checkpoints); `tools/qa/lint.py` passes. The
software run has no save chip (homebrew run for the SD card): the save mirror read back identical, the chip write is not
verified. Real DS Lite + R4: not run yet.

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/ledgerine/autoqa` on the SD card (the folder must exist). The first title screen of the boot starts
the same QA run as the key combo and renames the file to `autoqa.used-<date>-<time>` before it starts, so the run happens once.
If the rename fails nothing starts and `autoqa-error.log` gets a line. Not tried on a card yet.
