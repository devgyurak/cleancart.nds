# Corvane

Original 3D hover shooter for the DS. Design doc: the user's Claude Docs page
"Corvane — 3D 액션 슈팅게임 기획서". Human docs (English, each with a Korean
twin `*-KO.md`): `README.md`, `docs/QA.md` (melonDS checklist + expected demo
screens), `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in `../AGENTS.md`.

## Build

```sh
./build.sh                  # corvane.nds (EEPROM 64 KB)
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=<name>      # one feature demo (names: Makefile DEMOS, 65 of them)
./build.sh demos            # all demos
./build.sh -j8 release      # build/release/{rom,demos} (tag corvane-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and
`build/nitrofs` (cart files) on each build when `tools/` changes.

## Code map

| Path | Contents |
|---|---|
| `common/cv_ipc.h` | ARM9↔ARM7 protocol: IPCSYNC values, FIFO channels (user 01–03), sound command words, input packet |
| `arm7/source/` | own ARM7 core: input/RTC/power via libnds, register-level sound engine (`sound7.c`) |
| `arm9/source/core/` | sys/IPC, irq, timer, dma, cvmath (hw div/sqrt), rng, input+replay, snd, save (direct SPI), nitro, cart (raw cart DMA), qalog (QA log on the SD card + `QARN` result block) |
| `arm9/source/gfx/` | vram (bank plan table), oam, text, fx, gl3d, models, terrain |
| `arm9/source/game/` | scenes, stage loop, boss, stagefx (per-stage hardware showcase), hud, ending |
| `arm9/source/demo/demo.c` | feature demos (`make DEMO=…`) |
| `tools/` | `mkassets.py` + `cvtools/`, `stage_editor.py`, `replay.py`, `stages/*.json` overrides |

## Stage → technique

1 coast: 4x4 compressed terrain, fog, 2D sea BG behind 3D + HBlank-DMA wave ·
2 sky: affine sky (**mode 2**), alpha-blended clouds, alpha-only fog, RTC night windows ·
3 station: 1024x512 large bitmap (mode 6, banks A–C), toon + edges, capture afterimage, ext-palette affine meteors ·
4 interior: screen swap + touch aim, Z-buffer, 4 lights, shadow volume ·
5 core: hierarchical boss + BoxTest picking, rear-plane bitmap (banks C/D), AA, sound-capture echo, polygon overload.

## Contracts other code depends on

- **Demo result block** at `0x0237F000`: `u32 'CVER', u32 FNV-1a(name), u32 frame, u32 values[13]`,
  written at frame 120. The last 4 KB of the ARM9 heap is reserved for it (`reduceHeapSize(4096)`).
- **QA log** (`core/qalog.c`, `tools/qa/`): RAM ring of `<frame> <scene> <event> key=val` lines,
  flushed to `<drive>cleancart/corvane/qa-<date>-<time>.log` only at checkpoints, scene changes,
  pause and the end (never inside a save transaction; every flush claims the slot-1 bus and
  closes the file). Ends with `RESULT PASS|FAIL`; the RAM result block reuses `0x0237F000`
  with id `QARN`. `tools/qa/parse_log.py` validates a log, `tools/qa/lint.py` keeps `qa*`
  symbols out of `gfx/`, `core/save.c`, `judge/`, `capture.c`. Headless SD:
  `SDDIR=out/mds/x tools/melonds/mds.sh run …` (melonDS gives an SD card only to homebrew
  ROMs, so the runner patches the in-memory game code; the ROM on disk is unchanged and the
  run has no save chip).
- **Save layout** (`core/save.c`): two copies (seq + BIOS CRC16) at 0x0000/0x0400
  (FLASH: 0x0000/0x10000), replay area at 0x8000 / 0x20000 / 0x0800 (EEPROM/FLASH/FRAM),
  max 30000 bytes. On an 8 KB chip (what melonDS emulates for our header game
  code `CRVN`) the EEPROM/FRAM builds detect the 0x2000 address wrap and keep the
  replay at 0x0800, max 6140 bytes; the FLASH build there keeps it at 0x0201
  (folded), max 7675. A longer run is cut to fit (playback ends early), never
  dropped in favour of an older one. `tools/replay.py dump x.sav` reads all of them.
- **Replay header** (16 bytes): magic `CVRP`, seed, run count, stage, RTC hour, lives, bombs.
  Change C (`core/input.c`) and Python (`tools/cvtools/stages.py`) together.
- **Stage file** format is documented at the top of `tools/cvtools/stages.py`.
- **Determinism**: gameplay may only use `rng_*` (seeded from a constant or the replay),
  input via `cv_in`, RTC via `input_rtc_hour()`. Never seed from timers/RTC; the
  boss beat timer is started inside VBlank; HUD shows stage frames, not wall time.
  A recorded run and its replay must produce identical `CvStage` memory
  (only heap pointers and message strings may differ).

## Adding things

- **A demo**: add a `d_xxx()` in `demo/demo.c` that uses only its feature plus the
  text console, finishes with `finish(v0..v3)` by frame 120, register it in `demos[]`,
  add the name to `DEMOS` in the `Makefile`, and add an expected-screen row to `docs/QA.md` and `docs/QA-KO.md`.
- **A texture**: add to `images.textures()`, `CvTex` enum and `tex_desc[]` in `gl3d.c`.
  Stage 5 loads textures only into banks A/B; keep the total small.
- **An asset**: generate it in `tools/cvtools/`, emit it from `mkassets.py`, add a row to
  `ASSETS.md` and `ASSETS-KO.md`.
- **A model**: build it in `gfx/models.c` with the `mb_*` helpers; faces are emitted
  counter-clockwise from outside automatically (verified: CCW = front on DS).
