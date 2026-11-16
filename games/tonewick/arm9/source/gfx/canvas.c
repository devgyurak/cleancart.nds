// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "gfx/canvas.h"

#define BMP_BASE 4   // 16 KB units: 0x06010000, above the font and the map (as the title)

void canvas_clear(Canvas *c, u8 color)
{
    memset(c->px, color, sizeof(c->px));
}

void canvas_rect(Canvas *c, int x0, int y0, int x1, int y1, u8 color)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > CANVAS_W) x1 = CANVAS_W;
    if (y1 > CANVAS_H) y1 = CANVAS_H;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            c->px[y * CANVAS_W + x] = color;
}

void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, u8 color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int n = dx > dy ? dx : dy;
    if (n == 0)
        n = 1;
    for (int i = 0; i <= n; i++)
    {
        int x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n;
        if (x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H)
            c->px[y * CANVAS_W + x] = color;
    }
}

void canvas_disc(Canvas *c, int cx, int cy, int r, u8 color)
{
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r && x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H)
                c->px[y * CANVAS_W + x] = color;
}

void canvas_ring(Canvas *c, int cx, int cy, int r, u8 color)
{
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
        {
            int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d <= r * r && d > (r - 1) * (r - 1) && x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H)
                c->px[y * CANVAS_W + x] = color;
        }
}

void canvas_attach(bool main, const u16 *palette, int entries)
{
    u16 *pal = main ? CV_PAL_BG_MAIN : CV_PAL_BG_SUB;
    swiCopy(palette, pal + CANVAS_PAL_FIRST, entries | COPY_MODE_HWORD);
    if (main)
    {
        REG_BG3CNT = BG_BMP8_256x256 | BG_BMP_BASE(BMP_BASE) | 1;   // behind the text (priority 0)
        REG_BG3PA = 256; REG_BG3PB = 0; REG_BG3PC = 0; REG_BG3PD = 256;
        REG_BG3X = 0; REG_BG3Y = 0;
        REG_DISPCNT = MODE_5_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG3_ACTIVE;
    }
    else
    {
        REG_BG3CNT_SUB = BG_BMP8_256x256 | BG_BMP_BASE(BMP_BASE) | 1;
        REG_BG3PA_SUB = 256; REG_BG3PB_SUB = 0; REG_BG3PC_SUB = 0; REG_BG3PD_SUB = 256;
        REG_BG3X_SUB = 0; REG_BG3Y_SUB = 0;
        REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG3_ACTIVE;
    }
}

void canvas_present(bool main, const Canvas *c)
{
    u8 *bg = (u8 *)(main ? 0x06000000 : 0x06200000) + BMP_BASE * 0x4000;
    // VRAM takes no byte writes: copy in words.
    swiCopy(c->px, bg, (sizeof(c->px) / 4) | COPY_MODE_WORD);
}
