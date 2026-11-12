// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Channel plan:
//   0  immediate copies and the geometry-FIFO display lists
//   1  HBlank raster tables (wave / scroll), or main-memory display FIFO
//      (the intro and two demos; never together with a raster table)
//   2  VBlank OAM+palette upload
//   3  fills, cart transfers, line-start (display sync) effects
//
// The display FIFO stream must not share channel 2: the hardware stops it at
// every VBlank and the VBlank handler restarts it at once, so a dma_wait() on
// that channel (oam_commit -> dma_at_vblank) almost never sees it idle.

#ifndef CV_DMA_H
#define CV_DMA_H

#include <nds.h>

#define CV_DMA_CH_COPY   0
#define CV_DMA_CH_HBLANK 1
#define CV_DMA_CH_VBLANK 2
#define CV_DMA_CH_MISC   3
#define CV_DMA_CH_MMEM   CV_DMA_CH_HBLANK

void dma_wait(int ch);
void dma_stop(int ch);

// Immediate copy (32-bit units). Flushes the source from the data cache.
void dma_copy(const void *src, void *dst, u32 bytes);
void dma_copy16(const void *src, void *dst, u32 bytes);

// DMA fill from the channel's fill register (fast VRAM / RAM clears).
void dma_fill(u32 value, void *dst, u32 bytes);

// One-shot copy that the hardware starts at the next VBlank.
void dma_at_vblank(const void *src, void *dst, u32 bytes);

// HBlank repeat: every visible line copies `units` 16-bit values from the
// next slice of `table` into `reg` (reloaded each line).
// Call dma_hblank_rearm() from the VBlank handler each frame.
void dma_hblank_start(const u16 *table, volatile void *reg, int units);
void dma_hblank_rearm(void);
void dma_hblank_stop(void);

// Display-list submission through the geometry-FIFO start mode.
void dma_gxfifo(const u32 *list, u32 words);

// Main memory display: feeds 256x192 16bpp pixels to the display FIFO.
void dma_mmem_start(const u16 *frame);
void dma_mmem_stop(void);

// Line-start (display sync) repeat: one 16-bit value per visible line from
// `table` into `reg`, e.g. a backdrop colour per scanline.
void dma_line_start(const u16 *table, volatile u16 *reg);
void dma_line_rearm(void);
void dma_line_stop(void);

#endif // CV_DMA_H
