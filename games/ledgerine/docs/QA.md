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
| `press-min` | `ledgerine-press-min.nds`: only `-2^63 / -1` in divider mode 2 (item `math-overflow-min64`); result block id `0x4D50444C` (`LDPM`), values[0] = 1 when done, values[1] bit 0 = the check. It is a ROM of its own because DeSmuME's divider hangs the whole emulator on this division | melonDS 1.1 soft / GL classic / GL compute: OK; DeSmuME: hangs (not graded) |
| `rehearsal` | `ledgerine-rehearsal.nds` boots straight into the rehearsal (no save chip, no NitroFS): every room's audit; result block id `0x4852444C` (`LDRH`), layout in `game/ledger.h`; graded by `python3 tools/judge.py` (boot `rehearsal`) | melonDS 1.1 soft / GL classic / GL compute: profile MATCH, see below |

Hardware: not run yet, see [HW-QA.md](HW-QA.md).

## Judge profiles (2026-11-16, build of this commit)

| Emulator | NG items | Notes |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute (headless, built-in replacement BIOS) | `ipc-empty-last` (empty read returns the next cell: word 7A000001h, not the last word B), `dma-itcm-runs` (the CPU halts for the whole DMA: 000d7f6b ticks against 00090027 for the DMA alone), `bios-arm7-guard` (the replacement BIOS copies from the BIOS area) | profiles `melonds-1.1-software`, `-gl-classic`, `-gl-compute` MATCH; identical on the three renderers |
| DeSmuME (py-desmume 0.0.9, HLE BIOS) | `math-div0`, `ipc-empty-last`, `ipc-disabled`, `timer-reload-deferred`, `dma-regs-unchanged`, `dma-itcm-runs`, `bios-arm7-guard` | profile `desmume-pydesmume-0.0.9` MATCH. The rehearsal used to hang there: DeSmuME stops dead (the host divides `INT64_MIN` by -1) on the mode 2 `-2^63 / -1` division, which is why that check now lives in the `press-min` ROM and the game and rehearsal never do it, and the QA run does it only in its judge suite, which needs an SD card (the lever of room 1 offers mode 0 only for the lowest debt). Worth reporting upstream |

The three NG items are results to check against hardware (items stay gating because GBATEK states them); the hardware run
(`docs/HW-QA.md`) would tell whether the emulator or the reading of GBATEK is at fault.

## Auto QA run

Each `cp` line of the QA log ends with `gx=uf<n>,of<n> rdl=<min>`: for the span since the previous checkpoint, the frames that showed DISP3DCNT bit 12 (colour buffer RDLINES underflow) and bit 13 (polygon/vertex RAM overflow) set, and the lowest RDLINES_COUNT (buffered lines minus 2, 0..46; `rdl=-` when no frame was sampled in the span, e.g. two checkpoints in one frame). Both flags are sticky and only read, never cleared by QA, so a count is "frames the flag was up". That holds only while the game does not clear them: a game that acknowledges bit 13 itself (`g3_overflowed()` in Corvane, Elevane, Heddlemere, Hushwell, Lightenette, Paneveil) reports its acknowledgements through `qa_game_gx_acks()` and each one counts as a frame with the flag up; it writes bit 12 as 0 so the underflow flag survives. The figures are a hint like `lag`: never hashed, never compared (`tools/qa/compare.py --info` lists them side by side), and `0,0` is what a scene without 3D shows. Emulators rarely raise these flags; hardware may.

Hidden mode of the release ROM (`ledgerine.nds`, SHA-256 identical to what ships): on the title screen press L and R, then X and Y, and
hold all four for 120 frames. The game plays the normal flow by itself: title, rooms 1 to 6 (the pilot finds each line's
dials with the GBATEK model, turns them with the keys and pulls the lever; Start closes the books), the credits and the title
again, then ends with `RESULT PASS`. Log on the SD card (`cleancart/ledgerine/qa-<date>-<time>.log`), result block `QARN` in the
rehearsal's layout, graded with `python3 games/ledgerine/tools/judge.py --profile <profile> <log>` (boot `qa`, extra item
`qa-verdict`). Headless: `SDDIR=out/lg/x tools/melonds/mds.sh run games/ledgerine/ledgerine.nds 3000 out/lg/x "300:L:140,300:R:140,300:X:140,300:Y:140"`
(create `out/lg/x/cleancart/ledgerine/` first; `RENDERER=gl|compute` for the GL renderers).

**Judge suite.** With the QA log on an SD card, the run ends (after `RESULT PASS` and the `QARN` block) with the `press-min` check and writes
`res-<stamp>-ledgerine-press-min.log` beside the log (`suite demo=press-min saved=1` in `suite-<stamp>.log`; no SD card: `suite skipped=no-sd`, which is the DeSmuME case). The `QARN` block is put back afterwards.
So one QA run grades every item: the `qa` boot from the block (item `rehearsal-done` = all six rooms closed), `press-min` from the res file (`tools/qa/ingest.py` on a card, `judge.py --boot press-min` headless).

| Renderer | QA run (2026-11-16) | `judge.py --profile` |
|---|---|---|
| melonDS 1.1 software | `RESULT PASS` at frame 1770, 6 checkpoints, `parse_log.py` ok | `melonds-1.1-software` MATCH |
| melonDS 1.1 GL classic | PASS | `melonds-1.1-gl-classic` MATCH |
| melonDS 1.1 GL compute | PASS | `melonds-1.1-gl-compute` MATCH |
| DeSmuME (py-desmume 0.0.9) | verdict 1, block at frame 1770 | `desmume-pydesmume-0.0.9` MATCH |

Two software runs give identical checkpoint hashes (`tools/qa/compare.py`, 6 checkpoints); `tools/qa/lint.py` passes. The
software run has no save chip (homebrew run for the SD card): the save mirror read back identical, the chip write is not
verified. Real DS Lite + R4: not run yet.

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/ledgerine/autoqa` on the SD card (the folder must exist). The first title screen of the boot starts
the same QA run as the key combo and renames the file to `autoqa.used-<date>-<time>` before it starts, so the run happens once.
If the rename fails nothing starts and `autoqa-error.log` gets a line. Not tried on a card yet.
