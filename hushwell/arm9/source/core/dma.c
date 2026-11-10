// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Every ARM9 DMA start mode, programmed through the channel registers.

#include <nds.h>

#include "core/dma.h"
#include "core/hw.h"

#define SRC(n)  (*(vu32 *)(0x040000B0 + (n) * 12))
#define DST(n)  (*(vu32 *)(0x040000B4 + (n) * 12))
#define CNT(n)  (*(vu32 *)(0x040000B8 + (n) * 12))
#define FILL(n) (*(vu32 *)(0x040000E0 + (n) * 4))

static const u16 *hb_table;
static volatile void *hb_reg;
static int hb_units;

static const u16 *line_table;
static volatile u16 *line_reg;

static const u16 *mmem_frame;

void dma_wait(int ch)
{
    while (CNT(ch) & CV_DMA_ON)
        ;
}

void dma_stop(int ch)
{
    CNT(ch) = 0;
    // A repeating channel may still be latched; read back to settle.
    (void)CNT(ch);
}

void dma_copy(const void *src, void *dst, u32 bytes)
{
    DC_FlushRange(src, bytes);
    dma_wait(CV_DMA_CH_COPY);
    SRC(CV_DMA_CH_COPY) = (u32)src;
    DST(CV_DMA_CH_COPY) = (u32)dst;
    CNT(CV_DMA_CH_COPY) = CV_DMA_ON | CV_DMA_NOW | CV_DMA_32 | (bytes >> 2);
    dma_wait(CV_DMA_CH_COPY);
}

void dma_copy16(const void *src, void *dst, u32 bytes)
{
    DC_FlushRange(src, bytes);
    dma_wait(CV_DMA_CH_COPY);
    SRC(CV_DMA_CH_COPY) = (u32)src;
    DST(CV_DMA_CH_COPY) = (u32)dst;
    CNT(CV_DMA_CH_COPY) = CV_DMA_ON | CV_DMA_NOW | CV_DMA_16 | (bytes >> 1);
    dma_wait(CV_DMA_CH_COPY);
}

void dma_fill(u32 value, void *dst, u32 bytes)
{
    const int ch = CV_DMA_CH_MISC;
    dma_wait(ch);
    FILL(ch) = value;
    SRC(ch) = (u32)&FILL(ch);
    DST(ch) = (u32)dst;
    CNT(ch) = CV_DMA_ON | CV_DMA_NOW | CV_DMA_32 | CV_DMA_SRC_FIX | (bytes >> 2);
    dma_wait(ch);
    if ((u32)dst < 0x03000000)
        DC_InvalidateRange(dst, bytes);
}

void dma_at_vblank(const void *src, void *dst, u32 bytes)
{
    const int ch = CV_DMA_CH_VBLANK;
    DC_FlushRange(src, bytes);
    dma_wait(ch);
    SRC(ch) = (u32)src;
    DST(ch) = (u32)dst;
    CNT(ch) = CV_DMA_ON | CV_DMA_VBLANK | CV_DMA_32 | (bytes >> 2);
}

void dma_hblank_start(const u16 *table, volatile void *reg, int units)
{
    hb_table = table;
    hb_reg = reg;
    hb_units = units;
    dma_hblank_rearm();
}

void dma_hblank_rearm(void)
{
    const int ch = CV_DMA_CH_HBLANK;
    if (!hb_table)
        return;
    CNT(ch) = 0;
    DC_FlushRange(hb_table, 192 * hb_units * 2);
    // Line 0 is set by the CPU during VBlank; HBlank N writes line N+1.
    volatile u16 *r = hb_reg;
    for (int i = 0; i < hb_units; i++)
        r[i] = hb_table[i];
    SRC(ch) = (u32)(hb_table + hb_units);
    DST(ch) = (u32)hb_reg;
    CNT(ch) = CV_DMA_ON | CV_DMA_HBLANK | CV_DMA_REPEAT | CV_DMA_16
            | CV_DMA_DST_RELOAD | (u32)hb_units;
}

void dma_hblank_stop(void)
{
    hb_table = NULL;
    dma_stop(CV_DMA_CH_HBLANK);
}

void dma_gxfifo(const u32 *list, u32 words)
{
    const int ch = CV_DMA_CH_COPY;
    DC_FlushRange(list, words * 4);
    dma_wait(ch);
    SRC(ch) = (u32)list;
    DST(ch) = 0x04000400;  // GXFIFO
    CNT(ch) = CV_DMA_ON | CV_DMA_GXFIFO | CV_DMA_32 | CV_DMA_DST_FIX | words;
    dma_wait(ch);
}

void dma_mmem_start(const u16 *frame)
{
    const int ch = CV_DMA_CH_VBLANK;
    mmem_frame = frame;
    DC_FlushRange(frame, 256 * 192 * 2);
    dma_stop(ch);
    SRC(ch) = (u32)frame;
    DST(ch) = 0x04000068;  // DISP_MMEM_FIFO
    // Four words per request; the display controller re-requests as it
    // drains, so the channel repeats for the whole frame.
    CNT(ch) = CV_DMA_ON | CV_DMA_MMEM | CV_DMA_REPEAT | CV_DMA_32
            | CV_DMA_DST_FIX | 4;
}

void dma_mmem_stop(void)
{
    mmem_frame = NULL;
    dma_stop(CV_DMA_CH_VBLANK);
}

void dma_line_start(const u16 *table, volatile u16 *reg)
{
    line_table = table;
    line_reg = reg;
    dma_line_rearm();
}

void dma_line_rearm(void)
{
    const int ch = CV_DMA_CH_MISC;
    if (!line_table)
        return;
    CNT(ch) = 0;
    DC_FlushRange(line_table, 192 * 2);
    SRC(ch) = (u32)line_table;
    DST(ch) = (u32)line_reg;
    CNT(ch) = CV_DMA_ON | CV_DMA_LINE | CV_DMA_REPEAT | CV_DMA_16
            | CV_DMA_DST_RELOAD | 1;
}

void dma_line_stop(void)
{
    line_table = NULL;
    dma_stop(CV_DMA_CH_MISC);
}
