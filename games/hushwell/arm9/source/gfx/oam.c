// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <string.h>

#include "core/dma.h"
#include "gfx/oam.h"

static u16 shadow[2][512] __attribute__((aligned(32)));
static int used[2];

static const u8 shape_of[] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2 };
static const u8 size_of[]  = { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 };

void oam_clear(int engine)
{
    u16 *s = shadow[engine];
    for (int i = 0; i < 128; i++)
    {
        s[i * 4 + 0] = 1 << 9;  // disabled (rotation off + "double" = hide)
        s[i * 4 + 1] = 0;
        s[i * 4 + 2] = 0;
    }
    used[engine] = 0;
}

int oam_add(int engine, int x, int y, int size, int tile, int pal, int prio,
            u32 flags, int affine_slot)
{
    if (used[engine] >= 128)
        return -1;
    if (x < -64 || x >= 256 || y < -64 || y >= 192)
        return -1;
    int i = used[engine]++;
    u16 *a = &shadow[engine][i * 4];

    u16 a0 = (y & 0xFF) | (shape_of[size] << 14);
    u16 a1 = (x & 0x1FF) | (size_of[size] << 14);
    if (flags & SPRF_AFFINE)
    {
        a0 |= 1 << 8;
        if (flags & SPRF_DOUBLE)
            a0 |= 1 << 9;
        a1 |= (affine_slot & 31) << 9;
    }
    else
    {
        if (flags & SPRF_HFLIP)
            a1 |= 1 << 12;
        if (flags & SPRF_VFLIP)
            a1 |= 1 << 13;
    }
    if (flags & SPRF_BLEND)
        a0 |= 1 << 10;
    else if (flags & SPRF_WINDOW)
        a0 |= 2 << 10;
    else if (flags & SPRF_BITMAP)
        a0 |= 3 << 10;
    if (flags & SPRF_MOSAIC)
        a0 |= 1 << 12;
    if (flags & SPRF_8BPP)
        a0 |= 1 << 13;

    a[0] = a0;
    a[1] = a1;
    // Bitmap OBJs use attr2 bits 12-15 as alpha (15 = opaque).
    a[2] = (tile & 0x3FF) | ((prio & 3) << 10) | ((pal & 15) << 12);
    return i;
}

void oam_affine(int engine, int slot, int angle, int32_t sx, int32_t sy)
{
    // sx/sy are 8.8 scale factors (256 = 1:1). The matrix maps screen to
    // texture space, so it is the inverse of the visible scale.
    int32_t s = sinLerp(angle) >> 4;   // 12 -> 8 fractional bits
    int32_t c = cosLerp(angle) >> 4;
    int32_t isx = (256 * 256) / (sx ? sx : 1);
    int32_t isy = (256 * 256) / (sy ? sy : 1);
    u16 *m = &shadow[engine][slot * 16 + 3];
    m[0]  = (u16)((c * isx) >> 8);
    m[4]  = (u16)((-s * isx) >> 8);
    m[8]  = (u16)((s * isy) >> 8);
    m[12] = (u16)((c * isy) >> 8);
}

int oam_count(int engine)
{
    return used[engine];
}

void oam_commit(void)
{
    dma_at_vblank(shadow, (void *)0x07000000, sizeof(shadow));
}

void oam_flush_now(void)
{
    dma_copy(shadow, (void *)0x07000000, sizeof(shadow));
}
