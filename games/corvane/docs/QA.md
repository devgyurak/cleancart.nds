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

- QA log result block (`io-qalog`, and the auto QA mode that follows): same address and
  layout, `demo_id` = `QARN` (0x4E524151). `values[0]` verdict (1 PASS, 2 FAIL), `[1]` log lines,
  `[2]` SD flags, `[3]` last checkpoint hash, `[4]` checkpoints, `[5]` lag frames,
  `[6]` save flags (bit0 chip write verified, bit1 SD mirror read-back), `[7..12]` per game.
  The log itself: `<drive>cleancart/<game>/qa-<date>-<time>.log` on the SD card;
  check it with `tools/qa/parse_log.py`.
- Demos that need input (sys-keys, io-touch, io-power, io-rtc, io-firmware) keep
  updating the screen after the checkpoint.

## Auto QA run

Hidden mode (same ROM as the release): on the title screen hold **L+R+X+Y for 120 frames**. The
game plays all five stages, the ending and the credits by itself (boss HP x0.1, hits cost no
life, weak numbers only; effects, sound and saves are unchanged) and finishes on the records
screen. The save the cart had (settings slots and the last-run replay area) is restored at the end. On a flash cart with an SD card it writes
`cleancart/corvane/qa-<date>-<time>.log` and `save-<n>.bin`; every run also fills the `QARN`
result block. Headless: `SDDIR=out/mds/x tools/melonds/mds.sh run games/corvane/corvane.nds 20000 out/mds/x
"60:A,200:A,500:L:140,500:R:140,500:X:140,500:Y:140"`, then `tools/qa/parse_log.py` on the log.

| Where | Result (2026-11-11, build from this tree) |
|---|---|
| melonDS 1.1 headless, software renderer, SD image | ✅ `RESULT PASS reason=credits` at frame 13,725, 5 stage checkpoints, 6 save mirrors read back identical (`chip_type=-1`: homebrew run has no save chip) |
| Same ROM, second run | ✅ all checkpoint hashes identical (`tools/qa/compare.py`) |
| DeSmuME headless (no SD) | ✅ `QARN` block verdict 1, last state hash `37db96a8` = melonDS (lag differs, not compared) |
| Real DS Lite + R4 | not run yet |

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
| io-qalog | writes a QA log (RAM ring, SD card when the cart has one), mirrors the save image, writes the `QARN` result block; headless runs need an SD image (`SDDIR=… tools/melonds/mds.sh`) | `values[]` as in the QARN layout below: verdict 1, lines, SD flags (bit0 log on SD, bit1 read-back OK), … | ✅ melonDS (SD image) `1 2f 3 …` log on the SD, read-back OK, `tools/qa/parse_log.py` passes; without SD `1 2f 0` (RAM only) |
| io-save-eeprom / flash / fram | WRITE+VERIFY OK, RELOAD OK | write, reload, seq +1, chip answers | ✅ eeprom `1 1 1 1`; flash `1 1 1 2`; fram `1 1 1 3`. melonDS attaches an 8 KB EEPROM to every build. No save chip on v0.1.0 (#8), fixed by #16 |
| io-firmware | name, favourite colour → ship colour, language, birthday, touch calibration | colour, language, name length, ship colour | ✅ `0 1 7 4e0c` name MELONDS, colour 0, English, 01/01, calibration shown |
| io-rtc | date and time update every second, stage 2 day/night | year, month, day, hour | ✅ `1a b a f` date and time tick; stage 2 sky DAY |
| io-touch | crosshair at the pen position, pixel/raw values | touching, x, y, raw x | ✅ `0 78 64 0` pen 120,100 while touched |
| io-power | A: top backlight, B: speaker, closing the lid sleeps | battery, lid, backlight, speaker | ✅ `f 0 0 1` battery OK, lid open, A/B toggle |
| acc-dual | front view top / plan view bottom (both 3D through the capture), then a large city drawn in a left and a right pass blended by the capture on the top screen (see [Capture-path cross-check](#capture-path-cross-check-acc-dual)) | frame latency, front/plan clear-colour pixels in banks D/C, composite left/right pixels, pass flags | ✅ software renderer `1 b4b4ab3b 1c501df9 1f` all pass |
| acc-split-tex / -bg / -relay / -rear / -rear-read / -rear-write / -hdma | one screen in two passes at 60 fps, the split moving every 8 frames; the captured side comes back as a texture, a bitmap BG, VRAM display + graphics capture, the 3D rear plane, or an HBlank DMA from the capture bank into engine B's bitmap BG (see [Same-screen two-pass](#same-screen-two-pass-acc-split)) | latency, variant, split x; hdma: frames with holes (blend << 8 \| capture only) | ✅ software renderer: full picture every frame in all seven |
| acc-stage | reversed-UV cards, edge-marked puppets on line-polygon strings, semi-transparent lantern over darkened 3D; grades itself (see [Accuracy cross-check](#accuracy-cross-check-acc-stage)) | cut-line pixels per card row, edge pixels, line pixels, pass flags | ✅ software renderer `120000 b7 2c1 1f` all pass; GL renderers differ (see below) |

## Judging (`expected.json`, `tools/judge.py`)

The result block of each demo is graded by `tools/judge.py` against `expected.json` (116 items, 59 of the 75
demos). Every item has a `level` from 1 to 4 and a `level_basis` ([docs/LEVELS.md](../../../docs/LEVELS.md)); levels never
change a verdict.

| Kind | What is judged | Examples |
|---|---|---|
| (a) self-check, documented constant | error counts 0, `sqrt(10^12)` = 1000000, CRC equal, 2048 polygons | sys-wram, sys-dma-*, sys-bios, sys-math, 3d-limits |
| (b) register read-back | the register reads back the value the demo wrote (write-only registers are not judged) | 2d-engines, 2d-window, 2d-blend, 3d-toon, 3d-compose |
| (c) timing figure | a count in a window derived from GBATEK (263 lines per frame, 33.513982 MHz bus) | sys-irq, sys-timer, sys-dma-vblank |
| (d) screen or sound only | not judged yet | sys-dma-hblank, sys-dma-disp, 2d-affine, 2d-mosaic, 2d-obj-normal, 2d-obj-affine, 2d-obj-bitmap, 2d-obj-line, 2d-vram, 2d-display, snd-mix, snd-pcm, snd-adpcm, snd-psg, snd-capture, snd-stream |

Sources: `GBATEK` and `rom` items gate, `unconfirmed` ones (no hardware value yet, for example polygon counts of
the models) are informational. Polygon counters read 0 in DeSmuME: those items are n/a there (`na_when`).

```sh
tools/melonds/mds.sh demos games/corvane/build/release/demos > demos.log     # RENDERER=gl|compute for the GL renderers
python3 games/corvane/tools/judge.py --profile melonds-1.1-software demos.log
RESULT_WORDS=16 tools/emu/emu.sh demos games/corvane > ds.log && python3 games/corvane/tools/judge.py --profile desmume-pydesmume-0.0.9 ds.log
```

Known profiles (measured 2026-11-12, nothing on real hardware yet): melonDS 1.1 software passes everything; the GL (Classic)
renderer fails acc-stage cut line, edge marking and lantern blend, GL (Compute) the lantern blend (the cases of
melonDS issues #2008, #2503, #2549, as in Lightenette); DeSmuME fails the lantern blend, has no polygon counters and
differs in two informational items (key IRQ count, `VEC_TEST` z). The 16 demos of kind (d) get ROM-side judging in a
later change (display capture, as in Lightenette and Elevane); sound demos need a capture check first.

## Accuracy cross-check (acc-stage)

`acc-stage` touches three features where melonDS's renderers were reported to
differ from hardware: [#2008](https://github.com/melonDS-emu/melonDS/issues/2008)
(reversed texture coordinates),
[#2503](https://github.com/melonDS-emu/melonDS/issues/2503) (semi-transparent OBJ
over 3D) and [#2549](https://github.com/melonDS-emu/melonDS/issues/2549) (edge
marking, line polygons). The scene is static. BLDCNT is `0100` (no effect) for
frames 0–39, `0140` (alpha) for 40–79 and `01C1` (darken BG0) from 80. The demo
captures the main screen (display capture to bank D) in each mode, counts pixels
and prints the grades at the bottom left. The same numbers can be read on hardware.

| Part | What is drawn | Expected (GBATEK / hardware report) |
|---|---|---|
| #2008 | three 16x16 paper cards at 1:1, texels 16–31 with magenta cut lines at 15 and 32: `FWD 16>32`, `REV 31>15` (the compensated mirror from the issue), `REV 32>16` | cut line only in `REV 32>16`, 18 pixels at its left edge: `SEAM 0 0 18` |
| #2549 | two puppets with polygon IDs 8 and 16 (edge colours yellow, cyan); strings and a floor grid as line polygons (triangles with two equal vertices) | outlines and lines present (counted outside the lantern) |
| #2503 | a 64x64 semi-transparent OBJ lantern, EVA 8 / EVB 16, over 3D | it alpha-blends the same in all three modes; in darken mode it is the only undarkened spot |

Result block: `values[0]` cut-line pixels (FWD, REV31 << 8, REV32 << 16),
`[1]` edge pixels, `[2]` line pixels, `[3]` pass flags (bit 0 cut lines,
1 edges, 2 lines, 3 lantern blend, 4 darken; hardware should give `1f`),
`[4..6]` lantern pixel at (232,132) per mode, `[7..8]` pixel at (140,140) in
none / darken mode. After the checkpoint A cycles the three modes.

Results on 2026-11-11, 1x. melonDS 1.1 runs headless (`tools/melonds/mds.sh`,
GL renderers on Mesa llvmpipe through `RENDERER=gl|compute`); DeSmuME is the
`tools/emu/` build. Hardware has not been run yet.

| Renderer | Cut lines | Edge px | Line px | Lantern none / alpha / darken | Flags | Differs from expected |
|---|---|---|---|---|---|---|
| melonDS software | 0 0 18 | 183 | 705 | `318f` `318f` `318f` | `1f` | — |
| melonDS OpenGL (Classic) | 0 **18 0** | **0** | 710 | **`0d2c`** `318f` **`0d2c`** | `14` | #2008: mirror off by one; #2549: no edge marking; #2503: lantern not blended without the alpha effect bits, including darken mode |
| melonDS OpenGL (Compute shader) | 0 0 18 | 183 | 834 | **`0d2c`** `318f` **`0d2c`** | `17` | #2503 as Classic. Edge marking is right at 1x; at `SCALE=2` the outline stays one output pixel thick (#2549) |
| DeSmuME | 0 0 18 | 188 | 338 | `318f` `318f` **`1d6e`** | `17` | darkens the blended lantern in darken mode; draws no horizontal line polygons |

Open questions that need hardware: whether horizontal line polygons are drawn
(melonDS draws them, DeSmuME doesn't) and the exact line pixel count (705 / 710
/ 834 / 338). The display capture saw the same values as the GL output here, so
in-ROM grading caught all three GL differences.

## Capture-path cross-check (acc-dual)

`acc-dual` runs the two display-capture techniques a game needs for 3D on both
screens and for scenes over the polygon limit, as reported broken in melonDS's
GL renderers: [#2618](https://github.com/melonDS-emu/melonDS/issues/2618)
(two-pass rendering blinks on master) and
[#2619](https://github.com/melonDS-emu/melonDS/issues/2619) (captured frames
do not upscale).

1. Calls 0–7 measure the frame latency: a full-screen quad whose colour names
   its call, captured once.
2. Calls 8–55 show a block city as a front view (top, red clear) and a plan view
   (bottom, blue clear) on alternate frames, as the ending does.
3. Calls 56–59 draw a larger city in one pass to show it overflows.
4. From call 60 the larger city is drawn in a left and a right pass (half
   frustum, half viewport, clear alpha 0). The second capture blends 3D and
   the first pass from bank B at EVA = EVB = 16. Engine B shows the composite
   on the top screen. After the checkpoint A switches between the dual view
   and the two-pass view.

The clear colour is latched with the polygon list, so it is set in the call
that sends the geometry, not with the capture.

Result block: `values[0]` latency, `[1]` front-clear pixels in D << 16 |
plan-clear pixels in C, `[2]` composite left << 16 | right pixels, `[3]` flags
(bit 0 views in the right banks, 1 one pass overflows, 2 both passes fit,
3 composite has both halves, 4 latency 0 or 1; hardware should give `1f`),
`[4..6]` polygons | overflow << 16 for one pass and passes 1 and 2, `[7]`
composite hash, `[8..9]` hashes of D and C, `[10]` cross counts (should be 0).

Results on 2026-11-11, 1x, headless. melonDS master is commit `4ce2188` (the
build #2618 names), run with `MELONDS_COMMIT=…`.

| Build / renderer | Latency | Views | One pass | Passes | Composite halves | Flags | Notes |
|---|---|---|---|---|---|---|---|
| 1.1 software | 1 | ✅ | 1536, overflow | 1232 + 1218 | 7248 / 7673 | `1f` | one bright column at the seam (x = 128): both passes draw it and the blend adds them |
| 1.1 OpenGL (Classic) | 1 | ✅ | same | same | 7214 / 7692 | `1f` | no seam column |
| 1.1 OpenGL (Compute) | 1 | ✅ | same | same | 7270 / 7684 | `1f` | seam column like software |
| master software / Classic / Compute | same as 1.1 | ✅ | same | same | same as 1.1 | `1f` | the two-pass view did not blink in four straight frames: #2618 is **not** reproduced by this scheme |
| DeSmuME | 1 | ✅ | 0, no overflow | 0 + 0 | 7164 / 7812 | `19` | polygon/vertex RAM counters read 0; no seam column |

At `SCALE=2` the top screen of the dual view alternates between the upscaled 3D
frame and the 1x captured frame on both 1.1 and master: #2619 is reproduced.
The hashes differ between renderers (rasterisation differs slightly), so a
check across renderers compares pixels with a tolerance, not hashes. Still
open: the seam column on hardware. The capture scheme of the #2618 homebrew is
found and reproduced by `acc-split-hdma` (see [Same-screen two-pass](#same-screen-two-pass-acc-split)).

## Same-screen two-pass (acc-split)

The seven `acc-split-*` builds chase [#2618](https://github.com/melonDS-emu/melonDS/issues/2618):
on master its homebrew shows the part left of a moving split black on about every
other frame (measured from the video attached to the issue; 1.1 is fine). Each
frame renders one side of the split live (half frustum, half viewport) and takes
the other side from the previous frame's capture. Walls and ground cover every
pixel and the backdrop is grey, so a grey (or black) pixel on the top screen means
the captured side is missing. Frames 100–107 (hdma: 150–153), top screen, headless:

| Way the captured side comes back | 1.1 soft | 1.1 GL / Compute | master `4ce2188` soft | master GL / Compute |
|---|---|---|---|---|
| `tex`: direct-colour texture behind the live pass | ✅ | ✅ | ✅ | ✅ |
| `bg`: 16bpp bitmap BG3 behind the 3D layer | ✅ | ✅ | ✅ | ✅ |
| `relay`: VRAM display mode on even frames, graphics capture on odd frames | ✅ | ✅ | ✅ | ✅ |
| `rear`: 3D rear plane (clear bitmap, texture slot 2) | ✅ | ❌ missing side every frame | ✅ | ❌ missing side every frame |
| `rear-read`: same, CPU reads one word of the capture first | ✅ | ❌ | ✅ | ✅ |
| `rear-write`: same, CPU reads and writes back one word first | ✅ | ❌ | ✅ | ✅ |
| `hdma`: HBlank DMA from the capture bank into engine B's bitmap BG | ✅ | ✅ | ✅ | ❌ left part missing every frame (GL black, Compute transparent); ROM result `0808` |

### Rear plane

The OpenGL renderers build the rear plane from the emulated VRAM copy
(`VRAMFlat_Texture`), not from the capture. On master a capture lives in a GPU
texture until the CPU touches that bank, so any CPU access fixes it there; on
1.1 even that does not. The missing side alternates left and right because the
passes do. This failure is a separate finding, not found in melonDS's issues.

### HBlank DMA from the capture bank: the #2618 mechanism

`hdma` is the scheme of the homebrew in #2618, read from its register writes in a
patched headless master core: engine A (3D) sits on the bottom screen and is only
captured; the top screen is engine B's 16bpp bitmap BG2, filled by a repeating
HBlank DMA (one 512-byte line per HBlank) from capture bank D, with BG2Y = −1 so a
line copied at HBlank N shows on scanline N + 1. Frames alternate:

| Frame | Capture into bank D | DMA reads | On hardware the top screen shows |
|---|---|---|---|
| even (P) | 3D only (right pass) | one line ahead, before the capture overwrites it | the last full picture |
| odd (Q) | 3D (left pass) + VRAM D, EVA = EVB = 16 | the line just captured | the new full picture |

melonDS master keeps captures on the GPU and runs them at VBlank (or when a
register changes). The first CPU or DMA read of a bank being captured
(VCount 0) copies the capture texture to emulated VRAM once and marks it synced,
and logs `READING VRAM AS IT IS BEING CAPTURED TO`. The DMA then copies the
previous frame's capture, which holds one pass only, so the left part is
missing. In the ROM's result the missing part sits in whole 64-line capture
blocks (lines 0–127 on one frame, 128–191 on the next). A debug patch that runs the capture up to the
current line on every scanline and leaves an unfinished capture unsynced
removes the hole, both in this ROM and in the homebrew. 1.1 and the software
renderer write captures to VRAM line by line, so they are fine.

The ROM grades itself: after each of the last 16 frames it reads the sub BG
(bank C) and counts transparent or black pixels on every fourth line. Result
`values[3]` = frames with holes, blend frames << 8 | capture-only frames;
`0000` passes, master GL and Compute give `0808`.

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
