// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>

#include "core/hw.h"
#include "gfx/gl3d.h"
#include "gfx/terrain.h"

#define GX_PORT(cmd) (*(vu32 *)(0x04000400 + ((cmd) << 2)))
#define GXP_BEGIN    GX_PORT(GX_BEGIN)
#define GXP_END      GX_PORT(GX_END)
#define GXP_COLOR    GX_PORT(GX_COLOR)
#define GXP_NORMAL   GX_PORT(GX_NORMAL)
#define GXP_TEXCOORD GX_PORT(GX_TEXCOORD)
#define GXP_VTX16    GX_PORT(GX_VTX16)
#define GXP_DIF_AMB  GX_PORT(GX_DIF_AMB)
#define GXP_SPE_EMI  GX_PORT(GX_SPE_EMI)

static CvTerrain terr;

void terrain_set(const CvTerrain *t)
{
    terr = *t;
}

static inline int hval(int r, int c)
{
    if (r < 0)
        r = 0;
    if (r >= terr.rows)
        r = terr.rows - 1;
    if (c < 0)
        c = 0;
    if (c >= TERRAIN_COLS)
        c = TERRAIN_COLS - 1;
    return terr.heights[r * TERRAIN_COLS + c];
}

int32_t terrain_height(int32_t x, int32_t z)
{
    if (!terr.heights || terr.corridor)
        return 0;
    // Grid coordinates in 20.12.
    int32_t gx = m_fdiv(x, TERRAIN_CELL) + CV_FXI(8);
    int32_t gz = -z;
    int c = gx >> 12, r = gz >> 12;
    int32_t fx = gx & 0xFFF, fz = gz & 0xFFF;
    int32_t h00 = hval(r, c), h01 = hval(r, c + 1);
    int32_t h10 = hval(r + 1, c), h11 = hval(r + 1, c + 1);
    int32_t a = h00 * 4096 + (h01 - h00) * fx;
    int32_t b = h10 * 4096 + (h11 - h10) * fx;
    int32_t h = a + (int32_t)(((int64_t)(b - a) * fz) >> 12);
    return h / 32;  // heights are 1/32 unit
}

int32_t terrain_corridor_half(int32_t z)
{
    if (!terr.heights)
        return CV_FXI(4);
    int r = (-z) >> 12;
    return CV_FXI(hval(r, 0)) / 32;
}

static u16 lerp_color(u16 a, u16 b, int t)  // t 0..31
{
    int r = (a & 31) + (((b & 31) - (a & 31)) * t >> 5);
    int g = ((a >> 5) & 31) + ((((b >> 5) & 31) - ((a >> 5) & 31)) * t >> 5);
    int bl = ((a >> 10) & 31) + ((((b >> 10) & 31) - ((a >> 10) & 31)) * t >> 5);
    return r | (g << 5) | (bl << 10);
}

static inline void vtx(int32_t x, int32_t y, int32_t z)
{
    GXP_VTX16 = (u32)(x & 0xFFFF) | ((u32)(y & 0xFFFF) << 16);
    GXP_VTX16 = (u32)(z & 0xFFFF);
}

static void draw_heightfield(int r0, int r1, bool fog)
{
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1) | (fog ? POLY_FOG : 0));
    for (int r = r0; r < r1; r++)
    {
        if (r < 0 || r + 1 >= terr.rows)
            continue;
        glPushMatrix();
        glTranslatef32(0, 0, -CV_FXI(r));
        GXP_BEGIN = 3;  // quad strip
        for (int c = 0; c < TERRAIN_COLS; c++)
        {
            int32_t x = (c - 8) * TERRAIN_CELL;
            for (int k = 0; k < 2; k++)
            {
                int h = hval(r + k, c);
                int slope = h - hval(r + k, c - 1);
                int t = (h >> 2) + 8 + (slope >> 1);
                t = t < 0 ? 0 : (t > 31 ? 31 : t);
                GXP_COLOR = lerp_color(terr.low_color, terr.high_color, t);
                // Texture repeats every 8 rows; keep t inside 12.4 range.
                GXP_TEXCOORD = (((u32)(c * 16) << 4) & 0xFFFF)
                             | ((u32)(((r & 7) + k) * 16) << 4 << 16);
                vtx(x, CV_FXI(h) / 32, -CV_FXI(k));
            }
        }
        GXP_END = 0;
        glPopMatrix(1);
    }
}

static void quad_n(u32 normal, int32_t x0, int32_t y0, int32_t z0,
                   int32_t x1, int32_t y1, int32_t z1,
                   int32_t x2, int32_t y2, int32_t z2,
                   int32_t x3, int32_t y3, int32_t z3, int ts)
{
    GXP_BEGIN = GX_QUADS;
    GXP_NORMAL = normal;
    GXP_TEXCOORD = 0;
    vtx(x0, y0, z0);
    GXP_TEXCOORD = (u32)(ts << 4);
    vtx(x1, y1, z1);
    GXP_TEXCOORD = (u32)(ts << 4) | ((u32)(ts << 4) << 16);
    vtx(x2, y2, z2);
    GXP_TEXCOORD = (u32)(ts << 4) << 16;
    vtx(x3, y3, z3);
    GXP_END = 0;
}

static void draw_corridor(int r0, int r1, bool fog)
{
    const u32 lights = POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1
                     | POLY_FORMAT_LIGHT2 | POLY_FORMAT_LIGHT3;
    const int32_t ceil_y = CV_FX(5.0);

    // Pass 1: floor (stripes) - lit by the four alarm lights.
    for (int pass = 0; pass < 3; pass++)
    {
        g3_texture_bind(pass == 0 ? TEX_STRIPES : pass == 1 ? TEX_HULL : TEX_MESH);
        int ts = pass == 1 ? 64 : 32;
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2) | lights
                  | (fog ? POLY_FOG : 0));
        GXP_DIF_AMB = RGB15(28, 28, 28) | 0x8000 | (RGB15(13, 12, 12) << 16);
        GXP_SPE_EMI = 0;
        for (int r = r0; r < r1; r++)
        {
            if (r < 0 || r >= terr.rows)
                continue;
            int32_t w = CV_FXI(hval(r, 0)) / 32;
            glPushMatrix();
            glTranslatef32(0, 0, -CV_FXI(r));
            if (pass == 0)
                quad_n(gx_normal(0, 4095, 0), -w, 0, 0, w, 0, 0, w, 0, -CV_FXI(1), -w, 0, -CV_FXI(1), ts);
            else if (pass == 1)
            {
                quad_n(gx_normal(4095, 0, 0), -w, 0, 0, -w, 0, -CV_FXI(1), -w, ceil_y, -CV_FXI(1), -w, ceil_y, 0, ts);
                quad_n(gx_normal(-4095, 0, 0), w, 0, 0, w, ceil_y, 0, w, ceil_y, -CV_FXI(1), w, 0, -CV_FXI(1), ts);
            }
            else
            {
                quad_n(gx_normal(0, -4095, 0), -w, ceil_y, 0, -w, ceil_y, -CV_FXI(1), w, ceil_y, -CV_FXI(1), w, ceil_y, 0, ts);
            }
            glPopMatrix(1);
        }
    }
}

void terrain_draw(int32_t cam_z, int visible_rows, bool fog)
{
    if (!terr.heights)
        return;
    int r0 = ((-cam_z) >> 12) - 2;
    int r1 = r0 + visible_rows;
    if (terr.corridor)
        draw_corridor(r0, r1, fog);
    else
        draw_heightfield(r0, r1, fog);
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

void shadow_volume(CvVec pos, int32_t half, int32_t floor_y)
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
    glPolyFmt(POLY_SHADOW | POLY_CULL_BACK | POLY_ALPHA(18) | POLY_ID(40));
    box_faces(half, top, bottom);
    glPopMatrix(1);
}
