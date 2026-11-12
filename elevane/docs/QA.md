# Elevane QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | "ELEVANE" and the tagline on the top screen, X hint at the bottom of it; licence, ARM7 and save lines on the bottom screen. X → credits | headless run: see below; not yet by hand in melonDS |
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
