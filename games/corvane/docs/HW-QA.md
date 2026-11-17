# Corvane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/corvane-hw-support.nds` on the card instead of `corvane.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Corvane (L1, graphics and system) is judged by its demos and the auto QA run. The QA run with its judge suite passed on a DS Lite on 2026-11-17 (run 1 below): 92 of 92 judged items OK. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROM: `build/release/rom/corvane-hw-support.nds` only; the QA run judges everything (48 demos after the credits). The stand-alone demo ROMs are only for re-running one whose file shows `saved=0`, (`io-qalog` needs none: its item is graded from the QA block file).
- The empty folder `cleancart/corvane/` (without it the ROMs write nothing) and the empty file `cleancart/corvane/autoqa` for the QA run

## Run

1. Game QA: start `corvane-hw-support.nds` and leave it alone until the suite ends (the marker file starts QA on the title).
2. Wait for the end of the suite (the log ends with `suite end demos=48 saved=48`); no demo needs a run of its own.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game corvane` (it copies `shot-*.bmp` too).
4. Compare the shots with melonDS (see `docs/QA.md`, "Auto QA run"): `python3 tools/qa/compare.py --info <melonds qa log> <card qa log>` lists the CRC per shot name, then open the console's `shot-*-st3-600.bmp` and the melonDS BMP of the same name side by side (today only stage 3 is shot).

## Record (one block per run)

### Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256s | `corvane-hw-support.nds` v0.4.0 `1840bfd91d3fcc9db3a5a7deb5498853e30a0895e22ef23e028501b14eb707c9` (same as the release); melonDS was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 56 lines, 5 checkpoints, build `01c2a1fd6f25`, started with `start=marker`. Suite: 48 of 48 demos `saved=1`, no timeout. Judge over the 49 block files (48 demos + the QA block): **L1 92/92 OK** +1 informational, `io-qalog-verdict` OK from the QA block |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: st1-st5 hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 60 / 182 / 227 / 181 / 200 (st1-st5); melonDS 58 / 118 / 145 / 119 / 129. The console runs the same game frames with more two-VBlank frames |
| Shot `st3-600`: screen, crc, same as melonDS? | top, `e2f3e3a0`; melonDS `9f1afc67`. 13 of 49,152 pixels differ, all on one diagonal polygon edge of the ship (x 128-153, y 123-135), each by one step of one colour channel (e.g. `0x5652` vs `0x5672`): edge colour interpolation rounding |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0 rdl=46` at every checkpoint, as in melonDS |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| `sys-irq-hblank-visible` | 31464 | 31323 | not run | not run | not run | yes, both inside 22848..31560 |
| `sys-irq-vcount` | 120 | 119 | not run | not run | not run | yes, both inside 118..121 |
| the other 90 judged items | as melonDS | | | | | no |

Notes:
- The `suite-*.log` ends after the last demo (`sys-irq saved=1`) without the `suite end` line melonDS writes. Every demo file is on the card, so no item is missing. Whether the console was switched off before that line or the end step stopped is not known yet; watch for it on the next run.
- Not judged, also different: `sys-timer` values[0] and [3] (information words).

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/corvane/docs/HW-QA.md`); expected values and levels change in a separate PR.
