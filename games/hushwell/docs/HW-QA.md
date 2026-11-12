# Hushwell hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

Hushwell (L1, sound and I/O) is judged by its demos and the auto QA run. Nothing here has been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROMs: `build/release/rom/hushwell.nds`, `build/release/demos/*.nds`
- The empty folder `cleancart/hushwell/` (without it the ROMs write nothing) and the empty file `cleancart/hushwell/autoqa` for the QA run

## Run

1. Game QA: start `hushwell.nds`, leave it alone until the credits (the marker file starts QA on the title).
2. Demos and rehearsal: start each ROM one by one; wait for `RESULT SAVED` (the rehearsal writes silently when its result screen shows), then reset.
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
