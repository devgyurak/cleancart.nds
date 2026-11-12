# Probes hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

The probes (L4) are research ROMs with no reference value; hardware is what they are for. Nothing here has been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
the probe item has a new reference value (recorded as proposed below).

## What to put on the card

- ROMs: `build/release/demos/*.nds` (every probe ROM)
- The empty folder `cleancart/probes/` (without it the ROMs write nothing)

## Run

1. Start every probe ROM one by one; wait for `RESULT SAVED`, then reset.
2. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game probes`.

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

Propose `known.hardware-dslite-r4.observed` (`probes/tools/judge.py --hardware --partial --observed`) to the user; after a yes it goes in `probes/expected.json` in its own PR. `level-judge` lists the probes that can move down to L2/L3 with `hw:probes/docs/HW-QA.md`.
