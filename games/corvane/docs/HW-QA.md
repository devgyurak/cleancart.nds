# Corvane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/corvane-hw-support.nds` on the card instead of `corvane.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Corvane (L1, graphics and system) is judged by its demos and the auto QA run. Nothing here has been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
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

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | |
| ROM SHA-256s | |
| `ingest.py` / `judge.py --hardware` summary | |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | |
| First checkpoint that differs from melonDS | |
| Lag frames | |
| Shot `st3-600`: screen, crc, same as melonDS? | |
| 3D flags (`gx=` `rdl=`) per checkpoint | |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| | | | | | | |

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/corvane/docs/HW-QA.md`); expected values and levels change in a separate PR.
