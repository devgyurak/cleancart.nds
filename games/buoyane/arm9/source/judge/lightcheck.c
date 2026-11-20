// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The light check (see judge/lightcheck.h). Result bits are listed in game/rehearsal.h.
//
// Projections. px_proj maps a vertex (pixel * 16) to exactly that screen pixel with W = 3 and
// depth z/12288; ident_proj is the identity (clip = vertex, W = 1); dot_proj gives every vertex a
// W equal to -z, so (a*w, b*w, -w) lands on the same screen point for any w. All 3D registers are
// written here: nothing relies on what a loader left behind.

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/vram.h"
#include "judge/capture.h"
#include "judge/lightcheck.h"

// POLYGON_ATTR bits
#define PA_CULL_NONE (3u << 6)
#define PA_FRONT     (1u << 7)
#define PA_FAR       (1u << 12)
#define PA_1DOT      (1u << 13)
#define PA_EQUAL     (1u << 14)
#define PA_FOG       (1u << 15)
#define PA_MODE(m)   ((u32)(m) << 4)
#define PA_ALPHA(a)  ((u32)(a) << 16)
#define PA_ID(i)     ((u32)(i) << 24)

// DISP3DCNT bits
#define D3_TEXTURE  (1u << 0)
#define D3_HIGHLIGHT (1u << 1)
#define D3_ALPHATEST (1u << 2)
#define D3_BLEND    (1u << 3)
#define D3_AA       (1u << 4)
#define D3_EDGE     (1u << 5)
#define D3_FOGALPHA (1u << 6)
#define D3_FOG      (1u << 7)

#define F(x) ((int32_t)((x) * 4096))

typedef struct { s32 m[16]; } Mtx;

static const Mtx px_proj = { { 24576, 0, 0, 0, 0, -32768, 0, 0, 0, 0, 4096, 0, -12288, 12288, 0, 12288 } };
static const Mtx ident_proj = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096 } };
static const Mtx dot_proj = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 0, -4096, 0, 0, 0, 0 } };

static void load_proj(const Mtx *m)
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix4x4((const m4x4 *)m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    GFX_TEX_FORMAT = 0;
}

static void wait_geometry(void)
{
    // GXSTAT bit 27: geometry engine busy. DISP_1DOT_DEPTH is written only when it is idle.
    while (GFX_STATUS & (1u << 27))
        ;
}

static void cutoff(u16 w)
{
    wait_geometry();
    GFX_CUTOFF_DEPTH = w;
}

// --- vertices ---------------------------------------------------------------------------

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

static void px_slope(int x0, int y0, int x1, int y1, int z0, int z1)
{
    glBegin(GL_QUADS);
    pxv(x0, y0, z0);
    pxv(x0, y1, z0);
    pxv(x1, y1, z1);
    pxv(x1, y0, z1);
    glEnd();
}

// Identity projection: clip x = (pixel - 128) / 128, y = (96 - pixel) / 96 (exact for y multiples of 3).
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

// A vertex of a dot_proj polygon: W = w (20.12), at pixel (x, y).
static void dotv(int x, int y, int32_t w, u16 c)
{
    int32_t a = (x - 128) * 32, b = (96 - y) * 128 / 3;
    GFX_COLOR = c;
    glVertex3v16(a * w / 4096, b * w / 4096, -w);
}

static void dot(int x, int y, int32_t w0, u16 c0, int32_t w1, u16 c1, u32 attr)
{
    GFX_POLY_FORMAT = attr;
    glBegin(GL_TRIANGLES);
    dotv(x, y, w0, c0);
    dotv(x, y, w1, c1);
    dotv(x, y, w1, c1);
    glEnd();
}

// --- state ------------------------------------------------------------------------------

static void gl_reset(u16 clear_555, u8 clear_alpha, u32 d3)
{
    wait_geometry();
    GFX_CONTROL = (u16)d3;
    GFX_CLEAR_COLOR = clear_555 | (u32)clear_alpha << 16 | PA_ID(63);
    GFX_CLEAR_DEPTH = 0x7FFF;
    GFX_ALPHA_TEST = 0;
    glFogShift(0);
    glFogOffset(0x3000);
    GFX_FOG_COLOR = 0;
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = 0;
    for (int i = 0; i < 8; i++)
        GFX_EDGE_TABLE[i] = 0;
    glCutoffDepth(0x7FFF);
}

// Draws `draw` for ten frames and captures at frame 4; true when the capture finished.
#define CAP_AT 4
#define READ_AT 8
#define SCENE_FRAMES 10

static bool capture(void (*draw)(void), bool only_3d)
{
    bool ok = false;
    for (int f = 0; f < SCENE_FRAMES; f++)
    {
        draw();
        g3_flush(false);
        if (f == CAP_AT)
            cap_start(only_3d);
        if (f == READ_AT)
            ok = cap_done();
        sys_vsync();
    }
    return ok;
}

// Textures in bank A (LCDC while writing) and palette in bank G: written once per scene.
#define TEX_GREY  0x000   // 8x8 direct colour (16, 16, 16)
#define TEX_DECAL 0x100   // 8x8 A5I3, every texel colour 1, alpha 0
#define TEX_WIRE  0x200   // 8x8 A5I3, every texel colour 2, alpha 15

static void textures_load(void)
{
    vram_set(VB_A, 0, 0);
    vu16 *g = (vu16 *)(0x06800000 + TEX_GREY);
    for (int i = 0; i < 64; i++)
        g[i] = RGB15(16, 16, 16) | 0x8000;
    // VRAM takes 16-bit writes only: two A5I3 texels per halfword
    vu16 *d = (vu16 *)(0x06800000 + TEX_DECAL), *w = (vu16 *)(0x06800000 + TEX_WIRE);
    for (int i = 0; i < 32; i++)
    {
        d[i] = (1 | 0 << 3) * 0x0101;
        w[i] = (2 | 15 << 3) * 0x0101;
    }
    vram_set(VB_A, 3, 0);
    vram_set(VB_G, 0, 0);
    vu16 *pal = (vu16 *)0x06894000;
    pal[0] = 0;
    pal[1] = RGB15(0, 31, 0);
    pal[2] = RGB15(31, 31, 31);
    for (int i = 3; i < 8; i++)
        pal[i] = 0;
    vram_set(VB_G, 3, 0);
}

// TEXIMAGE_PARAM for an 8x8 texture (S and T size code 0) at `off`: format 7 direct, 6 A5I3.
static u32 tex_param(u32 off, int format)
{
    return off >> 3 | (u32)format << 26;
}

static void tquad(int x0, int y0, int x1, int y1)
{
    glBegin(GL_QUADS);
    GFX_TEX_COORD = 0;
    pxv(x0, y0, 0);
    GFX_TEX_COORD = 8u << 4 << 16;
    pxv(x0, y1, 0);
    GFX_TEX_COORD = (8u << 4 << 16) | (8u << 4);
    pxv(x1, y1, 0);
    GFX_TEX_COORD = 8u << 4;
    pxv(x1, y0, 0);
    glEnd();
}

static void display_setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT = 0;
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
    glViewport(0, 0, 255, 191);
    glClearPolyID(63);
}

// --- dot --------------------------------------------------------------------------------

#define BLUE  RGB15(0, 0, 31)
#define RED   RGB15(31, 0, 0)
#define GREEN RGB15(0, 31, 0)
#define WHITE RGB15(31, 31, 31)

#define DOT_OPAQUE (PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1))

static int dot_pass;
static const int32_t dot_near = F(1), dot_far = F(4);

static void line(int x0, int x1, int y, u32 attr)
{
    GFX_POLY_FORMAT = attr;
    glBegin(GL_TRIANGLES);
    dotv(x0, y, dot_near, WHITE);
    dotv(x1, y, dot_near, WHITE);
    dotv(x1, y, dot_near, WHITE);
    glEnd();
}

static void draw_dot(void)
{
    load_proj(&dot_proj);
    switch (dot_pass)
    {
        case 0:   // cut-off W 2.0, no anti-aliasing
            cutoff(16);
            dot(32, 48, dot_near, WHITE, dot_near, WHITE, DOT_OPAQUE);
            dot(64, 48, dot_far, WHITE, dot_far, WHITE, DOT_OPAQUE);
            dot(96, 48, dot_far, WHITE, dot_far, WHITE, DOT_OPAQUE | PA_1DOT);
            dot(128, 48, dot_far, RED, dot_near, GREEN, DOT_OPAQUE);
            dot(160, 48, dot_near, GREEN, dot_far, RED, DOT_OPAQUE);
            GFX_POLY_FORMAT = DOT_OPAQUE;   // a 1x0 polygon, far: one pixel wide
            glBegin(GL_TRIANGLES);
            dotv(192, 48, dot_far, WHITE);
            dotv(193, 48, dot_far, WHITE);
            dotv(193, 48, dot_far, WHITE);
            glEnd();
            dot(64, 72, dot_near, WHITE, dot_near, WHITE, DOT_OPAQUE);   // the guard of the AA dot
            break;
        case 1:   // anti-aliasing on
            cutoff(16);
            dot(64, 72, dot_near, WHITE, dot_near, WHITE, DOT_OPAQUE);
            dot(128, 72, dot_near, WHITE, dot_near, WHITE, PA_ALPHA(30) | PA_CULL_NONE | PA_ID(2));
            line(32, 224, 96, DOT_OPAQUE);
            line(32, 224, 120, PA_ALPHA(30) | PA_CULL_NONE | PA_ID(3));
            break;
        case 2:   // depth 0: nothing is a 1-dot
            cutoff(0);
            {
                Mtx m = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 0, -4096, 0, 0, 0, 0 } };
                m.m[0] = m.m[5] = F(1);
                m.m[11] = -F(1);
                load_proj(&m);
                dot(96, 96, F(1), WHITE, F(1), WHITE, DOT_OPAQUE);
            }
            break;
        default:  // depth 7FFF: W 1, 64 and 1000 are all drawn
            cutoff(0x7FFF);
            {
                static const int w[3] = { 1, 64, 1000 };
                for (int i = 0; i < 3; i++)
                {
                    int32_t k = w[i] * 4096;
                    Mtx m = { { k, 0, 0, 0, 0, k, 0, 0, 0, 0, 0, -k, 0, 0, 0, 0 } };
                    load_proj(&m);
                    dot(48 + i * 48, 96, F(1), WHITE, F(1), WHITE, DOT_OPAQUE);
                }
            }
            break;
    }
}

static u32 shown(int x, int y)
{
    return cap_pixel(x, y) != BLUE;
}

static void check_dot(u32 *v)
{
    gl_reset(BLUE, 31, D3_BLEND);
    u32 bits = 0;
    bool ok = true;

    dot_pass = 0;
    ok &= capture(draw_dot, true);
    bits |= shown(32, 48) << 0 | !shown(64, 48) << 1 | shown(96, 48) << 2
          | (cap_pixel(128, 48) == RED) << 3 | (cap_pixel(160, 48) == GREEN) << 4 | shown(192, 48) << 5
          | shown(64, 72) << 9;
    v[W_DOT_COLOURS] = cap_pixel(128, 48) | (u32)cap_pixel(160, 48) << 16;

    dot_pass = 1;
    gl_reset(BLUE, 31, D3_BLEND | D3_AA);
    ok &= capture(draw_dot, true);
    bits |= !shown(64, 72) << 8 | shown(128, 72) << 10;
    u32 lo = 0, lt = 0;
    for (int x = 24; x < 232; x++)
    {
        lo += shown(x, 96);
        lt += shown(x, 120);
    }
    v[W_AA_LINES] = lo | lt << 16;

    dot_pass = 2;
    gl_reset(BLUE, 31, D3_BLEND);
    ok &= capture(draw_dot, true);
    bits |= !shown(96, 96) << 6;

    dot_pass = 3;
    gl_reset(BLUE, 31, D3_BLEND);
    ok &= capture(draw_dot, true);
    bits |= (shown(48, 96) && shown(96, 96) && shown(144, 96)) << 7;

    if (ok)
    {
        v[W_CAPS] |= 1u << LC_DOT;
        v[W_DOT] = bits;
    }
}

// --- fog ----------------------------------------------------------------------------------

#define FOGCOL RGB15(31, 31, 0)
#define F_CLEAR BLUE

static int fog_pass;

static void fog_scene_colour(void)
{
    load_proj(&ident_proj);
    const u32 opaque = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
    // far fog quad, behind FOG_OFFSET
    GFX_POLY_FORMAT = opaque | PA_FOG;
    GFX_COLOR = RED;
    id_quad(16, 141, 112, 180, F(0.5));
    // translucent fog polygon over an opaque base without the fog bit (case A) and with it (case B)
    GFX_POLY_FORMAT = opaque | PA_ID(2);
    GFX_COLOR = RED;
    id_quad(130, 21, 180, 60, F(0.5));
    GFX_POLY_FORMAT = opaque | PA_FOG | PA_ID(3);
    GFX_COLOR = RED;
    id_quad(190, 21, 240, 60, F(0.5));
    GFX_POLY_FORMAT = PA_ALPHA(16) | PA_CULL_NONE | PA_FOG | PA_ID(4);
    GFX_COLOR = GREEN;
    id_quad(136, 24, 174, 57, F(0.4));
    id_quad(196, 24, 234, 57, F(0.4));
}

static void fog_scene_alpha(void)
{
    load_proj(&ident_proj);
    const u32 opaque = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
    GFX_COLOR = RED;
    GFX_POLY_FORMAT = opaque | PA_FOG;                   // in front of FOG_OFFSET
    id_quad(16, 141, 112, 180, F(-0.5));
    GFX_POLY_FORMAT = opaque | PA_FOG;                   // behind it
    id_quad(144, 141, 240, 180, F(0.5));
    GFX_POLY_FORMAT = opaque | PA_ID(2);                 // the reference: no fog, same frame, same alpha
    id_quad(16, 81, 112, 120, F(-0.5));
}

static void draw_fog(void)
{
    if (fog_pass == 3)
        fog_scene_alpha();
    else
        fog_scene_colour();
}

static void fog_table(int d)
{
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = (u8)d;
}

static void check_fog(u32 *v)
{
    u32 bits = 0;
    bool ok = true;
    u16 px127 = 0, px126 = 0;

    // pass 0: colour fog, density 127, rear plane without the fog bit
    fog_pass = 0;
    gl_reset(F_CLEAR, 31, D3_BLEND | D3_FOG);
    GFX_FOG_COLOR = FOGCOL | 31u << 16;
    fog_table(127);
    ok &= capture(draw_fog, true);
    px127 = cap_pixel(64, 160);
    bits |= (px127 == FOGCOL) << 2;
    bits |= (cap_pixel(155, 40) != FOGCOL && cap_pixel(155, 40) != F_CLEAR) << 4;    // case A: no fog
    bits |= (cap_pixel(215, 40) == FOGCOL) << 5;                                      // case B: fog
    bits |= (cap_pixel(8, 8) == F_CLEAR) << 7;                                        // rear plane, bit 15 off

    // pass 1: the same with the rear-plane fog bit (CLEAR_COLOR bit 15)
    fog_pass = 1;
    gl_reset(F_CLEAR | 0x8000, 31, D3_BLEND | D3_FOG);
    GFX_FOG_COLOR = FOGCOL | 31u << 16;
    fog_table(127);
    ok &= capture(draw_fog, true);
    bits |= (cap_pixel(8, 8) == FOGCOL) << 6;

    // pass 2: density 120 (126 and 127 differ by less than one level of the 5-bit capture)
    fog_pass = 2;
    gl_reset(F_CLEAR, 31, D3_BLEND | D3_FOG);
    GFX_FOG_COLOR = FOGCOL | 31u << 16;
    fog_table(120);
    ok &= capture(draw_fog, true);
    px126 = cap_pixel(64, 160);
    bits |= (px126 != FOGCOL) << 3;

    // pass 3: fog on the alpha only, through the BG0 blend onto a blue backdrop
    fog_pass = 3;
    gl_reset(0, 0, D3_BLEND | D3_FOG | D3_FOGALPHA);
    GFX_FOG_COLOR = RGB15(31, 31, 31);   // alpha 0
    fog_table(64);
    CV_PAL_BG_MAIN[0] = BLUE;
    REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG0 | BLEND_DST_BACKDROP;
    REG_BLDALPHA = 0x1010;
    ok &= capture(draw_fog, false);
    u16 fnear = cap_pixel(64, 160), ffar = cap_pixel(192, 160), fref = cap_pixel(64, 100);
    bits |= (fnear == fref) << 0 | (ffar != fref && ffar != BLUE) << 1;
    REG_BLDCNT = 0;
    v[W_FOG_NEAR_REF] = fnear | (u32)fref << 16;
    v[W_FOG_127_126] = px127 | (u32)px126 << 16;

    if (ok)
    {
        v[W_CAPS] |= 1u << LC_FOG;
        v[W_FOG] = bits;
    }
}

// --- edge ---------------------------------------------------------------------------------

#define EDGE_CLEAR RGB15(0, 0, 12)
#define EDGE_A     RGB15(16, 16, 16)
#define EDGE_B     RGB15(4, 12, 4)
#define EDGE_RED   RGB15(31, 0, 0)
#define EDGE_YEL   RGB15(31, 31, 0)

static void draw_edge(void)
{
    load_proj(&px_proj);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(8);
    GFX_COLOR = EDGE_A;
    px_quad(40, 48, 120, 144, 0);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(16);
    GFX_COLOR = EDGE_B;
    px_quad(120, 72, 200, 120, 1024);
    // a real one-pixel line (degenerate triangle) with its own polygon ID (the clear plane has ID 63): edge marking
    // plus anti-aliasing must leave no gap along it
    cutoff(16);
    load_proj(&dot_proj);
    line(32, 224, 168, PA_ALPHA(31) | PA_CULL_NONE | PA_ID(8));
}

static void check_edge(u32 *v)
{
    bool ok = true;
    gl_reset(EDGE_CLEAR, 31, D3_BLEND | D3_EDGE);
    GFX_EDGE_TABLE[1] = EDGE_RED;
    GFX_EDGE_TABLE[2] = EDGE_YEL;
    ok &= capture(draw_edge, true);
    u16 a_right = cap_pixel(119, 96), b_left = cap_pixel(120, 96), b_top = cap_pixel(160, 72);
    u16 a_top = cap_pixel(80, 48);
    u32 bits = (a_right == EDGE_RED) << 0 | (b_left == EDGE_B) << 1 | (b_top == EDGE_YEL) << 2
             | (a_top == EDGE_RED) << 3 | (a_right == EDGE_RED || a_top == EDGE_RED) << 5;

    gl_reset(EDGE_CLEAR, 31, D3_BLEND | D3_EDGE | D3_AA);
    GFX_EDGE_TABLE[1] = EDGE_RED;
    GFX_EDGE_TABLE[2] = EDGE_YEL;
    ok &= capture(draw_edge, true);
    u16 a_top_aa = cap_pixel(80, 48);
    u32 row = 0;
    for (int x = 32; x < 224; x++)
        row += cap_pixel(x, 168) != EDGE_CLEAR;
    // the AA edge is the red edge colour mixed with the clear colour (0,0,12): no green, some but not full red
    u32 r = a_top_aa & 31, g = (a_top_aa >> 5) & 31;
    bits |= (g == 0 && r > 0 && r < 31) << 4 | (row == 192) << 6;

    v[W_EDGE_A] = a_right | (u32)b_left << 16;
    v[W_EDGE_B] = b_top | (u32)a_top_aa << 16;
    if (ok)
    {
        v[W_CAPS] |= 1u << LC_EDGE;
        v[W_EDGE] = bits;
    }
}

// --- depth --------------------------------------------------------------------------------

#define Z_CLEAR RGB15(0, 0, 10)
#define Z_BASE  RGB15(12, 12, 12)
#define Z_FLAT  RGB15(31, 31, 0)
#define Z_BAND  GREEN
#define Z_DECAL RED
#define Z_FAR   RGB15(31, 16, 0)
#define Z_LATCH RGB15(0, 31, 31)

static const int zeq_flat_z[5] = { 2, 3, 4, 5, 6 };   // steps 2 4 5 6 8 against the base at 5

static void draw_depth(void)
{
    const u32 base = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
    load_proj(&px_proj);
    GFX_POLY_FORMAT = base;
    GFX_COLOR = Z_BASE;
    px_quad(12, 72, 244, 104, 4);
    px_quad(32, 112, 224, 136, 6);
    px_slope(16, 144, 240, 184, 300, 3000);
    GFX_POLY_FORMAT = base | PA_EQUAL;
    GFX_COLOR = Z_FLAT;
    for (int i = 0; i < 5; i++)
        px_quad(20 + i * 44, 72, 52 + i * 44, 105, zeq_flat_z[i]);
    GFX_COLOR = Z_BAND;
    px_slope(32, 112, 224, 137, 3, 9);   // z in multiples of 3: the band edges are x 104 and 152 whatever the depth formula
    GFX_COLOR = Z_DECAL;
    px_slope(40, 144, 208, 185, 589, 2615);
    // far plane: slanted quads reaching past it (clip z over W = 12288); bit 12 clear hides them
    GFX_COLOR = Z_FAR;
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(2);
    px_slope(20, 8, 60, 30, 2000, 20000);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_FAR | PA_ID(3);
    px_slope(70, 8, 110, 30, 2000, 20000);
    // POLYGON_ATTR inside Begin/End: the second (back-facing) triangle keeps the culling mode of
    // the first until the next BEGIN
    GFX_COLOR = Z_LATCH;
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(4);
    glBegin(GL_TRIANGLES);
    pxv(130, 8, 0);
    pxv(130, 30, 0);
    pxv(160, 30, 0);
    GFX_POLY_FORMAT = PA_ALPHA(31) | PA_FRONT | PA_ID(4);
    pxv(170, 8, 0);
    pxv(200, 30, 0);
    pxv(170, 30, 0);
    glEnd();
}

static u32 zeq_has(int x0, int y0, int x1, int y1, u16 c)
{
    u32 n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            n += cap_pixel(x, y) == c;
    return n;
}

static u32 zeq_not(int x0, int y0, int x1, int y1, u16 c)
{
    return (u32)((x1 - x0) * (y1 - y0)) - zeq_has(x0, y0, x1, y1, c);
}

static void check_depth(u32 *v)
{
    gl_reset(Z_CLEAR, 31, D3_BLEND);
    if (!capture(draw_depth, true))
        return;
    u32 flat[5];
    for (int i = 0; i < 5; i++)
        flat[i] = zeq_has(20 + i * 44, 72, 52 + i * 44, 104, Z_FLAT);
    int left = -1, right = -1;
    for (int x = 32; x < 224; x++)
    {
        if (cap_pixel(x, 124) != Z_BAND)
            continue;
        if (left < 0)
            left = x;
        right = x;
    }
    u32 holes = zeq_not(40, 144, 208, 184, Z_DECAL);
    u32 extra = zeq_not(0, 104, 256, 105, Z_CLEAR) + zeq_not(0, 136, 256, 137, Z_CLEAR) + zeq_not(0, 184, 256, 185, Z_CLEAR);
    u32 bits = (flat[2] == 32 * 32) << 0 | (flat[0] == 0) << 1 | (flat[4] == 0) << 2 | (holes == 0) << 3
             | (extra == 0) << 4
             | (cap_pixel(23, 9) == Z_CLEAR) << 5 | (cap_pixel(73, 9) == Z_FAR) << 6
             | (cap_pixel(137, 28) == Z_LATCH && cap_pixel(172, 28) == Z_LATCH) << 7;
    v[W_DEPTH] |= bits;
    v[W_BAND_L] = left < 0 ? 0 : (u32)left;
    v[W_BAND_R] = right < 0 ? 0 : (u32)right;
    v[W_ZEQ_3] = flat[0] | flat[4] << 16;
    v[W_ZEQ_1] = flat[1] | flat[3] << 16;
    v[W_HOLES] = holes;
    v[W_CAPS] |= 1u << LC_DEPTH;
}

// --- blend --------------------------------------------------------------------------------

static int blend_pass;

static void draw_blend(void)
{
    load_proj(&px_proj);
    switch (blend_pass)
    {
        case 0:   // highlight shading, decal, wireframe with an A5I3 texture
            GFX_TEX_FORMAT = tex_param(TEX_GREY, 7);
            GFX_PAL_FORMAT = 0;
            GFX_COLOR = WHITE;
            GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_MODE(2) | PA_ID(1);
            tquad(16, 21, 64, 60);
            GFX_TEX_FORMAT = tex_param(TEX_DECAL, 6);
            GFX_COLOR = RED;
            GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_MODE(1) | PA_ID(2);
            tquad(80, 21, 128, 60);
            GFX_TEX_FORMAT = 0;
            GFX_COLOR = BLUE;
            GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(3);
            px_quad(144, 21, 192, 60, 100);
            GFX_TEX_FORMAT = tex_param(TEX_WIRE, 6);
            GFX_COLOR = WHITE;
            GFX_POLY_FORMAT = PA_ALPHA(0) | PA_CULL_NONE | PA_ID(4);
            tquad(144, 21, 192, 60);
            GFX_TEX_FORMAT = 0;
            break;
        case 1:   // toon shading: the vertex red picks the table entry
            GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_MODE(2) | PA_ID(1);
            GFX_COLOR = RGB15(5, 0, 0);
            px_quad(16, 21, 64, 60, 0);
            GFX_COLOR = RGB15(20, 0, 0);
            px_quad(80, 21, 128, 60, 0);
            break;
        case 2:   // a translucent polygon on a clear plane of alpha 0
            GFX_POLY_FORMAT = PA_ALPHA(16) | PA_CULL_NONE | PA_ID(1);
            GFX_COLOR = RED;
            px_quad(16, 21, 64, 60, 0);
            break;
        case 3:   // alpha test with the reference at its maximum
            GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
            GFX_COLOR = RED;
            px_quad(16, 21, 64, 60, 0);
            break;
        default:  // the 3D alpha through the BG0 blend: alpha 31 and alpha 15
            GFX_POLY_FORMAT = PA_ALPHA(31) | PA_CULL_NONE | PA_ID(1);
            GFX_COLOR = RED;
            px_quad(16, 21, 64, 60, 0);
            GFX_POLY_FORMAT = PA_ALPHA(15) | PA_CULL_NONE | PA_ID(2);
            px_quad(80, 21, 128, 60, 0);
            break;
    }
}

static void check_blend(u32 *v)
{
    bool ok = true;
    u32 bits = 0;
    textures_load();

    // 0: highlight (every toon table entry white: Rt = 63, Rs = 33 of the grey texel), decal, wire
    blend_pass = 0;
    gl_reset(BLUE, 31, D3_BLEND | D3_TEXTURE | D3_HIGHLIGHT);
    for (int i = 0; i < 32; i++)
        GFX_TOON_TABLE[i] = WHITE;
    ok &= capture(draw_blend, true);
    u16 hi = cap_pixel(40, 40), dec = cap_pixel(104, 40);
    u16 wire = CAP_BAD;
    u32 other = 0;
    for (int y = 19; y <= 23; y++)
    {
        u16 p = cap_pixel(168, y);
        if (p != BLUE && p != WHITE && p != CAP_SENTINEL)
        {
            other++;
            wire = p;
        }
    }
    bits |= (hi == WHITE) << 0 | (dec == RED) << 1 | (other > 0) << 2;

    // 1: toon
    blend_pass = 1;
    gl_reset(BLUE, 31, D3_BLEND);
    for (int i = 0; i < 32; i++)
        GFX_TOON_TABLE[i] = RGB15(i, 31 - i, (i * 3) & 31);
    ok &= capture(draw_blend, true);
    bits |= (cap_pixel(40, 40) == RGB15(5, 26, 15) && cap_pixel(104, 40) == RGB15(20, 11, 28)) << 4;

    // 2: the clear plane has alpha 0
    blend_pass = 2;
    gl_reset(BLUE, 0, D3_BLEND);
    ok &= capture(draw_blend, true);
    u16 bypass = cap_pixel(40, 40);
    bits |= (bypass == RED) << 3;

    // 3: alpha test
    blend_pass = 3;
    gl_reset(BLUE, 31, D3_BLEND | D3_ALPHATEST);
    GFX_ALPHA_TEST = 0x1F;
    ok &= capture(draw_blend, true);
    u32 atest = (cap_pixel(40, 40) == BLUE);

    // 4: the BG0 blend path (graphics capture)
    blend_pass = 4;
    gl_reset(0, 0, D3_BLEND);
    CV_PAL_BG_MAIN[0] = BLUE;
    REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG0 | BLEND_DST_BACKDROP;
    REG_BLDALPHA = 0x1010;
    ok &= capture(draw_blend, false);
    u16 p31 = cap_pixel(40, 40), p15 = cap_pixel(104, 40);
    bits |= (p31 == RED && p15 != p31 && p15 != BLUE) << 5;
    REG_BLDCNT = 0;

    v[W_HI_DECAL] = hi | (u32)dec << 16;
    v[W_WIRE_BYPASS] = wire | (u32)bypass << 16;
    if (ok)
    {
        v[W_CAPS] |= 1u << LC_BLEND;
        v[W_BLEND] = bits;
        v[W_DEPTH] |= atest << 8;
    }
}

// --- the run ------------------------------------------------------------------------------

static const char *const names[LC_SCENES] = { "dot", "fog", "edge", "depth", "blend" };

const char *lightcheck_scene_name(int scene)
{
    return scene >= 0 && scene < LC_SCENES ? names[scene] : "?";
}

void lightcheck_scene(int scene, u32 *v)
{
    display_setup();
    switch (scene)
    {
        case LC_DOT:   check_dot(v); break;
        case LC_FOG:   check_fog(v); break;
        case LC_EDGE:  check_edge(v); break;
        case LC_DEPTH: check_depth(v); break;
        case LC_BLEND: check_blend(v); break;
        default: break;
    }
    REG_DISPCAPCNT = 0;
    REG_BLDCNT = 0;
}

void lightcheck_all(u32 *v)
{
    for (int s = 0; s < LC_SCENES; s++)
        lightcheck_scene(s, v);
}
