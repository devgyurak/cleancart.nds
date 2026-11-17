# Lightenette hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/lightenette-hw-support.nds` on the card instead of `lightenette.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Lightenette (L2) is judged by the rehearsal, the `l2-*` demos and the auto QA run. The QA run with its judge suite ran on a DS Lite on 2026-11-17 (run 1 below): of 37 judged items 29 OK, 6 informational and 2 gating items NG (`snd-wave-pan-right-exact`, `3d-limits-ram`), both findings about our expected values. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROM: the hw-support build of `lightenette.nds` only. The QA run judges everything: after `RESULT PASS` it writes the QA block (it carries the rehearsal and `acc-stage` items) and runs the ten `l2-*` demos, one result file each, with a progress file `suite-<stamp>.log`. Stand-alone demo, rehearsal and `acc-stage` ROMs are only for re-running one item whose suite line shows `saved=0` (a Wood kernel gives them no SD card).
- The empty folder `cleancart/lightenette/` (without it the ROMs write nothing) and the empty file `cleancart/lightenette/autoqa` for the QA run

## Run

1. Game QA: start `lightenette-hw-support.nds` and leave it alone (the marker file starts QA on the title). The demos show on screen after the credits.
2. Wait for the end of the suite (melonDS writes `suite end demos=10 saved=10`; see the note in run 1) before powering off.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game lightenette`.

## Record (one block per run)

### Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256s | `lightenette-hw-support.nds` v0.3.0 `e7fc0ad92f620290896bf4c162f95403769ec4e50096ce13532bcafe9a519bea` (same as the release); melonDS was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 31 lines, 6 checkpoints, build `c40913a7fff7`, started with `start=marker`. Suite: QA block and 10 of 10 demos `saved=1`, no timeout. Judge over the 11 block files: QA block L1 14/14, L2 6/6 OK +4 informational; demos: 2 gating NG (below), the rest OK. In total 29 OK, **2 NG**, 6 informational, no n/a |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None against melonDS 1.1 soft: act1-act5 and judge hashes equal (`compare.py`, 0 differences). GL Classic differs from act2 on, GL Compute from act3 on (the known renderer NGs of their profiles) |
| Lag frames | 89 / 1 / 1 / 1 / 1 / 41 (act1-act5, judge); melonDS soft 1 / 0 / 0 / 0 / 0 / 37. Act 1 includes the save write mirrored to the SD card |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0 rdl=46` at every checkpoint, as melonDS |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| `snd-wave-pan-right-exact` (GBATEK, want 0..2) | **3** (NG) | 3 (NG) | 3 (NG) | 3 (NG) | OK (profile, not run) | hardware agrees with melonDS, not with the GBATEK 2/128 bound: a finding about our expectation. Reference swing at pan 0 199 on both |
| `3d-limits-ram` (GBATEK, want 2048) | **2979** (NG) | 2048 | 2048 | 2048 | n/a (profile, not run) | yes: on the console the polygon count of RAM_COUNT read before the flush keeps counting past 2048 (2980 triangles submitted in the last frame), melonDS stops at 2048 |
| `3d-limits-overflow` (unconfirmed) | 47 | 47 | 47 | 47 | n/a (profile, not run) | no |
| `l2-acc-dual` values[4] (one pass: polygons, overflow flag; not judged) | 2310, 1 | 1536, 1 | not compared | not compared | not run | yes, the same RAM_COUNT behaviour; `acc-dual-over` / `-passes` are OK on both. The capture hashes values[7..9] differ (which polygons are dropped) |
| `line-polygon` / `act4-hline` / `act4-dline` / `act5-line-polygon` (unconfirmed) | 697 / 208 / 208 / 93 | 697 / 208 / 208 / 93 | 698 / 208 / 208 / 93 | 826 / 208 / 208 / 186 | not run | equal to melonDS soft |
| `acc-dual-halves` (unconfirmed) | 0x8 | 0x8 | 0x8 | 0x8 | not run | no |
| `sys-irq-hblank` | 31520 | 31532 | not compared | not compared | not run | both inside 31297..31560 |
| the other judged items | as melonDS soft | | | | | no |

Notes:
- The `suite-*.log` ends after the last demo (`l2-sys-irq saved=1`, frame 3069) without the `suite end` line melonDS writes 3 frames after that demo. Every demo file is on the card, so no item is missing. The same line is missing in the Corvane and Hushwell runs; whether the console was switched off before it or the end step stops on hardware is still open.
- Not judged, also different: QA lag total (values[53]), `snd-capture` values[5] (capture peak).

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/lightenette/docs/HW-QA.md`); expected values and levels change in a separate PR.
