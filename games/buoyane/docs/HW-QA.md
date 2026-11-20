# Buoyane hardware QA

[한국어](HW-QA-KO.md)

Not run yet. Buoyane has not been on a real console: every verdict in `expected.json` comes from GBATEK and the emulators.
The point of the run is the two `unconfirmed` items (`fog-alpha-glitch`, `aa-marked-line-complete`) and the GBATEK items the emulators disagree on
(`aa-opaque-dot-hidden`, `wire-texture-alpha`, `edge-aa-translucent`) plus `attr-latch-begin`: one run on a DS Lite shows what the console does
(a `hw:` item, or a move to `probes/` at level 4).

Steps (skill `nds-hw-qa`; DS Lite + R4 Plus): put `buoyane-hw-support.nds` (the release ROM with game code `####`, built by `tools/hwsupport.py`, so the
card gets the SD card) on the card with the folder `cleancart/buoyane/` and an empty file `autoqa` in it, start it, leave the title alone. The QA run plays
the 30 approaches, writes `qa-*.log`, and the judge suite writes `res-*-check-<scene>.log` for the five scenes. Collect them, then
`tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`, and record the table here and in the Korean twin.

| Item | Emulator result (software / GL Classic / GL Compute / DeSmuME) | Hardware |
|---|---|---|
| `aa-opaque-dot-hidden` | NG everywhere (GBATEK item, gates) | open |
| `fog-alpha-glitch` | NG everywhere | open |
| `wire-texture-alpha` | NG everywhere (GBATEK item, gates) | open |
| `aa-marked-line-complete` | informational: OK software and GL Compute; NG GL Classic, DeSmuME | open |
| `edge-aa-translucent` | OK software; NG GL Compute, GL Classic, DeSmuME | open |
| `attr-latch-begin` | OK on all four | open |
