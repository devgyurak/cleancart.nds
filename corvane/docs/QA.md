# Corvane QA

[한국어](QA-KO.md)

QA requirement from the design document: check every demo build and the main build
in **melonDS**, and record the melonDS version, demo name and expected screen.

## Rules shared by all demo builds

- Build: `./build.sh DEMO=<name>` → `corvane-<name>.nds` (`./build.sh demos` for all)
- Screen: the demo name and results appear on a text BG console on the main screen
  (3D demos use a BG1 console).
- Checkpoint: 120 frames after start the screen is complete and two `CHECKPOINT`
  lines are printed. The same values are written to main RAM at `0x0237F000`.

```
0x0237F000 u32 magic   = 0x52455643 ("CVER")
0x0237F004 u32 demo_id = FNV-1a 32 hash of the demo name
0x0237F008 u32 frame   = VBlank count at the checkpoint
0x0237F00C u32 values[13] (per demo, see the table below)
```

- Demos that need input (sys-keys, io-touch, io-power, io-rtc, io-firmware) keep
  updating the screen after the checkpoint.

## QA record

melonDS version: 1.1 (macOS app and the same core headless)  Tester: Claude Code, automated (see [melonDS QA record](#melonds-qa-record-2026-11-11))  Date: 2026-11-10/11

| Demo | What to check / expected screen | Checkpoint values (values[0..3]) | melonDS result |
|---|---|---|---|
| sys-ipc | console shows IPCSYNC value A, FIFO ping 1–2 frames, input packets ≈120 | IPCSYNC, ping frames, packet count, KEYXY | ✅ `a 1 78 7f` sync A, ping 1 frame, 120 packets |
| sys-wram | WRAMCNT 3→0→3, 0 errors in the 32 KB pattern | error count, CRC16, final WRAMCNT | ✅ `0 c4d9 3` CRC c4d9, 0 errors. Was a data abort on v0.1.0 (#6), fixed by #14 |
| sys-irq | top half blue (VBlank) / bottom half red (VCount 96), counters shown | VBlank 120, HBlank ≈31,000, VCount 120, timer ≈20 | ✅ `78 7ac3 78 13` top blue / bottom red; HBlank 31,427 |
| sys-timer | four prescalers, cascade seconds ≈2, beats ≈16 | seconds, beats, 1:1 measurement µs, 1:64 measurement µs | ✅ `1 f a1 1d4fb` 1 s and 15 beats at the checkpoint, the same as DeSmuME (≈2 s / ≈16 beats need the full 2 s); 161 µs, 120,059 µs |
| sys-dma-now | 0 errors copying 4 KB | error count | ✅ `0 1000` 0 errors |
| sys-dma-vblank | background colour changes every frame, 120/120 verified | verified frames | ✅ `78` 120/120 frames verified |
| sys-dma-hblank | blue→red gradient background from top to bottom | colour at line 100, HBlank count | ✅ `4c8c` blue→red gradient |
| sys-dma-cart | cart DMA CRC = NitroFS CRC = expected | found (1), CRC ×3 | ✅ `1 42b8 42b8 42b8` cart DMA = NitroFS = expected 42b8 |
| sys-dma-gfx | textured cube rotating | list words, polygon count | ✅ `9a 2` cube rotating, 2 visible faces. Polygon counts were 0 (#21), fixed by #22 |
| sys-dma-disp | top: colour gradient (main-memory FIFO) / bottom: per-line bars | pixel value, bar colour | ✅ `f190 208c` FIFO gradient on top, per-line bars below. Hung on v0.1.0 (#7), fixed by #15 |
| sys-dma-fill | 0 errors filling VRAM and RAM | error count, value | ✅ `0 deadbeef` 0 errors |
| sys-bios | LZ77 VRAM = WRAM, CpuSet/FastSet, Div 142, Sqrt 1000, CRC16 | difference 0, FastSet value, 142, CRC | ✅ `0 12345678 8e c14f` LZ77 diff 0, Div 142 (8e), CRC c14f |
| sys-math | 4 divisions, sqrt(1e12)=1000000, len(3,4,0)=5×4096, atan2=4096 | error count, sqrt, len, atan2 | ✅ `0 f4240 5000 1000` 0 errors, sqrt 1000000, len 5×4096, atan2 4096 |
| sys-keys | buttons/X/Y/touch/lid shown live, key IRQ count | KEYS, key IRQs, KEYXY | ✅ `0 1 7f` buttons, X/Y, pen and lid shown live |
| 2d-engines | top blue, bottom red, screens swap every 60 frames | POWERCNT, DISPCNT ×2 | ✅ `820f 11110 10100` top blue, bottom red, swap every 60 frames |
| 2d-text | bottom-screen HUD (4bpp) + an 8bpp console line at the bottom | BG3CNT_SUB, BG0CNT_SUB | ✅ `e84 b00` HUD + 8bpp console line |
| 2d-affine | rotating sky (affine) + growing CORVANE logo (extended affine) | BG2PA, BG3PA | ✅ `ffda d9` rotating sky + growing logo. Read 0 on v0.1.0 (#11), fixed by #19 |
| 2d-bitmap | 0–59: 16bpp title + 8bpp logo, 60+: 1024x512 nebula scrolling | DISPCNT, BG2CNT, phase 2 | ✅ `10406 4003 2` title + logo, then the nebula scrolling |
| 2d-mosaic | text and explosion sprites get coarser mosaic | MOSAIC | ✅ `eeee` mosaic grows. Read 0 on v0.1.0 (#11), fixed by #19 |
| 2d-obj-normal | 100 bullet sprites moving in circles, half behind the BG | OAM count 100 | ✅ `64` 100 sprites, half behind the BG |
| 2d-obj-affine | four differently coloured meteors rotating/scaling + a growing explosion | OAM count | ✅ `5` 4 meteors + growing explosion |
| 2d-obj-bitmap | two 64x64 pilot cut-ins (one semi-transparent) | OAM count | ✅ `2` two cut-ins, one blended |
| 2d-obj-map2d | the same explosion on top (1D) and bottom (2D mapping) | DISPCNT_SUB | ✅ `11000` same explosion top and bottom |
| 2d-obj-line | 120 large bullets on one band — some vanish (line limit) | OAM count | ✅ `78` 120 sprites on one band, the line limit drops some |
| 2d-extpal | same illustration: left original colours / right recoloured + four meteor colours | DISPCNT, map entry | ✅ `c0011310 1000` original / recoloured halves + 4 meteor colours |
| 2d-window | only the two rectangles are bright, circular radar mask below | WININ, WINOUT, WINOUT_SUB | ✅ `1f1f 3f 1f1b` bright rectangles, round radar mask |
| 2d-blend | every 30 frames: alpha / brighten / darken / semi-transparent OBJ | BLDCNT, BLDALPHA | ✅ `2340 60a` the four modes cycle |
| 2d-brightness | top fades to white, bottom to black, independently | MASTER_BRIGHT ×2 | ✅ `4007 800f` top to white, bottom to black |
| 2d-vram | table of VRAMCNT A–I for the 8 layouts | table hash | ✅ `73a1b396` VRAMCNT table for the 8 layouts |
| 2d-capture | 3D on both screens (boss core / ship) | DISPCAPCNT (no console) | ✅ `1320000` 3D on both screens |
| 2d-display | 0–59 VRAM display, 60+ main-memory FIFO display | pixel values | ✅ `0 800a` VRAM display, then main-memory FIFO. Hung on v0.1.0 (#7), fixed by #15 |
| 3d-fifo | ship (DMA list) over terrain (port writes) | polygon/vertex count, list size | ✅ `118 29c 14a` 280 polygons, 668 vertices. Was a data abort on v0.1.0 (#6), fixed by #14; polygon counts were 0 (#21), fixed by #22 |
| 3d-matrix | boss whose panels, arms and claws move hierarchically | boss HP total, polygon count | ✅ `190 6b` boss parts move hierarchically, 107 polygons |
| 3d-light | station with four coloured lights circling | polygon count | ✅ `27` four coloured lights circling, 39 polygons |
| 3d-clip | camera skims the walls, near-plane clipping | polygon/vertex count | ✅ `21 86` near-plane clipping, 33 polygons / 134 vertices. Was a data abort on v0.1.0 (#6), fixed by #14 |
| 3d-tex-a3i5 / a5i3 / pal4 / pal16 / pal256 / 4x4 / direct | a quad in that texture format rotating, with repeat, flip and texture-matrix scrolling | texture id, size | ✅ a3i5 `0 10`; a5i3 `1 20`; pal4 `2 20`; pal16 `3 40`; pal256 `4 40`; 4x4 `5 80`; direct `6 20`. Quad rotating with repeat, flip and scroll; 1 polygon. Polygon counts were 0 (#21), fixed by #22 |
| 3d-poly | striped floor, front/back-culled cubes, translucent airship, ship shadow | polygon count | ✅ `51` floor, culled cubes, airship, ship shadow; 81 polygons |
| 3d-toon | 0–59 toon / 60+ highlight shading | DISP3DCNT | ✅ `60f 6b` highlight shading after frame 60 |
| 3d-edge | outlines per polygon ID | DISP3DCNT | ✅ `62d 6b` outlines per polygon ID |
| 3d-fog | 0–59 distance fog / 60+ alpha-only fog | DISP3DCNT | ✅ `6cd 15c` alpha-only fog after frame 60. Was a data abort on v0.1.0 (#6), fixed by #14 |
| 3d-aa | cube with AA + alpha-tested wire mesh | DISP3DCNT | ✅ `61d 3` AA cube + alpha-tested mesh |
| 3d-depth | 0–59 W-buffer and solid rear plane / 60+ Z-buffer and bitmap rear plane | DISP3DCNT | ✅ `460d e` Z-buffer + bitmap rear plane after frame 60 |
| 3d-test | BOX INSIDE/OUTSIDE toggles, POS and VEC results | inside count, w, vec x, vec z | ✅ `66 51a8 9b7 fffff36c` BOX INSIDE, POS and VEC shown |
| 3d-compose | 3D cube in front of → behind the 2D sea, bullet sprites over 3D, BG0 shaking | BG0CNT, BG2CNT | ✅ `3 1d09` cube behind the sea, bullets over 3D |
| 3d-limits | triangles exceed 2048 and some vanish, overflow frames > 0 | overflow frames, polygon RAM | ✅ `2f 800` 2980 triangles sent, polygon RAM full at 2048, 47 overflow frames. Was 1692 / 0 on v0.1.0 (#9), fixed by #17 |
| snd-mix | boss BGM + 8 different effects panned left/right | song number, 8 | ✅ `6 8` no gaps in the WAV; a few clipped samples |
| snd-pcm | PCM8 one-shot → PCM16 one-shot → PCM8 loop → PCM16 loop | sample numbers | ✅ `13 b` four sounds in order, no gaps |
| snd-adpcm | ADPCM jingle (one-shot) → hum (loop) | sample numbers | ✅ `4 f` jingle then hum, no gaps |
| snd-psg | beeps on channels 8–13 with changing duty, noise | 8, 15 | ✅ `8 f` square sweep + noise; stops at the checkpoint |
| snd-capture | stage 3 song + echo | 1 | ✅ `1` song + echo, no gaps |
| snd-stream | seamless ambient pad | size, sample rate | ✅ `2b000 2b11` 176,128 bytes at 11,025 Hz, no gaps |
| io-cart | NitroFS file sizes, cart DMA OK, stage 3 info | total, load ok, cart DMA, spawn count | ✅ `61c4c 1 1 2d` NitroFS mounted, all files OK, cart DMA OK. Was a data abort on v0.1.0 (#6), fixed by #14 |
| io-save-eeprom / flash / fram | WRITE+VERIFY OK, RELOAD OK | write, reload, seq +1, chip answers | ✅ eeprom `1 1 1 1`; flash `1 1 1 2`; fram `1 1 1 3`. melonDS attaches an 8 KB EEPROM to every build. No save chip on v0.1.0 (#8), fixed by #16 |
| io-firmware | name, favourite colour → ship colour, language, birthday, touch calibration | colour, language, name length, ship colour | ✅ `0 1 7 4e0c` name MELONDS, colour 0, English, 01/01, calibration shown |
| io-rtc | date and time update every second, stage 2 day/night | year, month, day, hour | ✅ `1a b a f` date and time tick; stage 2 sky DAY |
| io-touch | crosshair at the pen position, pixel/raw values | touching, x, y, raw x | ✅ `0 78 64 0` pen 120,100 while touched |
| io-power | A: top backlight, B: speaker, closing the lid sleeps | battery, lid, backlight, speaker | ✅ `f 0 0 1` battery OK, lid open, A/B toggle |

## Main build checklist

| Item | How to check | melonDS result |
|---|---|---|
| Boot logo → intro → title | just run it | ✅ The intro's display FIFO only got through by luck on v0.1.0 (#7, fixed by #15) |
| Demo play | wait 20 s on the title; the five stages play in turn; a key returns to the title | ✅ Stages 1→5 in turn, 1100 frames each, title in between; B returns to the title |
| New game → stages 1–5 → ending → staff roll → records | play from start to finish | ✅ Scripted run (ships kept up) through every boss, the dual-screen ending, the staff roll and the records (531,160). On v0.1.0 the game stopped on a black screen after the stage 1 briefing (#6, fixed by #14) |
| Continue | lose all ships, A restarts the same stage | ✅ Restarts stage 1 with 3 ships, three times in a row |
| Resume | clear a stage, power off, power on, CONTINUE | ✅ Stage 1 clear → PROGRESS SAVED → reboot with the .sav → CONTINUE → stage 2 briefing, score kept. No save at all on v0.1.0 (#8, fixed by #16) |
| Records | date and time are saved from the RTC | ✅ New record shows the RTC date and time |
| Replay | X on the records screen replays the same run exactly | ✅ `CvStage` memory of the run and its replay is identical at stage frames 300, 900 and 1500 except `message` / `message_timer` |
| Pause | Start, R advances one frame, after 10+ s the backlights go off → a key brings them back | ✅ R steps exactly one frame; after 600 idle frames power register 0 goes 0x0D → 0x01 (both backlights off), a key restores 0x0D |
| Lid | close during play → sleep → open: game is paused | ✅ Closed: power register 0x10 (sleep); opened: stage paused until Start |
| Screen swap setting | SCREEN SWAP ON in settings → 3D on the bottom, touch aiming | ✅ 3D on the bottom, the reticle follows the pen and shots hit |
| Licences | Credits → X shows THIRD_PARTY_NOTICES, MPL-2.0, picolibc and the other texts | ✅ All 8 texts. On v0.1.0 the FatFs text was missing and accented names showed as `??` (#10, fixed by #18) |
| Sound | no dropouts or noise, 16 channels in the boss fight | ✅ Measured, not heard: no dropouts in the snd demos or the stage 1 boss fight. A few clipped samples in the loudest fights (at most 0.2 % of a 5 s window) |

## melonDS QA record (2026-11-11)

Issue #3. Every result above comes from this run.

### Setup

- **melonDS:** 1.1, the official macOS universal app on Apple Silicon. It ran with the JIT off, the software renderer, direct boot, and generated firmware with the user name "melonDS".
- **Headless runner:** the same 1.1 core runs headless through `tools/melonds/mds.sh` (#13). It gave the screenshots, the scripted keys, touch and lid, memory traces and the sound capture.
- **App against runner:** the demo result blocks of the app were read over its GDB stub (`tools/melonds/gdbread.py`). They matched the headless core byte for byte. In the app, the main game was also driven into stage 1 with injected keys.

### Builds

- **First pass:** `corvane-v0.1.0` (identical to `develop` 8e65131), built with `./build.sh release`. It covered the game, both save variants and all 65 demos.
- **Second pass:** `develop` with the fix PRs #14–#20 and #22 merged locally. That build:
  - builds without warnings;
  - gives the same SHA-256 on two clean builds;
  - passes the same checks in headless DeSmuME.

### How it was checked

- A person didn't play or listen. The screen was read from screenshots of the core, and sound was measured from its output (48 kHz WAV).
- The full play-through was scripted. The ship count was held up with a memory poke, and stage 5 needs moving up and down to reach the boss panels.

### Failures found

Each failure has a `FIX:` issue.

| Issue | What melonDS showed on v0.1.0 | Fixed by |
|---|---|---|
| #6 | After the stage 1 briefing the main game stopped on a black screen: a data abort on shared WRAM, which the MPU doesn't map. `sys-wram`, `io-cart`, `3d-fifo`, `3d-clip` and `3d-fog` hung the same way | #14 |
| #7 | `2d-display` and `sys-dma-disp` hung: the OAM upload waited on the display FIFO DMA channel | #15 |
| #8 | No save chip at all (header game code `####`), so records, settings, CONTINUE and replay were never kept | #16 |
| #9 | `3d-limits` peaked at 1692 polygons and never overflowed | #17 |
| #10 | The licence viewer missed the FatFs text and showed accented names as `??` | #18 |
| #11 | `2d-affine` and `2d-mosaic` reported 0 (write-only registers read back) | #19 |
| #12 | The briefing START didn't sortie while the text typed | #20 |
| #21 | 3D demos reported 0 polygons | #22 |

### Notes

- **Save chip:** melonDS picks the chip from the header game code, so every build gets an 8 KB EEPROM.
  - The EEPROM and FRAM builds detect it and keep the replay at 0x0800, max 6140 bytes.
  - The FLASH build has no JEDEC ID there and caps its replay at 7675 bytes.
  - A normal stage fits. A rapid-fire scripted stage doesn't, and then no replay is kept; the slots stay intact.
- **Why DeSmuME missed these:** it has no MPU, reads write-only registers back, skips display-sync DMA, reads the polygon RAM registers as 0, and takes the save chip from `SAVETYPE`.
- **Values that differ from DeSmuME but are fine:**
  - `sys-timer` reports 1 s and 15 beats at frame 120, the same as DeSmuME.
  - `io-firmware` and `io-touch` reflect each emulator's firmware.
  - `sys-irq` and `sys-ipc` counts differ by a few.
- **Not checked:**
  - real hardware and DSi;
  - flashcart kernels and TWiLight Menu++ with the new header game code;
  - the melonDS JIT and OpenGL renderers;
  - listening by a person.

## Automated smoke-test record (DeSmuME)

Before melonDS QA, the session that created this repository checked the following on
headless DeSmuME 0.9.12 (py-desmume 0.0.9, x86_64 container). This does not replace
melonDS QA.

- 65 demos: all reached the frame-120 checkpoint (result block magic checked). DeSmuME
  doesn't report polygon RAM usage or the overflow flag, so `3d-limits` reads 0.
- Saves: EEPROM 64 KB / FLASH / FRAM each wrote, verified and reloaded.
- Cart DMA: the probe.bin CRC matched the NitroFS path and the expected value.
- Main build: progressed from boot through stages 1–5, every boss, results, the
  dual-screen 3D ending, the staff roll and record saving.
- Determinism: for stage 1, the stage state memory (CvStage, 25.5 KB) of a run recorded
  with input and of its replay from the records screen was identical at frames 300 and
  600 (the only differing bytes were heap pointers and the message string).
