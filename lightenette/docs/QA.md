# Lightenette QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | "LIGHTENETTE" and the prompt on the top screen; licence/ARM7/save lines on the bottom screen | ✅ DeSmuME headless (2026-11-11); not yet in melonDS |
| Title → Start (act 1) | top: shadow and paper marionette, strings to a crossbar, MATCH % ; bottom: four sliders. Drag a slider: its limb swings, MATCH rises. At 85%+ for 20 frames: "CLEAR!". A returns to the title, B backs out. Scripted clear: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (result block id `0x31544341`: score, cleared, angles + 90) | ✅ DeSmuME and melonDS soft headless, scripted touches (2026-11-11); hands-on feel in melonDS not yet checked |
| Title → Select | test menu (up/down, A, B backs out): *Accuracy judge* (after ~2 s the values appear; B/Start returns to the title) and *Act 2: turn the dolls*, *Act 3: find the marks*, *Act 4: untangle*, *Act 5: the finale* and *Rehearsal: acts 1-5* (the acts play themselves, then the judge; the screen ends with `ACT1 97 OK F59` / `ACT2 OK F… SEAM 0/0/18` / `ACT3 OK F… <lantern pixels>`) | ✅ DeSmuME, melonDS soft (2026-11-11) for the judge; menu keys not yet tried by hand |
| Title → Select → Act 2 | top: three paper dolls on plinths with strings, the chosen one marked; bottom: per doll NOW/AIM facing and OK/TURN. Left/Right or tap chooses, L/R (or tap the lower half) turns: the card narrows, flips, widens. Dolls A front, B back, C back for 20 frames: "CLEAR!", then the picture is captured and `SEAM 0 0 18 OK` / `EDGE n OK` show on the top screen. Scripted: `30:SELECT,50:DOWN,70:A,100:RIGHT,110:R,135:RIGHT,145:L` (result block id `0x32544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| Title → Select → Act 3 | top: nearly black stage (3D with BLDCNT 01C1) with three painted marks and a glowing translucent lantern; bottom: a map with the marks (X, `*` once found) and the lantern (@). D-pad or drag moves the lantern; keep it within 16 px of a mark for 15 frames to light it. All three found: "CLEAR!", the lantern parks, then `LAMP <none> <alpha> <darken> OK/NG` and `DARK <plain> <darkened> OK` show. Scripted: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:A:4,100:LEFT:8,100:DOWN:16,140:LEFT:32,140:UP:40,200:RIGHT:76,200:DOWN:4` (result block id `0x33544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| Title → Select → Act 4 | top: crossbar, four coloured strings (dark quad + line polygon) to four pegs on a board, a horizontal and a diagonal thread below; bottom: per peg the string on it (S1-S4), the cursor, CROSSINGS n. L/R or tap chooses a peg, A or a second tap swaps the two strings. 0 crossings for 20 frames: "CLEAR!", then `HLINE n OK` / `DLINE n OK`. Scripted: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:DOWN:4,80:A:4,100:A:2,104:RIGHT:2,108:A:2,118:A:2,122:RIGHT:2,126:RIGHT:2,130:A:2,140:A:2,144:LEFT:2,148:A:2` (result block id `0x34544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| Title → Select → Act 5 | top: dark stage with three dolls on plinths with three edge colours, line-polygon strings and the lantern; bottom: per doll NOW/AIM side, LIT/DARK, OK/TODO. D-pad or drag moves the lantern; L/R/A (or a tap on the lower part) turns the doll under it. All lit and turned (A back, B front, C back) for 20 frames: "CLEAR!", the lantern parks, then `SEAM`, `EDGE`, `LINE`, `LAMP`, `DARK` show. Scripted: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:DOWN:4,80:DOWN:4,90:A:4,110:UP:8,130:LEFT:36,160:R:2,185:RIGHT:72,250:R:2` (result block id `0x35544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
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

The result block now has 64 words (`CvResult.values[61]`); read them all (`RESULT_WORDS=64` for DeSmuME).

Act 3 block extras (`values[18..26]`): cleared 1, marks 3, lantern pixel NONE/ALPHA/DARK, backdrop pixel plain/darkened, captures done 1, clear frame 181. Measured (headless, 2026-11-11):

| Renderer | lantern NONE/ALPHA/DARK | backdrop plain/darkened |
|---|---|---|
| melonDS software | 25da/25da/25da | 2463/0c21 |
| melonDS GL classic | 0d2c/25da/0d2c | 2463/0c21 |
| melonDS GL compute | 0d2c/25da/0d2c | 2463/0c21 |
| DeSmuME | 25da/25da/1972 | 2463/1042 |

Act 4 and 5 block extras (`values[27..44]`, headless, 2026-11-11):

| Renderer | act 4 horizontal / diagonal thread px | act 5 seam | act 5 edge px (3 colours) | act 5 string line px | act 5 lantern NONE/ALPHA/DARK | backdrop plain/darkened |
|---|---|---|---|---|---|---|
| melonDS software | 208 / 208 | 0/0/18 | 76/76/76 | 93 | 318f/318f/318f | 2463/0c21 |
| melonDS GL classic | 208 / 208 | 18/0/0 | 0/0/0 | 93 | 0d2c/318f/0d2c | 2463/0c21 |
| melonDS GL compute | 208 / 208 | 0/0/18 | 76/76/76 | 186 | 0d2c/318f/0d2c | 2463/0c21 |
| DeSmuME | 8 / 208 | 0/0/18 | 80/80/80 | 93 | 318f/318f/1d6e | 2463/1042 |

Act 5 puts the clean-mirror back (doll A) and the front (doll B) next to the plain-mirror back (doll C), so melonDS GL classic shows the #2008 cut line on doll A (`18/0/0`) instead of doll C. The thread and string pixel counts are `unconfirmed` (informational): DeSmuME hardly draws the horizontal line polygon (8 px) and melonDS compute draws the vertical strings twice as thick.

`tools/judge.py` on the rehearsal ROM (1100 frames; it ends at frame ~864 on melonDS and ~954 on DeSmuME):

| Renderer | verdict | NG items |
|---|---|---|
| melonDS software | OK (exit 0) | - |
| melonDS GL classic | NG (exit 1) | seam-uv, edge-marking, lantern-blend-equal, act2-seam, act2-edge-marking, act3-lantern-equal, act5-seam, act5-edge-3-colours, act5-lantern-equal |
| melonDS GL compute | NG (exit 1) | lantern-blend-equal, act3-lantern-equal, act5-lantern-equal |
| DeSmuME | NG (exit 1) | lantern-blend-equal, act3-lantern-equal, act5-lantern-equal |

`line-polygon`, `act4-hline`, `act4-dline` and `act5-line-polygon` are `unconfirmed` and only informational (line-polygon 705/710/834/338). Expected: seam 0/0/18 and the lantern equal in all modes (GBATEK). The numbers equal corvane's
`acc-stage` demo. DeSmuME ≠ melonDS ≠ hardware.
