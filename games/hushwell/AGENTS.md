# Hushwell

Original first-person 3D survival horror for the DS. Design doc: the user's Claude
Docs page "Hushwell — 3D 서바이벌 호러 게임 기획서". Human docs (English, each with a
Korean twin `*-KO.md`): `README.md`, `docs/QA.md` (melonDS checklist + expected demo
screens), `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in `../../AGENTS.md`.
Created from the corvane template: `arm7/`, `common/` and most of `arm9/source/core`
and `gfx` are corvane's layers (keep fixes in sync when they apply to both).

## Build

```sh
./build.sh                  # hushwell.nds (EEPROM 64 KB)
./build.sh SAVE=flash|fram  # save-chip variants
./build.sh DEMO=<name>      # one feature demo (names: Makefile DEMOS, 66 of them)
./build.sh demos            # all demos
./build.sh -j8 release      # build/release/{rom,demos} (tag hushwell-vX.Y.Z runs this in CI)
./build.sh clean
```

`tools/mkassets.py` regenerates every asset into `build/gen` (C arrays) and
`build/nitrofs` (cart files) on each build when `tools/` changes.

## Controls (melonDS keys in brackets)

D-pad walk/turn (arrows) · A examine/doors/radio (X) · B hold run (Z) · Y flashlight (A) ·
X map↔recorder (S) · L hold hush (Q) · R hold aim, +A flare (W) · Start pause (Return) ·
Select swap screens (Backspace) · touch: map pins, recorder, dial lock.
QA: title + Select = start chapter; pause + hold L+R + A = clear chapter;
pause + X lights on, Y vision mask off, B developer console, R frame step.

## Code map

| Path | Contents |
|---|---|
| `common/cv_ipc.h` | ARM9↔ARM7 protocol (from corvane) |
| `arm7/source/` | own ARM7 core: input/RTC/power via libnds, register-level sound engine |
| `arm9/source/core/` | sys/IPC, irq, timer, dma, cvmath, rng, input+replay, snd, save (direct SPI), nitro, cart, qa + qalog (auto QA mode, SD log; copies of corvane's) |
| `arm9/source/gfx/` | vram (layout table incl. `VL_GAME`), oam, text, fx, gl3d (textures), models (creature + props), terrain (billboards, shadow volume) |
| `arm9/source/game/world.c` | map loading, player, creature AI (hearing/sight/BFS paths), items, doors, radios, flares, pause, `scene_chapter` |
| `arm9/source/game/render.c` | grid renderer (direct port writes, software per-corner lighting), props, creature hierarchy, particles, main-screen sprites |
| `arm9/source/game/chfx.c` | per-chapter hardware set-up (table at the top), vision mask, film, dual-screen chase |
| `arm9/source/game/hud.c` | bottom screen: affine map, meter, recorder, dial lock, pins, console lines |
| `arm9/source/game/qa_game.c`, `qa_tune.h` | auto QA: tune, BFS autopilot, run judge |
| `arm9/source/game/scenes.c`, `ending.c`, `game.c` | 2D scenes, endings, run state and save mapping |
| `arm9/source/demo/demo.c` | feature demos (`make DEMO=…`) |
| `tools/` | `mkassets.py` + `cvtools/` (`maps.py` ASCII maps + replay logs, `images.py`, `audio.py`, codecs) |

## Chapter → technique

0 road: in-car opening (WIN0 windshield, affine wipers), HBlank-DMA rain BG, fog, streamed BGM, W-buffer ·
1 lobby: WIN0/WIN1 vision mask, lamps, shadow volume, Z-buffer, 4x4 walls, keyhole OBJ window ·
2 hydro: translucent water, capture afterimage (bank C), affine caustics BG2 rippled by HBlank DMA on BG2X, VCount split, alpha-only fog, echo ·
3 archive (`VL_ARCHIVE`: OBJ in B, ext OBJ palettes in F): film, bitmap OBJ photo, portrait in 4 ext palettes, mosaic grain, vignette, capture recording played reversed + dial ·
4 chapel: RTC visitor (edge marking), toon table, alpha-test branches, W-buffer, rear-plane sky (`VL_CORE`) ·
5 well: shaft descent opening, dual-screen 3D chase (`VL_DUAL3D`), swarm polygon overflow, OBJ line limit, per-engine blackouts, AA.
Recorder (all chapters): REC captures the left mixer into RAM (`snd_record`, ARM7 capture 0 timed by channel 1), REV streams it reversed, PHOTO captures the screen into bank D (not in chapter 4 or the chase). The ARM7 samples the microphone each VBlank (`CvInputPacket.mic`).

## Judging

`expected.json` + `tools/judge.py` grade the demo result blocks and the auto QA run (demo `qa`); see `docs/QA.md`,
"Judging". They started as Corvane's (same demo names, same documented items); values that depend on this game's scenes
(polygon counts, HUD registers, the logo CRC) are Hushwell's own. Every item has `level` 1-4 and `level_basis`
(`../../docs/LEVELS.md`). When a demo's `finish()` values change, update `expected.json` and re-measure every profile.
Expected values come from GBATEK, a report or the ROM's own logic; measured-only ones are `unconfirmed`. The screen demos capture their own output (bank D, `probe_capture*` in `demo/demo.c`) and the
ARM7 reports channel/capture state in `CvInputPacket` for the sound demos; the game's chapters are not judged beyond the QA run.

## Contracts other code depends on

- **Map file** format is documented at the top of `tools/cvtools/maps.py`; the build
  checks that every chapter's exit, keys, tapes and radios are reachable. Objects are
  addressed by index (sorted), and `taken` bits (`cv_game.taken_lo/hi`) use those indices.
- **Auto QA mode** (skill `nds-qa-mode`; `core/qa.*`, `core/qalog.*` identical to corvane's): title,
  hold **L+R+X+Y for 120 frames**. The pilot reads the loaded map: BFS to tapes and keys, the chase key
  last, opens doors it holds the key for, sets the chapter 3 dial lock on the touch screen, takes the
  exit (the well in chapter 5, with all five tapes: ending "The Well"), taps A through the other scenes.
  Numbers only: creature hits cost no health (`qa_tune()->hurt_damage`), fixed run seed, RTC hour 12,
  mic 0. Ends on the records screen with `RESULT PASS` (fails on death, a missing full ending,
  an unexpected scene or a stuck pilot). Saves run as shipped (chapter starts, radios, records) and
  are mirrored to the SD card; the cart's own save is restored at the end.
- **Demo result block** at `0x0237F000`: `u32 'CVER', u32 FNV-1a(name), u32 frame, u32 values[13]`,
  written at frame 120. The last 4 KB of the ARM9 heap is reserved for it.
- **Save layout** (`core/save.c`, magic `HWSV`): two copies (seq + BIOS CRC16) at
  0x0000/0x0400 (FLASH: 0x0000/0x10000), replay area at 0x8000 / 0x20000 / 0x0800
  (EEPROM/FLASH/FRAM), max 30000 bytes, as in corvane. On an 8 KB chip (what melonDS
  emulates for our header game code `HSWL`) the EEPROM/FRAM builds detect the 0x2000
  address wrap and keep the replay at 0x0800, max 6140 bytes (FLASH build without a
  JEDEC ID: max 7675, and each write goes out as one SPI command because an EEPROM
  reads the 3-byte addresses as 2-byte ones). A longer run is cut to the runs that
  fit (`replay_blob(max)` with `save_replay_capacity()`), so its replay stops early;
  a log that still doesn't fit clears the area. Progress (`cont_*`) is written at
  every radio and chapter start.
- **Replay header** (32 bytes): magic `CVRP`, seed, run count, then the run start state
  (`CvRunStart`: chapter, RTC hour, health, flares, radio, keys, battery, tapes, taken bits,
  options = touch-look, night-visit and mic settings); each run entry carries the mic level.
  Paused frames and the scripted openings are not logged (`replay_suspend`).
  Change C (`core/input.c`) and Python (`tools/cvtools/maps.py`) together.
- **Determinism**: gameplay may only use `rng_*` (seed from the save or the replay), input
  via `cv_in`, RTC via `input_rtc_hour()`. Timers drive only display/sound (heartbeat,
  flashlight flicker).
- **VRAM**: chapters keep engine B in banks H/I (`VL_GAME`, `VL_CORE`); the HUD fits 32 KB.

## Adding things

- **A chapter map**: edit the ASCII in `maps.CHAPTERS` (legend at the top of `maps.py`);
  the build fails if the exit or an item becomes unreachable. Per-chapter hardware goes in
  `chfx.c` (`chfx_setup`/`chfx_frame`/`chfx_teardown`).
- **A demo**: add a `d_xxx()` in `demo/demo.c` that uses only its feature plus the text
  console, finishes with `finish(v0..v3)` by frame 120, register it in `demos[]`, add the
  name to `DEMOS` in the `Makefile`, and add an expected-screen row to `docs/QA.md` and `docs/QA-KO.md`.
- **A texture**: add to `images.textures()`, `CvTex` enum and `tex_desc[]` in `gl3d.c`.
- **An asset**: generate it in `tools/cvtools/`, emit it from `mkassets.py`, add a row to
  `ASSETS.md` and `ASSETS-KO.md`.
- **A model**: build it in `gfx/models.c` with the `mb_*` helpers (set `keep_color` for parts
  that must also draw unlit in another colour, like the night visitor).
