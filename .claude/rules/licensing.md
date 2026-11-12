# Licensing (always applies)

- New source/script/build file → SPDX header first:
  `SPDX-License-Identifier: Apache-2.0` + `SPDX-FileCopyrightText: 2026 Corvane contributors`.
- New asset → produced by code in `games/<game>/tools/` (or CC0 with source URL) and listed
  in `games/<game>/ASSETS.md` and its Korean twin `ASSETS-KO.md` with generator, source and licence. Never copy, trace,
  sample or "recreate" existing art, fonts, music, sounds or levels.
- Exception: README screenshots/GIFs captured from our own ROMs go in
  `games/<game>/docs/images/` only. They are docs, not ROM assets: never built into the
  ROM, no ASSETS.md row, same licence as the game, no third-party content in frame.
  README images show the top screen only (the physical top screen, also when a scene swaps
  the screens), scaled 2x nearest-neighbour to 512×384, and say in the caption which emulator
  captured them.
  The repository logo (`assets/logo/*.svg`) is output of `scripts/mklogo.py`:
  change the script and re-run it, never hand-edit the SVGs. `assets/` at the repository root
  is repository branding, not ROM assets: no `ASSETS.md` row.
- New library linked into a ROM → same change updates `THIRD_PARTY_NOTICES`,
  `licenses/<lib>-*.txt`, the in-game credits (`game/scenes.c` `credits[]`) and the
  licence viewer's file list.
  Prefer not to add libraries; the ARM7 core stays our own (no DSWiFi/Maxmod/Mbed TLS).
- Emulator, tool, library and platform names (melonDS, DeSmuME, BlocksDS, GBATEK, Nintendo DS, …) may be used for factual description in docs, tools, comments, commit messages and `expected.json`. They never appear in game names, ROM content (in-game text, assets, NitroFS files) or code identifiers built into the ROM — except the third-party credits and licence texts that rule 4 requires. Third-party game names, characters and logos are not used anywhere.
- Build artifacts (`build/`, `*.nds`, `*.sav`) are never committed.
- Run the `nds-license-audit` skill (or `license-auditor` agent) before calling work done.
