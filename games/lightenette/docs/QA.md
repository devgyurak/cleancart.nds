# Lightenette QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`).

| Scene | Check | Status |
|---|---|---|
| Title | generated picture (lantern glow, marionette on a control bar, LIGHTENETTE wordmark), prompt and key hints on the top screen; licence/ARM7/save lines on the bottom screen; the title song plays. Start → act 1, X → credits, Select → test menu | ✅ DeSmuME headless (2026-11-11); not yet by hand in melonDS |
| Title → X (credits) | scrolling credits (libnds, BlocksDS crts MPL-2.0 with source URL, picolibc, libgcc); X opens the licence viewer (all 8 texts of `licenses/`, X next file, B back); without NitroFS the viewer says "CART FILES NOT AVAILABLE" | ✅ DeSmuME headless (credits and viewer); not yet by hand in melonDS |
| Title → Start (act 1) | top: shadow and paper marionette, strings to a crossbar, MATCH % ; bottom: four sliders. Drag a slider: its limb swings, MATCH rises. At 85%+ for 20 frames: "CLEAR!". A returns to the title, B backs out. Scripted clear: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (result block id `0x31544341`: score, cleared, angles + 90) | ✅ DeSmuME and melonDS soft headless, scripted touches (2026-11-11); hands-on feel in melonDS not yet checked |
| Title → Select | test menu (up/down, A, B backs out): *Accuracy judge* (after ~2 s the values appear; B/Start returns to the title) and *Act 2: turn the dolls*, *Act 3: find the marks*, *Act 4: untangle*, *Act 5: the finale* and *Rehearsal: acts 1-5* (the acts play themselves, then the judge; the screen ends with one row above the scene, `A1 OK A2 OK A3 OK A4 OK A5 OK`; all text stays left of x = 128 or above the scene) | ✅ DeSmuME, melonDS soft (2026-11-11) for the judge; menu keys not yet tried by hand |
| Title → Select → Act 2 | top: three paper dolls on plinths with strings, the chosen one marked; bottom: per doll NOW/AIM facing and OK/TURN. Left/Right or tap chooses, L/R (or tap the lower half) turns: the card narrows, flips, widens. Dolls A front, B back, C back for 20 frames: "CLEAR!", then the picture is captured and `SEAM 0 0 18 OK` / `EDGE n OK` show on the top screen. Scripted: `30:SELECT,50:DOWN,70:A,100:RIGHT,110:R,135:RIGHT,145:L` (result block id `0x32544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| Title → Select → Act 3 | top: nearly black stage (3D with BLDCNT 01C1) with three painted marks and a glowing translucent lantern; bottom: a map with the marks (X, `*` once found) and the lantern (@). D-pad or drag moves the lantern; keep it within 16 px of a mark for 15 frames to light it. All three found: "CLEAR!", the lantern parks, then `LAMP <none> <alpha> <darken> OK/NG` and `DARK <plain> <darkened> OK` show. Scripted: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:A:4,100:LEFT:8,100:DOWN:16,140:LEFT:32,140:UP:40,200:RIGHT:76,200:DOWN:4` (result block id `0x33544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| Title → Select → Act 4 | top: crossbar, four coloured strings (dark quad + line polygon) to four pegs on a board, a horizontal and a diagonal thread below; bottom: per peg the string on it (S1-S4), the cursor, CROSSINGS n. L/R or tap chooses a peg, A or a second tap swaps the two strings. 0 crossings for 20 frames: "CLEAR!", then `HLINE n OK` / `DLINE n OK`. Scripted: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:DOWN:4,80:A:4,100:A:2,104:RIGHT:2,108:A:2,118:A:2,122:RIGHT:2,126:RIGHT:2,130:A:2,140:A:2,144:LEFT:2,148:A:2` (result block id `0x34544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| Title → Select → Act 5 | top: dark stage with three dolls on plinths with three edge colours, line-polygon strings and the lantern; bottom: per doll NOW/AIM side, LIT/DARK, OK/TODO. D-pad or drag moves the lantern; L/R/A (or a tap on the lower part) turns the doll under it. All lit and turned (A back, B front, C back) for 20 frames: "CLEAR!", the lantern parks, then `SEAM`, `EDGE`, `LINE`, `LAMP`, `DARK` show. Scripted: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:DOWN:4,80:DOWN:4,90:A:4,110:UP:8,130:LEFT:36,160:R:2,185:RIGHT:72,250:R:2` (result block id `0x35544341`) | ✅ DeSmuME headless, screenshots looked at (2026-11-11); hands-on feel not checked |
| rehearsal demo | `lightenette-rehearsal.nds` boots straight into the rehearsal, no save chip or NitroFS; result block id `0x4C454852`. `python3 games/lightenette/tools/judge.py <log>` grades it against `expected.json` | see below |
| acc-stage demo | `lightenette-acc-stage.nds`, same numbers as the title → Select scene; result block `0x0237F000`: magic `0x52455643`, id `0x4A44474C` | see below |
| sys-irq demo | `lightenette-l2-sys-irq.nds`; item(s) `sys-irq-hblank`; expected screen: top blue / bottom red backdrop, IRQ counts; HBlank count 31557 (gate 31297..31560) | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| sys-dma-vblank demo | `lightenette-l2-sys-dma-vblank.nds`; item(s) `sys-dma-vblank-frames`; expected screen: pink backdrop, `FRAMES VERIFIED: 120/120` | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| sys-math demo | `lightenette-l2-sys-math.nds`; item(s) `sys-math-sqrt64`; expected screen: divide lines, `SQRT(1E12) = 1000000`, `LEN(3,4,0) = 20480/4096`, `ATAN2(1,1) = 4096` | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| 3d-limits demo | `lightenette-l2-3d-limits.nds`; item(s) `3d-limits-ram, 3d-limits-overflow`; expected screen: grid of small triangles, `SUBMITTED 2980  RAM 2048`, `OVERFLOW FRAMES 47` | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| acc-dual demo | `lightenette-l2-acc-dual.nds`; item(s) `acc-dual-over, acc-dual-passes, acc-dual-halves`; expected screen: top: block city with a seam down the middle (two passes); bottom: LATENCY 1, DUAL / ONE PASS / PASSES / HALVES lines with four OK | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| 2d-affine demo | `lightenette-l2-2d-affine.nds`; item(s) `2d-affine-pixels`; expected screen: top: a rotated affine tile picture (pseudo-random colours, black holes), no text (the judge frame shows BG2 alone) | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| 2d-mosaic demo | `lightenette-l2-2d-mosaic.nds`; item(s) `2d-mosaic-pixels`; expected screen: top: the console text as 4x4 blocks (mosaic 3), coloured blocks at the bottom | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| 2d-obj-bitmap demo | `lightenette-l2-2d-obj-bitmap.nds`; item(s) `2d-obj-bitmap-pixels`; expected screen: a lit gradient disc (bitmap OBJ) below two text lines | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |
| snd-capture demo | `lightenette-l2-snd-capture.nds`; item(s) `snd-capture-signal`; expected screen: text only; the capture peak is in the block (values[5], about 0x3800) | melonDS 1.1 soft/GL/compute and DeSmuME headless, 2026-11-12; see "Level 2 demos" |

## Auto QA run

Hidden mode of the release ROM (`lightenette.nds`, SHA-256 identical to what ships): on the title screen press L and R, then X and Y, and
hold all four for 120 frames. The game plays the normal flow by itself: acts 1-5 with the rehearsal's scripts, the credits, then one
accuracy-judge pass. Log on the SD card (`cleancart/lightenette/qa-<date>-<time>.log`), result block `QARN` in the rehearsal's layout, graded with
`python3 games/lightenette/tools/judge.py --profile <renderer> <log>` (boot `qa`, extra item `qa-verdict`). Headless: `SDDIR=out/lt/x tools/melonds/mds.sh run
games/lightenette/lightenette.nds 7000 out/lt/x "300:L:140,300:R:140,300:X:140,300:Y:140"` (`RENDERER=gl|compute` for the GL renderers; DeSmuME: `RESULT_WORDS=64
tools/emu/emu.sh run ... 4000 ...`).

| Renderer | QA run (2026-11-11) | `judge.py --profile` | QA judge values = rehearsal demo's |
|---|---|---|---|
| melonDS 1.1 software | ✅ `RESULT PASS` at frame 2090 | ✅ `melonds-1.1-software` MATCH (no NG) | ✅ identical (clear frames excluded) |
| melonDS 1.1 GL classic | ✅ PASS | ✅ `melonds-1.1-gl-classic` MATCH (the 9 known NG: seam-uv, act2-seam, act5-seam, edge-marking, act2-edge-marking, act5-edge-3-colours, lantern-blend-equal, act3-lantern-equal, act5-lantern-equal) | ✅ identical |
| melonDS 1.1 GL compute | ✅ PASS | ✅ `melonds-1.1-gl-compute` MATCH | ✅ identical |
| DeSmuME (py-desmume 0.0.9) | ✅ verdict 1 | ✅ `desmume-pydesmume-0.0.9` MATCH | ✅ identical |

Two software runs give identical checkpoint hashes (`tools/qa/compare.py`, 6 checkpoints). With QA off nothing changed: the rehearsal run through the
test menu gives the same result block as before. The software run has no save chip (homebrew run for the SD card): the save mirror read back identical, the chip
write is not verified. Real DS Lite + R4: not run yet.

## Level 2 demos (2026-11-12)

Nine demo ROMs `lightenette-l2-<name>.nds` carry the 12 level 2 items taken over from Corvane (the measurement code is
Corvane's; the 2D pictures are generated at run time, no assets). Each writes its block at frame 120 (id = FNV-1a 32 of
`l2-<name>`). Measured with the 2026-11-12 build (`mds.sh demos`, `emu.sh demos`, each ROM graded with
`judge.py --demo lightenette-l2-<name> --profile <profile>`):

| Emulator | all 12 items | profile |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute | OK (the informational `3d-limits-overflow` 47 and `acc-dual-halves` also match) | MATCH, no NG, no n/a |
| DeSmuME (py-desmume 0.0.9) | the other 8 OK | MATCH: n/a `3d-limits-ram`, `3d-limits-overflow`, `acc-dual-over`, `acc-dual-passes` (polygon RAM counters read 0) |

Values seen: HBlank 31557 (melonDS) / 31374 (DeSmuME), polygons 2048, overflow frames 47, acc-dual flags `0x1f` (DeSmuME `0x19`),
capture peak about 0x3800 (melonDS) / 0x3cf2 (DeSmuME). Three headless runs of every demo agree between the three melonDS
renderers except the `acc-dual` picture hashes (renderer differences, not graded). The auto QA run was repeated after the ARM7
gained the sound read-outs the capture demo needs (PASS on soft / GL / compute, DeSmuME verdict 1, 6 checkpoints identical in two
software runs, `tools/qa/lint.py` ok). Real DS Lite + R4: not run.

## Conclusion (2026-11-11)

The rehearsal reproduces the graphics bugs that are currently open for the OpenGL (Classic)
renderer. Each one shows up as NG items on that renderer, while the software renderer passes every
gating item:

| Bug | What the ROM sees on OpenGL (Classic) | NG items |
|---|---|---|
| Reversed texture coordinates | the cut line shows on the compensated doll instead of the plain one (judge and act 2 `0/18/0`, act 5 `18/0/0`) | `seam-uv`, `act2-seam`, `act5-seam` |
| Edge marking | no outlines at all (0 edge pixels) | `edge-marking`, `act2-edge-marking`, `act5-edge-3-colours` |
| Translucent OBJ over a brightness effect | the lantern changes with the blend mode (e.g. `0d2c/318f/0d2c` instead of one value) | `lantern-blend-equal`, `act3-lantern-equal`, `act5-lantern-equal` |

These NG results are the intended outcome, not faults of the game: every act still clears on that
renderer, and its profile (`--profile`) matches. Not covered yet: internal resolutions above 1x
(all runs are at 1x) and real hardware.

## Judge measurements (2026-11-11, headless; GL = Mesa llvmpipe; not yet on hardware)

Result block `values[0..3]` (seam rows, edge, line, flags) and lantern values `values[4..6]`:

| Renderer | seam | edge | line | flags | lantern NONE/ALPHA/DARK |
|---|---|---|---|---|---|
| melonDS software | 0/0/18 | 185 | 697 | 71f | 318f/318f/318f |
| melonDS GL classic | 0/18/0 | 0 | 698 | 714 | 0d2c/318f/0d2c |
| melonDS GL compute | 0/0/18 | 185 | 826 | 717 | 0d2c/318f/0d2c |
| DeSmuME | 0/0/18 | 192 | 322 | 717 | 318f/318f/1d6e |

`flags` bits 0-4 are the on-screen passes (seam, edge, line, lantern, darken); bits 8-10 say that the
display capture of BLDCNT mode 0100 / 0140 / 01C1 finished. A value is only valid when its capture bit
is set; the pixels of a capture that did not finish read `0xFFFF` and `judge.py` grades the items that
depend on them NG (bank D is filled with a sentinel before every capture, so a stale picture cannot
pass). The strings now stop 3 px above the puppets: string ends that overlapped a puppet polygon (another
polygon ID, so edge-marked) made the edge and line counts depend on where the strings ended. Compared with corvane's `acc-stage` (edge 183/0/183/188, line 705/710/834/338) the edge
counts are 2 / 0 / 2 / 4 higher and the line counts 8 / 12 / 8 / 16 lower; seam, flags logic and lantern
values are unchanged.

Rehearsal block extras (`values[9..17]`): act 1 score 97, cleared 1, clear frame 59, finished 1, act 2 cleared 1 (`values[13]`) — identical on all four renderers (the acts' logic is emulator-independent). The act 2 picture grades (`values[14]` seam per doll, `values[15]` edge pixels):

| Renderer | act 2 seam | act 2 edge |
|---|---|---|
| melonDS software | 0/0/18 | 228 |
| melonDS GL classic | 0/18/0 | 0 |
| melonDS GL compute | 0/0/18 | 228 |
| DeSmuME | 0/0/18 | 240 |

The result block now has 64 words (`CvResult.values[61]`); read them all (`RESULT_WORDS=64` for DeSmuME).

Act 3 block extras (`values[18..26]`): cleared 1, marks 3, lantern pixel NONE/ALPHA/DARK, backdrop pixel plain/darkened, capture bits 7 (bit m = BLDCNT mode m finished), clear frame 181. The lantern pixel is read at (214, 140), well inside the glow and away from the stage's curtain edge at x = 232 (the first version sampled (232, 132) on that edge). Measured (headless, 2026-11-11):

| Renderer | lantern NONE/ALPHA/DARK | backdrop plain/darkened |
|---|---|---|
| melonDS software | 35d1/35d1/35d1 | 2463/0c21 |
| melonDS GL classic | 0d4e/31b1/0d4e | 2463/0c21 |
| melonDS GL compute | 0d4e/35d1/0d4e | 2463/0c21 |
| DeSmuME | 31b1/31b1/1d90 | 2463/1042 |

Act 4 and 5 block extras (`values[27..44]`, headless, 2026-11-11):

| Renderer | act 4 horizontal / diagonal thread px | act 5 seam | act 5 edge px (3 colours) | act 5 string line px | act 5 lantern NONE/ALPHA/DARK | backdrop plain/darkened |
|---|---|---|---|---|---|---|
| melonDS software | 208 / 208 | 0/0/18 | 76/76/76 | 93 | 318f/318f/318f | 2463/0c21 |
| melonDS GL classic | 208 / 208 | 18/0/0 | 0/0/0 | 93 | 0d2c/318f/0d2c | 2463/0c21 |
| melonDS GL compute | 208 / 208 | 0/0/18 | 76/76/76 | 186 | 0d2c/318f/0d2c | 2463/0c21 |
| DeSmuME | 8 / 208 | 0/0/18 | 80/80/80 | 93 | 318f/318f/1d6e | 2463/1042 |

Act 5 puts the clean-mirror back (doll A) and the front (doll B) next to the plain-mirror back (doll C), so melonDS GL classic shows the #2008 cut line on doll A (`18/0/0`) instead of doll C. The thread and string pixel counts are `unconfirmed` (informational): DeSmuME hardly draws the horizontal line polygon (8 px) and melonDS compute draws the vertical strings twice as thick.

`tools/judge.py` on the rehearsal ROM (run 1800 frames; the rehearsal ends at frame ~864 on melonDS and ~954 on DeSmuME, and at ~1640 in the worst case where every act runs to its own limit):

| Renderer | verdict | NG items | `--profile` |
|---|---|---|---|
| melonDS software | OK (exit 0) | - | `melonds-1.1-software` matches (exit 0) |
| melonDS GL classic | NG (exit 1) | seam-uv, edge-marking, lantern-blend-equal, act2-seam, act2-edge-marking, act3-lantern-equal, act5-seam, act5-edge-3-colours, act5-lantern-equal | `melonds-1.1-gl-classic` matches (exit 0) |
| melonDS GL compute | NG (exit 1) | lantern-blend-equal, act3-lantern-equal, act5-lantern-equal | `melonds-1.1-gl-compute` matches (exit 0) |
| DeSmuME | NG (exit 1) | lantern-blend-equal, act3-lantern-equal, act5-lantern-equal | `desmume-pydesmume-0.0.9` matches (exit 0) |

`judge.py --profile NAME` exits 0 only when the set of failing gating items equals the `known` list of
that profile in `expected.json`; any other set is exit 1 and the run names the newly failing and the
newly passing items. A reviewer reported that melonDS master `4ce2188` already fixed #2503; we have
**not** verified that. If it is true, the GL profiles report the three lantern items as NEWLY PASSING
and exit 1 until the profile is updated.

The three `*-lantern-equal` items also require the lantern pixel to differ from the backdrop pixel
(`unlike` in `expected.json`): a lantern that is not drawn at all would otherwise give three identical
backdrop pixels and pass as "all equal".

Gating items: `seam-uv`, `act2-seam`, `act5-seam` are qualitative (source `report:melonDS#2008`): rows 0 and 1
must show no cut line and row 2 (the uncompensated reverse) must show one. The exact `0/0/18`
(18 = the card height) is an emulator measurement, so it is graded only as the informational
`*-exact` items (`unconfirmed`). `line-polygon`, `act4-hline`, `act4-dline` and `act5-line-polygon` are
`unconfirmed` too (line-polygon 697/698/826/322). The lantern items expect equal values in all modes
(GBATEK). DeSmuME ≠ melonDS ≠ hardware.

Checks of the judge itself (2026-11-11, `judge.py` from an unzipped kit layout, no `../`):

| Input | Result |
|---|---|
| a real software block | OK, exit 0 |
| a block whose lantern pixels are all `ffff` (capture bits still set) | NG (6 items), exit 1 |
| a block with every capture bit clear | NG (12 items: "capture not finished"), exit 1 |
| `--boot nosuch` / `--boot acc-stage` on a rehearsal block | exit 2 (unknown / mismatching boot) |
| a block left by the act 3 loop (id `ACT3`) | NG "REHEARSAL INCOMPLETE", exit 1 |
| malformed `PEEK` / `RESULT` lines in the log | skipped with a warning, the good block is graded |
| missing log file, invalid `expected.json` | exit 2 |
| `--demo NAME` on an `mds.sh demos` log (`<name> RESULT …`) | grades that ROM's block |
