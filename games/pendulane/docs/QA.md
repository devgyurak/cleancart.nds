# Pendulane QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | scaffold text title (`TODO(title)`), licence/ARM7/save lines on the bottom screen; Start opens the bench ledger, X the credits | headless run only |
| Bench ledger | the four math edges (ARM9 cycles) next to the book values 36 / 68 / 68 / 26, the three differences next to 10 / 32 / 0, "MATH RESULTS RIGHT", B back | headless run only |
| Credits | scrolling credits, X opens the licence viewer | not yet checked |
| rehearsal demo | `pendulane-rehearsal.nds` boots straight into the bench; result block id `0x48524450` (`PDRH`); grade with `judge.py --profile` as in the README | ✅ 2026-11-17: all four profiles (DeSmuME py-desmume 0.0.9, melonDS 1.1 software / GL Classic / GL Compute) read 5 of 5 gating items OK. Differences: DeSmuME 10 / 32 / 0, melonDS 9 / 32 / 0 (the book says 10 / 32 / 0; the window is ±2). No hardware values yet |

Hardware: not run yet, see [HW-QA.md](HW-QA.md).

## Auto QA run

Hidden mode of the release ROM (`pendulane.nds`): on the title screen press L and R, then X and Y, and hold all four for 120 frames. It
plays the normal flow (title → credits → title), runs the bench once and ends with `RESULT PASS`. Log on the SD card
(`cleancart/pendulane/qa-<date>-<time>.log`). Headless: `SDDIR=out/pd/x tools/melonds/mds.sh run games/pendulane/pendulane.nds 2400 out/pd/x
"300:L:140,300:R:140,300:X:140,300:Y:140"` (create `out/pd/x/cleancart/pendulane/` first). Verified 2026-11-17 in melonDS 1.1 (software):
`RESULT PASS` at frame 1137, 2 checkpoints, `tools/qa/compare.py` on two runs 0 differences, `CPU9 cpsr` mode 0x1f; the `QARN` block graded with
`judge.py` on melonDS 1.1 software, GL Classic, GL Compute and DeSmuME: OK on each.

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/pendulane/autoqa` on the SD card (the folder must exist). The first title screen of the boot starts the same QA run and
renames the file to `autoqa.used-<date>-<time>` before it starts.
