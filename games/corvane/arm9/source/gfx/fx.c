// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/sys.h"
#include "gfx/fx.h"

#define BLDCNT(e)   (*(vu16 *)((e) == FX_SUB ? 0x04001050 : 0x04000050))
#define BLDALPHA(e) (*(vu16 *)((e) == FX_SUB ? 0x04001052 : 0x04000052))
#define BLDY(e)     (*(vu16 *)((e) == FX_SUB ? 0x04001054 : 0x04000054))
#define MOSAIC(e)   (*(vu16 *)((e) == FX_SUB ? 0x0400104C : 0x0400004C))

static int bright_level[2];

static u16 bright_value(int level)
{
    if (level == 0)
        return 0;
    if (level > 0)
        return (1 << 14) | (level > 16 ? 16 : level);
    return (2 << 14) | (-level > 16 ? 16 : -level);
}

void fx_bright(int engines, int level)
{
    if (engines & FX_MAIN)
    {
        REG_MASTER_BRIGHT = bright_value(level);
        bright_level[0] = level;
    }
    if (engines & FX_SUB)
    {
        REG_MASTER_BRIGHT_SUB = bright_value(level);
        bright_level[1] = level;
    }
}

void fx_fade_out(int engines, int frames, bool to_white)
{
    for (int f = 1; f <= frames; f++)
    {
        int lvl = 16 * f / frames;
        fx_bright(engines, to_white ? lvl : -lvl);
        sys_vsync();
    }
}

void fx_fade_in(int engines, int frames, bool from_white)
{
    for (int f = frames - 1; f >= 0; f--)
    {
        int lvl = 16 * f / frames;
        fx_bright(engines, from_white ? lvl : -lvl);
        sys_vsync();
    }
}

void fx_mosaic(int engine, int bg, int obj)
{
    MOSAIC(engine) = (bg & 15) | ((bg & 15) << 4) | ((obj & 15) << 8)
                   | ((obj & 15) << 12);
}

void fx_transition_out(int frames)
{
    // Mosaic needs the BG mosaic bit (bit 6 of BGxCNT) on visible layers.
    for (int i = 0; i < 4; i++)
    {
        ((vu16 *)0x04000008)[i] |= 1 << 6;
        ((vu16 *)0x04001008)[i] |= 1 << 6;
    }
    for (int f = 1; f <= frames; f++)
    {
        int m = 15 * f / frames;
        fx_mosaic(FX_MAIN, m, m);
        fx_mosaic(FX_SUB, m, m);
        fx_bright(FX_BOTH, -16 * f / frames);
        sys_vsync();
    }
    fx_mosaic(FX_MAIN, 0, 0);
    fx_mosaic(FX_SUB, 0, 0);
}

void fx_alpha(int engine, int first, int second, int eva, int evb)
{
    BLDCNT(engine) = (first & 63) | (1 << 6) | ((second & 63) << 8);
    BLDALPHA(engine) = (eva & 31) | ((evb & 31) << 8);
}

void fx_brighten(int engine, int targets, int evy)
{
    BLDCNT(engine) = (targets & 63) | (2 << 6);
    BLDY(engine) = evy & 31;
}

void fx_darken(int engine, int targets, int evy)
{
    BLDCNT(engine) = (targets & 63) | (3 << 6);
    BLDY(engine) = evy & 31;
}

void fx_blend_off(int engine)
{
    BLDCNT(engine) = 0;
}

void fx_win_rect(int win, int x0, int y0, int x1, int y1)
{
    vu16 *h = (vu16 *)(win ? 0x04000042 : 0x04000040);
    vu16 *v = (vu16 *)(win ? 0x04000046 : 0x04000044);
    x0 = x0 < 0 ? 0 : (x0 > 255 ? 255 : x0);
    x1 = x1 < 0 ? 0 : (x1 > 255 ? 255 : x1);
    y0 = y0 < 0 ? 0 : (y0 > 191 ? 191 : y0);
    y1 = y1 < 0 ? 0 : (y1 > 192 ? 192 : y1);
    *h = (x0 << 8) | (x1 & 0xFF);
    *v = (y0 << 8) | (y1 & 0xFF);
}
