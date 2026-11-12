# Probes QA

[한국어](QA-KO.md)

The probes are graded by nobody: no hardware value exists for them. This page records what the emulators do (the
measured table) and what each ROM should show on screen.

## Rules shared by all probe ROMs

- Build: `./build.sh DEMO=l4-<name>` gives `probes-l4-<name>.nds` (`./build.sh` for all 14; `./build.sh -j8 release` for the release folders).
- Screen: the probe name and results appear on a text BG console (3D probes use a BG1 console on the main screen).
- Checkpoint: 120 frames after start the screen is complete and `CHECKPOINT` lines are printed. The same values are
  written to main RAM at `0x0237F000` (`u32 magic 'CVER'`, `u32 FNV-1a of the demo name`, `u32 frame`, `u32 values[13]`).
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
the ROM booted normally. Every probe reached its checkpoint on all four emulators.

## Expected screens

Checked in headless DeSmuME (2026-11-12, `out/demos/*_final.png`, all 14 looked at): the picture of every probe is on screen
(cube, ship over terrain, boss, station, wall corridor, floor with cubes, card rows with lantern, two-pass city, FIFO gradients,
sprite band). Most probes print their console on the top screen over it; `l4-2d-display` shows only the gradient, and
`l4-sys-dma-disp`, `l4-acc-dual`, `l4-acc-split-*` print their console on the bottom screen. DeSmuME's screenshots were
the only ones inspected by eye: the melonDS runs were compared by their result blocks (the same numbers as Corvane's demos give).

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
| l4-acc-split-hdma | the same with an HBlank DMA from the capture bank into engine B's bitmap BG | latency, variant, split x, frames with holes | `1 6 56 0` |

## Measured table (2026-11-12)

Release probe ROMs built from this tree, warning-free. The "Reference value" is the value the item's check was written
from (Corvane's measurement on melonDS software); it is **not** a hardware value. Items with a mask list the masked value.

| Item | Probe | Reference value | melonDS soft | melonDS GL classic | melonDS GL compute | DeSmuME |
|---|---|---|---|---|---|---|
| `sys-dma-gfx-polys` | l4-sys-dma-gfx | 2 | 2 | 2 | 2 | 0 |
| `3d-fifo-polys` | l4-3d-fifo | 280 | 280 | 280 | 280 | 0 |
| `3d-fifo-verts` | l4-3d-fifo | 668 | 668 | 668 | 668 | 0 |
| `3d-clip-polys` | l4-3d-clip | 33 | 33 | 33 | 33 | 0 |
| `3d-light-polys` | l4-3d-light | 39 | 39 | 39 | 39 | 0 |
| `3d-matrix-polys` | l4-3d-matrix | 107 | 107 | 107 | 107 | 0 |
| `3d-poly-polys` | l4-3d-poly | 81 | 81 | 81 | 81 | 0 |
| `3d-test-inside` | l4-3d-test | 102 | 102 | 102 | 102 | 102 |
| `3d-test-pos-w` | l4-3d-test | 20904 | 20904 | 20904 | 20904 | 20904 |
| `3d-test-vec-x` | l4-3d-test | 2487 | 2487 | 2487 | 2487 | 2487 |
| `3d-test-vec-z` | l4-3d-test | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964077 |
| `acc-stage-cut-exact` | l4-acc-stage | 1179648 (mask 0xff0000) | 1179648 | 0 | 1179648 | 1179648 |
| `acc-dual-both` | l4-acc-dual | 1 (mask 0x1) | 1 | 1 | 1 | 1 |
| `acc-split-hdma-holes` | l4-acc-split-hdma | 0 | 0 | 0 | 0 | 0 |
| `2d-obj-line-budget` | l4-2d-obj-line | 1 to 2 | 3 | 3 | 3 | 3 |
| `sys-dma-disp-pixels` | l4-sys-dma-disp | 0 | 0 | 0 | 0 | 736 |
| `2d-display-pixels` | l4-2d-display | 0 | 0 | 0 | 0 | 768 |
| `seam-uv-exact` | l4-acc-stage | 1179648 | 1179648 | 4608 | 1179648 | 1179648 |
| `rear-plane-latency` | l4-acc-split-rear | 0 to 1 | 1 | 1 | 1 | 1 |

What differs between the emulators:

- **DeSmuME** reads 0 for the polygon and vertex RAM counters (7 items; the judge calls them n/a), `3d-test-vec-z` is one
  higher (`...077` against `...076`), and the main-memory display FIFO capture of `l4-sys-dma-disp` and `l4-2d-display`
  has 736 of 768 and 768 of 768 wrong samples (capture source B is not what the display shows there).
- **melonDS GL classic** has no cut line in the third card row (`acc-stage-cut-exact` 0) but one in the second (`seam-uv-exact`
  is 0x1200: rows 0/18/0 instead of 0/0/18). Soft and GL compute give 0/0/18.
- **All four** keep the last OBJ group on a line of 120 sprites (`2d-obj-line-budget` 3: no emulator drops sprites at the
  per-line budget, and nobody has measured where a console does).
- Everything else (polygon counts, `BOX_TEST`/`POS_TEST`/`VEC_TEST` results, front/plan views, HBlank-DMA holes, rear-plane latency) is the same
  on the three melonDS renderers; the 3D-test values match DeSmuME too except `vec-z`.

## Hardware

Not measured. When a hardware result exists, the item moves to a lower level (`../../docs/LEVELS.md`) and out of this collection.
