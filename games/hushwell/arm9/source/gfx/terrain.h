// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Billboards and shadow volumes, submitted by writing the GXFIFO command
// ports directly (no display lists).

#ifndef CV_TERRAIN_H
#define CV_TERRAIN_H

#include <nds.h>

#include "core/cvmath.h"

// Camera-facing quad of half-size `size` with the bound texture.
void bb_draw(CvVec pos, int32_t size, u16 color, int alpha, int poly_id, int tex_size);
// Shadow volume (mask + shadow passes) below `pos` down to the floor. The
// shadow pass uses `id`: polygons with that ID (the caster) stay unshadowed.
void shadow_volume(CvVec pos, int32_t half, int32_t floor_y, int id);

#endif // CV_TERRAIN_H
