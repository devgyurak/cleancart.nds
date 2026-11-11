// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Accuracy judge. One static scene touches three features where melonDS's
// renderers were reported to differ from hardware (melonDS issues #2008
// reversed UVs, #2503 semi-transparent OBJ over 3D, #2549 edge marking and
// line polygons). The scene grades itself from display captures, so the same
// numbers can be read on any emulator or on hardware: in the result block at
// CV_RESULT_ADDR and on screen. It uses no NitroFS file.

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "core/input.h"
#include "core/sys.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"
#include "judge/judge.h"

#define MAIN_BG  ((u8 *)0x06000000)
#define MAIN_OBJ ((u8 *)0x06400000)
#define CHECKPOINT 120

static CvText con;

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

void judge_lamp_load(void)
{
    acc_make_lamp();
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


static void acc_ok(int x, int y, bool ok)
{
    text_put(&con, x, y, ok ? TP_GREEN : TP_RED, ok ? "OK" : "NG");
}

void judge_frame(void)
{
    if (cv_in.pressed & KEY_A)
        acc_blend((acc_mode + 1) % 3);
    acc_draw();
}

// Engine A: BG0 = 3D, BG1 = text console, OBJ for the lantern.
static void judge_setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST);
    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
    REG_BG0CNT = 1;
    REG_BG1CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
    text_clear(&con);
    g3_setup_lights(0);
    REG_DISPCNT_SUB = MODE_0_2D;
    fx_bright(FX_BOTH, 0);
}

static void frame_end(void)
{
    oam_commit();
    sys_vsync();
    input_update();
}

void judge_text(int y, int pal, const char *s)
{
    text_put(&con, 0, y, pal, s);
}

void judge_run(JudgeResult *res)
{
    judge_setup();
    text_put(&con, 0, 0, TP_YELLOW, "LIGHTENETTE JUDGE");
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
        frame_end();
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

    u32 flags = seam_ok | (edges > 0) << 1 | (lines > 0) << 2 | blend_ok << 3 | dark_ok << 4;
    u32 v0 = (u32)seam[0] | (u32)seam[1] << 8 | (u32)seam[2] << 16;
    volatile CvResult *r = CV_RESULT;
    r->demo_id = JUDGE_ID;
    r->frame = cv_frame;
    r->values[0] = v0;
    r->values[1] = edges;
    r->values[2] = lines;
    r->values[3] = flags;
    r->values[4] = lamp[0];
    r->values[5] = lamp[1];
    r->values[6] = lamp[2];
    r->values[7] = out_none;
    r->values[8] = out_dark;
    r->magic = CV_RESULT_MAGIC;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    text_put(&con, 0, 22, TP_GRAY, "A: BLDCNT  B/START: BACK");
    text_printf(&con, 0, 1, TP_GREEN, "CHECKPOINT %08lX %08lX", (unsigned long)v0,
                (unsigned long)edges);
    text_printf(&con, 0, 2, TP_GREEN, "           %08lX %08lX", (unsigned long)lines,
                (unsigned long)flags);

    if (res)
    {
        for (int i = 0; i < 3; i++)
        {
            res->seam[i] = seam[i];
            res->lamp[i] = lamp[i];
        }
        res->edges = edges;
        res->lines = lines;
        res->out_none = out_none;
        res->out_dark = out_dark;
        res->flags = flags;
    }
}

void judge_idle(void)
{
    for (;;)
    {
        judge_frame();
        frame_end();
    }
}
