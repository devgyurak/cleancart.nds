# Probes hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

The probes (L4) are research ROMs with no reference value; hardware is what they are for. Nothing here has been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
the probe item has a new reference value (recorded as proposed below).

## What to put on the card

- ROMs: the **hardware-support copies**, `build/release/rom/probes-*-hw-support.nds` (19 of them; the release zip
  `probes-vX.Y.Z.zip` holds them). The probe ROMs carry the game code `PRBS` (melonDS needs it for its save chip), and the
  R4 Plus (Wood) kernel attaches the SD card only to ROMs with the code `####`: on the plain ROMs `RESULT SAVED` never
  appears and no file is written. A copy differs from its ROM in the four game code bytes and the header CRC only
  (`tools/hwsupport.py`). Do not put the plain `demos/` ROMs on the card.
- The empty folder `cleancart/probes/` (without it the ROMs write nothing)

## Run

1. Start the 19 ROMs one by one, in any order (run the Buoyane-origin pair `l4-3d-zeq-slope` and `l4-3d-fog-dot` first
   if time is short). Wait for `RESULT SAVED` under the checkpoint, then reset. Every ROM stops at its checkpoint
   (frame 120; `l4-vcount-write` and `l4-tex-late` earlier) and keeps the picture, so nothing needs a button.
2. About 15 to 20 minutes for all 19 (a few seconds per ROM, the rest is choosing the next file in the kernel menu).
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game probes`.

## Record (one block per run)

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | |
| ROM SHA-256s | |
| `ingest.py` / `judge.py --hardware` summary | |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| | | | | | | |

## Follow-up

Propose `known.hardware-dslite-r4.observed` (`probes/tools/judge.py --hardware --partial --observed`) to the user; after a yes it goes in `probes/expected.json` in its own PR. `level-judge` lists the probes that can move down to L2/L3 with `hw:probes/docs/HW-QA.md`.
