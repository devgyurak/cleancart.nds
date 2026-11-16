// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Demo builds (make DEMO=<name>): one measurement scene as its own ROM, for
// emulator CI and the QA sheet.
//
// "rehearsal" is the Elevane rehearsal (game/rehearsal.c). The "l3-*" demos are
// the level 3 (timing) scenes taken over from corvane: the display-capture routing
// latency and the same-screen two-pass schemes (melonDS #2618 / #2619), and the
// HBlank DMA backdrop. Each writes the demo result block at 0x0237F000 at frame
// 120: u32 'CVER', u32 FNV-1a(demo name), u32 frame, u32 values[13]. The names carry
// the l3- prefix so their block ids differ from corvane's.

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/sdcard.h"
#include "core/sys.h"
#include "demo/demo.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
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

// Engine A: BG0 text console on VRAM A (2D demos).
static void con_2d(void)
{
    display_reset();
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
    display_reset();
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
    con_printf("ELEVANE DEMO: %s\n", name);
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
    // Only when the card holds cleancart/elevane/ (core/sdcard.h): the block goes to the SD card.
    if (sd_result_save("elevane", demo_name))
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

// ---------------------------------------------------------------------------
// ROM-side judging (corvane demo.c): engine A's own output is captured into bank D and
// compared with what GBATEK says must appear; counts go to values[4..6].

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

// Sound judge: the ARM7 reports which of its 16 channels have Start/Busy set (SOUNDCNT bit 31)
// and the state of the two capture units once per frame in the input packet.

// ---------------------------------------------------------------------------
// System

static u16 line_table[192 * 2] __attribute__((aligned(32)));

static void d_sys_dma_hblank(void)
{
    con_2d();
    demo_title("SYS-DMA-HBLANK");
    con_printf("BACKDROP GRADIENT PER LINE\n");
    for (int y = 0; y < 192; y++)
        line_table[y] = RGB15(y / 8, 4, 31 - y / 8);
    dma_hblank_start(line_table, CV_PAL_BG_MAIN, 1);
    irq_add_vblank(dma_hblank_rearm);
    u32 h0 = cv_hblank_count;
    for (int f = 0; f < CHECKPOINT; f++)
        vsync();
    // Judge: the HBlank DMA writes entry y into the backdrop for line y (GBATEK:
    // an HBlank DMA runs at the start of every visible line's HBlank), so the
    // captured backdrop of line y has line_table[y]'s colour.
    bool done = probe_capture();
    u32 n = 0, bad = 0;
    for (int y = 8; y < 184; y += 8, n++)
    {
        u16 px = probe_px(250, y);
        bad += !(px & 0x8000) || (px & 0x7FFF) != (line_table[y] & 0x7FFF);
    }
    probe_release();
    probe_values(n, bad, done);
    finish(line_table[100], cv_hblank_count - h0, 0, 0);
}

// ---------------------------------------------------------------------------
// Helpers of the capture demos

// x is exact for any pixel; y is exact when y % 3 == 0.
#define ACC_PX(x) ((x) * 32 - 4096)
#define ACC_PY(y) (4096 - (y) * 128 / 3)

static void acc_rect(int x0, int y0, int x1, int y1)
{
    glBegin(GL_QUADS);
    glVertex3v16(ACC_PX(x0), ACC_PY(y0), 0);
    glVertex3v16(ACC_PX(x0), ACC_PY(y1), 0);
    glVertex3v16(ACC_PX(x1), ACC_PY(y1), 0);
    glVertex3v16(ACC_PX(x1), ACC_PY(y0), 0);
    glEnd();
}

static void acc_color(u16 c)
{
    glColor3b((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
}

static void acc_ok(int x, int y, bool ok)
{
    text_put(&con, x, y, ok ? TP_GREEN : TP_RED, ok ? "OK" : "NG");
}

// Engine B: bank C as a 16bpp bitmap BG, bank D as twelve 64x64 bitmap OBJs (what gfx/dual.c does).
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
    oam_clear(ENG_SUB);
    for (int i = 0; i < 12; i++)
    {
        int x = (i % 4) * 64, y = (i / 4) * 64;
        oam_add(ENG_SUB, x, y, SPR_64x64, (x / 8) + (y / 8) * 32, 15, 0, SPRF_BITMAP, 0);
    }
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
    display_reset();
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
    idle_fn = split_idle;
    finish((u32)dual_latency, (u32)variant, (u32)split_x(CHECKPOINT - dual_latency),
           bad_q << 8 | bad_p);
}

static void d_acc_split_tex(void)   { split_demo(SPLIT_TEX, "ACC-SPLIT-TEX"); }
static void d_acc_split_bg(void)    { split_demo(SPLIT_BG, "ACC-SPLIT-BG"); }
static void d_acc_split_relay(void) { split_demo(SPLIT_RELAY, "ACC-SPLIT-RELAY"); }
static void d_acc_split_rear(void)  { split_demo(SPLIT_REAR, "ACC-SPLIT-REAR"); }
static void d_acc_split_rear_read(void)  { split_demo(SPLIT_REAR_READ, "ACC-SPLIT-REAR-READ"); }
static void d_acc_split_rear_write(void) { split_demo(SPLIT_REAR_WRITE, "ACC-SPLIT-REAR-WRITE"); }
static void d_acc_split_hdma(void) { split_demo(SPLIT_HDMA, "ACC-SPLIT-HDMA"); }

// ---------------------------------------------------------------------------

typedef struct {
    const char *name;
    void (*fn)(void);
} DemoEntry;

static const DemoEntry demos[] = {
    { "l3-sys-dma-hblank", d_sys_dma_hblank },
    { "l3-acc-dual", d_acc_dual },
    { "l3-acc-split-tex", d_acc_split_tex },
    { "l3-acc-split-bg", d_acc_split_bg },
    { "l3-acc-split-relay", d_acc_split_relay },
    { "l3-acc-split-rear", d_acc_split_rear },
    { "l3-acc-split-rear-read", d_acc_split_rear_read },
    { "l3-acc-split-rear-write", d_acc_split_rear_write },
    { "l3-acc-split-hdma", d_acc_split_hdma },
};

void demo_main(const char *name)
{
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
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
