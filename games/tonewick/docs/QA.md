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

Rehearsal and QA run (boot `qa`) of the release ROM give the same verdicts. **Run the profiles at an hour of 12 or later**: `rtc-ampm-24h` reads the AM/PM flag, which is 0 either way before noon, so it is n/a then and a profile comparison reports `NEWLY N/A`. melonDS: `RTC=2026-11-16-15-30-00`; DeSmuME (host clock): `RTC_HOUR=15 tools/emu/emu.sh run ...`.

| Emulator | NG items | Notes |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute (headless, built-in replacement BIOS, `RTC=2026-11-16-15-30-00`) | `snd-hold-level` (the hold bit is not implemented: the output returns to 0 after a one-shot), `cap-src-channel` (bit 1 is ignored: the mixer sum 4000h is captured, not channel 0's 2000h), `cap-add-wrap` (a capture with the add bit set writes nothing), `cap-both-negative` (reads -4000h, the two channels added, instead of -8000h), `pm-reg-mirror` (the generated firmware says DS Lite, register 5 reads 0 where register 4 reads 40h) | profiles `melonds-1.1-software`, `-gl-classic`, `-gl-compute` MATCH; identical on the three renderers. The three save items are n/a in the QA run with an SD image (a homebrew run has no save chip); with the emulated 8 KB EEPROM (`SAV=`, profile `melonds-1.1-software-save-chip`) all three are OK |
| DeSmuME (py-desmume 0.0.9, HLE BIOS, EEPROM, local hour 15 via `RTC_HOUR=15`) | `snd-short-busy` (a 3-word one-shot ends and clears the busy bit), `snd-short-silent` (a tape of 3 words plays), `snd-hold-level`, `snd-noise-lfsr` (152 matching bits), `snd-adpcm-loop-reload` (the loop does not reload), `cap-add-wrap` (no capture), `cap-both-negative` (reads -2000h), `pm-reg-mirror`, `save-wel-reset` (WEL stays set after the write) | profile `desmume-pydesmume-0.0.9` MATCH. `snd-duty7-low` passes there |

All NG items are results to check against hardware (items stay gating because GBATEK states them); the capture bugs in
particular are only documented in GBATEK. What happens when the DS Lite disagrees is written in [HW-QA.md](HW-QA.md), section "Judging the hardware result".

## Auto QA run

Each `cp` line of the QA log ends with `gx=uf<n>,of<n> rdl=<min>`: for the span since the previous checkpoint, the frames that showed DISP3DCNT bit 12 (colour buffer RDLINES underflow) and bit 13 (polygon/vertex RAM overflow) set, and the lowest RDLINES_COUNT (buffered lines minus 2, 0..46; `rdl=-` when no frame was sampled in the span, e.g. two checkpoints in one frame). Both flags are sticky and only read, never cleared by QA, so a count is "frames the flag was up". That holds only while the game does not clear them: a game that acknowledges bit 13 itself (`g3_overflowed()` in Corvane, Elevane, Heddlemere, Hushwell, Lightenette, Paneveil) reports its acknowledgements through `qa_game_gx_acks()` and each one counts as a frame with the flag up; it writes bit 12 as 0 so the underflow flag survives. The figures are a hint like `lag`: never hashed, never compared (`tools/qa/compare.py --info` lists them side by side), and `0,0` is what a scene without 3D shows. Emulators rarely raise these flags; hardware may.

Hidden mode of the release ROM (`tonewick.nds`): on the title screen press L and R, then X and Y, and hold all four for 120
frames. The game plays the normal flow by itself: title, districts 1 to 4 (the pilot finds each line's dials with the GBATEK
model, turns them with the keys and listens; Start ends the shift), the credits and the title again, then ends with
`RESULT PASS`. Log on the SD card (`cleancart/tonewick/qa-<date>-<time>.log`), result block `QARN` in the rehearsal's layout,
graded from the runner output (the `RESULT` line; the SD log holds no result block) with `python3 games/tonewick/tools/judge.py --boot qa --profile <profile>` (extra item `qa-verdict`). Headless:
`RTC=2026-11-16-15-30-00 SDDIR=out/tw/x tools/melonds/mds.sh run games/tonewick/tonewick.nds 3000 out/tw/x "300:L:140,300:R:140,300:X:140,300:Y:140" | python3 games/tonewick/tools/judge.py --boot qa --profile melonds-1.1-software`
(create `out/tw/x/cleancart/tonewick/` first; `RENDERER=gl|compute` for the GL renderers; `parse_log.py` checks the SD log).

| Renderer | QA run (2026-11-16) | `judge.py --profile` |
|---|---|---|
| melonDS 1.1 software | `RESULT PASS` at frame 1582, 4 checkpoints, `parse_log.py` ok | `melonds-1.1-software` MATCH |
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
