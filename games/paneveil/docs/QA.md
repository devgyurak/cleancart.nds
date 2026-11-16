# Paneveil QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | the 3D rose window (a preset of its own) turning behind the glass lettering "PANEVEIL", on a dark panel with the tagline, a blinking PRESS START and the X hint, the clerestory theme playing (stopped when leaving); licence, ARM7 and save lines on the bottom screen. Start opens the window list, X the credits | headless run: see below; not yet by hand in melonDS |
| Window list | "CHAPTER n" header, the six windows of the chapter, left/right changes the chapter, A starts, B back | headless run only |
| Window (3D) | top: the rose window turning about the vertical axis against the backlight, lead frame; bottom: the 12 settings (NOW / ORDER), steps and par, `KILN OK` lamp. Up/down chooses a setting, left/right turns it one step, A fires when everything matches ("FIRED ***"), A again goes to the next window, B list. Select: 2D schematic (the same state as text), solvable without 3D | ✅ melonDS 1.1 software (screenshots of 2-01 looked at); hands-on feel not checked |
| Title → X (credits) | scrolling credits (libnds, BlocksDS crts MPL-2.0 with source URL, picolibc, libgcc); X opens the licence viewer (every text of `licenses/`, X next file, B back); without NitroFS the viewer says "CART FILES NOT AVAILABLE" | not yet checked |
| rehearsal demo | `paneveil-rehearsal.nds` boots straight into the rehearsal (no save chip, NitroFS): the kiln check, the first window of all five chapters (scripted) and the light box; result block id `0x48525650` (`PVRH`), values[3..15] are the 13 kiln items, values[16..30] the 15 light box items (the block is published at about frame 510; run 1300). Grade: `RESULT_WORDS=64 tools/emu/emu.sh run games/paneveil/paneveil-rehearsal.nds 1000 out/x "" 999 \| python3 games/paneveil/tools/judge.py --profile desmume-pydesmume-0.0.9` or `tools/melonds/mds.sh run … 1000 out/x "" 999 \| python3 games/paneveil/tools/judge.py --profile melonds-1.1-software` (`RENDERER=gl`, `compute`) | ✅ all four profiles measured 2026-11-16, `judge.py --profile` MATCH on each (30 of 30 gating items OK on melonDS 1.1 software and GL Compute; 29 of 30 on DeSmuME py-desmume 0.0.9 and melonDS 1.1 GL Classic, where `poly-wire-edges` is NG: wireframe polygons (alpha 0) are not drawn at all, the profile records it). No hardware values yet |

Hardware: not run yet, see [HW-QA.md](HW-QA.md).

## Auto QA run

Hidden mode of the release ROM (`paneveil.nds`): on the title screen press L and R, then X and Y, and
hold all four for 120 frames. The game plays the normal flow by itself (title → window list → all 30 windows played by the
scripted player `game/script.c` → list → title → credits → title), runs the kiln check and the light box once, writes the
save once and ends with `RESULT PASS`. Log on the SD card
(`cleancart/paneveil/qa-<date>-<time>.log`). Headless: `SDDIR=out/pv/x tools/melonds/mds.sh run
games/paneveil/paneveil.nds 24000 out/pv/x "300:L:140,300:R:140,300:X:140,300:Y:140"` (create `out/pv/x/cleancart/paneveil/` first for the SD log). Verified 2026-11-16 in melonDS 1.1 (software): `RESULT PASS` at frame 3971, 30 of 30 windows baked, chapters mask `0x1f`, 7 checkpoints, `tools/qa/compare.py` on two runs: 0 differences; the `QARN` block (rehearsal layout plus the QA words at values[48..54]) graded with `judge.py --profile` MATCH on melonDS 1.1 software, GL Classic, GL Compute and DeSmuME (31 items; `poly-wire-edges` NG on DeSmuME and GL Classic as in the rehearsal). Save round trip (boot twice with a save file): chip type 2 (EEPROM) 8192 bytes, `CPU9 cpsr` mode 0x1f (no data abort).

## Starting QA without the keys (SD marker)

Put an empty file `cleancart/paneveil/autoqa` on the SD card (the folder must exist). The first title
screen of the boot starts the same QA run as the key combo and renames the file to
`autoqa.used-<date>-<time>` before it starts, so the run happens once. If the rename fails nothing
starts and `autoqa-error.log` gets a line.
