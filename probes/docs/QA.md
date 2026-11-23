# Probes QA

[한국어](QA-KO.md)

The probes are graded by nobody: no hardware value exists for them. This page records what the emulators do (the
measured table) and what each ROM should show on screen.

## Rules shared by all probe ROMs

- Build: `./build.sh DEMO=l4-<name>` gives `probes-l4-<name>.nds` (`./build.sh` for all 25; `./build.sh -j8 release` for the release folders).
- Screen: the probe name and results appear on a text BG console (3D probes use a BG1 console on the main screen).
- Checkpoint: 120 frames after start the screen is complete and `CHECKPOINT` lines are printed. The same values are
  written to main RAM at `0x0237F000` (`u32 magic 'CVER'`, `u32 FNV-1a of the demo name`, `u32 frame`, `u32 values[13]`).
  Ten probes do not wait for frame 120 and finish as soon as their measurement is done: `l4-vcount-write` (frame 3),
  `l4-3d-zeq-slope` (9), `l4-tex-late` (57), `l4-3d-fog-dot` (65) and the six `l4-3d-ram-*` probes (tri 35, strip 31,
  quad 36, mix 38, fit 56, fit2 44); the frames are those of melonDS, DeSmuME is up to 7 frames off.
- The probes need no input. There is no QA mode and no play-through.

## Measuring

```sh
./build.sh -j8 release
RENDERER=soft    tools/melonds/mds.sh demos probes/build/release/demos > soft.log      # also gl, compute
RESULT_WORDS=16  tools/emu/emu.sh demos probes > desmume.log
python3 probes/tools/judge.py --observed --partial soft.log      # the `observed` JSON of expected.json known
python3 probes/tools/judge.py --profile melonds-1.1-software soft.log   # exit 0 = unchanged
```

`SAVE 2 ...` in the melonDS output (type 2, not 0: a save chip is attached, as for any game code melonDS does not list) shows
the ROM booted normally. Every probe reached its checkpoint on all seven profiles (melonDS 1.1 and master `4ce2188`, each software / GL classic / GL compute, and DeSmuME). Master: `MELONDS_COMMIT=4ce2188 RENDERER=... tools/melonds/mds.sh demos ...`.

## Expected screens

Checked in headless DeSmuME (2026-11-12, `out/demos/*_final.png`, all 14 older probes looked at; `l4-acc-rear-plane` in melonDS soft/GL/Compute and DeSmuME by result block): the picture of every probe is on screen
(cube, ship over terrain, boss, station, wall corridor, floor with cubes, card rows with lantern, two-pass city, FIFO gradients,
sprite band). Most probes print their console on the top screen over it; `l4-2d-display` shows only the gradient, and
`l4-sys-dma-disp`, `l4-acc-dual`, `l4-acc-split-*` print their console on the bottom screen. DeSmuME's screenshots were
the only ones inspected by eye: the melonDS runs were compared by their result blocks (the same numbers as Corvane's demos give).
The six `l4-3d-ram-*` probes were looked at on 2026-11-24 in headless melonDS (`out/mds/demos/*_final.png`, all six; the file is
from whichever of the six melonDS runs wrote it last) and in DeSmuME (`l4-3d-ram-tri` only).

| Probe | What to check / expected screen | Checkpoint values (values[0..3]) | Observed (melonDS soft) |
|---|---|---|---|
| l4-sys-dma-gfx | textured cube rotating | cube list words, polygon count | `9a 2` (list words 154, 2 polygons) |
| l4-sys-dma-disp | top: colour gradient (main-memory FIFO) / bottom: per-line bars | pixel value, bar colour | `f190 208c` |
| l4-2d-obj-line | 120 large bullets on one band; nobody drops the last group | OAM count | `78` (120 sprites) |
| l4-2d-display | 0-59 VRAM display, 60+ main-memory FIFO display | pixel values | `0 800a` |
| l4-3d-fifo | ship (DMA list) over terrain (port writes) | polygon/vertex count, list size | `118 29c 14a` (280 / 668) |
| l4-3d-matrix | boss whose panels, arms and claws move hierarchically | boss HP total, polygon count | `190 6b` (107 polygons) |
| l4-3d-light | station with four coloured lights circling | polygon count | `27` (39 polygons) |
| l4-3d-clip | camera skims the walls, near-plane clipping | polygon/vertex count | `21 86` (33 / 134) |
| l4-3d-poly | striped floor, front/back-culled cubes, translucent airship, ship shadow | polygon count | `51` (81 polygons) |
| l4-3d-test | BOX INSIDE/OUTSIDE toggles, POS and VEC results | inside count, w, vec x, vec z | `66 51a8 9b7 fffff36c` |
| l4-acc-stage | reversed-UV cards, edge-marked puppets on line-polygon strings, semi-transparent lantern over darkened 3D | cut-line pixels per card row, edge pixels, line pixels, pass flags | `120000 b7 2c1 1f` (soft) |
| l4-acc-dual | front view top / plan view bottom, then a large city in two passes blended by the capture | frame latency, front/plan clear-colour pixels, composite pixels, pass flags | `1 b4b4ab3b 1c501df9 1f` (soft) |
| l4-acc-split-rear | one screen in two passes at 60 fps, the split moving every 8 frames; the captured side comes back as the 3D rear plane | latency, variant, split x | `1 3 56` |
| l4-acc-split-hdma | the same with an HBlank DMA from the capture bank into engine B's bitmap BG | latency, variant, split x, frames with holes | `1 6 56 0` (master GL/Compute `1 6 56 808`: 8 + 8 frames with holes, 2056 hole pixels) |
| l4-vcount-write | console lines `WRITE <value> AT <line>: <n> LINES, RES <r>/16` (VCOUNT writes at 212, 205, 212 and 100 against a frame without a write, means of 8 frames with a dithered poll delay; the probe finishes as soon as it is done (frame 3), not at frame 120; a write at line 100 may tear the picture of that frame on a console) | lines of the four cases, then the signed residual x16 of each (0xFFFF when no whole line was added) and the frame length in values[4..8] | `11 4 3 51` (residuals x16 `-20 -36 4 -10`, all within about 2 cycles, frame length 560191; they move with the code layout, see below the measured table) |
| l4-tex-late | console lines `WRITE AT <line>: <n> NOT NEW` (a full-screen textured quad whose palette entry is rewritten at that line after the swap, interrupts off; one captured column) | lines that do not show the new colour for writes at 200, 213, 214, 216, then 192, 230, 250 in values[4..6] and 215 in values[8] | `0 0 0 192` |
| l4-3d-fog-dot | console lines `FOG START`, `BORDER`, `AA LINES` (six captures of the 3D layer: fog boundary, FOG_SHIFT 11, screen-border edge marking, a queued 1-dot, anti-aliased lines, depth Equal one step from the base; then one more capture, fog on the alpha only through a BG0 blend, `FOG ALPHA`) | fog start, its two colours, FOG_SHIFT 11 bits, its colours | `2 1f001f 7 7fe0001f` (values[4..7]: border `3ff03ff`... see the measured table) |
| l4-3d-zeq-slope | console lines `BAND <left>..<right>`, `DECAL HOLES <n>` (one capture of the 3D layer: Buoyane's depth scene, a z 3..9 slope drawn Equal over a flat base at z 6, and a decal slope over a slanted floor) | band left end, right end, decal holes | `69 98 0` hex (105, 152, 0; soft) |
| l4-acc-rear-plane | the same two-pass scheme with the rear plane (stands for Elevane preset 2); the rear plane is set up first, then the opaque samples of the captured pass are counted (4096 = both halves) | latency, variant, split x, 0, opaque samples, left half, right half (values[4..6]) | `1 7 56 0`, `1000 800 800` (soft, DeSmuME); GL: `580 580 0` |
| l4-3d-ram-tri | a field of dim green 4x4 cells on dark blue, one separate triangle per cell (the second list of 2852: the cells the platform kept), under the console lines `POLY END .. MAX .. EVENTS ..`, `VERT ..`, `OVERFLOW FLAG AT <n>`, `DRAW 2852: POLY .. VERT ..`, `DRAWN <n> FIRST GAP <n>`, `FLAGS`. melonDS: the field ends 2 cells into row 34 of 46; DeSmuME fills all 46 rows. Finishes when done, not at frame 120 | first four events of the polygon counter, `sent:counter` (values[4..12]: vertex counter events, event counts and flags, sweep end, maximum, draw-list counters, cells drawn) | `8000801 0 0 0` (one event, 2049:2048), `DRAWN 2048 FIRST GAP 2048` |
| l4-3d-ram-strip | the same with one triangle strip per row of 62 triangles (a cell is half of an 8x4 block) | as tri | `8000801 0 0 0`, `VERT END 2116`, `DRAWN 2048 FIRST GAP 2048` |
| l4-3d-ram-quad | the same with one separate quad per cell. melonDS: the field ends 48 cells into row 25 | as tri | `6000601 0 0 0` (1537:1536), `DRAWN 1536 FIRST GAP 1536` |
| l4-3d-ram-mix | a table `LIST DRAWN GAP FLAG@ VERT` with the rows A..F over the picture of the last list (F: strip triangles, triangles, then quads) | list A and list B: cells drawn and first gap, then the polygon at which DISP3DCNT bit 13 was first seen set and the vertex counter there (values[4..11]: lists C..F, values[12]: flags) | `8000800 18000801 79c079c 1800079d` (A 2048 / 2048, flag at 2049; B 1948 / 1948, flag at 1949) |
| l4-3d-ram-fit | a table `LIST GAP FLAG@ AFTER` with the rows a..i over the picture of the last list (i) | lists a..d: first gap, flag polygon << 12, cells after the gap << 24 (values[4..8]: lists e..i, values[12]: flags) | `801800 801800 801800 801800` (gap 2048, flag at 2049; list e `771770`: 1904, 1905) |
| l4-3d-ram-fit2 | the same table with the rows j..p over the picture of the last list (p) | lists j..m (values[4..6]: lists n..p, values[12]: flags) | `801800 801800 6e76e6 801800` (list l: 1766, 1767) |

## Measured table (2026-11-24)

All 25 probes were run again on 2026-11-24 on every profile after the six `l4-3d-ram-*` probes were added to `demo.c` (fresh logs).
Of the 45 older items 43 gave the value recorded on 2026-11-12 (`l4-vcount-write` and `l4-tex-late` 2026-11-16, `l4-3d-zeq-slope` and
`fog-alpha-glitch` 2026-11-21) on every profile; `vcw-212-residual` and `dot-queued-cutoff` moved on melonDS (see below the table).
Release probe ROMs built from this tree, warning-free. The "Reference value" is the value or range in the item's check, a recorded
observation (what the emulators gave when the probe was written, or the value the behaviour was expected to have); it is **not** a hardware value, and the judge compares each emulator with its own column. Items with a mask list the masked value.

| Item | Probe | Reference value | 1.1 soft | 1.1 GL classic | 1.1 GL compute | master soft | master GL classic | master GL compute | DeSmuME |
|---|---|---|---|---|---|---|---|---|---|
| `sys-dma-gfx-polys` | l4-sys-dma-gfx | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 0 |
| `3d-fifo-polys` | l4-3d-fifo | 280 | 280 | 280 | 280 | 280 | 280 | 280 | 0 |
| `3d-fifo-verts` | l4-3d-fifo | 668 | 668 | 668 | 668 | 668 | 668 | 668 | 0 |
| `3d-clip-polys` | l4-3d-clip | 33 | 33 | 33 | 33 | 33 | 33 | 33 | 0 |
| `3d-light-polys` | l4-3d-light | 39 | 39 | 39 | 39 | 39 | 39 | 39 | 0 |
| `3d-matrix-polys` | l4-3d-matrix | 107 | 107 | 107 | 107 | 107 | 107 | 107 | 0 |
| `3d-poly-polys` | l4-3d-poly | 81 | 81 | 81 | 81 | 81 | 81 | 81 | 0 |
| `3d-test-inside` | l4-3d-test | 102 | 102 | 102 | 102 | 102 | 102 | 102 | 102 |
| `3d-test-pos-w` | l4-3d-test | 20904 | 20904 | 20904 | 20904 | 20904 | 20904 | 20904 | 20904 |
| `3d-test-vec-x` | l4-3d-test | 2487 | 2487 | 2487 | 2487 | 2487 | 2487 | 2487 | 2487 |
| `3d-test-vec-z` | l4-3d-test | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964077 |
| `acc-stage-cut-exact` | l4-acc-stage | 1179648 (mask 0xff0000) | 1179648 | 0 | 1179648 | 1179648 | 0 | 1179648 | 1179648 |
| `acc-dual-both` | l4-acc-dual | 1 (mask 0x1) | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `acc-split-hdma-holes` | l4-acc-split-hdma | 0 | 0 | 0 | 0 | 0 | 2056 | 2056 | 0 |
| `2d-obj-line-budget` | l4-2d-obj-line | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 |
| `sys-dma-disp-pixels` | l4-sys-dma-disp | 0 | 0 | 0 | 0 | 0 | 653 | 653 | 736 |
| `2d-display-pixels` | l4-2d-display | 0 | 0 | 0 | 0 | 0 | 768 | 768 | 768 |
| `seam-uv-exact` | l4-acc-stage | 1179648 | 1179648 | 4608 | 1179648 | 1179648 | 4608 | 1179648 | 1179648 |
| `rear-plane-latency` | l4-acc-split-rear | 0 to 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `rear-plane` | l4-acc-rear-plane | non-zero | 4096 | 1408 | 1408 | 4096 | 1408 | 1408 | 4096 |
| `rear-plane-full` | l4-acc-rear-plane | 4096 | 4096 | 1408 | 1408 | 4096 | 1408 | 1408 | 4096 |
| `vcw-212-lines` | l4-vcount-write | 10 to 11 | 11 | 11 | 11 | 11 | 11 | 11 | 0 |
| `vcw-212-residual` | l4-vcount-write | -256 to 256 (x16) | -20 | -20 | -20 | -20 | -20 | -20 | 65535 |
| `vcw-205-lines` | l4-vcount-write | 4 | 4 | 4 | 4 | 4 | 4 | 4 | 0 |
| `vcw-212-210-lines` | l4-vcount-write | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 0 |
| `vcw-outside-lines` | l4-vcount-write | 0 | 51 | 51 | 51 | 51 | 51 | 51 | 0 |
| `tex-late-192` | l4-tex-late | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `tex-late-200` | l4-tex-late | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `tex-late-213` | l4-tex-late | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `tex-late-214` | l4-tex-late | 0 to 192 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `tex-late-215` | l4-tex-late | 0 to 192 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `tex-late-216` | l4-tex-late | 0 to 192 | 192 | 192 | 192 | 192 | 192 | 192 | 0 |
| `tex-late-230` | l4-tex-late | 0 to 192 | 192 | 192 | 192 | 192 | 192 | 192 | 0 |
| `tex-late-250` | l4-tex-late | 0 to 192 | 192 | 192 | 192 | 192 | 192 | 192 | 0 |
| `fog-boundary-start` | l4-3d-fog-dot | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 |
| `fog-shift-11` | l4-3d-fog-dot | 7 | 7 | 7 | 7 | 7 | 7 | 7 | 7 |
| `edge-screen-border` | l4-3d-fog-dot | 67044351 | 67044351 | 692726090 | 67044351 | 67044351 | 692726090 | 67044351 | 67044351 |
| `poly-right-edge-pixel` | l4-3d-fog-dot | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `dot-queued-cutoff` | l4-3d-fog-dot | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| `aa-line-pixels` | l4-3d-fog-dot | 12583104 | 12583104 | 0 | 12583104 | 12583104 | 0 | 12583104 | 0 |
| `zeq-step-boundary` | l4-3d-fog-dot | 0 | 67109888 | 1024 | 67109888 | 67109888 | 1024 | 67109888 | 0 |
| `fog-alpha-glitch` | l4-3d-fog-dot | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `zeq-slope-band-left` | l4-3d-zeq-slope | 101..107 | 105 | 32 | 105 | 105 | 32 | 105 | 118 |
| `zeq-slope-band-right` | l4-3d-zeq-slope | 149..155 | 152 | 127 | 152 | 152 | 127 | 152 | 141 |
| `zeq-slope-decal-holes` | l4-3d-zeq-slope | 0 | 0 | 1920 | 0 | 0 | 1920 | 0 | 2120 |

### Polygon and vertex RAM probes (2026-11-24)

The 40 items of the six `l4-3d-ram-*` probes record whole result words, so they are listed as hex words (in the order of the item's `values`) with their reading under
them. All six melonDS profiles (1.1 and master `4ce2188`, software / GL classic / GL compute) gave the same words, and that value is
the reference of each item (for an item with several words the judge compares the first word with the reference and records all of
them). Reading: events are `polygons sent:counter`; counters are `polygon counter / vertex counter`; "bit 13" is the polygon after
which DISP3DCNT bit 13 (polygon/vertex RAM overflow) was first seen set; "first gap" is the index of the first cell not drawn. What a
DS Lite gave is in [HW-QA.md](HW-QA.md).

| Item | Probe | melonDS, all six profiles (the reference) | DeSmuME |
|---|---|---|---|
| `ram-tri-poly-events` | l4-3d-ram-tri | `08000801 00000000 00000000 00000000`<br>2049:2048 | `00000001 00000000 00000000 00000000`<br>n/a: 1:0 |
| `ram-tri-vert-events` | l4-3d-ram-tri | `18000801 00000000 00000000 00000000`<br>2049:6144 | `00000001 00000000 00000000 00000000`<br>n/a: 1:0 |
| `ram-tri-flag` | l4-3d-ram-tri | `08011c11`<br>1 and 1 events, flags 0x1c, bit 13 at polygon 2049 | `00000411`<br>1 and 1 events, flags 0x04, bit 13 not seen |
| `ram-tri-sweep-counters` | l4-3d-ram-tri | `18000800 18000800`<br>end 2048 / 6144, maximum 2048 / 6144 | `00000000 00000000`<br>n/a: end 0 / 0, maximum 0 / 0 |
| `ram-tri-draw` | l4-3d-ram-tri | `08000800 18000800`<br>2048 drawn, first gap 2048, counters 2048 / 6144 | `0b240b24 00000000`<br>2852 drawn, no gap, counters 0 / 0 |
| `ram-strip-poly-events` | l4-3d-ram-strip | `08000801 00000000 00000000 00000000`<br>2049:2048 | `00000001 00000000 00000000 00000000`<br>n/a: 1:0 |
| `ram-strip-vert-events` | l4-3d-ram-strip | `08440801 00000000 00000000 00000000`<br>2049:2116 | `00000001 00000000 00000000 00000000`<br>n/a: 1:0 |
| `ram-strip-flag` | l4-3d-ram-strip | `08011c11`<br>1 and 1 events, flags 0x1c, bit 13 at polygon 2049 | `00000411`<br>1 and 1 events, flags 0x04, bit 13 not seen |
| `ram-strip-sweep-counters` | l4-3d-ram-strip | `08440800 08440800`<br>end 2048 / 2116, maximum 2048 / 2116 | `00000000 00000000`<br>n/a: end 0 / 0, maximum 0 / 0 |
| `ram-strip-draw` | l4-3d-ram-strip | `08000800 08440800`<br>2048 drawn, first gap 2048, counters 2048 / 2116 | `0b240b24 00000000`<br>2852 drawn, no gap, counters 0 / 0 |
| `ram-quad-poly-events` | l4-3d-ram-quad | `06000601 00000000 00000000 00000000`<br>1537:1536 | `00000001 00000000 00000000 00000000`<br>n/a: 1:0 |
| `ram-quad-vert-events` | l4-3d-ram-quad | `18000601 00000000 00000000 00000000`<br>1537:6144 | `00000001 00000000 00000000 00000000`<br>n/a: 1:0 |
| `ram-quad-flag` | l4-3d-ram-quad | `06011c11`<br>1 and 1 events, flags 0x1c, bit 13 at polygon 1537 | `00000411`<br>1 and 1 events, flags 0x04, bit 13 not seen |
| `ram-quad-sweep-counters` | l4-3d-ram-quad | `18000600 18000600`<br>end 1536 / 6144, maximum 1536 / 6144 | `00000000 00000000`<br>n/a: end 0 / 0, maximum 0 / 0 |
| `ram-quad-draw` | l4-3d-ram-quad | `06000600 18000600`<br>1536 drawn, first gap 1536, counters 1536 / 6144 | `0b240b24 00000000`<br>2852 drawn, no gap, counters 0 / 0 |
| `ram-mix-a` | l4-3d-ram-mix | `08000800 18000801`<br>2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 6144) | `0b240b24 00000000`<br>2852 drawn, no gap, bit 13 not seen |
| `ram-mix-b` | l4-3d-ram-mix | `079c079c 1800079d`<br>1948 drawn, first gap 1948, bit 13 at polygon 1949 (vertex counter 6144) | `0b240b24 00000000`<br>2852 drawn, no gap, bit 13 not seen |
| `ram-mix-c` | l4-3d-ram-mix | `08000800 17860801`<br>2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 6022) | `0b240b24 00000000`<br>2852 drawn, no gap, bit 13 not seen |
| `ram-mix-d` | l4-3d-ram-mix | `08000800 13c80801`<br>2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 5064) | `0b240b24 00000000`<br>2852 drawn, no gap, bit 13 not seen |
| `ram-mix-e` | l4-3d-ram-mix | `06880688 17fe0689`<br>1672 drawn, first gap 1672, bit 13 at polygon 1673 (vertex counter 6142) | `0b240b24 00000000`<br>2852 drawn, no gap, bit 13 not seen |
| `ram-mix-f` | l4-3d-ram-mix | `08000800 18000801`<br>2048 drawn, first gap 2048, bit 13 at polygon 2049 (vertex counter 6144) | `0b240b24 00000000`<br>2852 drawn, no gap, bit 13 not seen |
| `ram-mix-flags` | l4-3d-ram-mix | `0000007e`<br>0x7e | `0000007e`<br>0x7e |
| `ram-fit-a` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-b` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-c` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-d` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-e` | l4-3d-ram-fit | `00771770`<br>first gap 1904, bit 13 at polygon 1905, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-f` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-g` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-h` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-i` | l4-3d-ram-fit | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit-flags` | l4-3d-ram-fit | `00001ff0`<br>0x1ff0 | `00001ff0`<br>0x1ff0 |
| `ram-fit2-j` | l4-3d-ram-fit2 | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-k` | l4-3d-ram-fit2 | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-l` | l4-3d-ram-fit2 | `006e76e6`<br>first gap 1766, bit 13 at polygon 1767, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-m` | l4-3d-ram-fit2 | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-n` | l4-3d-ram-fit2 | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-o` | l4-3d-ram-fit2 | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-p` | l4-3d-ram-fit2 | `00801800`<br>first gap 2048, bit 13 at polygon 2049, 0 after it | `00000b24`<br>no gap, bit 13 not seen, 0 after it |
| `ram-fit2-flags` | l4-3d-ram-fit2 | `000007f0`<br>0x7f0 | `000007f0`<br>0x7f0 |

What differs between the emulators:

- **Values that depend on where the code sits in the ROM** (2026-11-24): adding the six RAM probes to `demo.c` changed two older
  values on melonDS although the code of their probes did not change. `vcw-212-residual` went from -6 to -20 (x16 cycles) on all six
  melonDS profiles, and with it the unjudged residuals of the other three cases in values[5..7] (14, 38, 8 to -36, 4, -10) and the
  frame length in values[8] (560189 to 560191); `dot-queued-cutoff` went from 1 to 0 on software and GL compute, 1.1 and master (GL
  classic gave 0 before and after). In the link map `d_vcount_write` moved by 0x1f8 and `d_3d_fog_dot` by 0xcf0, both with their size
  unchanged; the build before the six probes (84ef084), built and run again on melonDS 1.1 software, still gave the old values, and
  moving the new entries to the end of `demos[]` did not bring them back. Both items time something (a cycle residual, and a 1-dot
  polygon in the FIFO against a register write), so expect them to move again whenever `demo.c` grows. DeSmuME gave the same values
  as before.
- **Polygon and vertex RAM limits** (`l4-3d-ram-*`, measured 2026-11-24): every melonDS profile holds the polygon counter at 2048
  and the vertex counter at 6144 (one event per counter, at the first polygon that does not fit), sets DISP3DCNT bit 13 at that
  polygon and draws the polygons before it: 2048 separate triangles, 2048 strip triangles, 1536 quads. In the mixed lists melonDS
  drops polygons by these two limits only (2048 drawn, or fewer when the vertices run out: mix B 1948, mix E 1672, fit e 1904,
  fit2 l 1766). DeSmuME reads 0 for both counters, never sets bit 13 and draws all 2852 cells of every list. A DS Lite counts every
  polygon and vertex sent and drops polygons earlier in most of the mixed lists ([HW-QA.md](HW-QA.md)).
- **DeSmuME** reads 0 for the polygon and vertex RAM counters (16 items; the judge calls them n/a), `3d-test-vec-z` is one
  higher (`...077` against `...076`), and the main-memory display FIFO capture of `l4-sys-dma-disp` and `l4-2d-display`
  has 736 of 768 and 768 of 768 wrong samples (capture source B is not what the display shows there).
- **melonDS GL classic** (1.1 and master) has no cut line in the third card row (`acc-stage-cut-exact` 0) but one in the second
  (`seam-uv-exact` is 0x1200: rows 0/18/0 instead of 0/0/18). Soft and GL compute give 0/0/18.
- **melonDS GL classic and compute, 1.1 and master** (not software, not DeSmuME): the captured pass of `l4-acc-rear-plane` is
  partial, 1408 of 4096 samples, all of them in the left half; the right half of the pass is empty. Same for Elevane preset 2
  (retired items `rear-plane`, `rear-plane-full`).
- **melonDS master `4ce2188` GL classic and compute** (melonDS#2618): the HBlank-DMA composite has holes in every frame,
  `acc-split-hdma-holes` 2056 (8 + 8 frames of the last 16 have holes; 1.1 and master software give 0). The same master builds
  read 653 (`sys-dma-disp-pixels`) and 768 (`2d-display-pixels`) wrong samples of the main-memory display FIFO where 1.1 reads 0.
- **All seven** keep the last OBJ group on a line of 120 sprites (`2d-obj-line-budget` 3: no emulator drops sprites at the
  per-line budget, and nobody has measured where a console does).
- **Equal on slopes and the fog alpha glitch** (`l4-3d-zeq-slope` and `fog-alpha-glitch` of `l4-3d-fog-dot`, measured 2026-11-21, the scenes Buoyane's hardware runs measured; the console values are Buoyane's, not these ROMs'): melonDS software and GL compute, 1.1 and master, put the band at 105..152 with no decal holes, as in Buoyane; GL classic draws the band over 32..127 and leaves 1920 decal pixels out; DeSmuME gives 118..141 and 2120. No profile shows the fog alpha glitch (0 everywhere, like the console in Buoyane's scene). Buoyane's hardware observation: band 109..159, 960 holes, no glitch.
- **VCOUNT writes** (`l4-vcount-write`, measured 2026-11-16, new probes for Heddlemere's review): every melonDS profile applies a write wherever it is made: 202 at line 212 repeats 11 lines, 202 at 205 four, 210 at 212 three, and 50 at line 100, outside the 202..212 range GBATEK allows, 51 lines (the model without a range). DeSmuME ignores all four writes (0 lines). **Texture palette written late** (`l4-tex-late`): melonDS (all profiles) shows a write made up to line 215 and loses one made from line 216 on completely (192 old lines), with no partial case in between; DeSmuME shows the new colour for every line, so it reads the palette after the last write we make.
- Everything else (polygon counts, `BOX_TEST`/`POS_TEST`/`VEC_TEST` results, front/plan views, rear-plane latency 1) is the same
  on all melonDS profiles; the 3D-test values match DeSmuME too except `vec-z`. Frame latency is 1 everywhere since the
  boot-time pending SwapBuffers is flushed before the measuring loop (before that fix it read 2 in the Elevane copies).

## Hardware

The six `l4-3d-ram-*` probes ran on a DS Lite (2026-11-23/24, two runs each; five of them as earlier builds); the record is in [HW-QA.md](HW-QA.md).
The other 19 probes have not been measured. When a hardware result exists, the item moves to a lower level (`../../docs/LEVELS.md`)
and out of this collection; for the RAM probes that proposal has not been made yet (HW-QA.md, Follow-up).
