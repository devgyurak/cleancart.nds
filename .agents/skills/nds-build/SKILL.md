---
name: nds-build
description: Build a DS game ROM in this repo with BlocksDS (main ROM, a single feature demo, all demos, or save-chip variants) and check that the build is warning-free and reproducible. Use whenever a ROM needs building or a build fails; not for running ROMs (see nds-emu-test, melonds-mac).
---

# Build a ROM

All builds go through the game's `build.sh`, which builds the pinned BlocksDS
Docker image (plus Python 3) and runs `make` inside it. Never use a host toolchain.

```sh
cd <game>                      # e.g. corvane
./build.sh                     # <game>.nds
./build.sh DEMO=sys-ipc        # <game>-sys-ipc.nds (one feature demo)
./build.sh demos               # every name in the Makefile DEMOS list (~4 min)
./build.sh SAVE=flash          # <game>-save-flash.nds (also: fram)
./build.sh -j8 release         # everything a release ships → build/release/{rom,demos}
./build.sh clean
```

Releases: pushing a tag `<game>-v<x.y.z>` runs `.github/workflows/release.yml`,
which calls `./build.sh release` and uploads `build/release/rom` (artifact `<tag>`)
and `build/release/demos` (`<tag>-demos`); a second job (the only one with
`contents: write`) zips them and attaches `<tag>.zip`, `<tag>-demos.zip` and
`SHA256SUMS` to a GitHub Release (suffix like `-rc.1` → pre-release; re-runs
replace the files). Every game's Makefile needs a `release`
target that fills those folders, each with LICENSE, NOTICE, THIRD_PARTY_NOTICES and
`licenses/`. Tag or push only when the user asks.

## Check the result

1. Warnings: `./build.sh 2>&1 | grep -E "error|warning"` must print nothing.
2. Size sanity: `ls -la <game>.nds` (corvane ≈ 1.1 MB).
3. Reproducibility (after toolchain/asset-pipeline changes):
   ```sh
   ./build.sh clean && ./build.sh && shasum -a 256 <game>.nds
   ./build.sh clean && ./build.sh && shasum -a 256 <game>.nds   # must match
   ```
4. Symbol lookups (addresses for the emulator harness or GDB):
   ```sh
   docker run --rm -v "$PWD":/w -w /w corvane-build:1 \
     /opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-nm build/<variant>/arm9.elf | grep cv_game
   ```
   `<variant>` is `corvane`, `corvane-<demo>` or `corvane-save-<type>`.
   Struct offsets: compile a tiny file with `offsetof` using `arm-none-eabi-gcc -S`
   (host compilers have a different pointer size).

## Common failures

- `No such file` for a header: include paths are `-Iarm9/source -Icommon -Ibuild/gen`.
- Name clash with libnds macros (e.g. `OAM_SUB`): prefix our identifiers (`ENG_SUB`).
- Asset step fails: run `python3 tools/mkassets.py build/gen build/nitrofs` on the
  host to see the Python traceback quickly (host Python is fine for diagnosis).
- `build.sh clean` also deletes demo ROMs; rebuild them with `./build.sh demos`.
