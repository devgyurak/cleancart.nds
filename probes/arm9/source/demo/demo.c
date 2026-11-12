// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Probe demo builds (make DEMO=l4-<name>). Each probe exercises one hardware
// behaviour that nobody has confirmed on a console, reports through a text
// BG console on the main screen, completes its checkpoint image at frame
// 120 and writes its values to the fixed result block (CV_RESULT_ADDR).
// The demo code is Corvane's (games/corvane/arm9/source/demo/demo.c).

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cart.h"
#include "core/cvmath.h"
#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/nitro.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "core/timer.h"
#include "demo/demo.h"
#include "game/game.h"
#include "game/stage.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"
#include "gfx/oam.h"
#include "gfx/terrain.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define F CV_FX
#define CHECKPOINT 120
#define MAIN_BG  ((u8 *)0x06000000)
#define SUB_BG   ((u8 *)0x06200000)
#define MAIN_OBJ ((u8 *)0x06400000)
#define SUB_OBJ  ((u8 *)0x06600000)

static CvText con;
static u32 demo_id;
static void (*idle_fn)(int f);

// ---------------------------------------------------------------------------
// Harness

static u32 hash_name(const char *s)
{
    u32 h = 2166136261u;
    while (*s)
        h = (h ^ (u8)*s++) * 16777619u;
    return h;
}

// Every demo starts from the same quiet display state.
static void reset_display(void)
{
    dma_hblank_stop();
    dma_line_stop();
    dma_mmem_stop();
    irq_set_hblank(NULL);
    irq_set_vcount(0, NULL);
    REG_DISPCAPCNT = 0;
    fx_blend_off(FX_MAIN);
    fx_blend_off(FX_SUB);
    fx_mosaic(FX_MAIN, 0, 0);
    fx_mosaic(FX_SUB, 0, 0);
    REG_WININ = REG_WINOUT = 0;
    REG_WININ_SUB = REG_WINOUT_SUB = 0;
    for (int i = 0; i < 4; i++)
    {
        ((vu16 *)0x04000010)[i * 2] = 0;
        ((vu16 *)0x04000012)[i * 2] = 0;
        ((vu16 *)0x04001010)[i * 2] = 0;
        ((vu16 *)0x04001012)[i * 2] = 0;
    }
    oam_clear(ENG_MAIN);
    oam_clear(ENG_SUB);
    oam_flush_now();
    lcdMainOnTop();
}

// Engine A: BG0 text console on VRAM A (2D demos).
static void con_2d(void)
{
    reset_display();
    vram_layout(VL_MENU);
    REG_DISPCNT = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    CV_PAL_BG_MAIN[0] = RGB15(1, 2, 6);
    text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
    con_init(&con);
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
}

// Engine A: 3D on BG0 with a BG1 text console on bank E (3D demos).
static void con_3d(CvVramLayout layout)
{
    reset_display();
    vram_layout(layout);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST);
    for (int t = 0; t < TEX_COUNT; t++)
        g3_texture_load((CvTex)t);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT = 1;
    REG_BG1CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
    con_init(&con);
    glClearColor(2, 3, 8, 31);
    g3_setup_lights(0);
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
}

static void demo_title(const char *name)
{
    con_color(TP_YELLOW);
    con_printf("PROBE: %s\n", name);
    con_color(TP_WHITE);
}

static void vsync(void)
{
    oam_commit();
    sys_vsync();
    input_update();
}

// Writes the checkpoint block and keeps the screen as it is.
static void __attribute__((noreturn)) finish(u32 a, u32 b, u32 c, u32 d)
{
    volatile CvResult *r = CV_RESULT;
    r->demo_id = demo_id;
    r->frame = cv_frame;
    r->values[0] = a;
    r->values[1] = b;
    r->values[2] = c;
    r->values[3] = d;
    r->magic = CV_RESULT_MAGIC;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    con_color(TP_GREEN);
    con_printf("CHECKPOINT %08lX %08lX\n", (unsigned long)a, (unsigned long)b);
    con_printf("           %08lX %08lX\n", (unsigned long)c, (unsigned long)d);
    con_color(TP_WHITE);
    for (int f = 0;; f++)
    {
        if (idle_fn)
            idle_fn(f);
        oam_commit();
        sys_vsync();
        input_update();
    }
}

static void spin_cube(int f, CvTex tex, int32_t scale)
{
    g3_camera(m_vec(0, F(1.0), F(3.2)), m_vec(0, 0, 0), 60, F(40));
    g3_texture_bind(tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10) | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1);
    glPushMatrix();
    glRotateYi(f * 150);
    glRotateXi(f * 90);
    glScalef32(scale, scale, scale);
    g3_draw(&cv_models[MDL_CUBE]);
    glPopMatrix(1);
}


// ---------------------------------------------------------------------------
// ROM-side judging. Demos that used to be checked by eye capture engine A's own
// output (BG + OBJ) and compare pixels with what GBATEK says must appear; the
// counts go to values[4..6] (n samples, n wrong, capture finished) and
// tools/judge.py grades them. Bank D is the sub-engine OBJ bank in the menu
// layout: it is the LCDC capture target for one frame and goes back afterwards.

#define PROBE_CAP ((vu16 *)0x06860000)

static bool probe_capture(void)
{
    vram_set(VB_D, 0, 0);
    for (int i = 0; i < 256 * 192 / 2; i++)
        ((vu32 *)PROBE_CAP)[i] = 0;     // a pixel that was not captured has bit 15 clear
    REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20);   // engine A output to bank D, 256x192
    for (int f = 0; f < 4 && (REG_DISPCAPCNT & (1u << 31)); f++)
        vsync();
    return !(REG_DISPCAPCNT & (1u << 31));
}

// Same, but with capture source B = the main-memory display FIFO (GBATEK: display capture can
// store the FIFO's pixels instead of the engine output).
static bool probe_capture_fifo(void)
{
    vram_set(VB_D, 0, 0);
    for (int i = 0; i < 256 * 192 / 2; i++)
        ((vu32 *)PROBE_CAP)[i] = 0;
    REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20) | CV_CAP_SRCB_FIFO | CV_CAP_SRC_B;
    for (int f = 0; f < 4 && (REG_DISPCAPCNT & (1u << 31)); f++)
        vsync();
    return !(REG_DISPCAPCNT & (1u << 31));
}

static void probe_release(void)
{
    vram_set(VB_D, 4, 0);
}

static u16 probe_px(int x, int y)
{
    return PROBE_CAP[y * 256 + x];
}

static void probe_values(u32 n, u32 bad, bool done)
{
    volatile CvResult *r = CV_RESULT;
    r->values[4] = n;
    r->values[5] = bad;
    r->values[6] = done;
}

// ---------------------------------------------------------------------------
// System

static void d_sys_dma_gfx(void)
{
    con_3d(VL_TERRAIN);
    demo_title("SYS-DMA-GFX");
    con_printf("CUBE LIST %lu WORDS VIA DMA\n", (unsigned long)cv_models[MDL_CUBE].words);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        spin_cube(f, TEX_HULL, F(1.2));
        g3_flush_measured(false);
        vsync();
    }
    finish(cv_models[MDL_CUBE].words, g3_flushed_poly_count(), 0, 0);
}

static u16 *mmem_frame;
static u16 bars[192] __attribute__((aligned(32)));
static void mmem_vblank(void)
{
    dma_mmem_start(mmem_frame);
    dma_line_rearm();
}

static void d_sys_dma_disp(void)
{
    con_2d();
    demo_title("SYS-DMA-DISP");
    // Sub screen shows the console; main shows the main-memory FIFO frame.
    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT_SUB = (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    text_init(&con, (u16 *)(SUB_BG + 31 * 2048), 0);
    con_init(&con);
    demo_title("SYS-DMA-DISP");
    con_printf("TOP: MAIN MEMORY DISPLAY FIFO\n");
    con_printf("BOTTOM BARS: LINE-START DMA\n");
    mmem_frame = memalign(32, 256 * 192 * 2);
    for (int y = 0; y < 192; y++)
    {
        bars[y] = RGB15((y >> 2) & 31, 4, 8);
        for (int x = 0; x < 256; x++)
            mmem_frame[y * 256 + x] = 0x8000 | RGB15(x >> 3, y >> 3, ((x ^ y) >> 3) & 31);
    }
    REG_DISPCNT = MODE_FIFO;
    dma_line_start(bars, (vu16 *)0x05000400);
    irq_add_vblank(mmem_vblank);
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    // Judge: what the main-memory display FIFO delivers is the frame in main memory: capture it
    // (source B = FIFO) and compare a grid of pixels. The sub screen's per-line bars (line-start DMA
    // into the sub palette) cannot be captured and stay unjudged.
    bool done = probe_capture_fifo();
    u32 n = 0, bad = 0;
    for (int y = 4; y < 192; y += 8)
        for (int x = 4; x < 256; x += 8, n++)
        {
            // capture source B (FIFO): bit 15 of the stored pixel is unused, compare the colour only
            u16 px = probe_px(x, y);
            bad += (px & 0x7FFF) != (mmem_frame[y * 256 + x] & 0x7FFF);
        }
    probe_release();
    probe_values(n, bad, done);
    finish(mmem_frame[96 * 256 + 128], bars[50], 0, 0);
}

static void obj_setup_main(void)
{
    swiFastCopy(cv_sprite_tiles, (void *)0x06400000, CV_SPRITE_TILES_SIZE / 4);
    swiCopy(cv_sprite_pal, CV_PAL_OBJ_MAIN, 16 | COPY_MODE_HWORD);
}

static void d_2d_obj_line(void)
{
    con_2d();
    demo_title("2D-OBJ-LINE");
    obj_setup_main();
    con_printf("120 SPRITES ON ONE BAND:\n");
    con_printf("THE LINE BUDGET RUNS OUT\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        oam_clear(ENG_MAIN);
        for (int i = 0; i < 120; i++)
            oam_add(ENG_MAIN, (i * 17 + f) & 255, 120 + (i % 3) * 4, SPR_16x16,
                    CV_SPR_BIGSHOT, 0, i & 3, 0, 0);
        vsync();
    }
    u32 count = oam_count(ENG_MAIN);
    // Judge (informational): the OBJ renderer has a per-line budget in cycles (a plain OBJ costs its
    // width). GBATEK's 1210 is the GBA figure and a DS line is longer, so this load (120 x 16 = 1920)
    // may or may not exceed it; no emulator drops sprites. Sprite i is in colour group i / 30 with priority 3 - group, so the highest group
    // that shows up on the line says how far the renderer got: group 3 (sprites 90-119) is
    // present when nothing is dropped.
    for (int g = 1; g < 4; g++)
        for (int k = 1; k < 16; k++)
            CV_PAL_OBJ_MAIN[g * 16 + k] = g == 1 ? RGB15(31, 0, 0) : g == 2 ? RGB15(0, 31, 0) : RGB15(0, 0, 31);
    oam_clear(ENG_MAIN);
    for (int i = 0; i < 120; i++)
        oam_add(ENG_MAIN, (i * 16) & 255, 150, SPR_16x16, CV_SPR_BIGSHOT, i / 30, 3 - i / 30, 0, 0);
    vsync();
    vsync();
    bool done = probe_capture();
    u32 mask = 0;
    for (int x = 0; x < 256; x++)
    {
        u16 px = probe_px(x, 157) & 0x7FFF;
        mask |= px == RGB15(31, 0, 0) ? 2 : px == RGB15(0, 31, 0) ? 4 : px == RGB15(0, 0, 31) ? 8 : 0;
    }
    probe_release();
    probe_values(0, 0, done);
    CV_RESULT->values[7] = mask;
    CV_RESULT->values[8] = mask & 8 ? 3 : mask & 4 ? 2 : mask & 2 ? 1 : 0;   // highest group on the line
    finish(count, 0, 0, 0);
}

static void d_2d_display(void)
{
    reset_display();
    vram_layout(VL_MMEM);
    u16 *fb = (u16 *)0x06800000;
    for (int i = 0; i < 256 * 192; i++)
        fb[i] = RGB15((i & 255) >> 3, (i >> 8) / 6, 10);
    REG_DISPCNT = MODE_FB0;
    for (int f = 0; f < 60; f++)
        vsync();
    mmem_frame = memalign(32, 256 * 192 * 2);
    for (int i = 0; i < 256 * 192; i++)
        mmem_frame[i] = 0x8000 | RGB15(10, (i & 255) >> 3, (i >> 8) / 6);
    for (int y = 0; y < 192; y++)
        bars[y] = RGB15(0, 0, y >> 3);
    dma_line_start(bars, (vu16 *)0x05000400);
    REG_DISPCNT = MODE_FIFO;
    irq_add_vblank(mmem_vblank);
    for (int f = 60; f < CHECKPOINT; f++)
        vsync();
    // Judge: the frame the VRAM display showed is still in bank A (read through LCDC, before the
    // console remaps it) ...
    u32 vn = 0, vbad = 0;
    for (int y = 4; y < 192; y += 8)
        for (int x = 4; x < 256; x += 8, vn++)
            vbad += (fb[y * 256 + x] & 0x7FFF) != (RGB15(x >> 3, y / 6, 10) & 0x7FFF);
    con_2d();
    vram_layout(VL_MMEM);
    REG_DISPCNT = MODE_FIFO;
    // ... and the main-memory display FIFO delivers the frame in main memory (capture source B).
    bool done = probe_capture_fifo();
    u32 n = 0, bad = 0;
    for (int y = 4; y < 192; y += 8)
        for (int x = 4; x < 256; x += 8, n++)
        {
            // capture source B (FIFO): bit 15 of the stored pixel is unused, compare the colour only
            u16 px = probe_px(x, y);
            bad += (px & 0x7FFF) != (mmem_frame[y * 256 + x] & 0x7FFF);
        }
    probe_release();
    probe_values(n, bad, done);
    CV_RESULT->values[7] = vn;
    CV_RESULT->values[8] = vbad;
    finish(fb[0], mmem_frame[0], 0, 0);
}

// ---------------------------------------------------------------------------
// 3D

static void d_3d_fifo(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-FIFO");
    con_printf("MODEL: DMA GEOMETRY FIFO\n");
    con_printf("GROUND: DIRECT PORT WRITES\n");
    CvStageData sd;
    stage_load(1, &sd);
    CvTerrain t = { sd.heights, sd.rows, false, RGB15(26, 23, 15), RGB15(16, 20, 10) };
    terrain_set(&t);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, F(3), F(4) - f * F(0.05)), m_vec(0, F(1.5), -f * F(0.05) - F(3)), 60, F(40));
        g3_texture_bind(TEX_TERRAIN);
        terrain_draw(-f * F(0.05), 20, false);
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16) | POLY_FORMAT_LIGHT0);
        g3_push_at(m_vec(0, F(2.5), -f * F(0.05)));
        g3_draw(&cv_models[MDL_PLAYER]);
        g3_pop();
        g3_flush_measured(true);
        vsync();
    }
    finish(g3_flushed_poly_count(), g3_flushed_vert_count(), cv_models[MDL_PLAYER].words, 0);
}

static void d_3d_matrix(void)
{
    con_3d(VL_CORE);
    vram_set(VB_C, 4, 0);
    demo_title("3D-MATRIX");
    con_printf("BOSS PARTS: PUSH/POP CHAIN\n");
    memset(&S, 0, sizeof(S));
    S.stage = 5;
    S.pl.pos = m_vec(0, F(2.5), F(6));
    boss_spawn(5, m_vec(0, F(2.5), 0));
    snd_bgm_stop();
    for (int f = 0; f < CHECKPOINT; f++)
    {
        S.frame = f;
        boss_update();
        S.bu[0].alive = false;
        memset(S.bu, 0, sizeof(S.bu));
        g3_camera(m_vec(F(3), F(4), F(5)), boss_pos(), 60, F(48));
        boss_draw();
        g3_flush_measured(true);
        vsync();
    }
    finish(boss_hp_max(), g3_flushed_poly_count(), 0, 0);
}

static void d_3d_light(void)
{
    con_3d(VL_CORRIDOR);
    demo_title("3D-LIGHT");
    con_printf("4 LIGHTS + MATERIAL/SPECULAR\n");
    g3_setup_lights(1);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        for (int i = 0; i < 4; i++)
        {
            int a = f * 300 + i * 8192;
            static const u16 cols[4] = { RGB15(31, 4, 2), RGB15(4, 8, 31), RGB15(31, 24, 2), RGB15(4, 31, 6) };
            glLight(i, cols[i], cosLerp(a) >> 3, floattov10(-0.4), sinLerp(a) >> 3);
        }
        g3_camera(m_vec(0, F(2), F(5)), m_vec(0, 0, 0), 60, F(40));
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(8) | POLY_FORMAT_LIGHT0
                  | POLY_FORMAT_LIGHT1 | POLY_FORMAT_LIGHT2 | POLY_FORMAT_LIGHT3);
        g3_texture_bind(TEX_COUNT);
        glPushMatrix();
        glRotateYi(f * 60);
        g3_draw(&cv_models[MDL_STATION]);
        glPopMatrix(1);
        g3_flush_measured(true);
        vsync();
    }
    finish(g3_flushed_poly_count(), 0, 0, 0);
}

static void d_3d_clip(void)
{
    con_3d(VL_CORRIDOR);
    demo_title("3D-CLIP");
    con_printf("CAMERA SKIMS THE WALLS\n");
    CvStageData sd;
    stage_load(4, &sd);
    CvTerrain t = { sd.heights, sd.rows, true, 0, 0 };
    terrain_set(&t);
    g3_setup_lights(1);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        int32_t z = -f * F(0.1);
        int32_t x = CV_FXMUL(F(3.2), m_sin(f * 300));
        g3_camera(m_vec(x, F(1.5), z), m_vec(-x, F(2), z - F(4)), 70, F(30));
        terrain_draw(z, 14, false);
        g3_flush_measured(false);
        vsync();
    }
    finish(g3_flushed_poly_count(), g3_flushed_vert_count(), 0, 0);
}

static void d_3d_poly(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-POLY");
    con_printf("TRANSLUCENT / SHADOW / CULL\n");
    for (int f = 0; f < CHECKPOINT; f++)
    {
        g3_camera(m_vec(0, F(3), F(5)), m_vec(0, F(0.5), 0), 60, F(40));
        // Floor
        g3_texture_bind(TEX_STRIPES);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
        glColor3b(255, 255, 255);
        glBegin(GL_QUADS);
        glTexCoord2t16(0, 0); glVertex3v16(F(-4), 0, F(-4));
        glTexCoord2t16(inttot16(128), 0); glVertex3v16(F(4), 0, F(-4));
        glTexCoord2t16(inttot16(128), inttot16(128)); glVertex3v16(F(4), 0, F(4));
        glTexCoord2t16(0, inttot16(128)); glVertex3v16(F(-4), 0, F(4));
        glEnd();
        // Back-culled and front-culled cubes.
        g3_texture_bind(TEX_HULL);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10) | POLY_FORMAT_LIGHT0);
        g3_push_at(m_vec(F(-2), F(1), 0)); g3_rot_y(f * 100); g3_draw(&cv_models[MDL_CUBE]); g3_pop();
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT | POLY_ID(11) | POLY_FORMAT_LIGHT0);
        g3_push_at(m_vec(F(2), F(1), 0)); g3_rot_y(f * 100); g3_draw(&cv_models[MDL_CUBE]); g3_pop();
        // Translucent airship (sorted after opaque), shadow under the player.
        glPolyFmt(POLY_ALPHA(16) | POLY_CULL_NONE | POLY_ID(20) | POLY_FORMAT_LIGHT0 | POLY_TRANS_SET_DEPTH);
        g3_push_at(m_vec(0, F(2.2), F(-1))); g3_rot_y(f * 60); g3_scale(F(0.6));
        g3_draw(&cv_models[MDL_AIRSHIP]); g3_pop();
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16) | POLY_FORMAT_LIGHT0);
        CvVec pp = m_vec(CV_FXMUL(F(1.5), m_sin(f * 200)), F(1.3), F(1.5));
        g3_push_at(pp); g3_draw(&cv_models[MDL_PLAYER]); g3_pop();
        shadow_volume(pp, F(0.5), 0);
        g3_flush_measured(true);
        vsync();
    }
    finish(g3_flushed_poly_count(), 0, 0, 0);
}

static void d_3d_test(void)
{
    con_3d(VL_TERRAIN);
    demo_title("3D-TEST");
    u32 inside = 0;
    int32_t clip[4] = { 0 };
    CvVec v = { 0 };
    for (int f = 0; f < CHECKPOINT; f++)
    {
        CvVec pos = m_vec(CV_FXMUL(F(5), m_sin(f * 300)), 0, 0);
        g3_camera(m_vec(0, F(1), F(5)), m_vec(0, 0, 0), 60, F(40));
        bool in = g3_box_test(m_sub(pos, m_vec(F(0.5), F(0.5), F(0.5))), m_vec(F(1), F(1), F(1)));
        inside += in;
        g3_pos_test(pos, clip);
        v = g3_vec_test(F(0.6), F(0.0), F(-0.8));
        g3_texture_bind(TEX_COUNT);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(10) | POLY_FORMAT_LIGHT0);
        g3_push_at(pos);
        g3_draw(&cv_models[MDL_CUBE]);
        g3_pop();
        text_printf(&con, 0, 3, in ? TP_GREEN : TP_RED, "BOX TEST: %s ", in ? "INSIDE " : "OUTSIDE");
        text_printf(&con, 0, 4, TP_WHITE, "POS %6ld %6ld %6ld %6ld", (long)clip[0], (long)clip[1],
                    (long)clip[2], (long)clip[3]);
        text_printf(&con, 0, 5, TP_WHITE, "VEC %6ld %6ld %6ld", (long)v.x, (long)v.y, (long)v.z);
        g3_flush_measured(true);
        vsync();
    }
    finish(inside, (u32)clip[3], (u32)v.x, (u32)v.z);
}

// Engine B: a 256x256 16bpp bitmap BG plus twelve bitmap sprites (bank D as sub OBJ).
static void sub_bitmap_setup(void)
{
    REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_ACTIVE
                    | DISPLAY_SPR_2D_BMP_256;
    REG_BG3CNT_SUB = 3 | (1 << 7) | (1 << 2) | (1 << 14);  // 16bpp 256x256 at 0
    REG_BG3PA_SUB = 256;
    REG_BG3PB_SUB = 0;
    REG_BG3PC_SUB = 0;
    REG_BG3PD_SUB = 256;
    REG_BG3X_SUB = 0;
    REG_BG3Y_SUB = 0;

    // Twelve bitmap sprites tile the screen from bank D (2D mapping, 256
    // pixels wide: tile index = x/8 + (y/8)*32).
    oam_clear(ENG_SUB);
    for (int i = 0; i < 12; i++)
    {
        int x = (i % 4) * 64, y = (i / 4) * 64;
        oam_add(ENG_SUB, x, y, SPR_64x64, (x / 8) + (y / 8) * 32, 15, 0, SPRF_BITMAP, 0);
    }
}

// ---------------------------------------------------------------------------
// Accuracy cross-check: one static scene that touches three features where
// melonDS's renderers were reported to differ from hardware (melonDS issues
// #2008 reversed UVs, #2503 semi-transparent OBJ over 3D, #2549 edge marking
// and line polygons). The scene grades itself from display captures, so the
// same numbers can be read on hardware, in the result block and on screen.

// Identity projection and modelview: clip x/y map straight to screen pixels.
// x is exact for any pixel; y is exact when y % 3 == 0.
#define ACC_PX(x) ((x) * 32 - 4096)
#define ACC_PY(y) (4096 - (y) * 128 / 3)
#define ACC_CAP   ((const u16 *)0x06860000)   // bank D in LCDC mode
#define ACC_CARD_X 88
#define ACC_LAMP_X 192
#define ACC_LAMP_Y 112

enum { ACC_NONE, ACC_ALPHA, ACC_DARK };

static const u16 acc_magenta = RGB15(31, 0, 31);
static const u16 acc_edge[2] = { RGB15(31, 31, 0), RGB15(0, 31, 31) };
static const u16 acc_string = RGB15(26, 26, 22);
static const u16 acc_grid = RGB15(8, 18, 9);
static const int acc_card_y[3] = { 48, 72, 96 };
// Texel columns per card row, left edge then right edge. The card is texels
// 16-31 with magenta cut lines at 15 and 32. Hardware samples a reversed span
// from its left value down, so 31->15 is the clean mirror and 32->16 shows the
// cut line at the left edge (#2008).
static const int acc_card_u[3][2] = { { 16, 32 }, { 31, 15 }, { 32, 16 } };

static u16 acc_card[64 * 16] __attribute__((aligned(32)));
static u8 acc_lamp[64 * 64 / 2] __attribute__((aligned(4)));
static int acc_tex;
static int acc_mode;

static void acc_make_card(void)
{
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int cx = x - 16;
            u16 c = 0;  // alpha 0: outside the card
            if (x == 15 || x == 32)
                c = 0x8000 | acc_magenta;
            else if (cx >= 0 && cx < 16)
            {
                // A paper figure facing right, so a mirror is easy to see.
                bool head = (cx - 7) * (cx - 7) + (y - 4) * (y - 4) <= 6;
                bool body = cx >= 5 && cx <= 9 && y >= 7 && y <= 12;
                bool arm = y == 8 && cx >= 9 && cx <= 13;
                bool legs = y >= 13 && y <= 14 && (cx == 5 || cx == 6 || cx == 8 || cx == 9);
                c = 0x8000 | (head || body || arm || legs ? RGB15(6, 4, 3) : RGB15(28, 25, 18));
            }
            acc_card[y * 64 + x] = c;
        }
    }
    DC_FlushRange(acc_card, sizeof(acc_card));
    glGenTextures(1, &acc_tex);
    glBindTexture(0, acc_tex);
    glTexImage2D(0, 0, GL_RGBA, 64, 16, 0, TEXGEN_TEXCOORD, acc_card);
}

static void acc_make_lamp(void)
{
    // 64x64 4bpp radial glow, 1D tile order, palette 15 ramps up to warm white.
    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int dx = 2 * x - 63, dy = 2 * y - 63;
            int r2 = dx * dx + dy * dy;
            int idx = r2 < 3600 ? 15 - r2 * 14 / 3600 : 0;
            int at = ((y / 8) * 8 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
            acc_lamp[at] |= (x & 1) ? idx << 4 : idx;
        }
    }
    swiCopy(acc_lamp, MAIN_OBJ, sizeof(acc_lamp) / 4 | COPY_MODE_WORD);
    for (int i = 0; i < 16; i++)
        CV_PAL_OBJ_MAIN[15 * 16 + i] = RGB15(i * 2, i * 3 / 2, i / 2);
}

static void acc_blend(int mode)
{
    // The lantern is a semi-transparent OBJ. GBATEK: such an OBJ alpha-blends
    // with a 2nd-target pixel below it whatever BLDCNT's effect bits say, and
    // the pixels it blends skip the darken effect. 0x0100 is the value from
    // #2503 that only the software renderer blended.
    static const u16 cnt[3] = { 0x0100, 0x0140, 0x01C1 };
    static const char *const names[3] = { "NONE ", "ALPHA", "DARK " };
    acc_mode = mode;
    REG_BLDCNT = cnt[mode];
    REG_BLDALPHA = 8 | (16 << 8);
    REG_BLDY = 10;
    text_printf(&con, 0, 3, TP_CYAN, "BLDCNT %04X %s", cnt[mode], names[mode]);
}

static void acc_rect(int x0, int y0, int x1, int y1)
{
    glBegin(GL_QUADS);
    glVertex3v16(ACC_PX(x0), ACC_PY(y0), 0);
    glVertex3v16(ACC_PX(x0), ACC_PY(y1), 0);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), 0);
    glVertex3v16(ACC_PX(x1), ACC_PY(y0), 0);
    glEnd();
}

// A triangle with two equal vertices is drawn as a 1-pixel line polygon.
static void acc_line(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_TRIANGLES);
    glVertex3v16(ACC_PX(x0), ACC_PY(y0), z);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), z);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), z);
    glEnd();
}

static void acc_color(u16 c)
{
    glColor3b((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
}

static void acc_draw(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Cards: polygon ID 0 is the clear-plane ID, so edge marking skips them
    // and the cut lines stay the only thing at their sides.
    glBindTexture(0, acc_tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor3b(255, 255, 255);
    for (int i = 0; i < 3; i++)
    {
        int y = acc_card_y[i], ul = acc_card_u[i][0], ur = acc_card_u[i][1];
        glBegin(GL_QUADS);
        glTexCoord2t16(inttot16(ul), 0);
        glVertex3v16(ACC_PX(ACC_CARD_X), ACC_PY(y), 0);
        glTexCoord2t16(inttot16(ul), inttot16(16));
        glVertex3v16(ACC_PX(ACC_CARD_X), ACC_PY(y + 18), 0);
        glTexCoord2t16(inttot16(ur), inttot16(16));
        glVertex3v16(ACC_PX(ACC_CARD_X + 16), ACC_PY(y + 18), 0);
        glTexCoord2t16(inttot16(ur), 0);
        glVertex3v16(ACC_PX(ACC_CARD_X + 16), ACC_PY(y), 0);
        glEnd();
    }

    // Puppets: IDs 8 and 16 pick edge colours 1 and 2. Unbind through libnds:
    // it skips glBindTexture() of the texture it believes is still bound.
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(8));
    acc_color(RGB15(12, 7, 5));
    acc_rect(150, 96, 162, 108);
    acc_rect(146, 108, 166, 132);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(16));
    acc_color(RGB15(5, 8, 13));
    acc_rect(190, 90, 202, 102);
    acc_rect(186, 102, 206, 126);

    // Strings in front of the puppets, floor grid behind them.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    acc_color(acc_string);
    acc_line(140, 54, 244, 54, -256);
    acc_line(150, 54, 152, 97, -256);
    acc_line(170, 54, 165, 109, -256);
    acc_line(190, 54, 196, 91, -256);
    acc_line(214, 54, 205, 103, -256);
    acc_color(acc_grid);
    for (int y = 150; y <= 186; y += 12)
        acc_line(132, y, 252, y, 256);
    for (int x = 140; x <= 240; x += 20)
        acc_line(x, 150, x, 186, 256);

    g3_flush(false);
    oam_clear(ENG_MAIN);
    oam_add(ENG_MAIN, ACC_LAMP_X, ACC_LAMP_Y, SPR_64x64, 0, 15, 1, SPRF_BLEND, 0);
}

static u16 acc_px(int x, int y)
{
    return ACC_CAP[y * 256 + x] & 0x7FFF;
}

static int acc_seam(int row)
{
    int n = 0;
    for (int y = acc_card_y[row] - 2; y < acc_card_y[row] + 21; y++)
        for (int x = ACC_CARD_X - 4; x < ACC_CARD_X + 20; x++)
            n += acc_px(x, y) == acc_magenta;
    return n;
}

// Pixels of the given colours on the puppet stage, outside the lantern.
static u32 acc_count(u16 a, u16 b)
{
    u32 n = 0;
    for (int y = 40; y < 192; y++)
    {
        for (int x = 128; x < 256; x++)
        {
            if (x >= ACC_LAMP_X && y >= ACC_LAMP_Y && y < ACC_LAMP_Y + 64)
                continue;
            u16 c = acc_px(x, y);
            n += c == a || c == b;
        }
    }
    return n;
}

static void acc_idle(int f)
{
    (void)f;
    if (cv_in.pressed & KEY_A)
        acc_blend((acc_mode + 1) % 3);
    acc_draw();
}

static void acc_ok(int x, int y, bool ok)
{
    text_put(&con, x, y, ok ? TP_GREEN : TP_RED, ok ? "OK" : "NG");
}

static void d_acc_stage(void)
{
    con_3d(VL_TERRAIN);
    // Bank D (sub OBJ in this layout, unused here) takes the captures.
    vram_set(VB_D, 0, 0);
    demo_title("ACC-STAGE");
    text_put(&con, 0, 4, TP_GRAY, "#2008 UV      #2549 EDGE+LINE");
    text_put(&con, 0, 6, TP_WHITE, "FWD 16>32");
    text_put(&con, 0, 9, TP_WHITE, "REV 31>15");
    text_put(&con, 0, 12, TP_WHITE, "REV 32>16");
    glClearColor(3, 3, 9, 31);
    glClearPolyID(0);
    glEnable(GL_OUTLINE);
    glSetOutlineColor(1, acc_edge[0]);
    glSetOutlineColor(2, acc_edge[1]);
    acc_make_card();
    acc_make_lamp();

    // The scene never moves; only BLDCNT changes at frames 40 and 80. Each
    // mode is captured once it has been on screen for a while.
    int seam[3] = { 0 };
    u32 edges = 0, lines = 0;
    u16 lamp[3] = { 0 }, out_none = 0, out_dark = 0;
    acc_blend(ACC_NONE);
    for (int f = 0; f < CHECKPOINT; f++)
    {
        if (f == 40)
            acc_blend(ACC_ALPHA);
        if (f == 80)
            acc_blend(ACC_DARK);
        if (f == 30 || f == 70 || f == 110)
            REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20);  // source A to bank D, 256x192
        if (f == 36 || f == 76 || f == 116)
        {
            bool done = !(REG_DISPCAPCNT & (1u << 31));
            lamp[acc_mode] = done ? acc_px(232, 132) : 0xFFFF;
            if (acc_mode == ACC_NONE)
            {
                for (int i = 0; i < 3; i++)
                    seam[i] = acc_seam(i);
                edges = acc_count(acc_edge[0], acc_edge[1]);
                lines = acc_count(acc_string, acc_grid);
                out_none = acc_px(140, 140);
            }
            if (acc_mode == ACC_DARK)
                out_dark = acc_px(140, 140);
        }
        acc_draw();
        vsync();
    }

    // Expectations from GBATEK and the hardware report in #2008: only the
    // uncompensated reverse row shows its cut line (18 pixels, card height);
    // the lantern blends the same way in all three modes; darken still works
    // outside it.
    bool seam_ok = seam[0] == 0 && seam[1] == 0 && seam[2] == 18;
    bool blend_ok = lamp[0] == lamp[1] && lamp[1] == lamp[2] && lamp[0] != out_none;
    bool dark_ok = out_dark != out_none;
    text_printf(&con, 0, 15, TP_WHITE, "SEAM %2d %2d %2d", seam[0], seam[1], seam[2]);
    acc_ok(14, 15, seam_ok);
    text_printf(&con, 0, 16, TP_WHITE, "EDGE %4lu", (unsigned long)edges);
    acc_ok(10, 16, edges > 0);
    text_printf(&con, 0, 17, TP_WHITE, "LINE %4lu", (unsigned long)lines);
    acc_ok(10, 17, lines > 0);
    text_printf(&con, 0, 18, TP_WHITE, "LAMP %04X %04X", lamp[0], lamp[1]);
    text_printf(&con, 0, 19, TP_WHITE, "     %04X", lamp[2]);
    acc_ok(10, 19, blend_ok);
    text_printf(&con, 0, 20, TP_WHITE, "DARK %04X %04X", out_none, out_dark);
    acc_ok(15, 20, dark_ok);

    volatile CvResult *r = CV_RESULT;
    r->values[4] = lamp[0];
    r->values[5] = lamp[1];
    r->values[6] = lamp[2];
    r->values[7] = out_none;
    r->values[8] = out_dark;
    u32 flags = seam_ok | (edges > 0) << 1 | (lines > 0) << 2 | blend_ok << 3 | dark_ok << 4;
    idle_fn = acc_idle;
    finish((u32)seam[0] | (u32)seam[1] << 8 | (u32)seam[2] << 16, edges, lines, flags);
}

// ---------------------------------------------------------------------------
// Capture-path cross-check: 3D on both screens and two-pass rendering, both
// through the display capture (melonDS issues #2618 and #2619). A block city
// is shown as a front view (top) and a plan view (bottom) on alternate
// frames, then as one large view drawn in a left and a right pass that the
// capture blends together. Engine B shows the captured pictures (bank C as a
// bitmap BG, bank D as bitmap OBJs); bank B holds the first pass.

#define DUAL_FRONT  RGB15(14, 6, 6)   // clear colour of the front view
#define DUAL_PLAN   RGB15(6, 6, 14)   // clear colour of the plan view
#define DUAL_LCDC_B ((const u16 *)0x06820000)
#define DUAL_LCDC_C ((const u16 *)0x06840000)
#define DUAL_LCDC_D ((const u16 *)0x06860000)
#define DUAL_SUB_C  ((const u16 *)0x06200000)  // bank C as sub BG
#define DUAL_SUB_D  ((const u16 *)0x06600000)  // bank D as sub OBJ
#define DUAL_CUBE   F(0.25)

enum { DK_PROBE, DK_FRONT, DK_PLAN, DK_SINGLE, DK_PASS1, DK_PASS2 };

static int dual_latency;          // frames from geometry to screen and capture
static bool dual_multi = true;    // after the checkpoint A switches modes
static u8 dual_kind[4], dual_cycle[4];
static int dual_shown = VB_D;     // bank engine B shows during two-pass mode
static int dual_polys[3];         // single pass, pass 1, pass 2
static bool dual_over[3];

static int dual_schedule(int c, int *cycle)
{
    *cycle = 0;
    if (c < 8)
        return DK_PROBE;
    if (c < 56 || (c >= 120 && !dual_multi))
        return (c & 1) ? DK_PLAN : DK_FRONT;
    if (c < 60)
        return DK_SINGLE;
    *cycle = ((c - 60) >> 1) & 1;
    return ((c - 60) & 1) ? DK_PASS2 : DK_PASS1;
}

// Applies to the frame the next VBlank shows, which carries the geometry of
// call c - dual_latency: display routing and capture. The clear colour is
// latched with the polygon list, so it is set with the geometry instead.
static void dual_config(int kind, int cycle)
{
    int target = cycle ? VB_D : VB_C, shown = cycle ? VB_C : VB_D;
    switch (kind)
    {
        case DK_FRONT:
            lcdMainOnTop();
            REG_DISPCNT = MODE_0_3D;
            vram_set(VB_D, 0, 0);
            vram_set(VB_C, 4, 0);
            REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(3) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
            break;
        case DK_PLAN:
            lcdMainOnBottom();
            REG_DISPCNT = MODE_0_3D;
            vram_set(VB_C, 0, 0);
            vram_set(VB_D, 4, 0);
            REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(2) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
            break;
        case DK_SINGLE:
            // Measured only: the screen shows the console, C and D are read back.
            lcdMainOnBottom();
            REG_DISPCNT = MODE_0_2D | ENABLE_3D | DISPLAY_BG1_ACTIVE;
            vram_set(VB_C, 0, 0);
            vram_set(VB_D, 0, 0);
            REG_DISPCAPCNT = 0;
            break;
        case DK_PASS1:
            // Engine B (top) shows the last composite; the 3D layer stays off
            // the main screen, which keeps the console. Clear alpha 0 marks the
            // pixels a pass did not draw.
            lcdMainOnBottom();
            REG_DISPCNT = MODE_0_2D | ENABLE_3D | DISPLAY_BG1_ACTIVE | (1 << 18);
            vram_set(target, 0, 0);
            vram_set(shown, 4, 0);
            dual_shown = shown;
            REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(1) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
            break;
        case DK_PASS2:
            // 3D + pass 1 from bank B (DISPCNT bits 18-19), both at full
            // weight: GBATEK multiplies each source by its alpha bit, so every
            // pixel comes from the pass that drew it.
            REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(target) | CV_CAP_SIZE_256x192
                           | CV_CAP_SRCA_3D | CV_CAP_SRC_BLEND | CV_CAP_EVA(16) | CV_CAP_EVB(16);
            break;
    }
}

static void dual_cube(int32_t x, int32_t y, int32_t z, int shade)
{
    int32_t s = DUAL_CUBE;
    glBegin(GL_QUADS);
    acc_color(RGB15(24 + shade, 23 + shade, 18));   // top
    glVertex3v16(x, y + s, z); glVertex3v16(x, y + s, z + s);
    glVertex3v16(x + s, y + s, z + s); glVertex3v16(x + s, y + s, z);
    acc_color(RGB15(18 + shade, 15, 11));           // front (+z)
    glVertex3v16(x, y, z + s); glVertex3v16(x + s, y, z + s);
    glVertex3v16(x + s, y + s, z + s); glVertex3v16(x, y + s, z + s);
    acc_color(RGB15(9, 9, 8));                      // back (-z)
    glVertex3v16(x, y, z); glVertex3v16(x, y + s, z);
    glVertex3v16(x + s, y + s, z); glVertex3v16(x + s, y, z);
    acc_color(RGB15(12, 11, 9));                    // left (-x)
    glVertex3v16(x, y, z); glVertex3v16(x, y, z + s);
    glVertex3v16(x, y + s, z + s); glVertex3v16(x, y + s, z);
    acc_color(RGB15(15, 13, 10));                   // right (+x)
    glVertex3v16(x + s, y, z); glVertex3v16(x + s, y + s, z);
    glVertex3v16(x + s, y + s, z + s); glVertex3v16(x + s, y, z + s);
    glEnd();
}

// nx x nz columns of 1-4 cubes, centred on the origin.
static void dual_city(int nx, int nz)
{
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
    for (int i = 0; i < nx; i++)
    {
        for (int k = 0; k < nz; k++)
        {
            int h = 1 + (i * 5 + k * 3 + (i ^ k)) % 4;
            int32_t x = (i - nx / 2) * DUAL_CUBE, z = (k - nz / 2) * DUAL_CUBE;
            for (int j = 0; j < h; j++)
                dual_cube(x, j * DUAL_CUBE, z, j);
        }
    }
}

// Perspective view; half -1 / 0 / 1 = left half, whole screen, right half.
static void dual_camera(CvVec eye, CvVec at, int32_t upz, int half)
{
    const int32_t t = 591, r = 788;  // near 0.25, 60 degrees vertical, 4:3
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf32(half > 0 ? 0 : -r, half < 0 ? 0 : r, -t, t, F(0.25), F(20));
    if (half < 0)
        glViewport(0, 0, 127, 191);
    else if (half > 0)
        glViewport(128, 0, 255, 191);
    else
        glViewport(0, 0, 255, 191);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(eye.x, eye.y, eye.z, at.x, at.y, at.z, 0, upz ? 0 : F(1), upz);
}

static void dual_geometry(int c, int kind)
{
    switch (kind)
    {
        case DK_PROBE:
        {
            // A full-screen quad whose colour names the call that sent it.
            static const u16 col[3] = { RGB15(31, 0, 0), RGB15(0, 31, 0), RGB15(0, 0, 31) };
            glViewport(0, 0, 255, 191);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
            acc_color(col[c % 3]);
            acc_rect(0, 0, 256, 192);
            break;
        }
        case DK_FRONT:
            glClearColor(14, 6, 6, 31);
            dual_camera(m_vec(0, F(0.9), F(4.2)), m_vec(0, F(0.4), 0), 0, 0);
            dual_city(8, 6);
            break;
        case DK_PLAN:
            glClearColor(6, 6, 14, 31);
            dual_camera(m_vec(0, F(4.4), 0), m_vec(0, 0, 0), F(-1), 0);
            dual_city(8, 6);
            break;
        default:
            // Alpha 0: the pixels a pass leaves empty drop out of the blend.
            glClearColor(0, 0, 0, 0);
            dual_camera(m_vec(0, F(3.2), F(4.6)), m_vec(0, F(0.2), F(0.2)), 0,
                        kind == DK_PASS1 ? -1 : kind == DK_PASS2 ? 1 : 0);
            dual_city(22, 14);
            break;
    }
}

static u32 dual_hash(const u16 *p)
{
    u32 h = 2166136261u;
    for (int i = 0; i < 256 * 192; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static int dual_count(const u16 *p, u16 colour)
{
    int n = 0;
    for (int i = 0; i < 256 * 192; i++)
        n += (p[i] & 0x7FFF) == colour;
    return n;
}

// One VBlank's work: route the frame now due, then send this call's geometry.
static void dual_call(int c)
{
    int cycle, kind = dual_schedule(c, &cycle);
    dual_kind[c & 3] = kind;
    dual_cycle[c & 3] = cycle;
    int s = c - dual_latency;
    if (c >= 8 && s >= 8)
        dual_config(dual_kind[s & 3], dual_cycle[s & 3]);
    dual_geometry(c, kind);
    g3_flush_measured(false);
    int slot = kind == DK_SINGLE ? 0 : kind == DK_PASS1 ? 1 : kind == DK_PASS2 ? 2 : -1;
    if (slot >= 0)
    {
        dual_polys[slot] = g3_flushed_poly_count();
        dual_over[slot] = g3_overflowed();
    }
}

static void dual_idle(int f)
{
    if (cv_in.pressed & KEY_A)
        dual_multi = !dual_multi;
    dual_call(CHECKPOINT + f);
}

static void d_acc_dual(void)
{
    con_3d(VL_DUAL3D);
    vram_set(VB_B, 0, 0);   // first-pass buffer; this demo draws no textures
    glDisable(GL_TEXTURE_2D);
    sub_bitmap_setup();
    BG_PALETTE_SUB[0] = RGB15(2, 2, 4);
    oam_clear(ENG_MAIN);
    demo_title("ACC-DUAL");
    REG_DISPCNT = MODE_0_3D;
    lcdMainOnTop();

    u32 front_d = 0, plan_c = 0, cross = 0, hash_d = 0, hash_c = 0;
    u32 left = 0, right = 0, comp_hash = 0;
    for (int c = 0; c < CHECKPOINT; c++)
    {
        if (c == 3)
            REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(1) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
        if (c == 6)
        {
            // The probe colour in the capture armed at call 3 names the call
            // whose geometry that frame showed.
            u16 p = DUAL_LCDC_B[96 * 256 + 128] & 0x7FFF;
            int k = p == RGB15(31, 0, 0) ? 0 : p == RGB15(0, 31, 0) ? 1 : p == RGB15(0, 0, 31) ? 2 : -1;
            dual_latency = k < 0 ? 9 : (3 - k) % 3;
        }
        if (c == 59)
        {
            // The last dual frames: D holds the front view, C the plan view.
            front_d = dual_count(DUAL_LCDC_D, DUAL_FRONT);
            plan_c = dual_count(DUAL_LCDC_C, DUAL_PLAN);
            cross = (u32)dual_count(DUAL_LCDC_D, DUAL_PLAN) << 16 | dual_count(DUAL_LCDC_C, DUAL_FRONT);
            hash_d = dual_hash(DUAL_LCDC_D);
            hash_c = dual_hash(DUAL_LCDC_C);
        }
        if (c == CHECKPOINT - 1)
        {
            // The composite engine B shows now.
            const u16 *p = dual_shown == VB_C ? DUAL_SUB_C : DUAL_SUB_D;
            for (int y = 0; y < 192; y++)
                for (int x = 0; x < 256; x++)
                    if (p[y * 256 + x] & 0x8000)
                    {
                        if (x < 128)
                            left++;
                        else
                            right++;
                    }
            comp_hash = dual_hash(p);
        }
        dual_call(c);
        vsync();
    }

    bool dual_ok = front_d > 0 && plan_c > 0 && cross == 0;
    bool single_over = dual_over[0];
    bool passes_ok = !dual_over[1] && !dual_over[2] && dual_polys[1] > 0 && dual_polys[2] > 0;
    bool halves_ok = left > 0 && right > 0;
    // Rows 1-7 below the title; finish() adds the checkpoint at rows 8-9.
    con_printf("LATENCY %d FRAME\n", dual_latency);
    con_printf("DUAL  FRONT %5lu PLAN %5lu\n", (unsigned long)front_d, (unsigned long)plan_c);
    con_printf("ONE PASS %4d POLYS OVER %d\n", dual_polys[0], single_over);
    con_printf("PASSES %4d + %4d\n", dual_polys[1], dual_polys[2]);
    con_printf("HALVES %5lu / %5lu\n", (unsigned long)left, (unsigned long)right);
    con_printf("TOP %08lX\n", (unsigned long)comp_hash);
    con_printf("A: DUAL VIEW / TWO-PASS VIEW\n");
    acc_ok(29, 2, dual_ok);
    acc_ok(29, 3, single_over);
    acc_ok(29, 4, passes_ok);
    acc_ok(29, 5, halves_ok);

    volatile CvResult *r = CV_RESULT;
    r->values[4] = (u32)dual_polys[0] | (u32)dual_over[0] << 16;
    r->values[5] = (u32)dual_polys[1] | (u32)dual_over[1] << 16;
    r->values[6] = (u32)dual_polys[2] | (u32)dual_over[2] << 16;
    r->values[7] = comp_hash;
    r->values[8] = hash_d;
    r->values[9] = hash_c;
    r->values[10] = cross;
    u32 flags = dual_ok | single_over << 1 | passes_ok << 2 | halves_ok << 3
              | (dual_latency <= 1) << 4;
    idle_fn = dual_idle;
    finish((u32)dual_latency, front_d << 16 | plan_c, left << 16 | right, flags);
}

// ---------------------------------------------------------------------------
// Same-screen two-pass rendering at 60 fps with a moving split, the scheme
// melonDS #2618 reports blinking on master (the left part goes black on about
// every other frame). Each frame renders one side of the split live and takes
// the other side from the previous frame's capture. Three ways to bring the
// captured side back, one per build:
//   tex    the capture as a direct-colour texture behind the live pass
//   bg     the capture as a 16bpp bitmap BG3 behind the 3D layer
//   relay  left always captured, right always live: even frames show the
//          last full picture in VRAM display mode while 3D draws the left
//          side; odd frames show 3D + BG3 and capture the graphics screen.
//   rear   the capture as the 3D rear plane (clear bitmap, texture slot 2;
//          bank B holds a far rear-plane depth in slot 3). The -read and
//          -write builds also let the CPU read, or read and write back, one
//          word of the capture before it becomes the rear plane: no change
//          on hardware, but emulators sync captured VRAM on CPU access.
//   hdma   the scheme of the homebrew in #2618: the top screen is engine B's
//          16bpp bitmap BG2 (bank C, BG2Y = -1), filled by a repeating
//          HBlank DMA from capture bank D, one line per HBlank. Even frames
//          capture the right pass alone and the DMA reads one line ahead
//          (the old full picture, before the capture overwrites it); odd
//          frames blend the left pass with D and the DMA reads the line just
//          captured. An emulator that runs captures late hands odd frames
//          the even frame's half picture: the left part goes missing.
// The bottom screen (engine B, bank H) carries the console; in hdma engine A
// is at the bottom with the console on BG1 (bank E).

enum { SPLIT_TEX, SPLIT_BG, SPLIT_RELAY, SPLIT_REAR, SPLIT_REAR_READ, SPLIT_REAR_WRITE,
       SPLIT_HDMA };

#define SPLIT_HDMA_SRC (*(vu32 *)(0x040000B0 + CV_DMA_CH_HBLANK * 12))
#define SPLIT_HDMA_DST (*(vu32 *)(0x040000B4 + CV_DMA_CH_HBLANK * 12))
#define SPLIT_HDMA_CNT (*(vu32 *)(0x040000B8 + CV_DMA_CH_HBLANK * 12))
#define SPLIT_SUB_C    ((const vu16 *)0x06200000)   // bank C as sub BG

static int split_variant;

static bool split_rear(void)
{
    return split_variant >= SPLIT_REAR && split_variant <= SPLIT_REAR_WRITE;
}

// hdma: even frames hold the right pass alone (P), odd frames blend (Q).
static bool split_hdma_p(int s)
{
    return !(s & 1);
}

// Moves every 8 frames, so both passes of a pair share it.
static int split_x(int s)
{
    return 48 + ((s >> 3) * 37) % 160;
}

static void split_config(int s)
{
    int x = (s & 1) ? VB_D : VB_C, y = (s & 1) ? VB_C : VB_D;
    u32 cap3d = CV_CAP_ENABLE | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
    u32 bmp = MODE_5_2D | ENABLE_3D | DISPLAY_BG0_ACTIVE | DISPLAY_BG3_ACTIVE;
    switch (split_variant)
    {
        case SPLIT_TEX:
            REG_DISPCNT = MODE_0_3D;
            vram_set(x, 0, 0);
            vram_set(y, 3, 1);   // texture slot 1: last frame's capture
            REG_DISPCAPCNT = cap3d | CV_CAP_BANK(x);
            break;
        case SPLIT_BG:
            REG_DISPCNT = bmp;
            vram_set(x, 0, 0);
            vram_set(y, 1, 0);   // main BG 0x06000000: last frame's capture
            // Source A is the whole engine output (3D + the bitmap BG), not only the 3D layer:
            // when the split moves left, the newly exposed part comes from last frame's bitmap.
            REG_DISPCAPCNT = (cap3d & ~CV_CAP_SRCA_3D) | CV_CAP_BANK(x);
            break;
        case SPLIT_REAR:
        case SPLIT_REAR_READ:
        case SPLIT_REAR_WRITE:
        {
            // Banks A and C take turns: one captures, the other is the rear
            // plane colour in texture slot 2.
            x = (s & 1) ? VB_C : VB_A;
            y = (s & 1) ? VB_A : VB_C;
            REG_DISPCNT = MODE_0_3D;
            vram_set(x, 0, 0);
            if (split_variant != SPLIT_REAR)
            {
                // y is still LCDC here, holding last frame's capture.
                vu16 *w = (vu16 *)(0x06800000 + (y == VB_A ? 0 : 0x40000));
                u16 v = w[0];
                if (split_variant == SPLIT_REAR_WRITE)
                    w[0] = v;
            }
            vram_set(y, 3, 2);
            REG_DISPCAPCNT = cap3d | CV_CAP_BANK(x);
            break;
        }
        case SPLIT_RELAY:
            if (!(s & 1))
            {
                // VRAM display of bank D (the last full picture); 3D draws
                // the left side into bank C.
                REG_DISPCNT = (2u << 16) | (3u << 18) | ENABLE_3D;
                vram_set(VB_C, 0, 0);
                REG_DISPCAPCNT = cap3d | CV_CAP_BANK(2);
            }
            else
            {
                // Right side live over the left side from bank C; the
                // graphics screen (BG + 3D) goes to bank D.
                REG_DISPCNT = bmp;
                vram_set(VB_C, 1, 0);
                REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_SIZE_256x192 | CV_CAP_BANK(3);
            }
            break;
        case SPLIT_HDMA:
        {
            // 3D is only captured; engine A shows the console. DISPCNT names
            // bank D as capture source B for the blend frames.
            REG_DISPCNT = CV_DISP_MODE_NORMAL | ENABLE_3D | DISPLAY_BG1_ACTIVE
                        | CV_DISP_VRAM_BLOCK(VB_D);
            u32 line = split_hdma_p(s) ? 1 : 0;
            if (split_hdma_p(s))
                REG_DISPCAPCNT = cap3d | CV_CAP_SRC_A | CV_CAP_BANK(VB_D);
            else
                REG_DISPCAPCNT = cap3d | CV_CAP_SRC_BLEND | CV_CAP_EVA(16) | CV_CAP_EVB(16)
                               | CV_CAP_BANK(VB_D);
            // HBlank N copies line N + line of bank D to the same line of
            // the sub BG; BG2Y = -1 shows it one scanline later.
            SPLIT_HDMA_CNT = 0;
            SPLIT_HDMA_SRC = 0x06860000 + line * 512;
            SPLIT_HDMA_DST = 0x06200000 + line * 512;
            SPLIT_HDMA_CNT = CV_DMA_ON | CV_DMA_HBLANK | CV_DMA_REPEAT | CV_DMA_32 | 128;
            break;
        }
    }
}

// Sub BG pixels that are transparent or black, every 4th line. The scene
// covers every pixel with a non-black colour; black is the 3D clear colour of
// the side a pass leaves out (master GL hands it over opaque).
static u32 split_hdma_holes(void)
{
    u32 n = 0;
    for (int y = 0; y < 192; y += 4)
        for (int x = 0; x < 256; x++)
        {
            u16 p = SPLIT_SUB_C[y * 256 + x];
            n += !(p & 0x8000) || !(p & 0x7FFF);
        }
    return n;
}

static void split_scene(int s)
{
    // Every pixel of a pass is covered (back wall, ground), so black on
    // screen can only come from a missing capture.
    int split = split_x(s);
    // Even frames: left side, odd frames: right side (hdma: the other way
    // round, so a blend frame pairs with the frame before it).
    bool right = (s & 1) ^ (split_variant == SPLIT_HDMA);
    const int32_t t = 591, r = 788;
    int32_t xs = -r + 2 * r * split / 256;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf32(right ? xs : -r, right ? r : xs, -t, t, F(0.25), F(20));
    if (right)
        glViewport(split, 0, 255, 191);
    else
        glViewport(0, 0, split - 1, 191);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(0, F(1.6), F(3.6), 0, F(0.3), 0, 0, F(1), 0);
    GFX_TEX_FORMAT = 0;
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
    glBegin(GL_QUADS);
    acc_color(RGB15(5, 7, 14));   // back wall
    glVertex3v16(F(-7), F(-1), F(-2.5)); glVertex3v16(F(7), F(-1), F(-2.5));
    glVertex3v16(F(7), F(5), F(-2.5)); glVertex3v16(F(-7), F(5), F(-2.5));
    acc_color(RGB15(10, 8, 5));   // ground
    glVertex3v16(F(-7), 0, F(-2.5)); glVertex3v16(F(-7), 0, F(3));
    glVertex3v16(F(7), 0, F(3)); glVertex3v16(F(7), 0, F(-2.5));
    glEnd();
    dual_city(10, 8);
}

static void split_geometry(int c)
{
    if (c < 8)
    {
        dual_geometry(c, DK_PROBE);
        return;
    }
    glClearColor(0, 0, 0, 0);
    // The rear plane bit is latched with the polygon list, like the colour.
    if (split_rear())
        glEnable(GL_CLEAR_BMP);
    if (split_variant == SPLIT_TEX)
    {
        // Last frame's capture, 256x256 direct colour at texture slot 1, on
        // a full-screen quad at the far end; texels with alpha 0 are skipped.
        glViewport(0, 0, 255, 191);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        GFX_TEX_FORMAT = (0x20000 >> 3) | (5 << 20) | (5 << 23) | (7 << 26);
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
        glColor3b(255, 255, 255);
        glBegin(GL_QUADS);
        glTexCoord2t16(0, 0);
        glVertex3v16(ACC_PX(0), ACC_PY(0), F(0.99));
        glTexCoord2t16(0, inttot16(192));
        glVertex3v16(ACC_PX(0), ACC_PY(192), F(0.99));
        glTexCoord2t16(inttot16(256), inttot16(192));
        glVertex3v16(ACC_PX(256), ACC_PY(192), F(0.99));
        glTexCoord2t16(inttot16(256), 0);
        glVertex3v16(ACC_PX(256), ACC_PY(0), F(0.99));
        glEnd();
    }
    split_scene(c);
}

static void split_call(int c)
{
    int s = c - dual_latency;
    if (c >= 8 && s >= 8)
        split_config(s);
    split_geometry(c);
    g3_flush(false);
}

static void split_idle(int f)
{
    split_call(CHECKPOINT + f);
}

static void split_demo(int variant, const char *name)
{
    split_variant = variant;
    reset_display();
    vram_layout(VL_DUAL3D);
    vram_set(VB_B, 0, 0);    // probe capture
    vram_off(VB_E);          // keep 0x06000000 for the bitmap BG
    vram_set(VB_H, 1, 0);    // sub BG: console
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP | GL_BLEND | GL_ALPHA_TEST);
    if (variant == SPLIT_TEX)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);
    lcdMainOnTop();
    REG_DISPCNT = MODE_0_3D;
    REG_BG0CNT = 0;
    REG_BG3CNT = 1 | (1 << 7) | (1 << 2) | (1 << 14);  // 16bpp 256x256 at 0, priority 1
    REG_BG3PA = 256;
    REG_BG3PB = 0;
    REG_BG3PC = 0;
    REG_BG3PD = 256;
    REG_BG3X = 0;
    REG_BG3Y = 0;
    CV_PAL_BG_MAIN[0] = RGB15(12, 12, 12);   // backdrop: grey, not black
    if (variant == SPLIT_HDMA)
    {
        // Engine A at the bottom: console on BG1 (bank E). Engine B on top:
        // the 16bpp bitmap the HBlank DMA fills (bank C), one line late.
        lcdMainOnBottom();
        vram_set(VB_E, 1, 0);
        vram_off(VB_H);
        vram_set(VB_C, 4, 0);
        for (int i = 0; i < 256 * 256; i++)
            ((vu16 *)SPLIT_SUB_C)[i] = 0;
        REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE;
        REG_BG1CNT = 31 << 8;
        text_load_tiles(MAIN_BG);
        text_load_palette(CV_PAL_BG_MAIN);
        CV_PAL_BG_MAIN[0] = RGB15(1, 2, 6);
        text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
        REG_DISPCNT_SUB = MODE_5_2D | DISPLAY_BG2_ACTIVE;
        REG_BG2CNT_SUB = (1 << 7) | (1 << 2) | (1 << 14);  // 16bpp 256x256 at 0
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
        REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
        REG_BG0CNT_SUB = 7 << 8;
        text_load_tiles(SUB_BG);
        text_load_palette(CV_PAL_BG_SUB);
        CV_PAL_BG_SUB[0] = RGB15(1, 2, 6);
        text_init(&con, (u16 *)(SUB_BG + 7 * 2048), 0);
    }
    con_init(&con);
    fx_bright(FX_BOTH, 0);
    demo_title(name);

    // hdma: alpha-0 pixels in the sub BG after each of the last 16 frames.
    u32 holes_p = 0, holes_q = 0, bad_p = 0, bad_q = 0;
    for (int c = 0; c < CHECKPOINT; c++)
    {
        if (c == 3)
            REG_DISPCAPCNT = CV_CAP_ENABLE | CV_CAP_BANK(1) | CV_CAP_SIZE_256x192 | CV_CAP_SRCA_3D;
        if (c == 6)
        {
            u16 p = DUAL_LCDC_B[96 * 256 + 128] & 0x7FFF;
            int k = p == RGB15(31, 0, 0) ? 0 : p == RGB15(0, 31, 0) ? 1 : p == RGB15(0, 0, 31) ? 2 : -1;
            dual_latency = k < 0 ? 9 : (3 - k) % 3;
            if (split_rear())
            {
                // Rear plane depth: farthest, no fog, for every pixel.
                for (int i = 0; i < 256 * 256; i++)
                    ((vu16 *)DUAL_LCDC_B)[i] = 0x7FFF;
                vram_set(VB_B, 3, 3);
            }
        }
        split_call(c);
        vsync();
        if (variant == SPLIT_HDMA && c >= CHECKPOINT - 16)
        {
            // The frame that just ended ran split_config(c - latency).
            u32 h = split_hdma_holes();
            if (split_hdma_p(c - dual_latency))
            {
                holes_p += h;
                bad_p += h != 0;
            }
            else
            {
                holes_q += h;
                bad_q += h != 0;
            }
        }
    }
    con_printf("LATENCY %d FRAME\n", dual_latency);
    con_printf("SPLIT X %d (MOVES EVERY 8)\n", split_x(CHECKPOINT - dual_latency));
    if (variant == SPLIT_HDMA)
    {
        con_printf("SUB BG HOLES, 8 FRAMES EACH:\n");
        con_printf(" CAPTURE ONLY %lu (%lu FR)\n", (unsigned long)holes_p, (unsigned long)bad_p);
        con_printf(" BLEND        %lu (%lu FR)\n", (unsigned long)holes_q, (unsigned long)bad_q);
    }
    idle_fn = split_idle;
    finish((u32)dual_latency, (u32)variant, (u32)split_x(CHECKPOINT - dual_latency),
           bad_q << 8 | bad_p);
}

static void d_acc_split_rear(void)  { split_demo(SPLIT_REAR, "ACC-SPLIT-REAR"); }
static void d_acc_split_hdma(void) { split_demo(SPLIT_HDMA, "ACC-SPLIT-HDMA"); }

// ---------------------------------------------------------------------------

typedef struct {
    const char *name;
    void (*fn)(void);
} DemoEntry;

// The ROM demo names carry the l4- prefix: their result-block ids differ from
// the Corvane, Hushwell, Lightenette and Elevane ones (the same demo code runs
// in Corvane under its plain name).
static const DemoEntry demos[] = {
    { "l4-sys-dma-gfx", d_sys_dma_gfx },     { "l4-sys-dma-disp", d_sys_dma_disp },
    { "l4-2d-obj-line", d_2d_obj_line },     { "l4-2d-display", d_2d_display },
    { "l4-3d-fifo", d_3d_fifo },             { "l4-3d-matrix", d_3d_matrix },
    { "l4-3d-light", d_3d_light },           { "l4-3d-clip", d_3d_clip },
    { "l4-3d-poly", d_3d_poly },             { "l4-3d-test", d_3d_test },
    { "l4-acc-stage", d_acc_stage },         { "l4-acc-dual", d_acc_dual },
    { "l4-acc-split-rear", d_acc_split_rear }, { "l4-acc-split-hdma", d_acc_split_hdma },
};

void demo_main(const char *name)
{
    demo_id = hash_name(name);
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    for (unsigned i = 0; i < sizeof(demos) / sizeof(demos[0]); i++)
    {
        if (strcmp(demos[i].name, name) == 0)
        {
            demos[i].fn();
            break;
        }
    }
    con_2d();
    con_printf("UNKNOWN DEMO: %s\n", name);
    for (;;)
        sys_vsync();
}
