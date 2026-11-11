# Lightenette QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | "LIGHTENETTE" and the prompt on the top screen; licence/ARM7/save lines on the bottom screen | ✅ DeSmuME headless (2026-11-11); not yet in melonDS |
| Title → Start (act 1) | top: shadow and paper marionette, strings to a crossbar, MATCH % ; bottom: four sliders. Drag a slider: its limb swings, MATCH rises. At 85%+ for 20 frames: "CLEAR!". A returns to the title, B backs out. Scripted clear: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (result block id `0x31544341`: score, cleared, angles + 90) | ✅ DeSmuME and melonDS soft headless, scripted touches (2026-11-11); hands-on feel in melonDS not yet checked |
| Title → Select | test menu (up/down, A, B backs out): *Accuracy judge* (after ~2 s the values appear; B/Start returns to the title) and *Rehearsal: act 1* (the act plays itself, then the judge; the screen ends with `ACT1 97 OK F59`) | ✅ DeSmuME, melonDS soft (2026-11-11) for the judge; menu keys not yet tried by hand |
| rehearsal demo | `lightenette-rehearsal.nds` boots straight into the rehearsal, no save chip or NitroFS; result block id `0x4C454852`. `python3 lightenette/tools/judge.py <log>` grades it against `expected.json` | see below |
| acc-stage demo | `lightenette-acc-stage.nds`, same numbers as the title → Select scene; result block `0x0237F000`: magic `0x52455643`, id `0x4A44474C` | see below |

## Judge measurements (2026-11-11, headless; GL = Mesa llvmpipe; not yet on hardware)

Result block `values[0..3]` (seam rows, edge, line, flags) and lantern values `values[4..6]`:

| Renderer | seam | edge | line | flags | lantern NONE/ALPHA/DARK |
|---|---|---|---|---|---|
| melonDS software | 0/0/18 | 183 | 705 | 1f | 318f/318f/318f |
| melonDS GL classic | 0/18/0 | 0 | 710 | 14 | 0d2c/318f/0d2c |
| melonDS GL compute | 0/0/18 | 183 | 834 | 17 | 0d2c/318f/0d2c |
| DeSmuME | 0/0/18 | 188 | 338 | 17 | 318f/318f/1d6e |

Rehearsal block extras (`values[9..12]`): act 1 score 97, cleared 1, clear frame 59, finished 1 — identical on all four renderers (the act's logic is emulator-independent).

`tools/judge.py` on the rehearsal ROM (330 frames):

| Renderer | verdict | NG items |
|---|---|---|
| melonDS software | OK (exit 0) | - |
| melonDS GL classic | NG (exit 1) | seam-uv, edge-marking, lantern-blend-equal |
| melonDS GL compute | NG (exit 1) | lantern-blend-equal |
| DeSmuME | NG (exit 1) | lantern-blend-equal |

`line-polygon` is `unconfirmed` (705/710/834/338) and only informational. Expected: seam 0/0/18 and the lantern equal in all modes (GBATEK). The numbers equal corvane's
`acc-stage` demo. DeSmuME ≠ melonDS ≠ hardware.
