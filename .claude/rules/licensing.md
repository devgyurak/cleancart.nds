# Licensing (always applies)

- New source/script/build file → SPDX header first:
  `SPDX-License-Identifier: Apache-2.0` + `SPDX-FileCopyrightText: 2026 Corvane contributors`.
- New asset → produced by code in `<game>/tools/` (or CC0 with source URL) and listed
  in `<game>/ASSETS.md` and its Korean twin `ASSETS-KO.md` with generator, source and licence. Never copy, trace,
  sample or "recreate" existing art, fonts, music, sounds or levels.
- Exception: README screenshots/GIFs captured from our own ROMs go in
  `<game>/docs/images/` only. They are docs, not ROM assets: never built into the
  ROM, no ASSETS.md row, same licence as the game, no third-party content in frame.
  The repository logo (`docs/logo/*.svg`) is output of `scripts/mklogo.py`:
  change the script and re-run it, never hand-edit the SVGs.
- New library linked into a ROM → same change updates `THIRD_PARTY_NOTICES`,
  `licenses/<lib>-*.txt` and the in-game credits (`game/scenes.c` `credits[]`).
  Prefer not to add libraries; the ARM7 core stays our own (no DSWiFi/Maxmod/Mbed TLS).
- No third-party trademarks, game names or characters in names, identifiers, text or assets.
- Build artifacts (`build/`, `*.nds`, `*.sav`) are never committed.
- Run the `nds-license-audit` skill (or `license-auditor` agent) before calling work done.
