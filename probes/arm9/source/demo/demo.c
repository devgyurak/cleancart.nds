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
#include "core/sdcard.h"
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
static const char *demo_name;
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
    // Only when the card holds cleancart/probes/ (core/sdcard.h): the block goes to the SD card.
    if (sd_result_save("probes", demo_name))
    {
        con_color(TP_GREEN);
        con_printf("RESULT SAVED\n");
        con_color(TP_WHITE);
    }
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
    // Boot leaves a SwapBuffers pending (glInit): let it happen first, or every
    // output of the measuring loop is one frame late (latency 2 instead of 1).
    while (GFX_STATUS & (1u << 27))
        ;
    sys_vsync();
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

// rear-plane: the rear-plane build of the probe collection (not a Corvane demo): the same
//          scheme as rear, and at the last call it counts the opaque samples of the captured
//          pass (the Elevane showcase preset 2 measurement).
enum { SPLIT_TEX, SPLIT_BG, SPLIT_RELAY, SPLIT_REAR, SPLIT_REAR_READ, SPLIT_REAR_WRITE,
       SPLIT_HDMA, SPLIT_REAR_PLANE };

#define SPLIT_HDMA_SRC (*(vu32 *)(0x040000B0 + CV_DMA_CH_HBLANK * 12))
#define SPLIT_HDMA_DST (*(vu32 *)(0x040000B4 + CV_DMA_CH_HBLANK * 12))
#define SPLIT_HDMA_CNT (*(vu32 *)(0x040000B8 + CV_DMA_CH_HBLANK * 12))
#define SPLIT_SUB_C    ((const vu16 *)0x06200000)   // bank C as sub BG

static int split_variant;

static bool split_rear(void)
{
    return (split_variant >= SPLIT_REAR && split_variant <= SPLIT_REAR_WRITE)
        || split_variant == SPLIT_REAR_PLANE;
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
        case SPLIT_REAR_PLANE:
        {
            // Banks A and C take turns: one captures, the other is the rear
            // plane colour in texture slot 2.
            x = (s & 1) ? VB_C : VB_A;
            y = (s & 1) ? VB_A : VB_C;
            REG_DISPCNT = MODE_0_3D;
            vram_set(x, 0, 0);
            if (split_variant == SPLIT_REAR_READ || split_variant == SPLIT_REAR_WRITE)
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

// rear-plane: opaque samples (every 12th pixel, 4096 in all; alpha bit set) of the bank that
// split_config() just mapped to LCDC for capturing. It holds the pass captured two calls ago, drawn over the
// rear plane (the pass before it): both halves of the picture are opaque only when the rear plane
// colour reached the 3D engine. total / left half / right half.
static u32 rear_n, rear_l, rear_r;

static void rear_sample(int s)
{
    const vu16 *b = (s & 1) ? (const vu16 *)0x06840000 : (const vu16 *)0x06800000;   // C : A
    rear_n = rear_l = rear_r = 0;
    for (int i = 0; i < 256 * 192; i += 12)
    {
        u32 o = b[i] >> 15;
        rear_n += o;
        if (i % 256 < 128)
            rear_l += o;
        else
            rear_r += o;
    }
}

static void split_call(int c)
{
    int s = c - dual_latency;
    if (c >= 8 && s >= 8)
        split_config(s);
    if (split_variant == SPLIT_REAR_PLANE && c == CHECKPOINT - 1 && s >= 8)
        rear_sample(s);
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
    // Boot leaves a SwapBuffers pending (glInit): let it happen first, or every
    // output of the measuring loop is one frame late (latency 2 instead of 1).
    while (GFX_STATUS & (1u << 27))
        ;
    sys_vsync();
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
    if (variant == SPLIT_REAR_PLANE)
    {
        con_printf("REAR PLANE OPAQUE %lu OF 4096\n", (unsigned long)rear_n);
        con_printf(" LEFT %lu RIGHT %lu\n", (unsigned long)rear_l, (unsigned long)rear_r);
        volatile CvResult *r = CV_RESULT;
        r->values[4] = rear_n;
        r->values[5] = rear_l;
        r->values[6] = rear_r;
    }
    idle_fn = split_idle;
    finish((u32)dual_latency, (u32)variant, (u32)split_x(CHECKPOINT - dual_latency),
           bad_q << 8 | bad_p);
}

static void d_acc_split_rear(void)  { split_demo(SPLIT_REAR, "ACC-SPLIT-REAR"); }
static void d_acc_split_hdma(void) { split_demo(SPLIT_HDMA, "ACC-SPLIT-HDMA"); }
static void d_acc_rear_plane(void) { split_demo(SPLIT_REAR_PLANE, "ACC-REAR-PLANE"); }

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Scanline timing (written for Heddlemere's review: games/heddlemere/arm9/source/judge/timing.c measures the same
// things as gating items; what no document settles lives here)

#define PR_DISPSTAT (*(volatile u16 *)0x04000004)
#define PR_VCOUNT   (*(volatile u16 *)0x04000006)
#define PR_GXSTAT   (*(volatile u32 *)0x04000600)
#define PR_SWAP     (*(volatile u32 *)0x04000540)
#define PR_GXPORT(p) (*(volatile u32 *)(0x04000400 + ((p) << 2)))
#define PR_T0 (*(volatile u16 *)0x04000100)
#define PR_T1 (*(volatile u16 *)0x04000104)
#define PR_NOW() ({ u32 h1_, l_, h2_; do { h1_ = PR_T1; l_ = PR_T0; h2_ = PR_T1; } while (h1_ != h2_); h1_ << 16 | l_; })
#define PR_NOINLINE __attribute__((noinline))

// Bounded polling waits (not the vsync() calls before them, which wait for the VBlank IRQ): once one gave up (a stuck VBlank) every later wait returns at once and the probe reports 0xFFFF.
static bool pr_timeout;
#define PR_SPIN_LIMIT 8000000u
#define PR_SPIN(cond) do { u32 n_ = 0; while (!pr_timeout && (cond)) { if (++n_ > PR_SPIN_LIMIT) { pr_timeout = true; break; } } } while (0)

// A sled of 65 ARM nops: jumping in at n runs n + 1 of them (one ARM9 cycle each in ITCM). Same as Heddlemere's judge.
static ITCM_CODE PR_NOINLINE __attribute__((target("arm"))) void pr_sled(u32 n)
{
    asm volatile("add pc, pc, %0, lsl #2\n\t"
                 "nop\n\t"
                 ".rept 64\n\t"
                 "nop\n\t"
                 ".endr" :: "r"(63 - (n & 63)) : "memory");
}

static u32 pr_lcg = 0x2545F491u;

// One frame from a VBlank rise to the next; `line` != 0: when the VCOUNT register reads `line` it is set to `value`.
// A random delay after each VBlank fall puts the poll grid at an independent phase for the rise that follows, so the
// mean over many frames carries no quantisation bias (one frame alone is off by up to a poll period).
static ITCM_CODE PR_NOINLINE u32 pr_frame(int line, int value)
{
    PR_SPIN(PR_DISPSTAT & 1);
    pr_lcg = pr_lcg * 1664525u + 1013904223u;
    pr_sled(pr_lcg >> 24);
    PR_SPIN(!(PR_DISPSTAT & 1));
    u32 a = PR_NOW();
    if (line)
    {
        PR_SPIN(PR_VCOUNT != line);
        if (!pr_timeout)
            PR_VCOUNT = (u16)value;
    }
    PR_SPIN(PR_DISPSTAT & 1);
    pr_lcg = pr_lcg * 1664525u + 1013904223u;
    pr_sled(pr_lcg >> 24);
    PR_SPIN(!(PR_DISPSTAT & 1));
    return PR_NOW() - a;
}

#define PR_FRAMES 8

static u32 pr_sum(int line, int value)
{
    u32 sum = 0;
    for (int i = 0; i < PR_FRAMES; i++)
    {
        pr_frame(0, 0);          // a frame without a write between two with one: the next starts from a normal state
        sum += pr_frame(line, value);
    }
    return sum;
}

// Mean frame length with a write minus the mean without, x16: in whole 2130-cycle lines (rounded), and the signed
// cycles left over x16 (only meaningful when the write added whole lines).
static void pr_vcount_case(int line, int value, u32 normal_sum, s32 *lines, s32 *residual16)
{
    u32 w = pr_sum(line, value);
    s32 d16 = (s32)(w - normal_sum) * 16 / PR_FRAMES;
    s32 half = 2130 * 8;
    s32 l = d16 >= 0 ? (d16 + half) / (2130 * 16) : -((-d16 + half) / (2130 * 16));
    *lines = l;
    *residual16 = d16 - l * 2130 * 16;
}

// VCOUNT writes: GBATEK allows them in 202..212 only; how many lines does a write add, where does it apply, and what is
// ignored? Cases: 202 written at line 212, 202 at 205, 210 at 212, 50 at line 100 (outside the range). A write at line
// 100 may tear the picture of that frame on a console.
static void d_vcount_write(void)
{
    con_2d();
    demo_title("VCOUNT-WRITE");
    con_printf("measuring...\n");
    vsync();
    u32 ime = REG_IME;
    REG_IME = 0;
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    CV_TM_DATA(0) = 0;
    CV_TM_DATA(1) = 0;
    CV_TM_CNT(1) = CV_TM_ON | CV_TM_CASCADE;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1;
    pr_timeout = false;
    u32 normal_sum = 0;
    for (int i = 0; i < PR_FRAMES; i++)
    {
        pr_frame(0, 0);
        normal_sum += pr_frame(0, 0);
    }
    static const int at[4] = { 212, 205, 212, 100 }, val[4] = { 202, 202, 210, 50 };
    s32 lines[4], res16[4];
    for (int i = 0; i < 4; i++)
        pr_vcount_case(at[i], val[i], normal_sum, &lines[i], &res16[i]);
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    REG_IME = ime;
    bool ok = !pr_timeout;
    for (int i = 0; i < 4; i++)
        con_printf("WRITE %3d AT %3d: %2ld LINES, RES %ld/16\n", val[i], at[i], ok ? (long)lines[i] : -1L, (long)res16[i]);
    con_printf("FRAME %lu CYCLES\n", (unsigned long)(normal_sum / PR_FRAMES));
    volatile CvResult *r = CV_RESULT;
    // The residual means something only when whole lines were added.
    for (int i = 0; i < 4; i++)
        r->values[4 + i] = ok && lines[i] > 0 ? (u32)res16[i] : 0xFFFF;
    r->values[8] = normal_sum / PR_FRAMES;
    finish(ok ? (u32)lines[0] : 0xFFFF, ok ? (u32)lines[1] : 0xFFFF, ok ? (u32)lines[2] : 0xFFFF, ok ? (u32)lines[3] : 0xFFFF);
}

// Texture memory read late: a full-screen quad uses palette entry 1 of the texture palette (bank G); the frame is swapped
// at the VBlank and the palette is rewritten through the LCDC mapping at the given line. The captured next frame shows
// how many of its 192 lines (one column) do not show the new colour (the old one, or black while the bank was in LCDC
// mode): the line from which a write is too late.
#define TL_A RGB15(31, 0, 0)
#define TL_B RGB15(0, 0, 31)
#define TL_TEX_OFS 0x10000      // texture 8x8 inside bank A, clear of what g3_texture_load put at the start
#define TL_PAL_OFS 0x3000       // palette inside bank G

static void tl_quad(void)
{
    PR_GXPORT(0x29) = 31u << 16 | 3u << 6;
    PR_GXPORT(0x2A) = 4u << 26 | (TL_TEX_OFS >> 3);
    PR_GXPORT(0x2B) = TL_PAL_OFS >> 4;
    PR_GXPORT(0x20) = 0x7FFF;
    PR_GXPORT(0x40) = 1;
    PR_GXPORT(0x22) = 0;
    PR_GXPORT(0x23) = 0xF000u | 0x1000u << 16, PR_GXPORT(0x23) = 0;
    PR_GXPORT(0x23) = 0xF000u | 0xF000u << 16, PR_GXPORT(0x23) = 0;
    PR_GXPORT(0x23) = 0x1000u | 0xF000u << 16, PR_GXPORT(0x23) = 0;
    PR_GXPORT(0x23) = 0x1000u | 0x1000u << 16, PR_GXPORT(0x23) = 0;
    PR_GXPORT(0x41) = 0;
}

static void tl_palette(u16 colour)
{
    vram_set(VB_G, 0, 0);
    ((vu16 *)0x06894000)[TL_PAL_OFS / 2 + 1] = colour;
    vram_set(VB_G, 3, 0);
}

static void tl_idle(void)
{
    u32 n = 0;
    while (n++ < 4000000u && (PR_GXSTAT & ((1u << 27) | (1u << 14) | 1u)))
        ;
}

// A polling wait that never ends (a stuck VBlank) ends the trial instead of hanging the ROM; the vsync() calls around it still need the VBlank IRQ.

static ITCM_CODE PR_NOINLINE void tl_frame(int line)
{
    PR_SPIN(PR_VCOUNT != 100);
    tl_quad();
    PR_SWAP = 0;
    for (int i = 0; i < 256 * 192 / 2; i++)
        ((vu32 *)PROBE_CAP)[i] = 0;
    REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20);   // captures the frame after this one
    u32 ime = REG_IME;
    REG_IME = 0;                                           // the write lands at the line asked for, not after an interrupt
    PR_SPIN(!(PR_DISPSTAT & 1));
    PR_SPIN(PR_VCOUNT != line);
    tl_palette(TL_B);
    REG_IME = ime;
}

static void d_tex_late(void)
{
    con_3d(VL_TERRAIN);
    demo_title("TEX-LATE");
    vram_set(VB_A, 0, 0);
    for (int i = 0; i < 32; i++)
        ((vu16 *)(0x06800000 + TL_TEX_OFS))[i] = 0x0101;
    vram_set(VB_A, 3, 0);
    *(volatile u16 *)0x04000060 |= 1;             // DISP3DCNT: texture mapping on
    PR_GXPORT(0x10) = 0, PR_GXPORT(0x15) = 0;
    PR_GXPORT(0x10) = 1, PR_GXPORT(0x15) = 0;
    static const int lines[8] = { 200, 213, 214, 216, 192, 230, 250, 215 };
    pr_timeout = false;
    u32 bad[8];
    u32 done = 0;
    for (int k = 0; k < 8; k++)
    {
        tl_palette(TL_A);
        tl_quad();
        PR_SWAP = 0;
        for (int f = 0; f < 3; f++)
            vsync();
        tl_idle();
        vram_set(VB_D, 0, 0);
        tl_frame(lines[k]);
        for (int f = 0; f < 3; f++)
            vsync();
        tl_idle();
        bool ok = !(REG_DISPCAPCNT & (1u << 31)) && !pr_timeout;
        u32 b = 0;
        for (int y = 0; y < 192; y++)
            b += (probe_px(128, y) & 0x7FFF) != TL_B;
        bad[k] = ok ? b : 0xFFFF;
        done |= ok ? 1u << k : 0;
        probe_release();
        con_printf("WRITE AT %3d: %3lu NOT NEW\n", lines[k], (unsigned long)bad[k]);
    }
    volatile CvResult *r = CV_RESULT;
    for (int k = 4; k < 7; k++)
        r->values[k] = bad[k];
    r->values[7] = done;
    r->values[8] = bad[7];
    finish(bad[0], bad[1], bad[2], bad[3]);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} DemoEntry;

// The ROM demo names carry the l4- prefix: their result-block ids differ from
// the Corvane, Hushwell, Lightenette and Elevane ones (the same demo code runs
// in Corvane under its plain name).
// --- Fog boundary, FOG_SHIFT 11, screen-border edges, a queued 1-dot, AA lines, depth Equal +-1 ------
// Observations of the fog / edge / 1-dot corners that GBATEK words loosely or not at all and that
// Buoyane (games/buoyane) therefore does not judge. Nothing here is graded: the judge records what each
// emulator gives. Every number is read from a capture of the 3D layer alone (DISPCAPCNT bit 24).
//
//   values[0]  fog boundary: how many of the two quads at OFFSET + 0.25 / + 0.75 steps are unfogged
//              (GBATEK: the first boundary lies at OFFSET + STEP, so 2; melonDS shifts the table by one entry, so it agrees)
//   values[1]  the quad at OFFSET + 0.25 step | + 0.75 step << 16 (raw RGB555)
//   values[2]  FOG_SHIFT 11: bit 0 quad before the offset unfogged, bit 1 / bit 2 quads after it fully fogged
//   values[3]  the quad before the offset | the first quad after it << 16 (raw)
//   values[4]  screen-border edge: pixel at (0, 96) | pixel at (255, 96) << 16 with edge marking and a polygon on the border
//   values[5]  a 1-dot queued behind 24 matrix multiplies while DISP_1DOT_DEPTH is raised to 7FFF: 1 = drawn
//   values[6]  anti-aliased 1-pixel lines, pixels found: opaque | translucent (alpha 30) << 16, x 32..224
//   values[7]  depth Equal, one step from the base: pixels drawn at -1 step | +1 step << 16 (of 1024 each)
//   values[8]  the screen-border polygon without edge marking: 1 when it covers the pixel at (255, 96) (GBATEK "Polygon Size")
//   values[9]  fog on the alpha only, through a BG0 blend onto a backdrop (moved from Buoyane's fog scene, same coordinates):
//              bit 0 a fog face in front of FOG_OFFSET looks like a same-frame alpha 31 face without fog (the "fog alpha glitch"),
//              bit 1 the fog face behind the offset differs from that face and from the backdrop
//   values[10] the pixels read for values[9]: the near fog face | the unfogged reference << 16

#define FD_CLEAR RGB15(0, 0, 10)
#define FD_RED   RGB15(31, 0, 0)
#define FD_FOG   RGB15(0, 31, 31)

static const m4x4 fd_ident = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096 } };
static const m4x4 fd_dot = { { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 0, -4096, 0, 0, 0, 0 } };
static const m4x4 fd_px = { { 24576, 0, 0, 0, 0, -32768, 0, 0, 0, 0, 4096, 0, -12288, 12288, 0, 12288 } };

static int fd_pass;
static const u16 *const FD_CAP = (const u16 *)0x06860000;

static u16 fd_at(int x, int y)
{
    return FD_CAP[y * 256 + x] & 0x7FFF;
}

static void fd_idv(int x, int y, int z)
{
    glVertex3v16((x - 128) * 32, (96 - y) * 128 / 3, z);
}

static void fd_idquad(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_QUADS);
    fd_idv(x0, y0, z);
    fd_idv(x0, y1, z);
    fd_idv(x1, y1, z);
    fd_idv(x1, y0, z);
    glEnd();
}

static void fd_dotv(int x, int y, int32_t w)
{
    int32_t a = (x - 128) * 32, b = (96 - y) * 128 / 3;
    GFX_COLOR = RGB15(31, 31, 31);
    glVertex3v16(a * w / 4096, b * w / 4096, -w);
}

static void fd_pxslope(int x0, int y0, int x1, int y1, int z0, int z1)
{
    glBegin(GL_QUADS);
    glVertex3v16(x0 * 16, y0 * 16, z0);
    glVertex3v16(x0 * 16, y1 * 16, z0);
    glVertex3v16(x1 * 16, y1 * 16, z1);
    glVertex3v16(x1 * 16, y0 * 16, z1);
    glEnd();
}

static void fd_pxquad(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_QUADS);
    glVertex3v16(x0 * 16, y0 * 16, z);
    glVertex3v16(x0 * 16, y1 * 16, z);
    glVertex3v16(x1 * 16, y1 * 16, z);
    glVertex3v16(x1 * 16, y0 * 16, z);
    glEnd();
}

static void fd_draw(void)
{
    const u32 opaque = POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix4x4(&fd_ident);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    GFX_TEX_FORMAT = 0;
    switch (fd_pass)
    {
        case 0:   // FOG_SHIFT 0, offset 0x2000, step 0x400: quads at +0.25 and +0.75 step
            glPolyFmt(opaque | POLY_FOG);
            GFX_COLOR = FD_RED;
            fd_idquad(16, 40, 96, 100, 2 * 0x2100 * 4096 / 0x7FFF - 4096);
            fd_idquad(160, 40, 240, 100, 2 * 0x2300 * 4096 / 0x7FFF - 4096);
            break;
        case 1:   // FOG_SHIFT 11: before the offset, and two quads after it
            glPolyFmt(opaque | POLY_FOG);
            GFX_COLOR = FD_RED;
            fd_idquad(8, 40, 72, 100, 2 * 0x1F00 * 4096 / 0x7FFF - 4096);
            fd_idquad(96, 40, 160, 100, 2 * 0x2010 * 4096 / 0x7FFF - 4096);
            fd_idquad(184, 40, 248, 100, 2 * 0x3000 * 4096 / 0x7FFF - 4096);
            break;
        case 2:   // a polygon on the screen border with edge marking
            glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(8));
            GFX_COLOR = RGB15(10, 10, 10);
            fd_idquad(0, 40, 256, 150, 0);
            break;
        case 3:   // a far 1-dot queued behind a backlog of matrix work while the cut-off is raised
            glCutoffDepth(16);
            glMatrixMode(GL_PROJECTION);
            glLoadMatrix4x4(&fd_dot);
            glMatrixMode(GL_MODELVIEW);
            while (GFX_STATUS & (1u << 27))
                ;
            for (int i = 0; i < 24; i++)
                glMultMatrix4x4(&fd_ident);
            glPolyFmt(opaque);
            glBegin(GL_TRIANGLES);
            fd_dotv(128, 96, F(4));
            fd_dotv(128, 96, F(4));
            fd_dotv(128, 96, F(4));
            glEnd();
            glCutoffDepth(0x7FFF);
            break;
        case 4:   // anti-aliased 1-pixel lines
            glCutoffDepth(16);
            glMatrixMode(GL_PROJECTION);
            glLoadMatrix4x4(&fd_dot);
            glMatrixMode(GL_MODELVIEW);
            for (int k = 0; k < 2; k++)
            {
                glPolyFmt((k ? POLY_ALPHA(30) : POLY_ALPHA(31)) | POLY_CULL_NONE | POLY_ID(2 + k));
                glBegin(GL_TRIANGLES);
                fd_dotv(32, 96 + k * 24, F(1));
                fd_dotv(224, 96 + k * 24, F(1));
                fd_dotv(224, 96 + k * 24, F(1));
                glEnd();
            }
            break;
        case 6:   // fog on the alpha only: Buoyane's fog scene, a fog face in front of and behind FOG_OFFSET and a reference without fog
            glPolyFmt(opaque | POLY_FOG);
            GFX_COLOR = FD_RED;
            fd_idquad(16, 141, 112, 180, -F(0.5));
            fd_idquad(144, 141, 240, 180, F(0.5));
            glPolyFmt(opaque | POLY_ID(2));
            fd_idquad(16, 81, 112, 120, -F(0.5));
            break;
        case 7:   // depth Equal on a slope: Buoyane's depth scene, the band against a flat base and the decal against a slanted floor
            glMatrixMode(GL_PROJECTION);
            glLoadMatrix4x4(&fd_px);
            glMatrixMode(GL_MODELVIEW);
            glPolyFmt(opaque);
            GFX_COLOR = RGB15(12, 12, 12);
            fd_pxquad(32, 112, 224, 136, 6);
            fd_pxslope(16, 144, 240, 184, 300, 3000);
            glPolyFmt(opaque | POLY_DEPTH_TEST_EQUAL);
            GFX_COLOR = RGB15(0, 31, 0);
            fd_pxslope(32, 112, 224, 137, 3, 9);
            GFX_COLOR = RGB15(31, 0, 0);
            fd_pxslope(40, 144, 208, 185, 589, 2615);
            break;
        default:  // depth Equal, one step (raw z 3 and 5 against the base at 4) from the base
            glMatrixMode(GL_PROJECTION);
            glLoadMatrix4x4(&fd_px);
            glMatrixMode(GL_MODELVIEW);
            glPolyFmt(opaque);
            GFX_COLOR = RGB15(12, 12, 12);
            fd_pxquad(12, 72, 244, 104, 4);
            glPolyFmt(opaque | POLY_DEPTH_TEST_EQUAL);
            GFX_COLOR = RGB15(31, 31, 0);
            fd_pxquad(20, 72, 52, 105, 3);
            fd_pxquad(60, 72, 92, 105, 5);
            break;
    }
    g3_flush(false);
}

static void fd_capture(bool only_3d)
{
    vram_set(VB_D, 0, 0);
    for (int i = 0; i < 256 * 192 / 2; i++)
        ((vu32 *)FD_CAP)[i] = 0x55555555;
    for (int f = 0; f < 8; f++)
    {
        fd_draw();
        if (f == 4)
            REG_DISPCAPCNT = (1u << 31) | (only_3d ? 1u << 24 : 0) | (3 << 16) | (3 << 20);
        vsync();
    }
}

static void fd_setup(u32 d3, u16 clear)
{
    while (GFX_STATUS & (1u << 27))
        ;
    GFX_CONTROL = (u16)(d3 | 1u << 3);
    GFX_CLEAR_COLOR = clear | 31u << 16 | 63u << 24;
    GFX_CLEAR_DEPTH = 0x7FFF;
    GFX_FOG_COLOR = FD_FOG | 31u << 16;
    GFX_FOG_OFFSET = 0x2000;
    for (int i = 0; i < 8; i++)
        GFX_EDGE_TABLE[i] = RGB15(31, 31, 0);
    glCutoffDepth(0x7FFF);
}

static void d_3d_fog_dot(void)
{
    con_3d(VL_TERRAIN);
    vram_set(VB_D, 0, 0);
    demo_title("3D-FOG-DOT");
    REG_DISPCNT &= ~DISPLAY_BG1_ACTIVE;   // the console stays off while capturing
    u32 v[11] = { 0 };
    u32 pend = 0;

    // 0: fog boundary. FOG_SHIFT 0 (step 400h): entry 0 is 0, entries 1.. are 127, so the first boundary decides:
    // GBATEK starts the rise at OFFSET + STEP (both quads in entry 0, unfogged); melonDS shifts its density table by one entry
    // (RenderFogDensityTable[0] = table[0]), which gives the same result, so all profiles are expected to agree.
    fd_pass = 0;
    fd_setup(1u << 7, FD_CLEAR);
    glFogShift(0);
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = i < 1 ? 0 : 127;
    fd_capture(true);
    u16 q0 = fd_at(56, 70), q1 = fd_at(200, 70);
    v[0] = (q0 == FD_RED) + (q1 == FD_RED);
    v[1] = q0 | (u32)q1 << 16;

    // 1: FOG_SHIFT 11: no table step left; recorded as is
    fd_pass = 1;
    fd_setup(1u << 7, FD_CLEAR);
    glFogShift(11);
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = i < 16 ? 0 : 127;
    fd_capture(true);
    u16 a = fd_at(40, 70), b = fd_at(128, 70), c = fd_at(216, 70);
    v[2] = (a == FD_RED) | (b == FD_FOG) << 1 | (c == FD_FOG) << 2;
    v[3] = a | (u32)b << 16;

    // 2: screen border with edge marking
    fd_pass = 2;
    fd_setup(1u << 5, FD_CLEAR);
    fd_capture(true);
    v[4] = fd_at(0, 96) | (u32)fd_at(255, 96) << 16;

    // 2b: the same polygon without edge marking: does it cover x 255 at all (GBATEK "Polygon Size": the right edge may drop)
    fd_pass = 2;
    fd_setup(0, FD_CLEAR);
    fd_capture(true);
    pend = fd_at(255, 96) == RGB15(10, 10, 10);

    // 3: queued dot
    fd_pass = 3;
    fd_setup(0, FD_CLEAR);
    fd_capture(true);
    v[5] = fd_at(128, 96) != FD_CLEAR;

    // 4: anti-aliased 1-pixel lines
    fd_pass = 4;
    fd_setup(1u << 4, FD_CLEAR);
    fd_capture(true);
    u32 lo = 0, lt = 0;
    for (int x = 32; x < 224; x++)
    {
        lo += fd_at(x, 96) != FD_CLEAR;
        lt += fd_at(x, 120) != FD_CLEAR;
    }
    v[6] = lo | lt << 16;

    // 5: depth Equal, one step from the base
    fd_pass = 5;
    fd_setup(0, FD_CLEAR);
    fd_capture(true);
    u32 m1 = 0, p1 = 0;
    for (int y = 72; y < 104; y++)
        for (int x = 0; x < 32; x++)
        {
            m1 += fd_at(20 + x, y) == RGB15(31, 31, 0);
            p1 += fd_at(60 + x, y) == RGB15(31, 31, 0);
        }
    v[7] = m1 | p1 << 16;
    v[8] = pend;

    // 6: fog on the alpha only, through the BG0 blend onto a blue backdrop (the 3D layer has alpha 0 where nothing is drawn)
    fd_pass = 6;
    fd_setup(1u << 6 | 1u << 7, 0);
    GFX_CLEAR_COLOR = 0 | 0u << 16 | 63u << 24;
    GFX_FOG_COLOR = RGB15(31, 31, 31);   // alpha 0
    glFogOffset(0x3000);
    for (int i = 0; i < 32; i++)
        GFX_FOG_TABLE[i] = 64;
    u16 backdrop = CV_PAL_BG_MAIN[0];
    CV_PAL_BG_MAIN[0] = RGB15(0, 0, 31);
    REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG0 | BLEND_DST_BACKDROP;
    REG_BLDALPHA = 0x1010;
    fd_capture(false);
    REG_BLDCNT = 0;
    CV_PAL_BG_MAIN[0] = backdrop;
    u16 fnear = fd_at(64, 160), ffar = fd_at(192, 160), fref = fd_at(64, 100);
    v[9] = (fnear == fref) | (ffar != fref && ffar != RGB15(0, 0, 31)) << 1;
    v[10] = fnear | (u32)fref << 16;

    REG_DISPCAPCNT = 0;
    REG_DISPCNT |= DISPLAY_BG1_ACTIVE;
    volatile CvResult *r = CV_RESULT;
    for (int i = 4; i < 11; i++)
        r->values[i] = v[i];
    con_printf("FOG START %lu (2)  SHIFT11 %lu\n", (unsigned long)v[0], (unsigned long)v[2]);
    con_printf("BORDER %08lX  QUEUED %lu\n", (unsigned long)v[4], (unsigned long)v[5]);
    con_printf("AA LINES %08lX  EQ+-1 %08lX\n", (unsigned long)v[6], (unsigned long)v[7]);
    con_printf("RIGHT PIXEL %lu\n", (unsigned long)v[8]);
    con_printf("FOG ALPHA %lu  %08lX\n", (unsigned long)v[9], (unsigned long)v[10]);
    finish(v[0], v[1], v[2], v[3]);
}

// The Equal band and the decal of Buoyane's depth scene (games/buoyane/arm9/source/judge/lightcheck.c, draw_depth), the same
// coordinates and raw z values; hardware observation there (hardware run 1 and 2): band x 109..159, decal holes 960.
//   values[0]  left end of the band, x of the first pixel at y 124 that has the band colour (0 = none)
//   values[1]  right end of the band
//   values[2]  decal pixels not drawn over x 40..208, y 144..184
static void d_3d_zeq_slope(void)
{
    con_3d(VL_TERRAIN);
    vram_set(VB_D, 0, 0);
    demo_title("3D-ZEQ-SLOPE");
    REG_DISPCNT &= ~DISPLAY_BG1_ACTIVE;   // the console stays off while capturing
    fd_pass = 7;
    fd_setup(0, FD_CLEAR);
    fd_capture(true);
    int left = -1, right = -1;
    for (int x = 32; x < 224; x++)
    {
        if (fd_at(x, 124) != RGB15(0, 31, 0))
            continue;
        if (left < 0)
            left = x;
        right = x;
    }
    u32 holes = 0;
    for (int y = 144; y < 184; y++)
        for (int x = 40; x < 208; x++)
            holes += fd_at(x, y) != RGB15(31, 0, 0);
    u32 l = left < 0 ? 0 : (u32)left, r = right < 0 ? 0 : (u32)right;
    REG_DISPCAPCNT = 0;
    REG_DISPCNT |= DISPLAY_BG1_ACTIVE;
    con_printf("BAND %lu..%lu\n", (unsigned long)l, (unsigned long)r);
    con_printf("DECAL HOLES %lu\n", (unsigned long)holes);
    finish(l, r, holes, 0);
}

static const DemoEntry demos[] = {
    { "l4-sys-dma-gfx", d_sys_dma_gfx },     { "l4-sys-dma-disp", d_sys_dma_disp },
    { "l4-2d-obj-line", d_2d_obj_line },     { "l4-2d-display", d_2d_display },
    { "l4-3d-fifo", d_3d_fifo },             { "l4-3d-matrix", d_3d_matrix },
    { "l4-3d-light", d_3d_light },           { "l4-3d-clip", d_3d_clip },
    { "l4-3d-poly", d_3d_poly },             { "l4-3d-test", d_3d_test },
    { "l4-acc-stage", d_acc_stage },         { "l4-acc-dual", d_acc_dual },
    { "l4-acc-split-rear", d_acc_split_rear }, { "l4-acc-split-hdma", d_acc_split_hdma },
    { "l4-acc-rear-plane", d_acc_rear_plane },
    { "l4-vcount-write", d_vcount_write },   { "l4-tex-late", d_tex_late },
    { "l4-3d-fog-dot", d_3d_fog_dot },       { "l4-3d-zeq-slope", d_3d_zeq_slope },
};

void demo_main(const char *name)
{
    demo_id = hash_name(name);
    demo_name = name;
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
