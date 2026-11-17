# Hushwell hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/hushwell-hw-support.nds` on the card instead of `hushwell.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Hushwell (L1, sound and I/O) is judged by its demos and the auto QA run. The QA run with its judge suite passed on a DS Lite on 2026-11-17 (run 1 below): 27 of 27 judged items OK, 10 n/a on a flash card. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROM: the hw-support build of `hushwell.nds` only. The QA run judges everything: after the credits it runs the 16 judged demos and writes one result file each. `io-save-flash` / `io-save-fram` are not run (other save chips): their items, and every `env: cart` item, are n/a on hardware (decision 2026-11-17; the R4 verifies no chip save). Stand-alone demo ROMs are only for re-running one demo whose file shows `saved=0`.
- The empty folder `cleancart/hushwell/` (without it the ROMs write nothing) and the empty file `cleancart/hushwell/autoqa` for the QA run

## Run

1. Game QA: start `hushwell-hw-support.nds` and leave it alone until the suite ends (the marker file starts QA on the title).
2. Wait for the end of the suite (the log ends with `suite end demos=16 saved=16`); no demo needs a run of its own.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game hushwell`.

## Record (one block per run)

### Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256s | `hushwell-hw-support.nds` v0.3.0 `3bad02796ecdeee48537f689d10408ed7433c4c6010b2b9afb26e0284744485f` (same as the release); melonDS was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 53 lines, 6 checkpoints, build `dcb1b33f744e`, started with `start=marker`. Suite: 16 of 16 demos `saved=1`, no timeout. Judge over the 17 block files (16 demos + the QA block): **L1 27/27 OK** +2 informational; n/a 10 (cart: `sys-dma-cart-found/-dma/-nitro`, `io-cart-ok/-ready`; chip save: `io-save-eeprom-write/-reload/-seq`; touch: `io-touch-x/-y`) |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: ch0-ch5 hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 83 / 2196 / 2113 / 1476 / 765 / 899 (ch0-ch5); melonDS 80 / 1824 / 1481 / 944 / 538 / 849 |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0` everywhere; `rdl` 46 / 46 / 43 / 46 / 46 / **39** (melonDS 46 at every checkpoint): the console's 3D render buffer ran lower in ch2 and ch5, with no underflow |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| `io-firmware-name` | 3 | 7 | not run | not run | not run | yes, the console owner's name vs melonDS's default; both inside 0..10 |
| `io-rtc-month` / `-day` / `-hour` | 1 / 15 / 6 | 11 / 17 / 11 | not run | not run | not run | yes, the console clock is unset (2002-01-15); all inside their ranges |
| cart items (5) | not ready (flash card) | ready | | | | n/a on a flash card |
| the other judged items | as melonDS | | | | | no |

Notes:
- The `suite-*.log` ends after the last demo (`io-save-eeprom saved=1`) without the `suite end` line melonDS writes (Corvane's run shows the same). Every demo file is on the card, so no item is missing. Whether the console was switched off before that line or the end step stopped is not known yet; watch for it on the next run.
- Not judged, also different: `snd-capture` values[5] (capture peak) and `io-save-eeprom` values[3].

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/hushwell/docs/HW-QA.md`); expected values and levels change in a separate PR.
