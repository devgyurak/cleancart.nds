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

melonDS version: ______  Tester: ______  Date: ______

| Demo | What to check / expected screen | Checkpoint values (values[0..3]) | melonDS result |
|---|---|---|---|
| sys-ipc | console shows IPCSYNC value A, FIFO ping 1–2 frames, input packets ≈120 | IPCSYNC, ping frames, packet count, KEYXY | |
| sys-wram | WRAMCNT 3→0→3, 0 errors in the 32 KB pattern | error count, CRC16, final WRAMCNT | |
| sys-irq | top half blue (VBlank) / bottom half red (VCount 96), counters shown | VBlank 120, HBlank ≈31,000, VCount 120, timer ≈20 | |
| sys-timer | four prescalers, cascade seconds ≈2, beats ≈16 | seconds, beats, 1:1 measurement µs, 1:64 measurement µs | |
| sys-dma-now | 0 errors copying 4 KB | error count | |
| sys-dma-vblank | background colour changes every frame, 120/120 verified | verified frames | |
| sys-dma-hblank | blue→red gradient background from top to bottom | colour at line 100, HBlank count | |
| sys-dma-cart | cart DMA CRC = NitroFS CRC = expected | found (1), CRC ×3 | |
| sys-dma-gfx | textured cube rotating | list words, polygon count | |
| sys-dma-disp | top: colour gradient (main-memory FIFO) / bottom: per-line bars | pixel value, bar colour | |
| sys-dma-fill | 0 errors filling VRAM and RAM | error count, value | |
| sys-bios | LZ77 VRAM = WRAM, CpuSet/FastSet, Div 142, Sqrt 1000, CRC16 | difference 0, FastSet value, 142, CRC | |
| sys-math | 4 divisions, sqrt(1e12)=1000000, len(3,4,0)=5×4096, atan2=4096 | error count, sqrt, len, atan2 | |
| sys-keys | buttons/X/Y/touch/lid shown live, key IRQ count | KEYS, key IRQs, KEYXY | |
| 2d-engines | top blue, bottom red, screens swap every 60 frames | POWERCNT, DISPCNT ×2 | |
| 2d-text | bottom-screen HUD (4bpp) + an 8bpp console line at the bottom | BG3CNT_SUB, BG0CNT_SUB | |
| 2d-affine | rotating sky (affine) + growing CORVANE logo (extended affine) | BG2PA, BG3PA | |
| 2d-bitmap | 0–59: 16bpp title + 8bpp logo, 60+: 1024x512 nebula scrolling | DISPCNT, BG2CNT, phase 2 | |
| 2d-mosaic | text and explosion sprites get coarser mosaic | MOSAIC | |
| 2d-obj-normal | 100 bullet sprites moving in circles, half behind the BG | OAM count 100 | |
| 2d-obj-affine | four differently coloured meteors rotating/scaling + a growing explosion | OAM count | |
| 2d-obj-bitmap | two 64x64 pilot cut-ins (one semi-transparent) | OAM count | |
| 2d-obj-map2d | the same explosion on top (1D) and bottom (2D mapping) | DISPCNT_SUB | |
| 2d-obj-line | 120 large bullets on one band — some vanish (line limit) | OAM count | |
| 2d-extpal | same illustration: left original colours / right recoloured + four meteor colours | DISPCNT, map entry | |
| 2d-window | only the two rectangles are bright, circular radar mask below | WININ, WINOUT, WINOUT_SUB | |
| 2d-blend | every 30 frames: alpha / brighten / darken / semi-transparent OBJ | BLDCNT, BLDALPHA | |
| 2d-brightness | top fades to white, bottom to black, independently | MASTER_BRIGHT ×2 | |
| 2d-vram | table of VRAMCNT A–I for the 8 layouts | table hash | |
| 2d-capture | 3D on both screens (boss core / ship) | DISPCAPCNT (no console) | |
| 2d-display | 0–59 VRAM display, 60+ main-memory FIFO display | pixel values | |
| 3d-fifo | ship (DMA list) over terrain (port writes) | polygon/vertex count, list size | |
| 3d-matrix | boss whose panels, arms and claws move hierarchically | boss HP total, polygon count | |
| 3d-light | station with four coloured lights circling | polygon count | |
| 3d-clip | camera skims the walls, near-plane clipping | polygon/vertex count | |
| 3d-tex-a3i5 / a5i3 / pal4 / pal16 / pal256 / 4x4 / direct | a quad in that texture format rotating, with repeat, flip and texture-matrix scrolling | texture id, size | |
| 3d-poly | striped floor, front/back-culled cubes, translucent airship, ship shadow | polygon count | |
| 3d-toon | 0–59 toon / 60+ highlight shading | DISP3DCNT | |
| 3d-edge | outlines per polygon ID | DISP3DCNT | |
| 3d-fog | 0–59 distance fog / 60+ alpha-only fog | DISP3DCNT | |
| 3d-aa | cube with AA + alpha-tested wire mesh | DISP3DCNT | |
| 3d-depth | 0–59 W-buffer and solid rear plane / 60+ Z-buffer and bitmap rear plane | DISP3DCNT | |
| 3d-test | BOX INSIDE/OUTSIDE toggles, POS and VEC results | inside count, w, vec x, vec z | |
| 3d-compose | 3D cube in front of → behind the 2D sea, bullet sprites over 3D, BG0 shaking | BG0CNT, BG2CNT | |
| 3d-limits | triangles exceed 2048 and some vanish, overflow frames > 0 | overflow frames, polygon RAM | |
| snd-mix | boss BGM + 8 different effects panned left/right | song number, 8 | |
| snd-pcm | PCM8 one-shot → PCM16 one-shot → PCM8 loop → PCM16 loop | sample numbers | |
| snd-adpcm | ADPCM jingle (one-shot) → hum (loop) | sample numbers | |
| snd-psg | beeps on channels 8–13 with changing duty, noise | 8, 15 | |
| snd-capture | stage 3 song + echo | 1 | |
| snd-stream | seamless ambient pad | size, sample rate | |
| io-cart | NitroFS file sizes, cart DMA OK, stage 3 info | total, load ok, cart DMA, spawn count | |
| io-save-eeprom / flash / fram | WRITE+VERIFY OK, RELOAD OK | write, reload, seq +1, chip answers | |
| io-firmware | name, favourite colour → ship colour, language, birthday, touch calibration | colour, language, name length, ship colour | |
| io-rtc | date and time update every second, stage 2 day/night | year, month, day, hour | |
| io-touch | crosshair at the pen position, pixel/raw values | touching, x, y, raw x | |
| io-power | A: top backlight, B: speaker, closing the lid sleeps | battery, lid, backlight, speaker | |

## Main build checklist

| Item | How to check | melonDS result |
|---|---|---|
| Boot logo → intro → title | just run it | |
| Demo play | wait 20 s on the title; the five stages play in turn; a key returns to the title | |
| New game → stages 1–5 → ending → staff roll → records | play from start to finish | |
| Continue | lose all ships, A restarts the same stage | |
| Resume | clear a stage, power off, power on, CONTINUE | |
| Records | date and time are saved from the RTC | |
| Replay | X on the records screen replays the same run exactly | |
| Pause | Start, R advances one frame, after 10+ s the backlights go off → a key brings them back | |
| Lid | close during play → sleep → open: game is paused | |
| Screen swap setting | SCREEN SWAP ON in settings → 3D on the bottom, touch aiming | |
| Licences | Credits → X shows THIRD_PARTY_NOTICES, MPL-2.0, picolibc and the other texts | |
| Sound | no dropouts or noise, 16 channels in the boss fight | |

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
