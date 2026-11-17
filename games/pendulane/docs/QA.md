# Pendulane QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | brass lettering "PENDULANE" (generated tile BG) with a pendulum of sprites swinging in front of it (about 32 degrees each side, 150 frames per swing), the tagline, a blinking START line, the pendulum theme playing (stopped when leaving); licence/ARM7/save lines on the bottom screen; Start opens the contraption list, Y the bench ledger, X the credits | headless run only |
| Contraption list | "THE WORKSHOP", the chapter name and how many are rung, the four contraptions with RUNG marks, left/right changes the chapter, up/down the contraption, A starts, B back | headless run only |
| Contraption | top: chapter, name, pendulum ticks, plan ticks with a gauge, one line per slot (the chosen station and its cost); up/down chooses a slot, left/right the station, A rings the bell when the ticks match ("THE BELL RINGS", next contraption) or says TOO MANY / TOO FEW TICKS; X runs the bench once and fills the bottom screen's ledger and seal; B list | ✅ melonDS 1.1 software (screenshots of 1-03 and 3-03 looked at); hands-on feel not checked |
| Rhythm finale | eight ticks fall to a line, A on the beat (±6 frames), HITS n / 8, back to the list | headless run only (the QA pilot taps on the exact frame) |
| Bench ledger | runs the whole bench once ("RUNNING THE BENCH...", about a second) and lists every measured cost next to its book value, green inside the judge's window, red outside; the bottom screen shows a seal per chapter; B back | ✅ melonDS 1.1 software (screenshot looked at: three chapters gold, highway and organ not) |
| Credits | scrolling credits, X opens the licence viewer | not yet checked |
| rehearsal demo | `pendulane-rehearsal.nds` boots straight into the bench; result block id `0x48524450` (`PDRH`); grade with `judge.py --profile` as in the README | ✅ 2026-11-17 (re-measured after the review fixes): all four profiles measured, `judge.py --profile` MATCH on each. melonDS 1.1 (all renderers): 11 of 15 gating targets OK (both cartridge gaps pass: 1280 and 2047). NG: `mem7-main-nonseq` (384 where GBATEK's 10-cycle N32 gives 576), `snd-pcm-busy` and `snd-adpcm-busy` (19 and 43: Busy clears at the end of the last sample, one later than GBATEK), `snd-hold-restart` (7: no held old value, 7 zero samples); the two onset differences are exactly GBATEK's 2 and 8. DeSmuME py-desmume 0.0.9: 7 of 15 OK; NG: the same sound items plus `snd-onset-pcm-psg` (6: PSG is heard from the first captured sample), `spi-512k-minus-4m` and both cartridge gaps (the transfer ends at once, edge 0), and `mem7-main-nonseq` (64); `mem7-main-seq` and `mem7-vram-seq` read 1024. Math differences DeSmuME 10 / 32 / 0, melonDS 9 / 32 / 0. SPI edges melonDS 59 / 122 / 250 / 508. No hardware values yet |

Hardware: not run yet, see [HW-QA.md](HW-QA.md).

## Auto QA run

Hidden mode of the release ROM (`pendulane.nds`): on the title screen press L and R, then X and Y, and hold all four for 120 frames. It
plays the normal flow (title → list → all 20 contraptions, each chapter ended by its rhythm piece → list → title → credits → title), runs the bench once and ends with `RESULT PASS`. Log on the SD card
(`cleancart/pendulane/qa-<date>-<time>.log`). Headless: `SDDIR=out/pd/x tools/melonds/mds.sh run games/pendulane/pendulane.nds 6000 out/pd/x
"300:L:140,300:R:140,300:X:140,300:Y:140"` (create `out/pd/x/cleancart/pendulane/` first). Verified 2026-11-17 in melonDS 1.1 (software):
`RESULT PASS` at frame 4680, 20 of 20 contraptions solved, chapters mask `0x1f`, 7 checkpoints, `tools/qa/compare.py` on two runs 0 differences, `CPU9 cpsr` mode 0x1f; the `QARN` block graded with
`judge.py --profile` on melonDS 1.1 software, GL Classic, GL Compute and DeSmuME: MATCH on each.

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/pendulane/autoqa` on the SD card (the folder must exist). The first title screen of the boot starts the same QA run and
renames the file to `autoqa.used-<date>-<time>` before it starts.
