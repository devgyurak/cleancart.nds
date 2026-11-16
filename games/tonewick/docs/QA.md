# Tonewick QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | the dusk moor, the signal mast and the shed with its scope window, "TONEWICK", the tagline and the X/Select hint on the top screen; licence, ARM7 and save lines on the bottom screen. Start opens district 1, X the credits, Select the test menu | headless run (screenshots looked at); not yet by hand in melonDS |
| Test menu | four districts, the morning log, the rehearsal, the credits; B back | headless run only |
| District | top: name, tower and GBATEK section, the three signal lines (`[X]` cleared), a scope with the model waveform (blue) and, after a listen, the ear's captured waveform (yellow) and a lamp (`TRUE EAR`, `THE EAR DISAGREES`, `NO SIGNAL`); bottom: the line, what it needs, brass dials (dial A: left/right, dial B: L/R or Y/X) and a lever (A). Touch: the left half of a dial turns it back, the right half on, the lever listens. Start ends the shift when all three lines are cleared, B back | headless run with the QA pilot and a keyed run (screenshots looked at); touch not tried by hand |
| Morning log | clock, power, tape (save chip) and firmware readings, each `AGREES` or `DIFFERS` with its rule; B back | headless run (screenshots looked at) |
| Credits | scrolls; X opens the licence viewer (needs NitroFS), B back | headless run (the QA run passes through it) |

A lever pull blocks for a few frames (the ARM7 runs a sound script and the ear listens, 5 to 8 frames per experiment, the
morning log line reads the devices); the lever is drawn pressed meanwhile. The speaker amplifier is switched off for the
length of an experiment, so nothing is heard in the game; the title music stops when the first experiment runs.

## Demo ROMs

| Demo | What it reports | Status |
|---|---|---|
| `rehearsal` | `tonewick-rehearsal.nds` boots straight into the rehearsal (no save chip, no NitroFS): the twelve lines solved by script and every district's audit; result block id `0x48525754` (`TWRH`), layout in `game/lab.h`; graded by `python3 tools/judge.py` (boot `rehearsal`) | melonDS 1.1 soft / GL classic / GL compute and DeSmuME: profile MATCH, see below |

Hardware: not run yet, see [HW-QA.md](HW-QA.md).

## Judge profiles (2026-11-16, build of this commit)

Rehearsal and QA run (boot `qa`) of the release ROM give the same verdicts.

| Emulator | NG items | Notes |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute (headless, built-in replacement BIOS) | `snd-hold-level` (the hold bit is not implemented: the output returns to 0 after a one-shot), `cap-add-wrap` (a capture with the add bit set writes nothing), `cap-both-negative` (reads -4000h, the two channels added, instead of -8000h), `pm-reg-mirror` (the generated firmware says DS Lite, register 5 reads 0 where register 4 reads 40h) | profiles `melonds-1.1-software`, `-gl-classic`, `-gl-compute` MATCH; identical on the three renderers. The three save items are n/a in the QA run with an SD image (a homebrew run has no save chip); with the emulated 8 KB EEPROM (`SAV=`, profile `melonds-1.1-software-save-chip`) all three are OK |
| DeSmuME (py-desmume 0.0.9, HLE BIOS, EEPROM) | `snd-short-silent` (a tape of 3 words plays), `snd-hold-level`, `snd-noise-lfsr` (152 matching bits), `snd-adpcm-loop-reload` (the loop does not reload), `cap-add-wrap` (no capture), `cap-both-negative` (reads -2000h), `pm-reg-mirror`, `save-wel-reset` (WEL stays set after the write) | profile `desmume-pydesmume-0.0.9` MATCH. `snd-duty7-low` passes there |

All NG items are results to check against hardware (items stay gating because GBATEK states them); the capture bugs in
particular are only documented in GBATEK. If the DS Lite disagrees, those two items move to `probes/` as level 4.

## Auto QA run

Hidden mode of the release ROM (`tonewick.nds`): on the title screen press L and R, then X and Y, and hold all four for 120
frames. The game plays the normal flow by itself: title, districts 1 to 4 (the pilot finds each line's dials with the GBATEK
model, turns them with the keys and listens; Start ends the shift), the credits and the title again, then ends with
`RESULT PASS`. Log on the SD card (`cleancart/tonewick/qa-<date>-<time>.log`), result block `QARN` in the rehearsal's layout,
graded with `python3 games/tonewick/tools/judge.py --profile <profile> <log>` (boot `qa`, extra item `qa-verdict`). Headless:
`SDDIR=out/tw/x tools/melonds/mds.sh run games/tonewick/tonewick.nds 3000 out/tw/x "300:L:140,300:R:140,300:X:140,300:Y:140"`
(create `out/tw/x/cleancart/tonewick/` first; `RENDERER=gl|compute` for the GL renderers).

| Renderer | QA run (2026-11-16) | `judge.py --profile` |
|---|---|---|
| melonDS 1.1 software | `RESULT PASS` at frame 1575, 4 checkpoints, `parse_log.py` ok | `melonds-1.1-software` MATCH |
| melonDS 1.1 GL classic | PASS | `melonds-1.1-gl-classic` MATCH |
| melonDS 1.1 GL compute | PASS | `melonds-1.1-gl-compute` MATCH |
| DeSmuME (py-desmume 0.0.9) | verdict 1 | `desmume-pydesmume-0.0.9` MATCH |

Two software runs give identical checkpoint hashes (`tools/qa/compare.py`); `tools/qa/lint.py` passes. The state hash covers
the completion bits and the lines solved only: the measured values move a little between runs on a console. The software
run has no save chip (homebrew run for the SD card); the chip run (`SAV=`) is graded by the `-save-chip` profile and reads
back the save (`SAVE 2 8192`, `CPU9 cpsr` mode 1F, no data abort). Real DS Lite + R4: not run yet.

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/tonewick/autoqa` on the SD card (the folder must exist). The first title screen of the boot
starts the same QA run as the key combo and renames the file to `autoqa.used-<date>-<time>` before it starts, so the run
happens once. If the rename fails nothing starts and `autoqa-error.log` gets a line. Not tried on a card yet.
