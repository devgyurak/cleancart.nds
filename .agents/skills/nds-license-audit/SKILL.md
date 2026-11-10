---
name: nds-license-audit
description: Check a DS game folder for licence compliance (SPDX headers, generated/CC0 assets listed in ASSETS.md, third-party notices, licence texts in NitroFS and in-game credits, no committed ROMs, no trademarks). Use before finishing any change that adds files, assets or libraries, or when the user asks about licences.
---

# Licence audit

Run from the game folder (e.g. `corvane/`).

1. **SPDX headers** on all sources and build files:
   ```sh
   for f in $(find arm7 arm9 common tools -type f \( -name '*.c' -o -name '*.h' -o -name '*.py' \)) Makefile Dockerfile build.sh; do
     head -3 "$f" | grep -q "SPDX-License-Identifier: Apache-2.0" || echo "MISSING: $f"; done
   ```
   Also check repo-level `tools/` and `scripts/`.
2. **Assets**: every output of `tools/mkassets.py` (C arrays in `build/gen/assets.h`,
   files in `build/nitrofs/`) has a row in `ASSETS.md` (and the same row in
   `ASSETS-KO.md`) with its generator. No
   binary asset files in git (`git ls-files | grep -Ei '\.(png|gif|bmp|wav|ttf|mid)$' | grep -v '/docs/images/'`
   should be empty unless listed as CC0 with a source URL). `docs/images/` may hold
   README screenshots/GIFs captured from our own ROMs; check that nothing there is
   referenced by the Makefile or `mkassets.py` (it must never reach the ROM).
3. **Linked libraries**: inspect the link map for what actually ends up in the ROM:
   `grep -oE 'lib[a-z0-9_]+\.a' build/<variant>/arm9.map build/<variant>/arm7.map | sort -u`.
   Expected: libnds9/libnds7, libc (picolibc), libgcc. Anything else needs a notice.
   `dswifi`, `maxmod`, `mbedtls`, `xm7` must not appear.
4. **Notices**: `THIRD_PARTY_NOTICES` covers every library from step 3; the texts are in
   `licenses/` and copied to `build/nitrofs/licenses/`; the credits array in
   `arm9/source/game/scenes.c` mentions each one (crts MPL-2.0 sentence + source URL,
   picolibc copyright, libnds).
5. **Originality**: grep names/text for third-party game titles or characters.
6. **Git hygiene**: `git status --short | grep -E '\.nds|\.sav|build/'` is empty
   (.gitignore covers them).

Report findings as a short list; fix what's in scope, ask about the rest.
