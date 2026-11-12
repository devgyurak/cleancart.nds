// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/hw.h"
#include "gfx/dual.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/vram.h"

DualProbe dual_probe;
static bool probing;

static unsigned calls;      // calls so far; the view of call c is (c & 1)
static int map_top, map_bottom;

// Engine B shows bank C as a 16bpp bitmap BG and bank D as twelve 64x64 bitmap OBJs.
static void sub_bitmap_setup(void)
{
    REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_2D_BMP_256;
    REG_BG3CNT_SUB = 3 | (1 << 7) | (1 << 2) | (1 << 14);   // 16bpp 256x256 at 0
    REG_BG3PA_SUB = 256;
    REG_BG3PB_SUB = 0;
    REG_BG3PC_SUB = 0;
    REG_BG3PD_SUB = 256;
    REG_BG3X_SUB = 0;
    REG_BG3Y_SUB = 0;
    oam_clear(ENG_SUB);
    for (int i = 0; i < 12; i++)
    {
        int x = (i % 4) * 64, y = (i / 4) * 64;
        oam_add(ENG_SUB, x, y, SPR_64x64, (x / 8) + (y / 8) * 32, 15, 0, SPRF_BITMAP, 0);
    }
}

void dual_begin(int top_map, int bottom_map)
{
    map_top = top_map;
    map_bottom = bottom_map;
    calls = 0;
    dual_probe.front_d = dual_probe.cross_d = dual_probe.plan_c = dual_probe.cross_c = 0;
    vram_layout(VL_DUAL3D);
    REG_DISPCAPCNT = 0;
    sub_bitmap_setup();
    oam_flush_now();
    // The main engine shows the first view's configuration already (a fade-in may
    // run before the first call).
    REG_BG0CNT = 1;
    REG_BG1CNT = map_top << 8;
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE;
    lcdMainOnTop();
}

// Routing of the frame the next VBlank shows (view `top`).
static void route(bool top)
{
    REG_BG0CNT = 1;                                         // 3D under the text
    REG_BG1CNT = 0 | ((top ? map_top : map_bottom) << 8);   // tiles at 0
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE;
    if (top)
    {
        lcdMainOnTop();
        vram_set(VB_D, 0, 0);   // LCDC: capture target
        vram_set(VB_C, 4, 0);   // engine B shows the last bottom frame
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(3) | CV_CAP_SIZE_256x192;   // source A: whole output
    }
    else
    {
        lcdMainOnBottom();
        vram_set(VB_C, 0, 0);
        vram_set(VB_D, 4, 0);   // engine B shows the last top frame
        REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(2) | CV_CAP_SIZE_256x192;
    }
}

void dual_probe_enable(bool on)
{
    probing = on;
}

// Every 8th line only: the read has to end inside the VBlank (a full bank takes a frame).
static u32 count_clear(const vu16 *bank, u16 colour)
{
    u32 n = 0;
    for (int y = 0; y < 192; y += 8)
        for (int x = 0; x < 256; x++)
            n += (bank[y * 256 + x] & 0x7FFF) == colour;
    return n;
}

bool dual_begin_call(void)
{
    if (calls > 0)
    {
        bool top = ((calls - 1) & 1) == 0;
        route(top);
        if (probing)
        {
            // This frame's capture target is the bank in LCDC mode: it still holds the
            // previous capture of the same kind.
            if (top)
            {
                const vu16 *d = (const vu16 *)0x06860000;
                dual_probe.front_d = count_clear(d, DUAL_FRONT_CLEAR);
                dual_probe.cross_d = count_clear(d, DUAL_PLAN_CLEAR);
            }
            else
            {
                const vu16 *c = (const vu16 *)0x06840000;
                dual_probe.plan_c = count_clear(c, DUAL_PLAN_CLEAR);
                dual_probe.cross_c = count_clear(c, DUAL_FRONT_CLEAR);
            }
        }
    }
    return (calls & 1) == 0;
}

void dual_end_call(void)
{
    g3_flush(false);
    calls++;
}

void dual_end(void)
{
    REG_DISPCAPCNT = 0;
    lcdMainOnTop();
}
