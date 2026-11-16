// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/cvmath.h"
#include "core/hw.h"
#include "gfx/glass.h"
#include "gfx/gl3d.h"

#define PETAL_W   700     // half width, 4.12
#define PETAL_TOP 2950
#define PETAL_BOT 740
#define LEAD_Z    90      // the lead frame stands a little in front of the glass

static CvModel lead;
static u32 lead_buf[1024];

static u32 vtx_xy(int x, int y) { return ((u32)x & 0xFFFF) | ((u32)y << 16); }

static int grid64(int v)
{
    return (v + (v < 0 ? -32 : 32)) / 64 * 64;   // 4.12 rounded to 1/64
}

// A spoke: a thin quad of four VTX_10 vertices (4.6 fixed point: rounded to the 1/64 grid).
static void lead_spoke(CvDl *d, int x0, int y0, int x1, int y1, int hw)
{
    // A thin quad around the segment (x0,y0)-(x1,y1), half width hw, along the normal.
    int dx = x1 - x0, dy = y1 - y0;
    int len = (int)m_len(m_vec(dx, dy, 0));
    if (len == 0)
        return;
    int nx = -dy * hw / len, ny = dx * hw / len;
    int cx[4] = { x0 + nx, x0 - nx, x1 - nx, x1 + nx };
    int cy[4] = { y0 + ny, y0 - ny, y1 - ny, y1 + ny };
    dl_cmd1(d, GX_BEGIN, GX_QUADS);
    for (int i = 0; i < 4; i++)
    {
        int gx = grid64(cx[i]) / 64, gy = grid64(cy[i]) / 64;
        dl_cmd1(d, GX_VTX10, ((u32)gx & 0x3FF) | ((u32)gy & 0x3FF) << 10 | (u32)(LEAD_Z / 64) << 20);
    }
    dl_cmd0(d, GX_END);
}

void glass_init(void)
{
    static bool ready;
    if (ready)
        return;
    ready = true;
    CvDl d;
    dl_begin(&d, lead_buf, sizeof(lead_buf) / 4);
    // Outer ring: 16 segments, radius 0.95, width 0.06 (VTX_16 then three VTX_XY).
    const int segs = 16;
    for (int i = 0; i < segs; i++)
    {
        int a0 = i * 32768 / segs, a1 = (i + 1) * 32768 / segs;
        int ro = 3950, ri = 3700;
        int x[4] = { m_cos(a0) * ro >> 12, m_cos(a0) * ri >> 12, m_cos(a1) * ri >> 12, m_cos(a1) * ro >> 12 };
        int y[4] = { m_sin(a0) * ro >> 12, m_sin(a0) * ri >> 12, m_sin(a1) * ri >> 12, m_sin(a1) * ro >> 12 };
        // counter-clockwise: outer a0, inner a0, inner a1, outer a1 is clockwise about the centre for the
        // outer->inner direction; the list is drawn with both faces on, so the order does not matter here.
        dl_cmd1(&d, GX_BEGIN, GX_QUADS);
        u32 p[2] = { vtx_xy(x[0], y[0]), (u32)LEAD_Z };
        dl_cmd(&d, GX_VTX16, 2, p);
        for (int k = 1; k < 4; k++)
            dl_cmd1(&d, GX_VTX_XY, vtx_xy(x[k], y[k]));
        dl_cmd0(&d, GX_END);
    }
    // A small hub ring (VTX_10 spokes): 8 spokes from r = 0.12 to r = 0.9.
    for (int i = 0; i < 8; i++)
    {
        int a = i * 32768 / 8;
        lead_spoke(&d, m_cos(a) * 500 >> 12, m_sin(a) * 500 >> 12, m_cos(a) * 3700 >> 12, m_sin(a) * 3700 >> 12, 60);
    }
    dl_end(&d, &lead, 4096);
}

// Polygon attribute from the settings (side, tint) with the depth test choice.
static void set_poly(const u8 *cur, bool equal, int id)
{
    static const u8 alpha[3] = { 31, 16, 0 };
    int side = cur[F_SIDE];
    u32 attr = (u32)alpha[cur[F_TINT]] << 16 | (u32)id << 24;
    if (side == 0 || side == 2)
        attr |= 1u << 7;      // render the front face
    if (side == 1 || side == 2)
        attr |= 1u << 6;      // render the back face
    if (equal)
        attr |= 1u << 14;     // depth test Equal
    else if (cur[F_DECAL])
        attr |= 1u << 11;     // a translucent base pass still writes its depth, so the decal can match it
    GFX_POLY_FORMAT = attr;
}

static void set_texture(const u8 *cur)
{
    static const CvTex motif[3] = { TEX_CHEVRON, TEX_ARC, TEX_BARS };
    u32 flags = 0;
    if (cur[F_EDGE] == 0)
        flags |= TXF_REPEAT_S | TXF_REPEAT_T;
    if (cur[F_FLIP] & 1)
        flags |= TXF_FLIP_S;
    if (cur[F_FLIP] & 2)
        flags |= TXF_FLIP_T;
    if (cur[F_CUTOUT])
        flags |= TXF_COLOR0;
    if (cur[F_GRAIN])
        flags |= TXF_MODE(1);
    g3_tex_set(motif[cur[F_MOTIF]], flags, cur[F_COLOUR]);
}

// The texture matrix of the grain setting: 1 swaps S and T, 2 doubles both, 3 does both.
static void set_grain(int grain)
{
    s32 m[16] = { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096 };
    if (grain & 1)
    {
        m[0] = 0; m[1] = 4096; m[4] = 4096; m[5] = 0;
    }
    if (grain & 2)
    {
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                m[i * 4 + j] *= 2;
    }
    MATRIX_CONTROL = 3;
    for (int i = 0; i < 16; i++)
        MATRIX_LOAD4x4 = (u32)m[i];
    MATRIX_CONTROL = 2;   // back to position and vector together: store / restore below are mode 2 stack commands
}

static void petal(const u8 *cur, int reps)
{
    int s = 32 * reps;
    GFX_BEGIN = GL_QUADS;
    GFX_TEX_COORD = 0;
    GFX_VERTEX16 = ((u32)(-PETAL_W) & 0xFFFF) | ((u32)PETAL_TOP << 16);
    GFX_VERTEX16 = 0;
    GFX_TEX_COORD = (u32)(s << 4) << 16;
    GFX_VERTEX16 = ((u32)(-PETAL_W) & 0xFFFF) | ((u32)PETAL_BOT << 16);
    GFX_VERTEX16 = 0;
    GFX_TEX_COORD = (u32)(s << 4) << 16 | ((u32)(s << 4) & 0xFFFF);
    GFX_VERTEX16 = ((u32)PETAL_W & 0xFFFF) | ((u32)PETAL_BOT << 16);
    GFX_VERTEX16 = 0;
    GFX_TEX_COORD = (u32)(s << 4) & 0xFFFF;
    GFX_VERTEX16 = ((u32)PETAL_W & 0xFFFF) | ((u32)PETAL_TOP << 16);
    GFX_VERTEX16 = 0;
    GFX_END = 0;
    (void)cur;
}

static void backlight(void)
{
    glPushMatrix();
    glTranslatef32(0, 0, -2600);
    GFX_POLY_FORMAT = 31u << 16 | 1u << 7 | 1u << 6 | 62u << 24;
    g3_tex_set(TEX_COUNT, 0, 0);
    GFX_COLOR = RGB15(31, 29, 22);
    GFX_BEGIN = GL_QUADS;
    GFX_VERTEX16 = ((u32)(-7000) & 0xFFFF) | ((u32)7000 << 16);
    GFX_VERTEX16 = 0;
    GFX_VERTEX16 = ((u32)(-7000) & 0xFFFF) | ((u32)(-7000) << 16);
    GFX_VERTEX16 = 0;
    GFX_VERTEX16 = ((u32)7000 & 0xFFFF) | ((u32)(-7000) << 16);
    GFX_VERTEX16 = 0;
    GFX_VERTEX16 = ((u32)7000 & 0xFFFF) | ((u32)7000 << 16);
    GFX_VERTEX16 = 0;
    GFX_END = 0;
    glPopMatrix(1);
}

void glass_draw(const u8 *cur, u32 turn)
{
    g3_camera(m_vec(0, 0, CV_FXI(3)), m_vec(0, 0, 0), 55, CV_FXI(12));
    backlight();

    int swing = (m_sin((int)(turn * 90)) * 2600) >> 12;
    glRotateYi(swing);

    int petals = 4 + cur[F_PETALS];
    int rings = 1 + cur[F_RINGS];
    int reps = 1 << cur[F_REPEAT];
    glStoreMatrix(1);                       // the window's own frame, restored for each ring
    set_grain(cur[F_GRAIN]);
    for (int r = 0; r < rings; r++)
    {
        glRestoreMatrix(1);
        int sc = 4096 - r * 1100;
        glScalef32(sc, sc, 4096);
        glRotateZi(r * (32768 / petals / 2));    // each ring is turned half a petal against the last
        for (int p = 0; p < petals; p++)
        {
            glPushMatrix();
            glRotateZi(p * 32768 / petals);
            GFX_COLOR = RGB15(31, 31, 31);
            set_poly(cur, false, 1 + (r * 8 + p) % 30);
            set_texture(cur);
            petal(cur, reps);
            if (cur[F_DECAL])
            {
                // A second pass over the same vertices: it only shows where the depth test Equal accepts it.
                set_poly(cur, true, 40);
                g3_tex_set(TEX_COUNT, 0, 0);
                GFX_COLOR = RGB15(31, 26, 8);
                petal(cur, 1);
            }
            glPopMatrix(1);
        }
    }
    glRestoreMatrix(1);
    g3_tex_set(TEX_COUNT, 0, 0);
    GFX_POLY_FORMAT = 31u << 16 | 1u << 7 | 1u << 6 | 50u << 24;
    GFX_COLOR = RGB15(2, 2, 5);
    g3_draw(&lead);
}
