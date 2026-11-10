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

Memory / DMA / IPC
- The ARM9 stack is in DTCM, which DMA can't reach: DMA buffers are static or heap.
- `DC_FlushRange` before DMA/ARM7 reads RAM the CPU wrote; `DC_InvalidateRange`
  after DMA writes RAM.
- HBlank DMA: CPU writes line 0 in VBlank, DMA source starts at line 1; re-arm every VBlank.
- IPC order: ARM9 installs FIFO handlers, *then* writes IPCSYNC READY; the ARM7
  sends input packets only after it sees READY (an unread backlog livelocks the FIFO IRQ).
- ARM7 is linked with `__shared_wram_size=0` so shared WRAM can be handed over.
- NitroFS may give the slot-1 bus to the ARM7: wrap AUXSPI and raw cart access in
  `cart_bus_claim()/cart_bus_release()`.
- Save SPI: send the last byte of every command with HOLD cleared (chip select
  drops); every busy-wait needs a timeout so a missing chip never hangs boot.
- Bounds: fixed arrays (`MAX_PARTS`, `MAX_PARTICLES`…) must cover the largest
  user; an overflow here corrupted the stack once.

Determinism
- Gameplay randomness only through `rng_*`; never seed from timers or the RTC.
- Timer-driven gameplay events start from VBlank and are consumed at most once per frame.
