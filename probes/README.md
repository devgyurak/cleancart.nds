# Probes

[한국어](README-KO.md)

Research probes for emulator developers: 14 small Nintendo DS demo ROMs that report what an emulator
does for behaviour **nobody has confirmed on hardware** (level L4, see [../docs/LEVELS.md](../docs/LEVELS.md)).
This is not a game: there is no title, no credits and no QA mode, only ROMs that run for 120 frames,
write a result block and keep the picture on screen.

What it is for: each probe records a number an emulator gives for something GBATEK does not say, or says wrongly
(polygon-RAM counts, `BOX_TEST`/`VEC_TEST` results, same-screen two-pass capture, the main-memory display FIFO, the
per-line OBJ budget, the reversed-UV seam). Nothing is graded against hardware, because no hardware value exists yet.
`tools/judge.py` **records** what each emulator does and tells you when it changes.

## Probes and items

19 items, all level L4, role `target`, source `unconfirmed` (informational: they never fail a run).

| Probe ROM | Items | What it shows |
|---|---|---|
| `probes-l4-sys-dma-gfx` | `sys-dma-gfx-polys` | a cube from a DMA-fed display list: polygons counted at the checkpoint |
| `probes-l4-3d-fifo` | `3d-fifo-polys`, `3d-fifo-verts` | a ship over terrain: polygon and vertex RAM counts |
| `probes-l4-3d-clip` | `3d-clip-polys` | near-plane clipping view: polygons |
| `probes-l4-3d-light` | `3d-light-polys` | a lit station: polygons |
| `probes-l4-3d-matrix` | `3d-matrix-polys` | a hierarchical boss: polygons |
| `probes-l4-3d-poly` | `3d-poly-polys` | floor, culled cubes, translucent airship, shadow: polygons |
| `probes-l4-3d-test` | `3d-test-inside`, `3d-test-pos-w`, `3d-test-vec-x`, `3d-test-vec-z` | `BOX_TEST`, `POS_TEST`, `VEC_TEST` results |
| `probes-l4-acc-stage` | `acc-stage-cut-exact`, `seam-uv-exact` | the reversed-UV cut line in pixels |
| `probes-l4-acc-dual` | `acc-dual-both` | front and plan views through two capture banks |
| `probes-l4-acc-split-hdma` | `acc-split-hdma-holes` | HBlank-DMA composite with holes (melonDS#2618 area) |
| `probes-l4-acc-split-rear` | `rear-plane-latency` | the rear-plane variant of the same-screen two-pass view: routing latency |
| `probes-l4-2d-obj-line` | `2d-obj-line-budget` | 120 sprites on one line: is the last group dropped? |
| `probes-l4-sys-dma-disp` | `sys-dma-disp-pixels` | the main-memory display FIFO seen by capture source B |
| `probes-l4-2d-display` | `2d-display-pixels` | the same, after a VRAM display frame |

The ROM demo names are `l4-<name>` (so their result-block ids differ from Corvane's); the item ids are the
ones the items had in Corvane's `expected.json`.

### Stand-ins for Lightenette and Elevane items

Lightenette's three seam items and Elevane's six HBlank-DMA / rear-plane items come from those games' own acts and
rehearsal, which cannot be copied here. The probes cover them through the equivalent Corvane demos, as far as the demo
measures the same thing:

| Item that moves here | Covered by | Note |
|---|---|---|
| Lightenette `seam-uv-exact` | `seam-uv-exact` (`l4-acc-stage`) | same acc-stage measurement, whole packed value 0/0/18 |
| Lightenette `act2-seam-exact`, `act5-seam-exact` | **not covered** | the doll cut lines of acts 2 and 5 only exist in the game |
| Elevane `hdma-holes-zero` | `acc-split-hdma-holes` (`l4-acc-split-hdma`) | same HBlank-DMA path and hole count |
| Elevane `hdma-composite`, `hdma-composite-full`, `hdma-composite-right-full` | **not covered** | the Corvane demo counts holes, not opaque pixels on each side of x = 128 |
| Elevane `rear-plane`, `rear-plane-full` | **not covered** | `rear-plane-latency` only measures the routing latency of the rear-plane path, not captured pixel counts |

## Measured so far

`expected.json` `known` holds four profiles (melonDS 1.1 software, GL classic, GL compute, DeSmuME) with the value every
emulator gave per item; the table is in [docs/QA.md](docs/QA.md). **No real hardware has been measured.** The values
are what emulators do, not what a console does.

## Build and use

```sh
./build.sh -j8 release          # build/release/demos (14 ROMs) and build/release/judge (expected.json + judge.py)
tools/melonds/mds.sh demos probes/build/release/demos > probes.log        # headless melonDS (RENDERER=soft|gl|compute)
RESULT_WORDS=16 tools/emu/emu.sh demos probes > probes.log                # headless DeSmuME
python3 probes/tools/judge.py probes.log                                 # print the values (informational, exit 0)
python3 probes/tools/judge.py --profile melonds-1.1-software probes.log  # exit 1 when a recorded value changed
python3 -m unittest discover -s probes/tools/tests                       # judge unit tests
```

Builds use the same pinned BlocksDS image as Corvane (Docker, see `Dockerfile`); a tag `probes-vX.Y.Z` runs
`./build.sh release` in CI.

## Licence

Apache-2.0 (see `LICENSE`). Every graphic, font, 3D model and sound comes from code in this repository
([ASSETS.md](ASSETS.md)). Third-party runtime libraries are listed in `THIRD_PARTY_NOTICES`
(licence texts in `licenses/`, also inside every ROM at `/licenses/`).
