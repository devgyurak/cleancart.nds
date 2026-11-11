---
paths:
  - "**/arm9/**/*.{c,h}"
  - "**/arm7/**/*.{c,h}"
  - "**/common/**/*.h"
---

# DS C code rules

Style: C only, 4-space indent, braces on their own line, `snake_case` with a
module prefix (`sys_`, `g3_`, `hud_`…), `Cv`-prefixed types, `CV_` macros.
Every file starts with the SPDX header. Match the density of comments around you:
explain *why* a register is set, not what the code says.

## Hardware pitfalls (each one cost a bug here)

Display / 2D
- DS BG modes differ from the GBA: **mode 1 makes BG3 affine, BG2 is text**. Use
  mode 2 (BG2+BG3 affine) or mode 4 (BG2 affine, BG3 extended). Mode 5: BG2/BG3
  extended (bitmaps). Mode 6: BG2 large bitmap 512 KB (banks A–D).
- Affine BG size bits: 0 = 128x128. A 256x256 affine BG needs `(1 << 14)`.
- VRAM ignores 8-bit writes: no byte `memset`/`memmove` into VRAM; use u16/u32
  loops, `swiCopy`/`swiFastCopy`, or DMA.
- Display capture writes only to a bank mapped as LCDC; VRAM display mode shows it.
- Extended palettes are written while their bank is LCDC, then remapped.
- Scene transitions leave master brightness faded: every scene must set it back.
- Many I/O registers are write-only (BGxPA–PD, MOSAIC, …): they read 0 on hardware
  and in melonDS (DeSmuME returns the written value). Keep a copy of what you wrote.

3D
- `glClearColor`/`glFogColor` take **5-bit** components; `glColor3b` takes 8-bit.
  libnds `sassert`s are live: out-of-range values stop the game on an assert screen.
- Front face is counter-clockwise on screen (GL convention, verified).
- Translucent polygons with the same polygon ID don't blend over each other:
  vary IDs per particle. `POLY_ALPHA(0)` means wireframe, not invisible.
- Edge-marking colour comes from `polyID >> 3`.
- The libnds texture allocator spans banks A–D. If a bank is reserved for
  something else (stage 5 rear-plane image in C/D), load fewer textures.
- Anything between the camera and the player fills the screen after near-plane
  clipping: despawn objects once they pass the player.
- Polygon/vertex RAM counters count the list being built and restart at the buffer
  swap: after flush + vsync they read 0. Read them just before the flush, after
  waiting for GXSTAT bit 27 (geometry busy) to clear so the FIFO has drained
  (otherwise the last commands are missing; a lone quad read 0). Corvane
  `g3_flush_measured()`, demos only: the game keeps `g3_flush()` because the wait
  can make a heavy frame miss VBlank.
- Polygons clipped or culled before polygon RAM don't count: a stress test that
  must hit the 2048-polygon limit keeps its geometry on screen.

Memory / DMA / IPC
- The ARM9 stack is in DTCM, which DMA can't reach: DMA buffers are static or heap.
- `DC_FlushRange` before DMA/ARM7 reads RAM the CPU wrote; `DC_InvalidateRange`
  after DMA writes RAM.
- HBlank DMA: CPU writes line 0 in VBlank, DMA source starts at line 1; re-arm every VBlank.
- Main-memory display FIFO DMA is stopped by hardware at every VBlank and must be
  restarted from the VBlank IRQ, so a `dma_wait()` on its channel almost never sees
  it idle (a hang in melonDS). Give it a channel no other code waits on (corvane:
  channel 1, `CV_DMA_CH_MMEM`, never with an HBlank table); never `dma_wait()` a
  channel a repeating DMA may be using. The same goes for the line-start DMA on
  channel 3 (corvane: the intro and the sys-dma-disp/2d-display demos): stop it
  before `dma_fill()`/cart reads use that channel.
- MPU: the BlocksDS crt0 maps no MPU region over shared WRAM (0x03000000) in DS mode,
  so any ARM9 access there is a data abort (black-screen freeze in melonDS and on
  hardware; DeSmuME has no MPU). While the ARM9 owns the WRAM (WRAMCNT=0), point
  region 2 (the 4 KB null guard, data perm 0) at 0x03000000/16 MB with data RW
  (perm nibble 2 = 3; the CP15 c5,c0,2 write needs ARM state, and libnds' setter
  takes no argument), uncached and unbuffered, and restore both before WRAMCNT
  hands the WRAM back. Don't borrow region 3: NitroFS and slot-2 DLDI read slot-2.
  Corvane: `mpu_map_swram()` in `core/sys.c`.
- IPC order: ARM9 installs FIFO handlers, *then* writes IPCSYNC READY; the ARM7
  sends input packets only after it sees READY (an unread backlog livelocks the FIFO IRQ).
- ARM7 is linked with `__shared_wram_size=0` so shared WRAM can be handed over.
- NitroFS may give the slot-1 bus to the ARM7: wrap AUXSPI and raw cart access in
  `cart_bus_claim()/cart_bus_release()`.
- Save SPI: send the last byte of every command with HOLD cleared (chip select
  drops); every busy-wait needs a timeout so a missing chip never hangs boot.
- Save chip presence: some EEPROMs read status 0xFF during their internal write, so
  `wait_ready()` must not treat 0xFF as "no chip"; decide that once at detection
  (open bus) and clear the chip-ok flag there.
- melonDS attaches no save chip to game code `####` (see `build-and-verify.md`)
  and, for a code it doesn't know, always emulates an 8 KB EEPROM whose addresses
  wrap at 0x2000 (a real 8 KB EEPROM also has 32-byte write pages). Detect the wrap
  (slot A seen again at 0x2000, else flip/restore a free probe byte through the
  wrap) and fit the layout into 8 KB with 32-byte pages (corvane `core/save.c`).
- FLASH builds: check the JEDEC ID (0x9F). Without one (DeSmuME, melonDS's EEPROM)
  an EEPROM reads our 3-byte addresses as 2-byte ones (slots land at addr >> 8)
  and wraps at 0x2000: cap the replay so folded writes stay below the wrap
  (corvane: 7675 bytes, `FOLD_REPLAY_MAX`), and write and read the replay in one
  SPI command each: chunked page writes restart one byte after the previous chunk
  on the folded address and overwrite each other (hushwell lost every replay so).
- A save that doesn't fit must not fail silently: cut it to the capacity (or clear
  the old data). A refused replay left the previous run on the chip, and "replay
  last run" offered that older run (hushwell, 8 KB chip).
- Bounds: fixed arrays (`MAX_PARTS`, `MAX_PARTICLES`…) must cover the largest
  user; an overflow here corrupted the stack once.

Text / UI
- Licence texts are UTF-8 (accented names): don't draw them byte-wise. Fold what
  the font lacks (corvane: U+00C0–U+00FF to ASCII, bounds-checked, rest '?'), and
  keep the viewer's file list equal to every file shipped in `licenses/`.
- An on-screen hint ("START: …") needs a live handler in every sub-state it shows
  in (e.g. while text is still typing), not just in the final one. Watch for an
  earlier combined test (`A | START`) that swallows the key before its own check.

Determinism
- Gameplay randomness only through `rng_*`; never seed from timers or the RTC.
- Timer-driven gameplay events start from VBlank and are consumed at most once per frame.
