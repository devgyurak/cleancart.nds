# Elevane QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | Start opens the puzzle list. "ELEVANE" and the tagline on the top screen, X hint at the bottom of it; licence, ARM7 and save lines on the bottom screen. X → credits | headless run: see below; not yet by hand in melonDS |
| Title → Start (list) | "CHOOSE AN ORDER", eight chapter 1 orders; up/down, A starts, B back | ✅ headless DeSmuME (2026-11-11); not yet by hand in melonDS |
| Puzzle (3D) | top: front elevation, blue blueprint wall = the order, blocks gold, blocks outside the order red; bottom: plan view, lit floor tiles = the order, cursor tile yellow; text overlay (ORDER/NOW grids, BLOCKS, PAR). D-pad moves, A +1, B -1, touch a plan cell +1, hold 30 frames empties it. Both views match: "MATCH! A: DELIVER", A delivers ("DELIVERED ***", stars by blocks vs par), A next puzzle, B list. Scripted solve of 1-01: `60:START,100:A,160:A,175:RIGHT,185:A,200:RIGHT,210:A,225:RIGHT,235:A,280:A` | ✅ DeSmuME; ✅ melonDS 1.1 soft, GL, compute with scripted touches (`160:T60/147:3,175:T105/147:3,190:T150/147:3,205:T196/147:3`): the screens match (≈300 of 98 304 pixels differ by more than 8 levels, text and edges only); hands-on feel in melonDS not yet checked |
| Puzzle (2D grid) | Select: both screens text only, front grid on top (`#` block, `X` extra, `+` missing, `.` empty), plan heights below (cursor yellow); same rules, solvable without 3D. Select again returns to 3D | ✅ DeSmuME (solve in 2D, back to 3D) |
| Puzzle → X (showcase) | the work as a plaza of copies on engine B's picture (top) with the text on the bottom screen: preset 1 (HBlank DMA) POLYS 1140+1140 (each pass under 2048, together above it), OVERFLOW 0 0, HOLES CAP 0 BLEND 0. A switches to preset 2 (rear plane), B/Start returns to the puzzle with the pile kept and 3D working | ✅ DeSmuME (both presets; polygon counters read 0 there, as DeSmuME returns 0); ✅ melonDS 1.1 soft/GL/compute and master `4ce2188` soft: preset 1 `HOLES 0 0`; ❌ master `4ce2188` GL (left half black) and Compute (left half grey): preset 1 `BLEND 6144`, the #2618 defect, puzzle play unaffected; preset 2: ✅ soft, ❌ GL (one side empty, a separate defect). Return to the puzzle ✅ DeSmuME, melonDS 1.1 soft/GL |
| Title → X (credits) | scrolling credits (libnds, BlocksDS crts MPL-2.0 with source URL, picolibc, libgcc); X opens the licence viewer (every text of `licenses/`, X next file, B back); without NitroFS the viewer says "CART FILES NOT AVAILABLE" | not yet checked |

## Auto QA run

Hidden mode of the release ROM (`elevane.nds`): on the title screen press L and R, then X and Y,
and hold all four for 120 frames. The game plays the normal flow by itself (title → credits →
title), writes the save once and ends with `RESULT PASS`. Log on the SD card
(`cleancart/elevane/qa-<date>-<time>.log`). Headless: `SDDIR=out/ev/x tools/melonds/mds.sh run
elevane/elevane.nds 2500 out/ev/x "300:L:140,300:R:140,300:X:140,300:Y:140"`. Chapters join the
flow as they land.

## Verified so far

See the pull request that introduced the skeleton for what was run where.
