// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "assets.h"
#include "game/lamps.h"
#include "gfx/gl3d.h"

#define F(x) ((int32_t)((x) * 4096))
#define PA_CULL_NONE (3u << 6)
#define PA_1DOT      (1u << 13)
#define PA_EQUAL     (1u << 14)
#define PA_FOG       (1u << 15)
#define PA_ALPHA(a)  ((u32)(a) << 16)
#define PA_ID(i)     ((u32)(i) << 24)

typedef struct { s32 m[16]; } Mtx;
static const Mtx ident_proj = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096 } };
static const Mtx dot_proj = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 0, -4096, 0, 0, 0, 0 } };
static const Mtx px_proj = { { 24576, 0, 0, 0, 0, -32768, 0, 0, 0, 0, 4096, 0, -12288, 12288, 0, 12288 } };

static void proj(const Mtx *m)
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix4x4((const m4x4 *)m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    GFX_TEX_FORMAT = 0;
}

static void wait_geometry(void)
{
    while (GFX_STATUS & (1u << 27))
        ;
}

// A vertex at pixel (x, y) with W = w (20.12): the same screen point for every w.
static void dotv(int x, int y, int32_t w, u16 c)
{
    int32_t a = (x - 128) * 32, b = (96 - y) * 128 / 3;
    GFX_COLOR = c;
    glVertex3v16(a * w / 4096, b * w / 4096, -w);
}

static void idv(int x, int y, int z)
{
    glVertex3v16((x - 128) * 32, (96 - y) * 128 / 3, z);
}

static void id_quad(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_QUADS);
    idv(x0, y0, z);
    idv(x0, y1, z);
    idv(x1, y1, z);
    idv(x1, y0, z);
    glEnd();
}

static void pxv(int x, int y, int z)
{
    glVertex3v16(x * 16, y * 16, z);
}

static void px_quad(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_QUADS);
    pxv(x0, y0, z);
    pxv(x0, y1, z);
    pxv(x1, y1, z);
    pxv(x1, y0, z);
    glEnd();
}

static const u16 lamp_colour[3] = { RGB15(31, 4, 4), RGB15(4, 31, 6), RGB15(31, 31, 31) };

// Four tick marks around (x, y): where to look for a 1-pixel light.
static void ticks(int x, int y)
{
    proj(&ident_proj);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(40);
    GFX_COLOR = RGB15(10, 12, 20);
    id_quad(x - 14, y - 1, x - 8, y + 2, F(0.9));
    id_quad(x + 8, y - 1, x + 14, y + 2, F(0.9));
    id_quad(x - 1, y - 14, x + 2, y - 8, F(0.9));
    id_quad(x - 1, y + 8, x + 2, y + 14, F(0.9));
}

static void state(u32 d3)
{
    wait_geometry();
    GFX_CONTROL = (u16)(d3 | 1u << 3);   // blend on
    GFX_FOG_COLOR = 0;
    glFogShift(2);
    glFogOffset(0x3000);
    glCutoffDepth(0x7FFF);
}

void lamps_reset(void)
{
    state(0);
    for (int i = 0; i < 8; i++)
        GFX_EDGE_TABLE[i] = 0;
}

static void ch_range(int type, int view)
{
    state(0);
    wait_geometry();
    glCutoffDepth(by_range_cut[view]);
    ticks(128, 96);
    proj(&dot_proj);
    int32_t w = by_range_w[type] * 4096 / 8;
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
    glBegin(GL_TRIANGLES);
    dotv(128, 96, w, lamp_colour[2]);
    dotv(128, 96, w, lamp_colour[2]);
    dotv(128, 96, w, lamp_colour[2]);
    glEnd();
}

static void ch_sectors(int type, int view)
{
    state(0);
    wait_geometry();
    glCutoffDepth(by_sect_cut[view]);
    ticks(128, 96);
    proj(&dot_proj);
    int32_t w1 = by_sect_w1[type] * 4096 / 8, w2 = by_sect_w2[type] * 4096 / 8;
    u16 c = lamp_colour[by_sect_first[type]], other = RGB15(20, 20, 6);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
    glBegin(GL_TRIANGLES);
    dotv(128, 96, w1, c);
    dotv(128, 96, w2, other);
    dotv(128, 96, w2, other);
    glEnd();
}

static void ch_fog(int type, int view)
{
    state(1u << 7);   // fog on colour and alpha
    GFX_FOG_COLOR = RGB15(16, 18, 24) | 31u << 16;
    for (int i = 0; i < 32; i++)
    {
        int b = i / 5 > 5 ? 5 : i / 5;
        GFX_FOG_TABLE[i] = by_fog_density[view][b];
    }
    proj(&ident_proj);
    int idx = by_fog_bucket[type] * 5 + 2;
    int depth = 0x3000 + idx * 0x100;                 // FOG_SHIFT 2: a table step is 100h
    int z = depth * 2 * 4096 / 0x7FFF - 4096;
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_FOG | PA_ID(1);
    GFX_COLOR = lamp_colour[0];
    id_quad(104, 69, 152, 123, z);
}

static const u16 edge_colour[4] = { 0, RGB15(31, 6, 6), RGB15(31, 31, 6), RGB15(6, 31, 31) };

static void ch_edge(int type, int view)
{
    state(1u << 5 | (view == 2 ? 1u << 4 : 0));   // edge marking, anti-aliasing in the third view
    for (int i = 1; i < 4; i++)
        GFX_EDGE_TABLE[i] = edge_colour[i];
    proj(&px_proj);
    int ia = by_edge_a[type], ib = by_edge_b[type];
    int za = view == 1 ? 1024 : 0, zb = view == 1 ? 0 : 1024;
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(ia << 3);
    GFX_COLOR = RGB15(14, 14, 18);
    px_quad(60, 56, 132, 136, za);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(ib << 3);
    GFX_COLOR = RGB15(10, 16, 12);
    px_quad(124, 72, 196, 120, zb);
}

static void ch_harbour(int type, int view)
{
    state(0);
    proj(&px_proj);
    int d = by_harbour_gap[type] - 2 * view;           // units; one unit is three depth steps
    int n = 5 + 3 * d;
    n = n < 1 ? 1 : n > 12 ? 12 : n;
    int zl = (3 * n + 2) / 4;                          // raw z of that depth step (4 -> step 5)
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
    GFX_COLOR = RGB15(12, 12, 16);
    px_quad(64, 60, 192, 132, 4);                      // the base: a quiet sea
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_EQUAL | PA_ID(2);
    GFX_COLOR = RGB15(31, 28, 12);                     // the moon path, drawn only where depth is Equal
    px_quad(112, 60, 144, 132, zl);
}

void lamps_draw(int chapter, int type, int view)
{
    switch (chapter)
    {
        case 1: ch_range(type, view); break;
        case 2: ch_sectors(type, view); break;
        case 3: ch_fog(type, view); break;
        case 4: ch_edge(type, view); break;
        default: ch_harbour(type, view); break;
    }
}
