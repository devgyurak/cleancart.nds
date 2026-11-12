---
name: nds-qa-mode
description: The hidden auto QA mode every game carries (L+R+X+Y hold on the title, the game plays itself to the credits and writes an SD log + QARN result block). Conventions, forbidden paths, log format, porting checklist and done criteria. Use when adding the mode to a game, changing it, or checking that a game's QA run is valid.
---

# Auto QA mode

Same ROM as the release (SHA-256 identical): no QA build. The mode is hidden; nothing
on screen shows it. Reference implementation: `games/corvane/` (`core/qa.c/.h`,
`core/qalog.c/.h`, `game/qa_game.c`, `game/qa_tune.h`). `core/` files are copied into each
game and kept identical (`diff -r games/corvane/arm9/source/core games/<game>/arm9/source/core` for
`qa*.c/h`); the game side is the game's own.

## Rules

1. **Entry**: title screen, hold **L+R+X+Y for 120 frames** (`qa_title_poll()`, no progress
   shown). Never at boot (DeSmuME hangs on a key held at power-on). Never while a replay
   plays. Does not clash with title+Select or pause+L+R+A.
   **Marker start** (`qa_marker_poll()`, next to `qa_title_poll()`): the first title frame of a boot
   checks `<drive>cleancart/<game>/autoqa` once. If it exists it is renamed to
   `autoqa.used-<date>-<time>` *before* the run starts (a power cut never repeats the run); a failed
   rename starts nothing and appends `rename failed errno=N` to `autoqa-error.log`. The run is
   identical to a key start (same seed, RTC, pilot, hashes); the log's `qa start` line carries
   `start=marker` or `start=keys`, never part of a hash. With no card or no marker nothing changes.
2. **Autopilot** = the game plays to the ending/credits itself. Input enters in
   `core/input.c` at the replay spot (`qa_autopilot()`); steering code reads game state but
   only produces keys/touch.
3. **Numbers only**: `qa_tune()` returns a game-defined `QaTune` (`qa_tune_on` while QA runs,
   `qa_tune_off` = 1x otherwise). Callers read `qa_tune()->field`; no `#if`, no
   `if (qa_active())` around gameplay. Effects, sound, HUD, scene flow stay as shipped.
4. **Forbidden paths**: no `qa_*` / `qalog_*` / `QA_*` / `QAR_*` symbol in `gfx/`,
   `core/save.c`, `judge/`, `capture.c` (`python3 tools/qa/lint.py`). Saves are mirrored by the
   caller after `save_write()` returns (`qa_save_written()`); render/judge/capture code is
   never touched, so QA changes no picture.
5. **Determinism**: fixed RNG seed, RTC hour fixed (`QA_RTC_HOUR`), no timers/RTC in the state
   hash. The same ROM twice gives the same checkpoint hashes (`tools/qa/compare.py`);
   emulator vs hardware differences are findings, not noise. Hash game state only, never
   tune values.
6. **SD log** (`core/qalog.c`): 64 KB RAM ring, `<frame> <scene> <event> key=val ...`; header
   events `hdr` (game, build, rtc, seed, sd, fat_fail, errno, drive, dldi, cart_ready,
   nitro, save_build, save_detected, save_available); `cp` lines (`name= hash= lag=`);
   last line `RESULT PASS|FAIL reason=...`. File `<drive>cleancart/<game>/qa-<date>-<time>.log`,
   saves `save-<date>-<time>-<n>.bin` (runs never overwrite each other). Flush only at scene changes, checkpoints and the end; every flush
   claims the slot-1 bus (`cart_bus_claim()`) and closes the file; never flush inside a save
   transaction (`qalog_hold()` if code could log there). No SD (DeSmuME): RAM only.
7. **RAM result block** at `0x0237F000`, same layout as the demo block, `demo_id` =
   `QARN`; magic written last. Words: verdict (1 PASS/2 FAIL), lines, SD flags
   (bit0 log on SD, bit1 read-back ok, bit2 write failed), last state hash, checkpoints,
   lag, save flags, then up to 6 game words. "Lag" is the number of VBlanks the game did not
   see between two autopilot calls (scene loads, a judge pass and 30 fps frames count too): a hint,
   never a gate, and never part of a state hash.
8. **Real-card risks to keep visible**: R4 kernels usually emulate no SPI save chip
   (`save_detected=-1`); the QA mode mirrors the save to the SD and logs chip type, write
   result and read-back CRC. Report "chip save not verifiable on R4". `nitro=0` is logged,
   not fatal. FAT mount is checked with `stat()` before `fatInitDefault()`.
9. **Settings and saves**: `qa_game_reset()` pins every save setting that changes the run
   (difficulty, screen swap, night mode, touch look, microphone...) to its default; the whole
   `cv_save` and the replay area are backed up at the start and put back at the end (settings with a
   *higher* `seq`: the chip's newer slot wins at the next boot), so a QA run never leaves boosted data
   or its own replay on a real cart.
10. **Combo vs title keys**: if the title already uses one of L/R/X/Y (lightenette: X = credits),
    make the title ignore it while the others are held; document the press order.
11. **Own result layout**: a game whose judge reads a block layout of its own (lightenette: the
    rehearsal's, graded by `tools/judge.py`, boot `qa`) rewrites the block in `qa_game_publish()`,
    which `qa.c` calls after the `QARN` block was written; other games leave it empty.
12. **Watchdog**: a scene longer than `QA_SCENE_LIMIT` frames ends the run with FAIL
   (`scene-timeout`). The pilot must never be able to loop forever.

## Porting checklist

1. Copy `core/qa.c/.h`, `core/qalog.c/.h`; add `buildid.h` generation to the Makefile and a
   `qalog` row in the game AGENTS.md code map.
2. Write `game/qa_tune.h` (struct `QaTune`) and `game/qa_game.c`: `qa_tune_on/off`,
   `qa_game_pilot`, `qa_game_scene` (judges: checkpoint per stage/act, pass at the end, fail on
   unexpected scenes), `qa_game_hash`, `qa_game_words`, `qa_game_name`, `qa_game_reset`,
   `qa_game_publish`. A frame of the game can take two VBlanks: count pilot calls, not `cv_frame`,
   for "every other frame" patterns.
3. Hook: `core/input.c` (autopilot + fixed RTC), title scene (`qa_title_poll()`), main scene loop
   (`qa_scene()`), after saves (`qa_save_written()`), the tuned numbers.
4. Docs: game `AGENTS.md` code map, `docs/QA.md` + `QA-KO.md` (twins together).
5. Verify (below), then the `nds-license-audit` skill and the `nds-hw-reviewer` agent.

## Done criteria (every game)

- `./build.sh` zero warnings; two clean builds same SHA-256.
- Headless melonDS soft with an SD image (`SDDIR=out/mds/x tools/melonds/mds.sh run ...`, combo
  injected with events like `500:L:140,500:R:140,500:X:140,500:Y:140` after the title is up):
  `RESULT PASS`, log parses (`tools/qa/parse_log.py`), save mirror read-back matches.
- DeSmuME (`tools/emu/emu.sh run`, `RESULT_WORDS=16`): `QARN` block verdict 1.
- Two runs, identical checkpoint hashes (`tools/qa/compare.py`).
- QA off: same screenshots / demo results as before the change.
- `tools/qa/lint.py` passes. Report what screenshots you looked at and what you did not;
  DeSmuME, melonDS and hardware results are separate.

Games with `expected.json` + `tools/judge.py` (bug-reproducing games, today lightenette) also
need: a full run on every renderer (melonDS soft, GL classic, GL compute, DeSmuME), the QA run
graded with `tools/judge.py --profile <renderer>` equal to the known profile (a vanished NG
= bug not reproduced = fail; an extra NG = fail), and the QA run's judge values equal to the
rehearsal demo's on the same renderer.
