# Hushwell hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/hushwell-hw-support.nds` on the card instead of `hushwell.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Hushwell (L1, sound and I/O) is judged by its demos and the auto QA run. Nothing here has been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
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

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | |
| ROM SHA-256s | |
| `ingest.py` / `judge.py --hardware` summary | |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | |
| First checkpoint that differs from melonDS | |
| Lag frames | |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| | | | | | | |

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/hushwell/docs/HW-QA.md`); expected values and levels change in a separate PR.
