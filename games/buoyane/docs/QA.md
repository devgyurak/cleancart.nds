# Buoyane QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`). "Headless" means the melonDS 1.1 core runner `tools/melonds/mds.sh` (software renderer) unless said otherwise; nothing here has been checked by hand in the melonDS application or on hardware yet.

| Scene | Check | Status |
|---|---|---|
| Title | a night harbour drawn by the 3D engine (sea strips fading into the fog, two headlands with edge marking, a lamp with a translucent sweeping beam) behind the lettering "BUOYANE", the slackwater song; Start opens the chart, X the credits | headless screenshot (frame 190) |
| Chart | five chapters with `n/6` finished, up/down chapter, left/right approach, A sails, B back to the title | headless screenshot |
| Voyage | top: the light of the true type in the chapter's view; bottom: three candidate lights. Left/right choose, A names it (chime when right, click when wrong), Select shows the almanac's look for each candidate, B or Start back. Three gates finish an approach | headless: all 30 played by the QA pilot |
| Credits and licence viewer | scrolling credits (libnds, BlocksDS crts MPL-2.0 with source URL, picolibc, libgcc); X opens the licence viewer | not yet checked |
| rehearsal demo | `buoyane-rehearsal.nds`: the first approach of every chapter (scripted), then the five light-check scenes; block id `0x48525942` (`BYRH`), published at about frame 500; run 1500. Grade: `tools/melonds/mds.sh run games/buoyane/buoyane-rehearsal.nds 1500 out/x "" 1499 \| python3 games/buoyane/tools/judge.py --profile melonds-1.1-software` (`RENDERER=gl`, `compute`; DeSmuME: `RESULT_WORDS=64 tools/emu/emu.sh run …` with `--profile desmume-pydesmume-0.0.9`) | ✅ all four profiles MATCH, 2026-11-17 |
| check ROMs | `buoyane-check-dot|fog|edge|depth|blend.nds`: one scene each (about 150 frames), ids `ckdo`, `ckfo`, `cked`, `ckde`, `ckbl`; every profile MATCH | ✅ 2026-11-17 |

## Auto QA run

Hidden mode of the release ROM (`buoyane.nds`): on the title screen press L and R, then X and Y, and hold all four for 120 frames, or put an
empty file `cleancart/buoyane/autoqa` on the SD card (the folder must exist). The game takes the normal flow by itself: title, chart, all 30
approaches (the scripted player `by_pilot_keys()` names the almanac's option at every gate), chart, title, credits, title; then the light check
runs once, the verdict is written (`RESULT PASS`), and the judge suite runs the five check scenes and writes each as `res-*-check-<scene>.log`.
Nothing is tuned (`qa_tune.h`); a checkpoint per finished chapter plus one for the judge and one at the credits.

Headless: `SDDIR=out/by/x tools/melonds/mds.sh run games/buoyane/buoyane.nds 12000 out/by/x "300:L:140,300:R:140,300:X:140,300:Y:140"` (create
`out/by/x/cleancart/buoyane/` first). Verified 2026-11-17 in melonDS 1.1 (software): `RESULT PASS` at about frame 3140, 30 of 30 approaches, chapters
mask `0x1f`, 7 checkpoints, `tools/qa/compare.py` on two runs: 0 differences, `tools/qa/lint.py` ok, the `QARN` block graded with `judge.py --profile` MATCH on melonDS 1.1
software and DeSmuME, `tools/qa/suite_compare.py`: the five suite blocks equal the stand-alone ROMs' (SAME). Save: the game saves after every finished approach (the run mirrors each write to the log); a normal boot on a save with progress shows it on the chart, and after the QA run ends the chart shows the progress from before it (not the run's 30/30).
Not yet: GL Classic and Compute QA runs, melonDS application, hardware.

## Reproducibility

Two clean `./build.sh` runs give the same SHA-256 (2026-11-17, scaffold stage; re-check before the release).
