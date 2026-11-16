// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/vram.h"
#include "judge/capture.h"
#include "judge/lightbox.h"

// Identity projection and modelview: clip x and y map straight to pixels. x is exact for any
// pixel, y for multiples of 3.
#define PX(x) ((x) * 32 - 4096)
#define PY(y) (4096 - (y) * 128 / 3)

#define C_CLEAR RGB15(1, 2, 6)
#define C_VF    RGB15(31, 8, 4)
#define C_FRONT RGB15(4, 28, 8)
#define C_BACK  RGB15(8, 12, 31)
#define C_BG1   RGB15(4, 10, 26)
#define C_FG1   RGB15(28, 24, 4)
#define C_BG2   RGB15(6, 22, 12)
#define C_WIRE  RGB15(31, 31, 31)
#define C_RED   RGB15(31, 4, 4)
#define C_BLUE  RGB15(4, 4, 31)

// Polygon attribute: alpha, back/front render bits, depth test, polygon id; mode modulation, no lights.
static void poly(int alpha, bool front, bool back, bool equal, int id)
{
    GFX_POLY_FORMAT = (u32)alpha << 16 | (front ? 1u << 7 : 0) | (back ? 1u << 6 : 0)
                    | (equal ? 1u << 14 : 0) | (u32)id << 24;
}

static void vtx16(int x, int y, int z)
{
    GFX_VERTEX16 = ((u32)PX(x) & 0xFFFF) | ((u32)PY(y) << 16);
    GFX_VERTEX16 = (u32)z & 0xFFFF;
}

static void vtx_xy(int x, int y) { GFX_VERTEX_XY = ((u32)PX(x) & 0xFFFF) | ((u32)PY(y) << 16); }
static void vtx_xz(int x, int z) { GFX_VERTEX_XZ = ((u32)PX(x) & 0xFFFF) | ((u32)z << 16); }
static void vtx_yz(int y, int z) { GFX_VERTEX_YZ = ((u32)PY(y) & 0xFFFF) | ((u32)z << 16); }
static void vtx10(int x, int y, int z)
{
    GFX_VERTEX10 = ((u32)(PX(x) / 64) & 0x3FF) | ((u32)(PY(y) / 64) & 0x3FF) << 10 | ((u32)z & 0x3FF) << 20;
}
static void vtx_diff(int dx, int dy)
{
    GFX_VERTEX_DIFF = ((u32)dx & 0x3FF) | ((u32)dy & 0x3FF) << 10;
}

static void begin_quads(void) { GFX_BEGIN = GL_QUADS; }
static void end_vtx(void) { GFX_END = 0; }

// Front order is counter-clockwise on screen: top-left, bottom-left, bottom-right, top-right.
static void quad(int x0, int y0, int x1, int y1, bool front)
{
    begin_quads();
    vtx16(x0, y0, 0);
    if (front)
    {
        vtx16(x0, y1, 0);
        vtx16(x1, y1, 0);
        vtx16(x1, y0, 0);
    }
    else
    {
        vtx16(x1, y0, 0);
        vtx16(x1, y1, 0);
        vtx16(x0, y1, 0);
    }
    end_vtx();
}

static void quad_z(int x0, int y0, int x1, int y1, int z)
{
    begin_quads();
    vtx16(x0, y0, z);
    vtx16(x0, y1, z);
    vtx16(x1, y1, z);
    vtx16(x1, y0, z);
    end_vtx();
}

// A textured quad with texel coordinates (s0,t0)..(s1,t1) mapped over the rectangle.
static void tquad(int x0, int y0, int x1, int y1, int s1, int t1)
{
    begin_quads();
    GFX_TEX_COORD = 0;
    vtx16(x0, y0, 0);
    GFX_TEX_COORD = (u32)(t1 << 4) << 16;
    vtx16(x0, y1, 0);
    GFX_TEX_COORD = (u32)(t1 << 4) << 16 | ((u32)(s1 << 4) & 0xFFFF);
    vtx16(x1, y1, 0);
    GFX_TEX_COORD = (u32)(s1 << 4) & 0xFFFF;
    vtx16(x1, y0, 0);
    end_vtx();
}

// --- samples ----------------------------------------------------------------------

static u32 hits;

static void expect(int x, int y, u16 want)
{
    hits += cap_pixel(x, y) == want;
}

// Palette entry of a texel of the light box texture in palette set `set`.
static u16 lbox_colour(int set, int u, int v)
{
    const u16 *pal = (const u16 *)cv_pal_tex;
    return pal[set * 8 + cv_tex_lbox[v * 8 + u]] & 0x7FFF;
}

static u16 pal_entry(int set, int idx)
{
    return ((const u16 *)cv_pal_tex)[set * 8 + idx] & 0x7FFF;
}

// --- scenes -------------------------------------------------------------------------

// Rows of 16x12 quads for the six vertex formats (y 6..18), the culling rows below them.
static const int vf_x[6] = { 8, 48, 88, 128, 168, 208 };
static const int cull_x[8] = { 8, 40, 72, 104, 136, 168, 200, 232 };
static const int back_x[4] = { 8, 48, 88, 128 };

static void scene_geometry(void)
{
    GFX_COLOR = C_VF;
    poly(31, true, false, false, 1);
    for (int i = 0; i < 6; i++)
    {
        int x0 = vf_x[i], x1 = x0 + 16, y0 = 6, y1 = 18;
        begin_quads();
        switch (i)
        {
            case 0:
                vtx16(x0, y0, 0); vtx16(x0, y1, 0); vtx16(x1, y1, 0); vtx16(x1, y0, 0);
                break;
            case 1:
                vtx10(x0, y0, 0); vtx10(x0, y1, 0); vtx10(x1, y1, 0); vtx10(x1, y0, 0);
                break;
            case 2:
                vtx16(x0, y0, 0); vtx_xy(x0, y1); vtx_xy(x1, y1); vtx_xy(x1, y0);
                break;
            case 3:
                vtx16(x0, y0, 0); vtx16(x0, y1, 0); vtx_xz(x1, 0); vtx16(x1, y0, 0);
                break;
            case 4:
                vtx16(x0, y0, 0); vtx_yz(y1, 0); vtx16(x1, y1, 0); vtx_yz(y0, 0);
                break;
            default:
                // 480 / 4096 of the clip range: 15 pixels wide, 11.25 high
                vtx16(x0, y0, 0); vtx_diff(0, -480); vtx_diff(480, 0); vtx_diff(0, 480);
                break;
        }
        end_vtx();
    }

    // Default culling: front faces only. Four front quads show, four back quads do not.
    GFX_COLOR = C_FRONT;
    poly(31, true, false, false, 2);
    for (int i = 0; i < 8; i++)
        quad(cull_x[i], 36, cull_x[i] + 24, 54, (i & 1) == 0);

    // Render bits: back only shows the two back quads, the two front quads are hidden by it.
    GFX_COLOR = C_BACK;
    poly(31, false, true, false, 3);
    for (int i = 0; i < 4; i++)
        quad(back_x[i], 66, back_x[i] + 24, 84, i < 2 ? false : true);
}

static void count_geometry(LightboxResult *r)
{
    hits = 0;
    for (int i = 0; i < 6; i++)
        expect(vf_x[i] + 8, i == 5 ? 11 : 12, C_VF);
    r->v[LB_VTX_FORMATS] = hits;
    hits = 0;
    for (int i = 0; i < 8; i++)
        expect(cull_x[i] + 12, 45, (i & 1) == 0 ? C_FRONT : C_CLEAR);
    r->v[LB_CULL_DEFAULT] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)
        expect(back_x[i] + 12, 75, i < 2 ? C_BACK : C_CLEAR);
    r->v[LB_BACK_RENDER] = hits;
}

static void scene_alpha(void)
{
    poly(31, true, true, false, 0);
    GFX_COLOR = C_BG1;
    quad(8, 6, 120, 60, true);
    poly(16, true, true, false, 1);
    GFX_COLOR = C_FG1;
    quad_z(16, 12, 112, 54, -256);
    poly(31, true, true, false, 0);
    GFX_COLOR = C_BG2;
    quad(136, 6, 248, 60, true);
    poly(0, true, true, false, 2);
    GFX_COLOR = C_WIRE;
    quad_z(144, 12, 168, 30, -256);
    quad_z(176, 12, 200, 30, -256);
    quad_z(208, 12, 232, 30, -256);
    quad_z(144, 36, 168, 54, -256);
}

static int near_mix(u16 got, u16 a, u16 b)
{
    for (int s = 0; s < 15; s += 5)
    {
        int mid = (((a >> s) & 31) + ((b >> s) & 31)) / 2;
        int d = (int)((got >> s) & 31) - mid;
        if (d < -3 || d > 3)
            return 0;
    }
    return 1;
}

static void count_alpha(LightboxResult *r)
{
    u32 n = 0;
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            n += near_mix(cap_pixel(28 + i * 24, 18 + j * 10), C_BG1, C_FG1);
    r->v[LB_ALPHA_HALF] = n;
    hits = 0;
    static const int wx[4] = { 144, 176, 208, 144 }, wy[4] = { 12, 12, 12, 36 };
    for (int i = 0; i < 4; i++)
        expect(wx[i] + 12, wy[i] + 9, C_BG2);
    r->v[LB_WIRE_HOLLOW] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)
        for (int d = -3; d <= 3; d++)
            expect(wx[i] + 12, wy[i] + d, C_WIRE);
    r->v[LB_WIRE_EDGES] = hits;
}

static void scene_depth(void)
{
    for (int i = 0; i < 8; i++)
    {
        int x0 = 8 + (i & 3) * 60, y0 = 12 + (i >> 2) * 48;
        poly(31, true, true, false, 0);
        GFX_COLOR = C_RED;
        quad_z(x0, y0, x0 + 40, y0 + 30, 0);
        poly(31, true, true, i < 4, 0);     // the first row: Equal, the second row: Less
        GFX_COLOR = C_BLUE;
        quad_z(x0, y0, x0 + 40, y0 + 30, 0);
    }
}

static void count_depth(LightboxResult *r)
{
    hits = 0;
    for (int i = 0; i < 4; i++)
        expect(8 + i * 60 + 20, 27, C_BLUE);
    r->v[LB_DEPTH_EQUAL] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)
        expect(8 + i * 60 + 20, 75, C_RED);
    r->v[LB_DEPTH_LESS] = hits;
}

// Samples at texel centres of a quad mapped 4 pixels per texel horizontally and 4.5 vertically.
static int cx(int x0, int texel) { return x0 + texel * 4 + 2; }
static int cy(int y0, int texel) { return y0 + (texel * 9 + 4) / 2; }

static const int samp[4] = { 1, 3, 5, 6 };

static void scene_tex1(void)
{
    poly(31, true, true, false, 0);
    GFX_COLOR = RGB15(31, 31, 31);
    g3_tex_set(TEX_LBOX, TXF_REPEAT_S, 4);
    tquad(8, 6, 72, 42, 16, 8);                                // repeat S
    g3_tex_set(TEX_LBOX, TXF_REPEAT_T, 4);
    tquad(80, 6, 112, 78, 8, 16);                              // repeat T
    g3_tex_set(TEX_LBOX, TXF_REPEAT_S | TXF_FLIP_S, 4);
    tquad(120, 6, 184, 42, 16, 8);                             // flip S
    g3_tex_set(TEX_LBOX, TXF_REPEAT_T | TXF_FLIP_T, 4);
    tquad(192, 6, 224, 78, 8, 16);                             // flip T
    g3_tex_set(TEX_LBOX, 0, 4);
    tquad(8, 48, 72, 84, 16, 8);                               // clamp
    g3_tex_set(TEX_COUNT, 0, 0);
}

static void count_tex1(LightboxResult *r)
{
    hits = 0;
    for (int i = 0; i < 4; i++)
    {
        expect(cx(8, 8 + samp[i]), cy(6, 3), lbox_colour(4, samp[i], 3));       // S repeat, second copy
        expect(cx(80, 3), cy(6, 8 + samp[i]), lbox_colour(4, 3, samp[i]));      // T repeat
    }
    r->v[LB_TEX_REPEAT] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)
    {
        expect(cx(120, 8 + samp[i]), cy(6, 3), lbox_colour(4, 7 - samp[i], 3)); // S flipped in the second copy
        expect(cx(192, 3), cy(6, 8 + samp[i]), lbox_colour(4, 3, 7 - samp[i])); // T flipped
    }
    r->v[LB_TEX_FLIP] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)
        expect(cx(8, 8 + samp[i]), cy(48, samp[i]), lbox_colour(4, 7, samp[i]));  // clamped: last column
    r->v[LB_TEX_CLAMP] = hits;
}

static const int hole_u[4] = { 2, 4, 2, 4 }, hole_v[4] = { 2, 2, 5, 5 };
static const int tr_u[4] = { 1, 1, 1, 1 }, tr_v[4] = { 2, 4, 5, 6 };

static void scene_tex2(void)
{
    poly(31, true, true, false, 0);
    GFX_COLOR = RGB15(31, 31, 31);
    g3_tex_set(TEX_LBOX, TXF_COLOR0, 4);
    tquad(8, 6, 40, 42, 8, 8);                                 // colour 0 transparent
    g3_tex_set(TEX_LBOX, 0, 4);
    tquad(48, 6, 80, 42, 8, 8);                                // colour 0 shown
    g3_tex_set(TEX_LBOX, 0, 6);
    tquad(88, 6, 120, 42, 8, 8);                               // palette set 6
    // Texture matrix (mode 3) swaps S and T; mode 1 of TEXIMAGE_PARAM applies it.
    MATRIX_CONTROL = 3;
    static const s32 m[16] = { 0, 4096, 0, 0, 4096, 0, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096 };
    for (int i = 0; i < 16; i++)
        MATRIX_LOAD4x4 = (u32)m[i];
    MATRIX_CONTROL = 1;
    g3_tex_set(TEX_LBOX, TXF_MODE(1), 4);
    tquad(128, 6, 160, 42, 8, 8);
    g3_tex_set(TEX_COUNT, 0, 0);
    MATRIX_CONTROL = 3;
    MATRIX_IDENTITY = 0;
    MATRIX_CONTROL = 1;
}

static void count_tex2(LightboxResult *r)
{
    hits = 0;
    for (int i = 0; i < 4; i++)
        expect(cx(8, hole_u[i]), cy(6, hole_v[i]), C_CLEAR);
    r->v[LB_TEX_C0_HIDDEN] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)
        expect(cx(48, hole_u[i]), cy(6, hole_v[i]), pal_entry(4, 0));
    r->v[LB_TEX_C0_SHOWN] = hits;
    hits = 0;
    static const int pu[4] = { 1, 3, 5, 6 }, pv[4] = { 1, 3, 4, 6 };
    for (int i = 0; i < 4; i++)
        expect(cx(88, pu[i]), cy(6, pv[i]), lbox_colour(6, pu[i], pv[i]));
    r->v[LB_TEX_PLTT_BASE] = hits;
    hits = 0;
    for (int i = 0; i < 4; i++)   // pixel (u, v) shows texel (S' = v, T' = u)
        expect(cx(128, tr_u[i]), cy(6, tr_v[i]), lbox_colour(4, tr_v[i], tr_u[i]));
    r->v[LB_TEX_COORD1] = hits;
}

// --- the run ------------------------------------------------------------------------

typedef struct {
    void (*draw)(void);
    void (*count)(LightboxResult *);
} Scene;

static const Scene scenes[LB_SCENES] = {
    { scene_geometry, count_geometry },
    { scene_alpha, count_alpha },
    { scene_depth, count_depth },
    { scene_tex1, count_tex1 },
    { scene_tex2, count_tex2 },
};

static const u8 scene_items[LB_SCENES][2] = {
    { LB_VTX_FORMATS, LB_BACK_RENDER }, { LB_ALPHA_HALF, LB_WIRE_EDGES }, { LB_DEPTH_EQUAL, LB_DEPTH_LESS },
    { LB_TEX_REPEAT, LB_TEX_CLAMP }, { LB_TEX_C0_HIDDEN, LB_TEX_COORD1 },
};

#define SCENE_FRAMES 18
#define CAP_AT 8
#define READ_AT 14

void lightbox_run(LightboxResult *r)
{
    memset(r, 0, sizeof(*r));
    for (int i = 0; i < LB_ITEMS; i++)
        r->v[i] = LB_BAD;

    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    g3_textures_load();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP | GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D | GL_BLEND);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT = 0;
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
    glClearColor(1, 2, 6, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);

    for (int s = 0; s < LB_SCENES; s++)
    {
        LightboxResult tmp;
        for (int i = 0; i < LB_ITEMS; i++)
            tmp.v[i] = LB_BAD;
        bool ok = false;
        for (int f = 0; f < SCENE_FRAMES; f++)
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_POSITION);
            glLoadIdentity();
            glColor3b(255, 255, 255);
            g3_tex_set(TEX_COUNT, 0, 0);   // libnds leaves the last uploaded texture bound
            scenes[s].draw();
            g3_flush(false);
            if (f == CAP_AT)
                cap_start();
            if (f == READ_AT && cap_done())
            {
                scenes[s].count(&tmp);
                ok = true;
            }
            sys_vsync();
        }
        if (ok)
        {
            r->done |= 1u << s;
            for (int i = scene_items[s][0]; i <= scene_items[s][1]; i++)
                r->v[i] = tmp.v[i];
        }
    }
    REG_DISPCAPCNT = 0;
}
