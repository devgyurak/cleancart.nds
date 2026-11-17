# Pendulane QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | scaffold text title (`TODO(title)`), licence/ARM7/save lines on the bottom screen; Start opens the bench ledger, X the credits | headless run only |
| Bench ledger | the four math edges (ARM9 cycles) next to the book values 36 / 68 / 68 / 26, the three differences next to 10 / 32 / 0, "MATH RESULTS RIGHT", B back | headless run only |
| Credits | scrolling credits, X opens the licence viewer | not yet checked |
| rehearsal demo | `pendulane-rehearsal.nds` boots straight into the bench; result block id `0x48524450` (`PDRH`); grade with `judge.py --profile` as in the README | ✅ 2026-11-17: all four profiles measured, `judge.py --profile` MATCH on each. melonDS 1.1 (all renderers): 9 of 13 gating targets OK. NG: `mem7-main-nonseq` (386 where GBATEK's 10-cycle N32 gives 576), `snd-pcm-busy` and `snd-adpcm-busy` (19 and 43: Busy clears at the end of the last sample, one later than GBATEK), `snd-hold-restart` (7: no held old value, 7 zero samples); the two onset differences are exactly GBATEK's 2 and 8. DeSmuME py-desmume 0.0.9: 5 of 13 OK; NG: the same sound items plus `snd-onset-pcm-psg` (6: PSG is heard from the first captured sample), and `spi-512k-minus-4m` (the SPI byte ends at once, edge 0), `mem7-main-seq` and `mem7-vram-seq` (1028 against 1022..1026), `mem7-main-nonseq` (68). Math differences DeSmuME 10 / 32 / 0, melonDS 9 / 32 / 0. SPI edges melonDS 59 / 122 / 250 / 508. No hardware values yet |

Hardware: not run yet, see [HW-QA.md](HW-QA.md).

## Auto QA run

Hidden mode of the release ROM (`pendulane.nds`): on the title screen press L and R, then X and Y, and hold all four for 120 frames. It
plays the normal flow (title → credits → title), runs the bench once and ends with `RESULT PASS`. Log on the SD card
(`cleancart/pendulane/qa-<date>-<time>.log`). Headless: `SDDIR=out/pd/x tools/melonds/mds.sh run games/pendulane/pendulane.nds 2400 out/pd/x
"300:L:140,300:R:140,300:X:140,300:Y:140"` (create `out/pd/x/cleancart/pendulane/` first). Verified 2026-11-17 in melonDS 1.1 (software):
`RESULT PASS` at frame 1137, 2 checkpoints, `tools/qa/compare.py` on two runs 0 differences, `CPU9 cpsr` mode 0x1f; the `QARN` block graded with
`judge.py --profile` on melonDS 1.1 software, GL Classic, GL Compute and DeSmuME: MATCH on each.

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/pendulane/autoqa` on the SD card (the folder must exist). The first title screen of the boot starts the same QA run and
renames the file to `autoqa.used-<date>-<time>` before it starts.
