// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// VRAM bank layouts. Each scene remaps banks A-I by writing VRAMCNT_x
// directly; the table below is the whole plan.
//
//            A        B        C        D        E       F       G        H       I
// MENU     BG0      BG1      subBG    subOBJ   mOBJ    BGext   OBJext   subBGx  subOBJx
// GAME     tex0     tex1     LCDC     LCDC     mBG     mOBJ    texpal   subBG   subOBJ
// ARCHIVE  tex0     mOBJ     LCDC     LCDC     mBG     OBJext  texpal   subBG   subOBJ
// TERRAIN  tex0     tex1     subBG    subOBJ   mBG     mOBJ    texpal   subBGx  subOBJx
// SKY      tex0     tex1     subBG    subOBJ   mBG     mOBJ    texpal   subBGx  subOBJx
// SPACE    BG0      BG1      BG2      LCDC     mOBJ    OBJext  texpal   subBG   subOBJ
// CORRIDOR tex0     tex1     subBG    subOBJ   mBG     mOBJ    texpal   subBGx  subOBJx
// CORE     tex0     tex1     clrImg   clrDep   mBG     mOBJ    texpal   subBG   subOBJ  (chapel)
// DUAL3D   tex0     tex1     LCDC/sBG LCDC/sOBJ mBG    mOBJ    texpal   -       -
// MMEM     LCDC     -        subBG    subOBJ   mOBJ    -       -        -       -

#ifndef CV_VRAM_H
#define CV_VRAM_H

typedef enum {
    VL_MENU,
    VL_GAME,
    VL_ARCHIVE,
    VL_TERRAIN,
    VL_SKY,
    VL_SPACE,
    VL_CORRIDOR,
    VL_CORE,
    VL_DUAL3D,
    VL_MMEM,
} CvVramLayout;

void vram_layout(CvVramLayout layout);
CvVramLayout vram_current(void);

// Bank indices for CV_VRAMCNT()
enum { VB_A, VB_B, VB_C, VB_D, VB_E, VB_F, VB_G, VB_WRAM, VB_H, VB_I };

void vram_set(int bank, int mst, int ofs);
void vram_off(int bank);

#endif // CV_VRAM_H
