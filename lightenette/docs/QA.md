# Lightenette QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | "LIGHTENETTE" and the prompt on the top screen; licence/ARM7/save lines on the bottom screen | ✅ DeSmuME headless (2026-11-11); not yet in melonDS |
| Title → Start (act 1) | top: shadow and paper marionette, strings to a crossbar, MATCH % ; bottom: four sliders. Drag a slider: its limb swings, MATCH rises. At 85%+ for 20 frames: "CLEAR!". A returns to the title, B backs out. Scripted clear: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (result block id `0x31544341`: score, cleared, angles + 90) | ✅ DeSmuME and melonDS soft headless, scripted touches (2026-11-11); hands-on feel in melonDS not yet checked |
| Title → Select | test menu (up/down, A, B backs out): *Accuracy judge* (after ~2 s the values appear; B/Start returns to the title) and *Act 2: turn the dolls* and *Rehearsal: acts 1-2* (both acts play themselves, then the judge; the screen ends with `ACT1 97 OK F59` / `ACT2 OK F… SEAM 0/0/18`) | ✅ DeSmuME, melonDS soft (2026-11-11) for the judge; menu keys not yet tried by hand |
| Title → Select → Act 2 | top: three paper dolls on plinths with strings, the chosen one marked; bottom: per doll NOW/AIM facing and OK/TURN. Left/Right or tap chooses, L/R (or tap the lower half) turns: the card narrows, flips, widens. Dolls A front, B back, C back for 20 frames: "CLEAR!", then the picture is captured and `SEAM 0 0 18 OK` / `EDGE n OK` show on the top screen. Scripted: `30:SELECT,50:DOWN,70:A,100:RIGHT,110:R,135:RIGHT,145:L` (result block id `0x32544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
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

Rehearsal block extras (`values[9..17]`): act 1 score 97, cleared 1, clear frame 59, finished 1, act 2 cleared 1 (`values[13]`) — identical on all four renderers (the acts' logic is emulator-independent). The act 2 picture grades (`values[14]` seam per doll, `values[15]` edge pixels):

| Renderer | act 2 seam | act 2 edge |
|---|---|---|
| melonDS software | 0/0/18 | 228 |
| melonDS GL classic | 0/18/0 | 0 |
| melonDS GL compute | 0/0/18 | 228 |
| DeSmuME | 0/0/18 | 240 |

The result block now has 32 words; read them all (`RESULT_WORDS=32` for DeSmuME).

`tools/judge.py` on the rehearsal ROM (480 frames):

| Renderer | verdict | NG items |
|---|---|---|
| melonDS software | OK (exit 0) | - |
| melonDS GL classic | NG (exit 1) | seam-uv, edge-marking, lantern-blend-equal, act2-seam, act2-edge-marking |
| melonDS GL compute | NG (exit 1) | lantern-blend-equal |
| DeSmuME | NG (exit 1) | lantern-blend-equal |

`line-polygon` is `unconfirmed` (705/710/834/338) and only informational. Expected: seam 0/0/18 and the lantern equal in all modes (GBATEK). The numbers equal corvane's
`acc-stage` demo. DeSmuME ≠ melonDS ≠ hardware.
