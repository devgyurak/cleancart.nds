// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Terrain and corridor geometry, submitted by writing the GXFIFO command
// ports directly (no display lists), plus billboards and shadow volumes.

#ifndef CV_TERRAIN_H
#define CV_TERRAIN_H

#include <nds.h>

#include "core/cvmath.h"

#define TERRAIN_COLS 17
#define TERRAIN_CELL CV_FX(0.875)   // x spacing; keeps vertices inside 4.12

typedef struct {
    const u8 *heights;   // rows * 17
    int rows;
    bool corridor;
    u16 low_color, high_color;
} CvTerrain;

void terrain_set(const CvTerrain *t);
// Height (20.12) of the ground at world x/z, bilinear.
int32_t terrain_height(int32_t x, int32_t z);
// Corridor half width at world z.
int32_t terrain_corridor_half(int32_t z);
void terrain_draw(int32_t cam_z, int visible_rows, bool fog);

// Camera-facing quad of half-size `size` with the bound texture.
void bb_draw(CvVec pos, int32_t size, u16 color, int alpha, int poly_id, int tex_size);
// Shadow volume (mask + shadow passes) below `pos` down to the floor.
void shadow_volume(CvVec pos, int32_t half, int32_t floor_y);

#endif // CV_TERRAIN_H
