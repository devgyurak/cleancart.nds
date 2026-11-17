# Pendulane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

Nothing in Pendulane has been measured on a console yet. This is the checklist for the run, so the first values can be recorded and the
`unconfirmed` items in `expected.json` can be promoted. The console is operated by a person (skill `nds-hw-qa`): create the empty folder
`cleancart/pendulane/` on the card and the rehearsal writes its result block there; `python3 tools/qa/ingest.py /Volumes/<card> --game
pendulane` grades it with `judge.py --hardware --partial`. Hardware NG items are findings, not profiles.

## What the hardware has to answer

| Question | Where it shows |
|---|---|
| Does DIV mode 0 take 18 bus clocks, modes 1 and 2 34, SQRT 13 (GBATEK "DS Maths")? | rehearsal values 3 to 5 (10, 32, 0 half cycles; window ±2) |
| Is the firmware SPI byte 8 x (64 - 8) = 448 cycles longer at 512 kHz than at 4 MHz (dividers /8 and /64 are inferred from the nominal rates)? | rehearsal value 6 (448, window ±8); raw edges 28 to 31 |
| Is the ARM7 main RAM non-sequential 32-bit access 10 cycles (GBATEK's text) or 5 ("reportedly")? Both emulators disagree with 10. | rehearsal value 16 (576 for 10; 320 for 5) |
| Do the other ARM7 memory timings (main RAM S32 = 2, code from main RAM, VRAM S32 = 2) match? | values 15, 17, 18 |
| Does the ARM9 write buffer move the edge (the fixed path is not the same on the console)? | the three differences stay in their windows; the raw edges in values 24 to 27 are information |

## Record

| Date | Console | Card | Result |
|---|---|---|---|
| — | — | — | not run yet |
