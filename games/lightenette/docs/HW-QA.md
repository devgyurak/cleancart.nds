# Lightenette hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/lightenette-hw-support.nds` on the card instead of `lightenette.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block and the ten `l2-*` demos as `res-*-lightenette-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

Lightenette (L2) is judged by the rehearsal, the `l2-*` demos and the auto QA run. Nothing here has been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROMs: `build/release/rom/lightenette.nds`, `build/release/demos/*.nds` (the rehearsal and `acc-stage` ROMs are among the demos)
- The empty folder `cleancart/lightenette/` (without it the ROMs write nothing) and the empty file `cleancart/lightenette/autoqa` for the QA run

## Run

1. Game QA: start `lightenette.nds`, leave it alone until the credits (the marker file starts QA on the title).
2. Demos and rehearsal: start each ROM one by one; wait for `RESULT SAVED` (the rehearsal writes silently when its result screen shows), then reset.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game lightenette`.

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

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/lightenette/docs/HW-QA.md`); expected values and levels change in a separate PR.
