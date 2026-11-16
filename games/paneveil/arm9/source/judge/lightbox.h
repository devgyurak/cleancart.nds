// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The light box: five small still scenes drawn with identity matrices (a vertex maps straight to
// screen pixels), clear colour, vertex colours and texels all fixed RGB555 values, captured from
// the 3D layer into bank D. Each judged item counts the samples that have the colour GBATEK says.
// Samples lie at texel centres and at least 3 pixels inside a polygon, so the rasteriser's edge
// rules never decide an item. A capture that did not finish gives LB_BAD in its items and leaves
// its bit clear in `done`.

#ifndef PV_LIGHTBOX_H
#define PV_LIGHTBOX_H

#include <nds.h>

#define LB_BAD 0xFFFFu

enum {
    LB_VTX_FORMATS,    // of 6 quads in the six vertex formats, those drawn where they belong (6)
    LB_CULL_DEFAULT,   // of 8 (4 front quads shown, 4 back quads hidden), the right ones (8)
    LB_BACK_RENDER,    // of 4 (2 back quads shown with the back bit, 2 front quads hidden by it) (4)
    LB_ALPHA_HALF,     // of 16 samples of an alpha-16 quad, those within 3 levels of the half mix
    LB_WIRE_HOLLOW,    // of 4 wireframe quads, those whose inside shows what lies behind (4)
    LB_WIRE_EDGES,     // edge-coloured pixels found in 7-pixel bands across the quads' top edges
    LB_DEPTH_EQUAL,    // of 4 coplanar pairs drawn with depth test Equal, those where the second shows (4)
    LB_DEPTH_LESS,     // of 4 coplanar pairs drawn with depth test Less, those where the second is hidden (4)
    LB_TEX_REPEAT,     // of 8 samples in repeated texture, right texels (8)
    LB_TEX_FLIP,       // of 8 samples in flipped repeats, right texels (8)
    LB_TEX_CLAMP,      // of 4 samples in the clamped part, texels of the last column (4)
    LB_TEX_C0_HIDDEN,  // of 4 colour-0 texels, those showing what lies behind (4)
    LB_TEX_C0_SHOWN,   // of 4 colour-0 texels, those showing palette entry 0 (4)
    LB_TEX_PLTT_BASE,  // of 4 texels, those with the colour of the palette set chosen by PLTT_BASE (4)
    LB_TEX_COORD1,     // of 4 texels, those transposed by the texture matrix in mode 1 (4)
    LB_ITEMS
};

#define LB_SCENES 5

typedef struct {
    u32 v[LB_ITEMS];
    u32 done;          // bit n: capture n finished
} LightboxResult;

// Takes over both engines, draws and captures the five scenes, leaves the 3D engine idle.
void lightbox_run(LightboxResult *r);

#endif // PV_LIGHTBOX_H
