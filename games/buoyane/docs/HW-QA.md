# Buoyane hardware QA

[한국어](HW-QA-KO.md)

Not run yet. Buoyane has not been on a real console: every verdict in `expected.json` comes from GBATEK and the emulators.
The point of the run is the four `unconfirmed` items (`aa-opaque-dot-hidden`, `fog-alpha-glitch`, `aa-marked-line-complete`,
`wire-texture-alpha`) and `edge-aa-translucent`, `attr-latch-begin`: one run on a DS Lite decides whether each becomes a gating `hw:` item
or moves to `probes/` (level 4).

Steps (skill `nds-hw-qa`; DS Lite + R4 Plus): put `buoyane-hw-support.nds` (the release ROM with game code `####`, built by `tools/hwsupport.py`, so the
card gets the SD card) on the card with the folder `cleancart/buoyane/` and an empty file `autoqa` in it, start it, leave the title alone. The QA run plays
the 30 approaches, writes `qa-*.log`, and the judge suite writes `res-*-check-<scene>.log` for the five scenes. Collect them, then
`tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`, and record the table here and in the Korean twin.

| Item | Emulator result (software / GL Classic / GL Compute / DeSmuME) | Hardware |
|---|---|---|
| `aa-opaque-dot-hidden` | NG everywhere | open |
| `fog-alpha-glitch` | NG everywhere | open |
| `wire-texture-alpha` | NG on melonDS (opaque wireframe, `a = 31`); NG on the others | open |
| `aa-marked-line-complete` | informational | open |
| `edge-aa-translucent` | OK software; NG GL Compute, GL Classic, DeSmuME | open |
| `attr-latch-begin` | OK on all four | open |
