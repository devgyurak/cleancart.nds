[한국어](HW-FINDINGS-KO.md)

# Hardware findings

What the games of this repository measured on a real console that GBATEK or the emulators do not say. One place
for emulator developers; the evidence (logs, tables, ROM hashes) stays in each game's `docs/HW-QA.md`.

Console for every entry: DS Lite (NTR) with an R4 Plus card (Wood kernel), hw-support builds of the release
ROMs, 2026-11-17 to 2026-11-20 (Buoyane run 1: 2026-11-20). Emulators: melonDS 1.1 (software, OpenGL Classic, OpenGL Compute; headless core)
and DeSmuME (py-desmume 0.0.9). One console only: a value can still differ on an original DS or a DSi.

The sections:

1. [The console disagrees with GBATEK](#1-the-console-disagrees-with-gbatek) (or GBATEK says nothing): the
   expected value in `expected.json` was changed to the console's (`source: hw:dslite-ntr`) or is still informational.
2. [GBATEK holds, an emulator does not](#2-gbatek-holds-an-emulator-does-not): gating items that pass on the console
   and fail on at least one emulator (the `ng` lists of the known profiles).
3. [Not judged, but different](#3-not-judged-but-different).

## 1. The console disagrees with GBATEK

| Game, item (level) | GBATEK / our first expectation | Console | melonDS 1.1 | DeSmuME | Status |
|---|---|---|---|---|---|
| Lightenette `snd-wave-pan-right-exact` (L2) | Pan 126 (hard right) leaves the left mixer at most 2 (of a swing of 199 at pan 0). GBATEK's rounding of the pan fraction reads two ways: with its MSB as two's complement the step is +1 / -2, as magnitude +1 / -1 | **3** (both runs) | 3 | 1 | expected value 3, `hw:dslite-ntr`, since Lightenette v0.3.1 |
| Lightenette `3d-limits-ram` (L2) | RAM_COUNT (4000604h) polygon count stops at 2048 | **2979** with 2980 polygons submitted before the flush: it keeps counting past 2048 (the overflow flag is set) | 2048 (clamped) | n/a (counters read 0) | range 2049..2980, `hw:dslite-ntr`, since v0.3.1. Also seen in Lightenette and Elevane `acc-dual` (one pass: 2310 on the console, 1536 in melonDS; not judged) |
| Ledgerine `ipc-empty-cleared` (L2) | An empty FIFO read after the remote side cleared its send FIFO returns 0 | **`7A00000Fh`**: A15, the last word of an earlier 16-word burst; neither B (the last word read) nor C (the word that was cleared). The error bit is set and acknowledges normally | 0 | 0 | judged as "not 0, acknowledge works" (`mask 0xC`, `expect 0x4`) since v0.2.2; the raw word is in `values[44]` from v0.2.2 on (one run so far); the exact word is judged by `ipc-empty-cleared-word` (L2, #209) |
| Tonewick `snd-short-silent` (L2) | A looped sample of 3 words (below the 4-word minimum) gives no sound | **16384** captured peak (three runs): it keeps the tape level, it is not silent | 0 | 16384 | range 16320..16448, `hw:dslite-ntr`, since v0.2.2 |
| Pendulane `mem7-main-nonseq` (L3) | 64 non-sequential ARM7 reads from main RAM: 9 cycles each from GBATEK's table, 576 | **383** (three runs): about 6 per read (main RAM N32 effective 7 against WRAM 1) | 384 | 64 | range 376..392, `hw:dslite-ntr`, since v0.2.2 |
| Pendulane `mem7-code-main-seq` (L3) | ARM7 code running from main RAM, 64 sequential words: 2 cycles per word, 128 | **142** (three runs): about 14 cycles more; cause not documented | 128 | 128 | range 139..145, `hw:dslite-ntr`, since v0.2.2 |
| Heddlemere `gx-trans-cycles` (L3) | MTX_TRANS costs 22 cycles | **35** (560 = 35 x16) | 35 (559) | 0 | range 556..564, `hw:dslite-ntr`, since Heddlemere v1.0.1 |
| Heddlemere `gx-box-cycles` (L3) | BOX_TEST costs 103 cycles | **about 126** (2020 = 126.25 x16) | 257 (4114) | 2 | range 2012..2028, `hw:dslite-ntr`, since v1.0.1 (one run so far) |
| Heddlemere `hblank-low-arm7` (L3) | The non-HBlank span of a line is 1613 cycles on the ARM7 ("for whatever reason"), 1606 on the ARM9 | **25712** (x16): the same as the ARM9 (25710); GBATEK's 1606 holds for both CPUs | 25343 (ARM9 25343) | 25634 (ARM9 25632) | range 25680..25744, `hw:dslite-ntr`, since v1.0.1 |
| Buoyane `wire-texture-alpha` (L2) | "Wire-frame polygons are always having Av=31, however, they can be made transparent by using Translucent Textures (ie. A5I3 or A3I5 formats) with At<31" (GBATEK, DS 3D Texture Blending). A wireframe with an A5I3 texture (alpha 15) is blended | **not blended**: no blended wire pixel (one run); the same on all four emulators | not blended | not blended | expected value 0, `hw:dslite-ntr`, since the next Buoyane release; it cannot tell "drawn opaque" from "not drawn" until the white wire pixels are counted |
| Buoyane `zeq-slope-band-left`, `zeq-slope-band-right` (L2, removed) | The Equal band on a sloping face runs from x 101..107 to x 149..155 (our model of GBATEK's +-200h window) | **109** and **159**: 4 and 7 pixels further right (one run). The width, 50, is inside the window GBATEK's tolerance gives | 105, 152 (width 47) | 118, 141 (width 23) | the width is judged (`zeq-slope-band-width`, 42..54, GBATEK); the position moves to `probes/` (follow-up) |
| Buoyane `zeq-decal-whole` (L2, removed) | A decal on a sloping base has no holes | **holes**: 960 pixels (`values[18]`, one run) | no holes (0) | 2120 (`0x848`) | not judged (rasteriser rounding is undocumented); the holes move to `probes/` (follow-up) |
| Buoyane `values[13]` `W_ZEQ_1` (not judged) | A plane one depth step (+-1) off the base is drawn by Equal inside the tolerance | **0** pixels | 1024 + 1024 pixels (`0x04000400`) | 0 | the probe `zeq-step-boundary` measures the same planes; the console value is to be recorded there |

The ARM7 HBlank span is polled by our own ARM7 core, so the poll loop may be part of the figure; the ARM9 figure with the
same method matches GBATEK, which is why the ARM7 one is listed.

**How Ledgerine reads A15.** A0..A15 fill a 16-entry ring and are read; B goes to entry 0 and is read, then an empty
read returns B. C goes to entry 1, the ARM7 clears its send FIFO before the ARM9 reads it, and the next empty read
returns A15. This fits if an empty read returns the entry before the read pointer and the clear puts the pointers back
to entry 0 (the entry before it, 15, still holds A15). It is a reading of one script, not a documented rule.

## 2. GBATEK holds, an emulator does not

Every item here passed on the console with its GBATEK expected value (or the value reported in the named melonDS
issue). "Fails on" lists the known profiles that have the item in `ng`.

| Game, item (level) | What the console does | Fails on |
|---|---|---|
| Paneveil `poly-wire-edges` (L1) | A polygon with alpha 0 draws its edges (wireframe) | melonDS GL Classic, DeSmuME |
| Corvane `acc-stage-edge` (L1), Lightenette `edge-marking`, `act2-edge-marking`, `act5-edge-3-colours` (L1) | Edge marking draws outlines (melonDS #2549) | melonDS GL Classic |
| Lightenette `seam-uv`, `act2-seam`, `act5-seam` (L2) | The reversed-UV cut line shows in row 2 only (melonDS #2008) | melonDS GL Classic |
| Lightenette `lantern-blend-equal`, `act3-lantern-equal`, `act5-lantern-equal` (L2) | A translucent OBJ looks the same with BLDCNT none, alpha and darken (melonDS #2503) | melonDS GL Classic, GL Compute, DeSmuME |
| Elevane `hdma-holes-zero`, `acc-split-hdma-holes` (L3) | HBlank DMA compositing leaves no hole in any frame (melonDS #2618) | melonDS 4ce2188 GL Classic and GL Compute (the profiles recorded for this item) |
| Ledgerine `ipc-empty-last` (L2) | An empty FIFO read returns the last word received (B) and sets the error bit | melonDS (returns the next cell, `7A000001h`), DeSmuME |
| Ledgerine `dma-itcm-runs` (L2) | The CPU keeps running ITCM code during a long DMA | melonDS (halts the CPU for the whole DMA), DeSmuME |
| Ledgerine `bios-arm7-guard` (L2) | ARM7 CpuSet with a source in the BIOS area writes nothing | melonDS (its replacement BIOS copies), DeSmuME (HLE BIOS) |
| Ledgerine `math-div0`, `ipc-disabled`, `timer-reload-deferred`, `dma-regs-unchanged` (L2) | Division by zero results and flag; a disabled FIFO ignores writes and peeks reads; a reload written while running waits for the next overflow; SAD, DAD and CNT_L keep their initial values after a transfer | DeSmuME |
| Tonewick `snd-hold-level` (L2) | With the hold bit a one-shot keeps its last sample | melonDS (returns to 0), DeSmuME |
| Tonewick `cap-src-channel` (L2) | Capture 0 with source bit 1 records channel 0 alone | melonDS (captures the mixer sum) |
| Tonewick `cap-add-wrap`, `cap-both-negative` (L2) | The two capture bugs GBATEK documents: add is ANDed with FFFFh, not clipped; two negative channels capture -8000h | melonDS (writes nothing / reads -4000h), DeSmuME |
| Tonewick `pm-reg-mirror` (L2) | Power management registers 5, 6, 7 mirror 4 on the DS Lite | melonDS (register 5 reads 0 where 4 reads 40h), DeSmuME |
| Tonewick `snd-short-busy`, `snd-noise-lfsr`, `snd-adpcm-loop-reload` (L2) | A 3-word one-shot keeps the busy bit; the noise channel follows the GBATEK shift register; an ADPCM loop started in the data part repeats exactly | DeSmuME |
| Pendulane `snd-pcm-busy`, `snd-adpcm-busy`, `snd-hold-restart` (L3) | Busy clears 18 samples after a 16-sample PCM16 start and 42 after a 32-sample ADPCM start; restarting a Hold channel gives one old sample, 2 zero samples, then the new data (258) | melonDS (19, 43, 7), DeSmuME |
| Pendulane `snd-onset-pcm-psg`, `spi-512k-minus-4m` (L3) | PCM16 sounds 2 samples after PSG; a firmware SPI byte at 512 kHz takes 448 ARM7 cycles more than at 4 MHz | DeSmuME |
| Heddlemere `gx-identity-slope`, `gx-pushpop-slope`, `gx-trans-mode2-extra`, `gx-scale-mode2-extra`, `gx-full-stall-rate`, `gx-halfirq-slope` (L3) | Geometry command cycle costs and FIFO stall as GBATEK gives them | DeSmuME |
| Heddlemere `vcount-write-lines` (L2) | A VCOUNT write at line 212 repeats 10 or 11 lines | DeSmuME |
| Heddlemere `hblank-low-arm9`, `swap-tail` (L3) | The HBlank flag is low for 1606 cycles of a line on the ARM9 (25710 x16); the engine is idle 392 cycles after the VBlank that follows a bare SWAP_BUFFERS (6256 = 391) | melonDS (25343, 5224), DeSmuME (25632, 528) |
| Buoyane `aa-opaque-dot-hidden` (L2) | With anti-aliasing on, an opaque 1-dot polygon is hidden (`values[1]` 0x7ff) | all four: melonDS software, GL Classic, GL Compute, DeSmuME |
| Buoyane `edge-aa-translucent` (L2) | With anti-aliasing a marked edge is neither the edge colour nor the face colour | melonDS GL Classic, GL Compute, DeSmuME |
| Buoyane `aa-marked-line-complete` (L2) | With anti-aliasing and edge marking a one-pixel line has no gap (192 of 192 pixels) | melonDS GL Classic, DeSmuME |

Items with `env` (they need the test harness, such as save chip access or the cartridge gap) are n/a on the R4 and are
not listed: Tonewick `save-wel-reset`, Pendulane `cart-gap1-clk5` and `cart-gap1-clk8`.

## 3. Not judged, but different

| What | Console | melonDS 1.1 | Where |
|---|---|---|---|
| RDLINES_COUNT (`rdl=` on the QA checkpoints) in games that draw no 3D | 63 | 46 | Ledgerine, Tonewick, Pendulane |
| RDLINES_COUNT in busy 3D scenes | drops to 37..43 in some checkpoints, never underflows | 46 everywhere | Elevane, Hushwell |
| Visible-line HBlank IRQ count over a span | 31464 (Corvane), 31520 / 31465 (Lightenette) | 31323, 31532 | inside the judged ranges |
| Pendulane raw timing edges `values[24..31]`, `[36..39]` | 2-3 higher | | not judged |
| Pendulane `snd-rate-slope` (informational) | 13, -10, -33 in three runs: it moves from run to run | 830 | stays `unconfirmed` |
| Buoyane `fog-alpha-glitch` (formerly L2 `unconfirmed`, removed) | 0: a fog face in front of FOG_OFFSET looks like a same-frame alpha 31 face without fog in none of the pixels read (one run, one scene) | 0 | to become a probe item (follow-up) |

## Adding an entry

After a hardware run (skill `nds-hw-qa`), add a row here and in [HW-FINDINGS-KO.md](HW-FINDINGS-KO.md) for every
item where the console disagreed with GBATEK, every gating item that now passes on the console and fails on an
emulator, and every difference that is not judged but worth knowing. Give the console value, the emulator values
and the status, and leave the evidence in the game's `docs/HW-QA.md`.
