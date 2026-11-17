// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/hw.h"
#include "gfx/vram.h"

static CvVramLayout current = VL_MENU;

void vram_set(int bank, int mst, int ofs)
{
    CV_VRAMCNT(bank) = CV_VRAM_ON | (mst & 7) | ((ofs & 3) << 3);
}

void vram_off(int bank)
{
    CV_VRAMCNT(bank) = 0;
}

// MST values per bank (GBATek VRAMCNT):
//  A-D : 0 LCDC, 1 main BG, 2 main OBJ (A,B) / sub BG (C) / sub OBJ (D) via 4,
//        3 texture slot
//  E   : 0 LCDC, 1 main BG, 2 main OBJ, 3 texture palette, 4 main BG ext pal
//  F,G : 0 LCDC, 1 main BG, 2 main OBJ, 3 texture palette, 4 main BG ext pal,
//        5 main OBJ ext pal
//  H   : 0 LCDC, 1 sub BG, 2 sub BG ext pal
//  I   : 0 LCDC, 1 sub BG, 2 sub OBJ, 3 sub OBJ ext pal

static void sub_engine_cd(void)
{
    vram_set(VB_C, 4, 0);   // sub BG 0x06200000
    vram_set(VB_D, 4, 0);   // sub OBJ 0x06600000
    vram_set(VB_H, 2, 0);   // sub BG ext palettes
    vram_set(VB_I, 3, 0);   // sub OBJ ext palettes
}

static void main_3d(void)
{
    vram_set(VB_A, 3, 0);   // texture slot 0
    vram_set(VB_B, 3, 1);   // texture slot 1 (4x4 palette indices)
    vram_set(VB_E, 1, 0);   // main BG 0x06000000 (64 KB)
    vram_set(VB_F, 2, 0);   // main OBJ 0x06400000 (16 KB)
    vram_set(VB_G, 3, 0);   // texture palette slot 0
    sub_engine_cd();
}

void vram_layout(CvVramLayout layout)
{
    current = layout;
    switch (layout)
    {
        case VL_MENU:
            vram_set(VB_A, 1, 0);   // main BG 0x06000000
            vram_set(VB_B, 1, 1);   // main BG 0x06020000
            vram_set(VB_E, 2, 0);   // main OBJ
            vram_set(VB_F, 4, 0);   // main BG ext palette slots 0-1
            vram_set(VB_G, 5, 0);   // main OBJ ext palette
            sub_engine_cd();
            break;
        case VL_TERRAIN:
        case VL_SKY:
        case VL_CORRIDOR:
            main_3d();
            break;
        case VL_CORE:
            // C/D hold the rear-plane (clear) colour and depth images as
            // texture slots 2/3; they start in LCDC so the CPU can draw
            // them. Engine B moves to H/I.
            vram_set(VB_A, 3, 0);
            vram_set(VB_B, 3, 1);
            vram_set(VB_C, 0, 0);
            vram_set(VB_D, 0, 0);
            vram_set(VB_E, 1, 0);
            vram_set(VB_F, 2, 0);
            vram_set(VB_G, 3, 0);
            vram_set(VB_H, 1, 0);   // sub BG (32 KB)
            vram_set(VB_I, 2, 0);   // sub OBJ (16 KB)
            break;
        case VL_SPACE:
            // 1024x512 large bitmap uses A-C (rows 0-383); D is the
            // display-capture buffer for the boost afterimage.
            vram_set(VB_A, 1, 0);
            vram_set(VB_B, 1, 1);
            vram_set(VB_C, 1, 2);
            vram_set(VB_D, 0, 0);   // LCDC
            vram_set(VB_E, 2, 0);   // main OBJ (meteors, bullets)
            vram_set(VB_F, 5, 0);   // main OBJ ext palette (meteor colours)
            vram_set(VB_G, 3, 0);   // texture palette (unused textures off)
            vram_set(VB_H, 1, 0);   // sub BG (32 KB)
            vram_set(VB_I, 2, 0);   // sub OBJ (16 KB)
            break;
        case VL_DUAL3D:
            vram_set(VB_A, 3, 0);
            vram_set(VB_B, 3, 1);
            vram_set(VB_C, 0, 0);   // alternates LCDC / sub BG each frame
            vram_set(VB_D, 0, 0);   // alternates LCDC / sub OBJ each frame
            vram_set(VB_E, 1, 0);
            vram_set(VB_F, 2, 0);
            vram_set(VB_G, 3, 0);
            vram_off(VB_H);
            vram_off(VB_I);
            break;
        case VL_JUDGE:
            main_3d();
            vram_set(VB_D, 0, 0);   // LCDC: display-capture target
            break;
        case VL_MMEM:
            vram_set(VB_A, 0, 0);   // LCDC (boot logo framebuffer)
            vram_off(VB_B);
            vram_set(VB_E, 2, 0);
            sub_engine_cd();
            break;
    }
}

CvVramLayout vram_current(void)
{
    return current;
}
