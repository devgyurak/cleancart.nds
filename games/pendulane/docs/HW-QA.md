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
| Does the ARM9 write buffer move the edge (the fixed path is not the same on the console)? | the three differences stay in their windows; the raw edges in values 24 to 27 are information |

## Record

| Date | Console | Card | Result |
|---|---|---|---|
| — | — | — | not run yet |
