---
name: nds-new-game
description: Start a new DS game folder in this repo from the corvane template (pinned BlocksDS build, own ARM7 core, licence files, asset generator skeleton, demo framework). Use when the user asks for a new game or prototype; not for changes inside an existing game.
---

# New game from the corvane template

1. Read the design request; if it names an existing commercial game, build an
   original game in that genre instead (no names, characters, art or levels).
2. Create `<name>/` (lowercase, no trademarks) and copy the infrastructure:
   ```sh
   mkdir <name> && cd <name>
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
3. Write `tools/mkassets.py` for the new game's assets (start from corvane's, keep the
   `CWriter`/`pack`/`lz` helpers) and an `ASSETS.md`.
4. Write `<name>/AGENTS.md` (code map, contracts, how to add things) and
   `<name>/CLAUDE.md` containing only `@AGENTS.md`.
5. Build (`nds-build`), smoke test (`nds-emu-test`), audit (`nds-license-audit`).
6. Add the game to the root `AGENTS.md` layout section.
