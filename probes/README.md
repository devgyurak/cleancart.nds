# Probes

[한국어](README-KO.md)

Research probes for emulator developers: 25 small Nintendo DS demo ROMs that report what an emulator
does for behaviour **nobody has confirmed on hardware** (level L4, see [../docs/LEVELS.md](../docs/LEVELS.md)).
This is not a game: there is no title, no credits and no QA mode, only ROMs that run for 120 frames (ten of them finish earlier),
write a result block and keep the picture on screen.

What it is for: each probe records a number an emulator gives for something GBATEK does not say, or says wrongly
(polygon-RAM counts and what happens when a list does not fit, `BOX_TEST`/`VEC_TEST` results, same-screen two-pass capture, the main-memory display FIFO, the
per-line OBJ budget, the reversed-UV seam). Nothing is graded against hardware: no hardware value exists for most probes, and what a DS Lite gave for the six
polygon and vertex RAM probes is recorded in [docs/HW-QA.md](docs/HW-QA.md), not used as a pass mark.
`tools/judge.py` **records** what each emulator does and tells you when it changes.

## Probes and items

85 items, all level L4, role `target`, source `unconfirmed` (informational: they never fail a run).

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
| `probes-l4-acc-dual` | `acc-dual-both` | front and plan views through two capture banks (an L2 prerequisite group in Elevane) |
| `probes-l4-acc-split-hdma` | `acc-split-hdma-holes` | HBlank-DMA composite with holes (melonDS#2618; Elevane judges it as an L3 target, here it is recorded for master too) |
| `probes-l4-acc-split-rear` | `rear-plane-latency` | the rear-plane variant of the same-screen two-pass view: routing latency, measured before the rear plane is set up (an L2 prerequisite in Elevane) |
| `probes-l4-acc-rear-plane` | `rear-plane`, `rear-plane-full` | the capture of a pass routed back as the 3D rear plane, set up first: opaque pixels of the captured pass (partial / all 4096) |
| `probes-l4-vcount-write` | `vcw-212-lines`, `vcw-212-residual`, `vcw-205-lines`, `vcw-212-210-lines`, `vcw-outside-lines` | VCOUNT writes: lines a write adds, the residual cycles, and whether a write outside the 202..212 range GBATEK allows is ignored (Heddlemere judges only the 10 to 11 line window as an L2 prerequisite). On melonDS the residual depends on where the code sits in the ROM ([docs/QA.md](docs/QA.md)) |
| `probes-l4-tex-late` | `tex-late-192` to `tex-late-250` | a texture palette rewritten at a given line after the buffer swap: from which line a late write is lost to the 3D render (rendering starts at line 214) |
| `probes-l4-3d-fog-dot` | `fog-boundary-start`, `fog-shift-11`, `edge-screen-border`, `poly-right-edge-pixel`, `dot-queued-cutoff`, `aa-line-pixels`, `zeq-step-boundary`, `fog-alpha-glitch` | the fog, edge and 1-dot corners GBATEK words loosely (where the first fog boundary lies, FOG_SHIFT 11, edge marking at the screen border, a queued 1-dot, anti-aliased line pixels, the depth Equal boundary step); moved out of Buoyane (games/buoyane), which judges only what GBATEK pins down; `fog-alpha-glitch` is fog on the alpha only behind a BG0 blend (Buoyane's hardware runs saw no glitch). On melonDS `dot-queued-cutoff` depends on where the code sits in the ROM ([docs/QA.md](docs/QA.md)) |
| `probes-l4-3d-zeq-slope` | `zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-slope-decal-holes` | depth Equal on slopes, the scene Buoyane's hardware runs measured (same coordinates and raw z): where the band of a slope drawn Equal over a flat base starts and ends, and how many pixels of a decal slope over a slanted floor fail the test (the hardware run saw the band 4 and 7 pixels to the right of the emulators and 960 holes; those values were measured in Buoyane, not by these ROMs) |
| `probes-l4-3d-ram-tri` | `ram-tri-poly-events`, `ram-tri-vert-events`, `ram-tri-flag`, `ram-tri-sweep-counters`, `ram-tri-draw` | polygon and vertex RAM limits with separate triangles (GBATEK gives RAM_COUNT the ranges 0..2048 and 0..6144 and says nothing about a list that does not fit): 65535 polygons in one list with both counters read after each (where a counter stops or wraps, at which polygon DISP3DCNT bit 13 is set), then a list of 2852 polygons, one per 4x4 cell, captured to count the cells that were kept. Written for this collection; the six `l4-3d-ram-*` ROMs ran on a DS Lite, five of them as earlier builds ([docs/HW-QA.md](docs/HW-QA.md)) |
| `probes-l4-3d-ram-strip` | `ram-strip-poly-events`, `ram-strip-vert-events`, `ram-strip-flag`, `ram-strip-sweep-counters`, `ram-strip-draw` | the same with one triangle strip per row (the polygon limit comes first: 64 vertices per 62 triangles) |
| `probes-l4-3d-ram-quad` | `ram-quad-poly-events`, `ram-quad-vert-events`, `ram-quad-flag`, `ram-quad-sweep-counters`, `ram-quad-draw` | the same with separate quads (the vertex limit comes first: 1536 x 4) |
| `probes-l4-3d-ram-mix` | `ram-mix-a` to `ram-mix-f`, `ram-mix-flags` | six lists of 2852 polygons that mix triangles, strip triangles and quads so that one limit is reached at a time: cells drawn, first cell not drawn, the polygon at which bit 13 was set and the vertex counter there |
| `probes-l4-3d-ram-fit` | `ram-fit-a` to `ram-fit-i`, `ram-fit-flags` | nine more mixed lists, written to test one reading of the DS Lite numbers of the ROMs above: first cell not drawn, the polygon at which bit 13 was set, cells drawn after the first gap |
| `probes-l4-3d-ram-fit2` | `ram-fit2-j` to `ram-fit2-p`, `ram-fit2-flags` | seven lists for the reading as it stood after `l4-3d-ram-fit` ran on the DS Lite; the same words |
| `probes-l4-2d-obj-line` | `2d-obj-line-budget` | 120 sprites on one line: is the last group dropped? |
| `probes-l4-sys-dma-disp` | `sys-dma-disp-pixels` | the main-memory display FIFO seen by capture source B |
| `probes-l4-2d-display` | `2d-display-pixels` | the same, after a VRAM display frame |

The ROM demo names are `l4-<name>` (so their result-block ids differ from the other games'); the item ids are the
ones the items had in Corvane's `expected.json`, except `rear-plane` and `rear-plane-full` (Elevane's retired ids) and the
`ram-*` ids, which are new here.

### Stand-ins for Lightenette and Elevane items

Lightenette's seam items come from that game's acts, which cannot be copied here; Elevane's HBlank-DMA hole items are L3
targets there (the probes also record them), and its rear-plane pixel items were retired from Elevane into this collection:

| Item | Covered by | Note |
|---|---|---|
| Lightenette `seam-uv-exact` | `seam-uv-exact` (`l4-acc-stage`) | same acc-stage measurement, whole packed value 0/0/18 |
| Lightenette `act2-seam-exact`, `act5-seam-exact` | **not covered** | the doll cut lines of acts 2 and 5 only exist in the game |
| Elevane `hdma-holes-zero` | `acc-split-hdma-holes` (`l4-acc-split-hdma`) | same HBlank-DMA path and hole count; still an Elevane L3 target |
| Elevane `hdma-composite`, `hdma-composite-full`, `hdma-composite-right-full` | **not covered** | the demo counts holes, not opaque pixels on each side of x = 128 |
| Elevane `rear-plane`, `rear-plane-full` | `rear-plane`, `rear-plane-full` (`l4-acc-rear-plane`) | the same scheme as the old preset 2 (capture routed into the rear plane, set up first), counted on the captured pass |

## Measured so far

`expected.json` `known` holds seven profiles (melonDS 1.1 and master `4ce2188`, each software / GL classic / GL compute, and DeSmuME) with the value every
emulator gave per item; the table is in [docs/QA.md](docs/QA.md). The values under `known` are what emulators do, not what a
console does. **Hardware:** the six `l4-3d-ram-*` probes ran on a DS Lite (2026-11-23/24, two runs each; five of them as earlier builds); the
result words, what was found and the rule fitted to them are in [docs/HW-QA.md](docs/HW-QA.md). No other probe has been measured on
hardware.

## Build and use

```sh
./build.sh -j8 release          # build/release/demos (25 ROMs), build/release/rom (the same 25 as `*-hw-support.nds`, see docs/HW-QA.md) and build/release/judge (expected.json + judge.py)
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
