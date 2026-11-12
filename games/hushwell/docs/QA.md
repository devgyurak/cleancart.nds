# Hushwell QA

[한국어](QA-KO.md)

QA requirement from the design document: check every demo build and the main build
in **melonDS**, and record the melonDS version, demo name and expected screen.

## Rules shared by all demo builds

- Build: `./build.sh DEMO=<name>` → `hushwell-<name>.nds` (`./build.sh demos` for all)
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

## Auto QA run

Hidden mode, same ROM as the release: on the title screen hold **L+R+X+Y for 120 frames**. The game
walks the prologue and chapters 1-5 by itself (creature hits cost no health; everything else as shipped),
opens the doors, dials the chapter 3 code, collects every tape, takes the well ending, plays the credits
and stops on the records screen. Log: `cleancart/hushwell/qa-<date>-<time>.log` on the SD card, result
block `QARN` at `0x0237F000`. Headless: `SDDIR=out/mds/x tools/melonds/mds.sh run games/hushwell/hushwell.nds
40000 out/mds/x "700:L:140,700:R:140,700:X:140,700:Y:140"`, then `tools/qa/parse_log.py`.

| Where | Result (2026-11-11, build from this tree) |
|---|---|
| melonDS 1.1 headless, software renderer, SD image | ✅ `RESULT PASS reason=credits` at frame 23,644, 6 chapter checkpoints, all tapes (`3e`), ending 2 (the well), 7 save mirrors read back identical (`chip_type=-1`: homebrew run, no save chip) |
| Same ROM, second run | ✅ all 6 checkpoint hashes identical (`tools/qa/compare.py`) |
| DeSmuME headless (no SD) | ✅ `QARN` block verdict 1, 6 chapters, tapes `3e`, last state hash `fea01683` = melonDS (lag differs, not compared) |
| Real DS Lite + R4 | not run yet |

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
| 2d-text | top: console; bottom: the chapter HUD (map frame, gauges, meter) with two console lines | BG3CNT_SUB, BG0CNT_SUB | |
| 2d-affine | rotating hand-drawn map tiles (affine) + growing HUSHWELL logo (extended affine) | BG2PA, BG3PA | |
| 2d-bitmap | 0–59: 16bpp title + 8bpp logo, 60+: 1024x512 plan of every chapter map scrolling | DISPCNT, BG2CNT, phase 2 | |
| 2d-mosaic | text and battery sprites get coarser mosaic | MOSAIC | |
| 2d-obj-normal | 100 mote/drop sprites moving in circles, half behind the BG | OAM count 100 | |
| 2d-obj-affine | four portraits in four colours rotating/scaling + a growing flare icon | OAM count | |
| 2d-obj-bitmap | two 64x64 torn photographs (one semi-transparent) | OAM count | |
| 2d-obj-map2d | the same tape icon on top (1D) and bottom (2D mapping) | DISPCNT_SUB | |
| 2d-obj-line | 120 key sprites on one band — some vanish (line limit) | OAM count | |
| 2d-extpal | radio illustration: left original colours / right recoloured + one portrait in four ext palettes | DISPCNT, map entry | |
| 2d-window | top: only two rectangles bright; bottom: HUD seen through a keyhole OBJ window | WININ, WINOUT, WINOUT_SUB | |
| 2d-blend | every 30 frames: alpha / brighten / darken / semi-transparent OBJ | BLDCNT, BLDALPHA | |
| 2d-brightness | top fades to white, bottom to black, independently | MASTER_BRIGHT ×2 | |
| 2d-vram | table of VRAMCNT A–I for the 10 layouts | table hash | |
| 2d-capture | 3D on both screens (escape ending: car on the road / the well) | DISPCAPCNT (no console) | |
| 2d-display | 0–59 VRAM display, 60+ main-memory FIFO display | pixel values | |
| 3d-fifo | the chapter 1 ward turning in place: walls by port writes, radio and flare box by DMA lists | polygon/vertex count, radio list size, map loaded | |
| 3d-matrix | the creature walking in place, head, arms and legs on a push/pop hierarchy | torso list size, polygon count | |
| 3d-light | altar with four coloured lights circling | polygon count | |
| 3d-clip | camera pressed against a ward wall, near-plane clipping | polygon/vertex count, map loaded | |
| 3d-tex-a3i5 / a5i3 / pal4 / pal16 / pal256 / 4x4 / direct | a quad in that texture format rotating, with repeat, flip and texture-matrix scrolling | texture id, size | |
| 3d-poly | tiled floor, front/back-culled cubes, translucent car, radio shadow | polygon count | |
| 3d-toon | 0–59 toon / 60+ highlight shading | DISP3DCNT | |
| 3d-edge | outlines per polygon ID | DISP3DCNT | |
| 3d-fog | the prologue road: 0–59 distance fog / 60+ alpha-only fog | DISP3DCNT, polygon count, map loaded | |
| 3d-aa | cube with AA + alpha-tested twigs | DISP3DCNT | |
| 3d-depth | 0–59 W-buffer and solid rear plane / 60+ Z-buffer and bitmap rear plane | DISP3DCNT | |
| 3d-test | BOX INSIDE/OUTSIDE toggles, POS and VEC results | inside count, w, vec x, vec z | |
| 3d-compose | 3D cube in front of → behind the 2D rain layer, sprites over 3D, BG0 shaking | BG0CNT, BG2CNT | |
| 3d-limits | triangles exceed 2048 and some vanish, overflow frames > 0 | overflow frames, polygon RAM | |
| snd-mix | chase BGM + 8 different effects panned left/right | song number, 8 | |
| snd-pcm | PCM8 one-shot (step) → PCM16 one-shot (thunder) → PCM8 loop → PCM16 loop | sample numbers | |
| snd-adpcm | ADPCM sting (one-shot) → hum (loop) | sample numbers | |
| snd-psg | beeps on channels 8–13 with changing duty, noise | 8, 15 | |
| snd-capture | archive song + echo | 1 | |
| snd-stream | seamless rain-and-drone ambience | size, sample rate | |
| snd-wave | a status line with the level changes of a 440 Hz wave and the swings (peak-to-peak) of the left mixer record | level changes A4, A3, swing at volume 100, hard-right swing | |
| io-cart | NitroFS file sizes, cart DMA OK, chapter 3 map info | total, load ok, cart DMA, object count | |
| io-save-eeprom / flash / fram | WRITE+VERIFY OK, RELOAD OK | write, reload, seq +1, chip answers | |
| io-firmware | name, favourite colour → flashlight colour, birthday today, language, touch calibration | colour, language, name length, light colour | |
| io-rtc | date and time update every second | year, month, day, hour | |
| io-touch | crosshair at the pen position, pixel/raw values | touching, x, y, raw x | |
| io-power | A: top backlight, B: speaker, closing the lid sleeps | battery, lid, backlight, speaker | |

## Judging (`expected.json`, `tools/judge.py`)

The result block of each demo is graded by `tools/judge.py` against `expected.json` (143 items, all 66
demos and the QA run). Every item has a `level` from 1 to 4 and a `level_basis` ([docs/LEVELS.md](../../../docs/LEVELS.md)); levels never
change a verdict.

| Kind | What is judged | Examples |
|---|---|---|
| (a) self-check, documented constant | error counts 0, `sqrt(10^12)` = 1000000, CRC equal, 2048 polygons | sys-wram, sys-dma-*, sys-bios, sys-math, 3d-limits |
| (b) register read-back | the register reads back the value the demo wrote (write-only registers are not judged) | 2d-engines, 2d-window, 2d-blend, 3d-toon, 3d-compose |
| (c) timing figure | a count in a window derived from GBATEK (263 lines per frame, 33.513982 MHz bus) | sys-irq, sys-timer, sys-dma-vblank |
| (d) screen or sound | judged in the ROM: the demo captures its own output (display capture to bank D) and compares pixels with GBATEK's rules; VRAM mappings are read back through their aliases; the sound demos read the channel and capture state the ARM7 reports | sys-dma-hblank, sys-dma-disp, 2d-affine, 2d-mosaic, 2d-obj-*, 2d-vram, 2d-display, snd-* |

Sources: `GBATEK` and `rom` items gate, `unconfirmed` ones (no hardware value yet, for example polygon counts of
the models) are informational. Polygon counters read 0 in DeSmuME: those items are n/a there (`na_when`).

```sh
tools/melonds/mds.sh demos games/hushwell/build/release/demos > demos.log     # RENDERER=gl|compute for the GL renderers
python3 games/hushwell/tools/judge.py --profile melonds-1.1-software demos.log
RESULT_WORDS=16 tools/emu/emu.sh demos games/hushwell > ds.log && python3 games/hushwell/tools/judge.py --profile desmume-pydesmume-0.0.9 ds.log
```

Known profiles (measured 2026-11-12, nothing on real hardware yet): melonDS 1.1 software, GL (Classic) and GL (Compute) pass every gating item;
DeSmuME has no polygon counters (those items are n/a) and differs in informational items only (key IRQ count, `VEC_TEST` z, the main-memory
display FIFO capture). Values that depend on this game's scenes (polygon counts, the HUD register values of 2d-text, WINOUT of engine B, the CRC of the
logo) are Hushwell's own and differ from Corvane's. The auto QA run of `hushwell.nds` is graded as demo `qa` (`--demo qa`): `RESULT PASS` in
melonDS 1.1 (software) and DeSmuME.

The kind (d) demos report their probe in `values[4..8]` (samples compared, wrong, capture finished, extras). What stays
unjudged: the sub screen's line-start DMA bars (`sys-dma-disp`), the pictures of the demos beyond the probed pixels, and the sound
output beyond "channel busy / capture ring not silent". Informational (`unconfirmed`) probes: the main-memory display FIFO capture
(`sys-dma-disp`, `2d-display`: fails in DeSmuME, what a capture sees while the display reads the same FIFO is not documented) and the
per-line OBJ budget (`2d-obj-line`: GBATEK describes about 1210 cycles per line, but no emulator drops the last sprites and the exact
accounting is not confirmed). The ARM7 reports its channel state in the input packet (`CvInputPacket.snd_busy`, `cap_peak`, `cap_busy`).

## Main build checklist

| Item | How to check | melonDS result |
|---|---|---|
| Boot logo → warning → title | just run it | |
| Demo play | wait 20 s on the title; the chapters play in turn; a key returns to the title | |
| New game → prologue → chapters 1–5 → ending → staff roll → records | play from start to finish | |
| Both endings | finish chapter 5 with fewer than 5 tapes (escape), then with all 5 and press A at the well (the well) | |
| Radios and death | use a radio, get struck three times, A on the death screen returns to that radio | |
| Continue | quit from pause, CONTINUE on the title resumes at the last radio | |
| Hearing | walking near the creature is safe, running or wading makes it come; L (hush) in the dark lets it pass | |
| Flares | R+A at the creature stuns it (it glows and shakes); flares run out | |
| Keyhole | A at a locked door shows the view through the keyhole | |
| Prologue opening | new game: you sit in the car, wipers sweep, the radio breaks up, a shape crosses the road; A skips | |
| Chapter 2 | caustic light ripples over the view (affine BG + HBlank DMA), stronger below mid-screen | |
| Chapter 3 | the projector plays a film (main-memory display); PLAY tape 3 and REC it, then REV: the code 4-1-7 appears and the voice speaks forwards; the dial opens the door | |
| Chapter 3 portrait | as the creature comes close a portrait flickers top-left: normal → faded → wrong → negative | |
| Recorder photo | PHOTO, then VIEW PHOTO shows the captured view on the bottom screen (not in chapter 4 or the chase) | |
| Microphone | breathe or speak into the mic near the creature: it hears you (MIC LISTENS off: it doesn't) | |
| Chapter 5 opening | the climb down the shaft before the tunnels; A skips | |
| Chapter 4 night visitor | set NIGHT VISIT to ALWAYS (or play between 0:00 and 4:59): an outlined figure follows and drains the battery | |
| Chapter 5 chase | taking the last key: both screens show 3D (ahead / behind); near the swarm the polygon count overflows | |
| Lid | close during play → sleep → open: game is paused, the creature is hunting you | |
| Records | clear time, ending and tapes saved with the RTC date | |
| Replay | X on the records screen replays the last chapter run exactly | |
| Pause tools | X lights on, Y vision mask off, B developer console, R advances a frame, 10 s idle turns the backlights off | |
| Screen swap | Select swaps the screens in play; SCREEN SWAP in settings starts with 3D below | |
| Licences | Credits → X shows THIRD_PARTY_NOTICES, MPL-2.0, picolibc and the other texts | |
| Sound | footsteps pan with the creature, static rises when it is near, heartbeat speeds up; no dropouts | |

## Automated smoke-test record (DeSmuME)

Before melonDS QA, the session that created this game checked the following on
headless DeSmuME 0.9.12 (py-desmume, x86_64 container, `tools/emu/`). This does not
replace melonDS QA.

- 2026-11-12 (snd-wave, PSG pan field): the release build has no warnings; all 66 demos graded in headless melonDS 1.1 (software, GL classic, GL compute) and DeSmuME, every `known` profile re-measured and MATCH; the auto QA run in melonDS soft with an SD image reached `RESULT PASS` (6 checkpoints, `parse_log.py`), a second run had identical checkpoint hashes (`compare.py`), `lint.py` passed. Not run: the DeSmuME QA run, a QA-off comparison.
- 66 demos: all reached the frame-120 checkpoint (result block magic checked). DeSmuME
  doesn't report polygon RAM usage or the overflow flag, so `3d-limits` and `3d-fifo`
  polygon counts read 0.
- Main build: boot → warning → title → radio → each of the six chapters entered and
  walked (map fills in and rotates, recorder opens, flashlight, flare fired), the creature
  heard running, chased, struck three times and retreated in between, death screen,
  QA chapter clear → chapter clear screen → dual-screen escape ending → credits → record
  saved, records/settings/sound-test screens, attract-mode demo play.
- Chapter 5 dual-screen chase, forced by writing `W.chase` in RAM: forward view on the
  top screen, unlit rear view on the bottom, pausing mid-chase keeps both screens intact.
- Two clean builds give the same SHA-256 for `hushwell.nds`.
- Scenario runs with a test harness that also writes RAM (player position, tapes) to skip
  walking:
  - chapter 3: the projector film plays through the main-memory display and ends, the
    torn photo and scratched code appear; the dial lock set to 4-1-7 by touch opens the door;
  - chapter 4: NIGHT VISIT = ALWAYS in settings is saved; the visitor appears as a black
    figure with a glowing outline, follows at about two cells and drains the battery;
  - chapter 5: with all five tapes the well asks "go down / climb out"; A gives the well
    ending (sky circle shrinking above, faces below) and the credits show ENDING: THE WELL;
    B gives the escape ending;
  - lid: closing it sleeps, opening it shows the pause screen, START resumes;
  - replay: a chapter 1 run (walk, turn, the creature's strike) and its replay from the
    records screen have identical chapter state at chapter frames 300 and 500, except the
    map heap pointers and the on-screen message pointer/timer.
- New features, same harness: the in-car opening (windshield window, sweeping wipers, the shape
  crossing), the chapter 5 shaft descent, the chapter 2 caustics ripple, the chapter 3 portrait
  switching extended palettes as the creature closes in, the recorder's REC (sound capture into
  RAM) then REV (reversed through the stream channel) revealing the code, and PHOTO / VIEW PHOTO.
  The replay check was repeated after the mic level joined the input log: still identical.
- Full playthrough without RAM writes: an input bot (buttons and touches only; it reads RAM
  to know where the player is, like looking at the screen) played from the title through the
  prologue and chapters 1-5, picked up all five tapes, opened every door, set the chapter 3
  dial by touch, used flares on the creature, took the well ending and sat through the staff
  roll to the records screen, in about 44,000 frames. It died twice (chapters 1 and 2) and
  carried on from the chapter start each time. The run found three bugs, all fixed: a
  retreating creature could still strike, the chase creature was nearly as fast as running
  with no pause after a hit, and the dual-screen rear view briefly rewrote the player's yaw.
- Not covered by the smoke tests: the
  swarm polygon overflow (DeSmuME reports 0), what the reversed recording actually sounds like
  and the microphone (headless runs have no audio input), and anything in melonDS.
