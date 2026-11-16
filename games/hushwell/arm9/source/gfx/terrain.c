// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/hw.h"
#include "gfx/gl3d.h"
#include "gfx/terrain.h"

#define GX_PORT(cmd) (*(vu32 *)(0x04000400 + ((cmd) << 2)))
#define GXP_BEGIN    GX_PORT(GX_BEGIN)
#define GXP_END      GX_PORT(GX_END)
#define GXP_COLOR    GX_PORT(GX_COLOR)
#define GXP_TEXCOORD GX_PORT(GX_TEXCOORD)
#define GXP_VTX16    GX_PORT(GX_VTX16)

static inline void vtx(int32_t x, int32_t y, int32_t z)
{
    GXP_VTX16 = (u32)(x & 0xFFFF) | ((u32)(y & 0xFFFF) << 16);
    GXP_VTX16 = (u32)(z & 0xFFFF);
}

void bb_draw(CvVec pos, int32_t size, u16 color, int alpha, int poly_id, int tex_size)
{
    CvVec r = m_scale(g3_cam_right(), size);
    CvVec u = m_scale(g3_cam_up(), size);
    glPushMatrix();
    glTranslatef32(pos.x, pos.y, pos.z);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(poly_id));
    GXP_COLOR = color;
    GXP_BEGIN = GX_QUADS;
    GXP_TEXCOORD = 0;
    vtx(-r.x - u.x, -r.y - u.y, -r.z - u.z);
    GXP_TEXCOORD = (u32)(tex_size << 4);
    vtx(r.x - u.x, r.y - u.y, r.z - u.z);
    GXP_TEXCOORD = (u32)(tex_size << 4) | ((u32)(tex_size << 4) << 16);
    vtx(r.x + u.x, r.y + u.y, r.z + u.z);
    GXP_TEXCOORD = (u32)(tex_size << 4) << 16;
    vtx(-r.x + u.x, -r.y + u.y, -r.z + u.z);
    GXP_END = 0;
    glPopMatrix(1);
}

static void box_faces(int32_t h, int32_t top, int32_t bottom)
{
    // Six quads of an axis-aligned box.
    GXP_BEGIN = GX_QUADS;
    vtx(-h, bottom, -h); vtx(h, bottom, -h); vtx(h, top, -h); vtx(-h, top, -h);
    vtx(-h, bottom, h); vtx(-h, top, h); vtx(h, top, h); vtx(h, bottom, h);
    vtx(-h, bottom, -h); vtx(-h, top, -h); vtx(-h, top, h); vtx(-h, bottom, h);
    vtx(h, bottom, -h); vtx(h, bottom, h); vtx(h, top, h); vtx(h, top, -h);
    vtx(-h, top, -h); vtx(h, top, -h); vtx(h, top, h); vtx(-h, top, h);
    vtx(-h, bottom, -h); vtx(-h, bottom, h); vtx(h, bottom, h); vtx(h, bottom, -h);
    GXP_END = 0;
}

void shadow_volume(CvVec pos, int32_t half, int32_t floor_y, int id)
{
    int32_t top = 0;
    int32_t bottom = floor_y - pos.y - CV_FX(0.2);
    if (bottom < -CV_FX(7.5))
        bottom = -CV_FX(7.5);
    g3_texture_bind(TEX_COUNT);
    glPushMatrix();
    glTranslatef32(pos.x, pos.y, pos.z);
    GXP_COLOR = RGB15(0, 0, 0);
    // Mask pass: back faces with polygon ID 0 mark the stencil.
    glPolyFmt(POLY_SHADOW | POLY_CULL_FRONT | POLY_ALPHA(1) | POLY_ID(0));
    box_faces(half, top, bottom);
    // Shadow pass: front faces with a different ID darken the marked area.
    glPolyFmt(POLY_SHADOW | POLY_CULL_BACK | POLY_ALPHA(18) | POLY_ID(id));
    box_faces(half, top, bottom);
    glPopMatrix(1);
}
