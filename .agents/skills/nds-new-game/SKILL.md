---
name: nds-new-game
description: Start a new DS game folder in this repo from the corvane template (pinned BlocksDS build, own ARM7 core, licence files, asset generator skeleton, demo framework). Use when the user asks for a new game or prototype; not for changes inside an existing game.
---

# New game from the corvane template

0. A new game starts from a brief of the `game-planner` agent (target level, area, judged items, concept). Set `target_level`, `area` and every item's `role` in `expected.json` (`docs/LEVELS.md`).
1. Read the design request; if it names an existing commercial game, build an
   original game in that genre instead (no names, characters, art or levels).
2. Create `games/<name>/` (lowercase; no third-party names, see rule 3 in `AGENTS.md`) and copy the infrastructure:
   ```sh
   mkdir games/<name> && cd games/<name>
   cp ../corvane/{Dockerfile,build.sh,Makefile,.gitignore,LICENSE} .
   cp -R ../corvane/licenses .
   cp ../corvane/THIRD_PARTY_NOTICES ../corvane/NOTICE .
   mkdir -p arm9/source arm7/source common tools docs
   cp -R ../corvane/arm7/source/* arm7/source/          # own ARM7 core (input, RTC, sound)
   cp ../corvane/common/cv_ipc.h common/
   cp -R ../corvane/arm9/source/core ../corvane/arm9/source/gfx arm9/source/  # reusable layers
   cp -R ../corvane/tools/cvtools tools/                # codecs, gfx encoders, font
   ```
   Docs: English `README.md`/`ASSETS.md`/`docs/QA.md`, each with a Korean `*-KO.md` twin.
   Then trim what the new game doesn't use and rename `corvane`/`Corvane` in the
   Makefile (`GAME_TITLE`, variant names, the ROM list in the `release` target),
   `build.sh` image tag, NOTICE and notices. Keep the `release` target: the release
   workflow builds tag `<name>-v<x.y.z>` with `./build.sh release`.
   Set a new header identity in the Makefile (see `.claude/rules/build-and-verify.md`):
   `GAME_CODE := <CODE> 00 <TITLE>` with a 4-char code used by no other game here
   and absent from melonDS's `src/ROMList.cpp`, and `UNIT_CODE := 0`; check that
   ndstool gets `-g $(GAME_CODE) -uc $(UNIT_CODE)`. Never leave `####`.
3. Check the copied layers carry the melonDS/hardware fixes from corvane issue #3.
   Once those fix PRs are merged into `develop`, corvane's `core/` and `gfx/` have
   them. If you copy before that, check and port by hand:
   - `core/sys.c`: MPU region 2 mapped over shared WRAM while the ARM9 owns it
     (`mpu_map_swram()`), else a data abort on the first WRAM access.
   - `core/dma.h`/`dma.c`: main-memory display FIFO off the VBlank OAM channel
     (`CV_DMA_CH_MMEM` = channel 1).
   - `core/save.c`: 8 KB EEPROM wrap detection, 32-byte pages, FLASH JEDEC check,
     no-chip decision in `save_detected_type()` rather than on a 0xFF status.
   - `gfx/gl3d.c`/`gl3d.h`: `g3_flush_measured()` for demos that report polygon counts.
   - `demo/demo.c` (if copied): no reads of write-only registers; the 3d-limits
     grid on screen.
   - Licence viewer (if copied from `game/scenes.c`): every `licenses/` file listed,
     UTF-8 folded; hints like "START: …" handled in every sub-state.
4. Write `tools/mkassets.py` for the new game's assets (start from corvane's, keep the
   `CWriter`/`pack`/`lz` helpers) and an `ASSETS.md`.
5. Write `<name>/AGENTS.md` (code map, contracts, how to add things) and
   `<name>/CLAUDE.md` containing only `@AGENTS.md`.
6. Build (`nds-build`), smoke test (`nds-emu-test`), audit (`nds-license-audit`).
   Run the demo suite and a boot → save → reload pass in headless melonDS too
  : `tools/melonds/mds.sh demos games/<game>/build/release/demos` and check
   every RESULT and that the `SAVE` type isn't 0; then
   `SAV=out/mds/<game>.sav tools/melonds/mds.sh run …` twice (save, then reload)
   and check the save comes back and the `CPU9 cpsr` mode isn't 0x17 (data abort).
   DeSmuME alone missed the MPU, DMA, header and save bugs.
7. A new game ships `expected.json` + `tools/judge.py` from the start, and every item in
   `expected.json` has `level` (1–4) and `level_basis` and `role` (`docs/LEVELS.md`; ask the `level-judge`
   agent). `python3 scripts/mklevels.py --check` must pass (add the game's `<!-- cell:<game> -->` marker
   to the Games table of both root READMEs).
8. The QA mode comes with `core/` (`qa.c/.h`, `qalog.c/.h`) and the Makefile `buildid.h` rule.
   Add `game/qa_tune.h` + `game/qa_game.c` and the hooks from skill `nds-qa-mode` (porting
   checklist) before the first release; a game without a working QA run isn't done.
9. Add the game to the root `AGENTS.md` layout section.
