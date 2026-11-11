// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Showcase mode: the finished work drawn as a plaza of copies, more polygons than
// one 3D frame holds (2048 polygons / 6144 vertices), so it is drawn in two passes,
// the left half and the right half of the screen, one pass per frame. Two ways to
// bring the passes together, the capture paths the corvane demos acc-split-hdma and
// acc-split-rear exercise:
//
//   preset 1  HBlank DMA (the path of melonDS #2618). Engine A is the bottom screen
//             and only captures; the top screen is engine B's 16bpp bitmap BG2, filled
//             one line per HBlank from capture bank D (BG2Y = -1: a line shows one
//             scanline late, a race against the beam). Even calls draw the right pass
//             alone (P), odd calls the left pass blended with bank D (Q).
//   preset 2  rear plane. Engine A is the top screen; banks A and C take turns: one
//             captures, the other is the 3D rear plane colour (texture slot 2) of the
//             next pass, bank B the far rear plane depth (slot 3).
//
// Both are optional: if an emulator draws one wrongly the showcase looks wrong and the
// puzzles are not affected. The routing for a frame is written one call after its
// geometry (latency 1, as in gfx/dual.c).

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/play.h"
#include "game/puzzle.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define F CV_FX

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

#define SHOW_LATENCY 1
#define SPLIT_X      128          // fixed: each pass then holds about half the polygons
#define POLY_TARGET  2300         // polygons of the whole plaza, above one frame's 2048

#define HDMA_SRC (*(vu32 *)(0x040000B0 + CV_DMA_CH_HBLANK * 12))
#define HDMA_DST (*(vu32 *)(0x040000B4 + CV_DMA_CH_HBLANK * 12))
#define HDMA_CNT (*(vu32 *)(0x040000B8 + CV_DMA_CH_HBLANK * 12))
#define SUB_C    ((vu16 *)0x06200000)      // bank C as sub BG
#define LCDC_A   ((vu16 *)0x06800000)
#define LCDC_B   ((vu16 *)0x06820000)
#define LCDC_C   ((vu16 *)0x06840000)

enum { PRESET_HDMA, PRESET_REAR, PRESET_COUNT };

static int preset;
static CvText info;
static int polys[2], verts[2];
static bool over[2];
static u32 holes_p, holes_q;   // preset 1: sub BG pixels transparent or black, per kind of frame

// --- the plaza ----------------------------------------------------------------

static void face(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                 int32_t x2, int32_t y2, int32_t z2, int32_t x3, int32_t y3, int32_t z3, u16 c)
{
    GFX_COLOR = c;
    glVertex3v16(x0, y0, z0); glVertex3v16(x1, y1, z1);
    glVertex3v16(x2, y2, z2); glVertex3v16(x3, y3, z3);
}

static void block(int32_t x, int32_t y, int32_t z, int32_t s, int tint)
{
    int a = 20 + tint * 3, b = 17 + (2 - tint) * 2, c = 9 + tint * 4;
    glBegin(GL_QUADS);
    face(x, y + s, z, x, y + s, z + s, x + s, y + s, z + s, x + s, y + s, z, RGB15(a, b, c));              // top
    face(x, y, z + s, x + s, y, z + s, x + s, y + s, z + s, x, y + s, z + s, RGB15(a - 4, b - 4, c - 3));   // front
    face(x + s, y, z, x + s, y + s, z, x + s, y + s, z + s, x + s, y, z + s, RGB15(a - 8, b - 8, c - 5));   // right
    face(x, y, z, x, y, z + s, x, y + s, z + s, x, y + s, z, RGB15(a - 9, b - 9, c - 6));                   // left
    glEnd();
}

// Copies of the player's structure in a grid wide enough to pass the polygon target.
static void plaza(const PzGrid *g, const PzPart *p)
{
    int blocks = pz_blocks(g, p);
    if (blocks < 1)
        blocks = 1;
    int copies = (POLY_TARGET + 4 * blocks - 1) / (4 * blocks);
    int cols = 1;
    while (cols * cols * 46 < copies * 72)   // plaza 7.2 wide x 4.6 deep
        cols++;
    int rows = (copies + cols - 1) / cols;
    int32_t pitch_x = F(7.2) / cols, pitch_z = F(4.6) / rows;
    int32_t pitch = pitch_x < pitch_z ? pitch_x : pitch_z;
    int dim = p->w > p->d ? p->w : p->d;
    if (p->h > dim)
        dim = p->h;
    int32_t su = pitch * 4 / 5 / dim;
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(3));
    for (int k = 0; k < copies; k++)
    {
        int32_t ox = F(-3.6) + (k % cols) * pitch_x + (pitch_x - su * p->w) / 2;
        int32_t oz = F(-2.6) + (k / cols) * pitch_z + (pitch_z - su * p->d) / 2;
        for (int y = 0; y < p->d; y++)
            for (int x = 0; x < p->w; x++)
                for (int z = 0; z < p->h; z++)
                    if (g->cell[y][x] >> z & 1)
                        block(ox + x * su, z * su, oz + (p->d - 1 - y) * su, su, k % 3);
    }
}

// right: the pass that draws the right half of the screen.
static void world(const PzGrid *g, const PzPart *p, bool right)
{
    const int32_t t = 591, r = 788;   // near 0.25, 60 degrees vertical, 4:3
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf32(right ? 0 : -r, right ? r : 0, -t, t, F(0.25), F(20));
    if (right)
        glViewport(SPLIT_X, 0, 255, 191);
    else
        glViewport(0, 0, SPLIT_X - 1, 191);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(0, F(4.2), F(3.6), 0, F(0), F(-0.5), 0, F(1), 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
    glBegin(GL_QUADS);
    GFX_COLOR = RGB15(5, 7, 14);   // back wall
    glVertex3v16(F(-7), F(-1), F(-2.7)); glVertex3v16(F(7), F(-1), F(-2.7));
    glVertex3v16(F(7), F(5), F(-2.7)); glVertex3v16(F(-7), F(5), F(-2.7));
    GFX_COLOR = RGB15(10, 8, 5);   // ground
    glVertex3v16(F(-7), 0, F(-2.7)); glVertex3v16(F(-7), 0, F(3.5));
    glVertex3v16(F(7), 0, F(3.5)); glVertex3v16(F(7), 0, F(-2.7));
    glEnd();
    plaza(g, p);
}

// --- routing -------------------------------------------------------------------

static bool hdma_p(int s) { return !(s & 1); }   // even: capture only (right pass)

static void route(int s)
{
    u32 cap3d = CV_CAP_ENABLE | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
    if (preset == PRESET_HDMA)
    {
        // 3D is only captured; engine A shows the text. DISPCNT names bank D as
        // capture source B for the blend frames.
        REG_DISPCNT = CV_DISP_MODE_NORMAL | ENABLE_3D | DISPLAY_BG1_ACTIVE | CV_DISP_VRAM_BLOCK(VB_D);
        u32 line = hdma_p(s) ? 1 : 0;
        if (hdma_p(s))
            REG_DISPCAPCNT = cap3d | CV_CAP_SRC_A | CV_CAP_BANK(VB_D);
        else
            REG_DISPCAPCNT = cap3d | CV_CAP_SRC_BLEND | CV_CAP_EVA(16) | CV_CAP_EVB(16) | CV_CAP_BANK(VB_D);
        // HBlank N copies line N + line of bank D to the same line of the sub BG.
        HDMA_CNT = 0;
        HDMA_SRC = 0x06860000 + line * 512;
        HDMA_DST = 0x06200000 + line * 512;
        HDMA_CNT = CV_DMA_ON | CV_DMA_HBLANK | CV_DMA_REPEAT | CV_DMA_32 | 128;
    }
    else
    {
        int x = (s & 1) ? VB_C : VB_A, y = (s & 1) ? VB_A : VB_C;
        REG_DISPCNT = MODE_0_3D;
        vram_set(x, 0, 0);
        vram_set(y, 3, 2);   // the other bank: last pass as the rear plane colour
        REG_DISPCAPCNT = cap3d | CV_CAP_BANK(x);
    }
}

static u32 count_holes(void)
{
    u32 n = 0;
    for (int y = 0; y < 192; y += 4)
        for (int x = 0; x < 256; x++)
        {
            u16 p = SUB_C[y * 256 + x];
            n += !(p & 0x8000) || !(p & 0x7FFF);
        }
    return n;
}

static void setup(void)
{
    display_reset();
    vram_layout(VL_DUAL3D);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP | GL_BLEND | GL_ALPHA_TEST | GL_TEXTURE_2D);
    GFX_TEX_FORMAT = 0;
    polys[0] = polys[1] = verts[0] = verts[1] = 0;
    over[0] = over[1] = false;
    holes_p = holes_q = 0;

    if (preset == PRESET_HDMA)
    {
        // Engine A at the bottom with the text on BG1 (bank E); engine B on top shows
        // the 16bpp bitmap the HBlank DMA fills (bank C), one line late.
        lcdMainOnBottom();
        vram_set(VB_E, 1, 0);
        vram_off(VB_H);
        vram_set(VB_C, 4, 0);
        for (int i = 0; i < 256 * 256; i++)
            SUB_C[i] = 0;
        REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE;
        REG_BG0CNT = 1;
        REG_BG1CNT = 31 << 8;
        text_load_tiles(MAIN_BG);
        text_load_palette(CV_PAL_BG_MAIN);
        text_init(&info, (u16 *)(MAIN_BG + 31 * 2048), 0);
        CV_PAL_BG_MAIN[0] = RGB15(1, 2, 6);
        REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG2_ACTIVE;
        REG_BG2CNT_SUB = (1 << 7) | (1 << 2) | (1 << 14);   // 16bpp 256x256 at 0
        REG_BG2PA_SUB = 256;
        REG_BG2PB_SUB = 0;
        REG_BG2PC_SUB = 0;
        REG_BG2PD_SUB = 256;
        REG_BG2X_SUB = 0;
        REG_BG2Y_SUB = -256;   // scanline y shows line y - 1
        CV_PAL_BG_SUB[0] = RGB15(12, 12, 12);
    }
    else
    {
        // Engine A on top; the text on engine B (bank H).
        lcdMainOnTop();
        vram_set(VB_H, 1, 0);
        // Banks A and C start empty (transparent rear plane), bank B is the far depth.
        vram_set(VB_A, 0, 0);
        vram_set(VB_B, 0, 0);
        vram_set(VB_C, 0, 0);
        for (int i = 0; i < 256 * 256; i++)
        {
            LCDC_A[i] = 0;
            LCDC_C[i] = 0;
            LCDC_B[i] = 0x7FFF;
        }
        vram_set(VB_B, 3, 3);
        REG_DISPCNT = MODE_0_3D;
        REG_BG0CNT = 0;
        CV_PAL_BG_MAIN[0] = RGB15(12, 12, 12);
        REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
        REG_BG0CNT_SUB = 7 << 8;
        text_load_tiles(SUB_BG);
        text_load_palette(CV_PAL_BG_SUB);
        text_init(&info, (u16 *)(SUB_BG + 7 * 2048), 0);
        CV_PAL_BG_SUB[0] = RGB15(1, 2, 6);
    }
    text_clear(&info);
}

static void teardown(void)
{
    dma_stop(CV_DMA_CH_HBLANK);
    REG_DISPCAPCNT = 0;
    glDisable(GL_CLEAR_BMP);
    lcdMainOnTop();
}

static void print_info(int call)
{
    char buf[40];
    text_clear(&info);
    text_center(&info, 1, TP_YELLOW, "SHOWCASE");
    text_center(&info, 3, TP_WHITE, preset == PRESET_HDMA ? "PRESET 1: HBLANK DMA" : "PRESET 2: REAR PLANE");
    snprintf(buf, sizeof(buf), "POLYS %d+%d  LIMIT 2048", polys[0], polys[1]);
    text_center(&info, 6, TP_GRAY, buf);
    snprintf(buf, sizeof(buf), "VERTS %d+%d  LIMIT 6144", verts[0], verts[1]);
    text_center(&info, 8, TP_GRAY, buf);
    snprintf(buf, sizeof(buf), "OVERFLOW %d %d", over[0], over[1]);
    text_center(&info, 10, over[0] || over[1] ? TP_RED : TP_GREEN, buf);
    if (preset == PRESET_HDMA)
    {
        snprintf(buf, sizeof(buf), "HOLES CAP %lu BLEND %lu", (unsigned long)holes_p, (unsigned long)holes_q);
        text_center(&info, 12, holes_p || holes_q ? TP_RED : TP_GREEN, buf);
    }
    snprintf(buf, sizeof(buf), "FRAME %d", call);
    text_center(&info, 15, TP_GRAY, buf);
    text_center(&info, 21, TP_GRAY, "A: PRESET   B: BACK");
}

EvScene scene_showcase(void)
{
    const PzGrid *g = play_grid();
    const PzPart *p = play_part();
    setup();
    fx_bright(FX_BOTH, 0);

    for (int c = 0;; c++)
    {
        int s = c - SHOW_LATENCY;
        if (s >= 0)
            route(s);
        bool right = hdma_p(c) == (preset == PRESET_HDMA);   // hdma: even = right pass; rear: even = left
        glClearColor(0, 0, 0, 0);
        if (preset == PRESET_REAR)
            glEnable(GL_CLEAR_BMP);   // latched with the polygon list
        world(g, p, right);
        g3_flush_measured(false);
        int slot = right ? 1 : 0;
        polys[slot] = g3_flushed_poly_count();
        verts[slot] = g3_flushed_vert_count();
        over[slot] = g3_overflowed();

        scene_frame_end();
        if (preset == PRESET_HDMA && c >= 8 && (c & 3) == 0)
        {
            // The frame that just ended ran route(c - latency).
            u32 h = count_holes();
            if (hdma_p(c - SHOW_LATENCY))
                holes_p = h;
            else
                holes_q = h;
        }
        if (c % 10 == 0)
            print_info(c);

        if (cv_in.pressed & KEY_A)
        {
            teardown();
            preset = (preset + 1) % PRESET_COUNT;
            setup();
            c = -1;
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
        if (cv_in.pressed & (KEY_B | KEY_START))
        {
            teardown();
            return SC_PLAY;
        }
    }
}
