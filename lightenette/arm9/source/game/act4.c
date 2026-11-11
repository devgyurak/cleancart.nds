// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 4: untangle the strings. Four strings hang from the crossbar to four pegs
// on the board below, but their lower ends are mixed up, so they cross. Pick a
// peg and then a second one to swap the two strings' lower ends; the act is
// cleared when no strings cross.
//
// Strings are line polygons (a triangle with two equal vertices) on top of a dark
// quad, so the puzzle is playable on a renderer that draws them differently; the
// bottom screen lists which string sits on which peg and how many cross. Two
// more threads, one horizontal and one diagonal, hang under the board as plain
// line polygons without a quad. They are not part of the puzzle: after the clear
// the act captures its own display and counts their pixels (melonDS #2549:
// the renderers differ on horizontal and slanted line polygons; there is no
// hardware value yet, so the judge treats the counts as information).
//
// Result block (CvResult at CV_RESULT_ADDR, demo_id ACT4_ID), every frame:
// values[0] cleared, [1] horizontal thread pixels, [2] diagonal thread pixels,
// [3] strings' pegs packed 4 bits each, [4] crossings, [5] 1 once the display
// capture finished (the thread counts are only valid then).

#include <nds.h>
#include <stdio.h>

#include "core/hw.h"
#include "core/input.h"
#include "core/sys.h"
#include "game/act4.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"
#include "judge/capture.h"

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

#define ACT4_ID 0x34544341u  // "ACT4"

// Identity projection: clip x/y map straight to screen pixels. x is exact for
// any pixel; y is exact when y % 3 == 0.
#define PX(x) ((x) * 32 - 4096)
#define PY(y) (4096 - (y) * 128 / 3)
#define CAP   ((const u16 *)0x06860000)   // bank D in LCDC mode

#define STRINGS    4
#define Y_TOP      42
#define Y_PEG      120
#define CLEAR_HOLD 20
#define CAP_AFTER  4
#define READ_AFTER 10

// The two measured threads: from-to, and the colours that are counted.
#define H_X0 24
#define H_X1 232
#define H_Y  150
#define D_X0 24
#define D_Y0 156
#define D_X1 232
#define D_Y1 177

static const int col_x[STRINGS] = { 40, 96, 152, 208 };
static const u16 string_col[STRINGS] = { RGB15(31, 26, 6), RGB15(6, 28, 31), RGB15(12, 31, 12), RGB15(31, 14, 26) };
static const u16 hcol = RGB15(31, 24, 4);
static const u16 dcol = RGB15(4, 24, 31);

static CvText con;   // main engine text (BG1)
static CvText sub;   // sub engine text (BG0)

static int peg_of[STRINGS];   // string i hangs from top slot i to bottom peg peg_of[i]
static int cur, pick;

static void color(u16 c)
{
    glColor3b((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
}

static void rect(int x0, int y0, int x1, int y1, int z)
{
    glBegin(GL_QUADS);
    glVertex3v16(PX(x0), PY(y0), z);
    glVertex3v16(PX(x0), PY(y1), z);
    glVertex3v16(PX(x1), PY(y1), z);
    glVertex3v16(PX(x1), PY(y0), z);
    glEnd();
}

static void line(int x0, int y0, int x1, int y1, u16 c)
{
    color(c);
    glBegin(GL_TRIANGLES);
    glVertex3v16(PX(x0), PY(y0), -256);
    glVertex3v16(PX(x1), PY(y1), -256);
    glVertex3v16(PX(x1), PY(y1), -256);
    glEnd();
}

static void string_draw(int x0, int y0, int x1, int y1, u16 c)
{
    glColor3b(60, 40, 25);
    glBegin(GL_QUADS);
    glVertex3v16(PX(x0 - 1), PY(y0), -200);
    glVertex3v16(PX(x1 - 1), PY(y1), -200);
    glVertex3v16(PX(x1 + 1), PY(y1), -200);
    glVertex3v16(PX(x0 + 1), PY(y0), -200);
    glEnd();
    line(x0, y0, x1, y1, c);
}

static int crossings(void)
{
    int n = 0;
    for (int i = 0; i < STRINGS; i++)
        for (int j = i + 1; j < STRINGS; j++)
            n += peg_of[i] > peg_of[j];
    return n;
}

static void draw_scene(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));

    glColor3b(90, 60, 40);
    rect(24, 36, 232, 40, -128);
    glColor3b(70, 50, 36);
    rect(24, Y_PEG, 232, Y_PEG + 8, -128);
    for (int i = 0; i < STRINGS; i++)
    {
        string_draw(col_x[i], Y_TOP, col_x[peg_of[i]], Y_PEG, string_col[i]);
        color(i == pick ? RGB15(31, 8, 8) : RGB15(22, 18, 14));
        rect(col_x[i] - 3, Y_PEG - 3, col_x[i] + 3, Y_PEG + 3, -100);
    }
    color(RGB15(30, 12, 10));
    rect(col_x[cur] - 5, Y_PEG + 12, col_x[cur] + 5, Y_PEG + 18, -100);

    line(H_X0, H_Y, H_X1, H_Y, hcol);
    line(D_X0, D_Y0, D_X1, D_Y1, dcol);
    g3_flush(false);
}

static u16 cap_px(int x, int y)
{
    return CAP[y * 256 + x] & 0x7FFF;
}

static u32 count_col(u16 c, int y0, int y1)
{
    u32 n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = 16; x < 240; x++)
            n += cap_px(x, y) == c;
    return n;
}

static void sub_draw(void)
{
    for (int j = 0; j < STRINGS; j++)
    {
        int x = col_x[j] / 8 - 1;
        int s = 0;
        for (int i = 0; i < STRINGS; i++)
            if (peg_of[i] == j)
                s = i;
        text_printf(&sub, x, 8, TP_GRAY, "PEG%d", j + 1);
        text_printf(&sub, x, 10, TP_WHITE, " S%d", s + 1);
        text_put(&sub, x, 12, TP_YELLOW, j == pick ? " ** " : (j == cur ? " ^^ " : "    "));
    }
    int n = crossings();
    text_printf(&sub, 1, 15, n ? TP_RED : TP_GREEN, "CROSSINGS %d  ", n);
}

static void setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP
              | GL_OUTLINE | GL_TEXTURE_2D);
    glEnable(GL_BLEND | GL_ALPHA_TEST);

    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE;
    REG_BG0CNT = 1;
    REG_BG1CNT = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&con, (u16 *)(MAIN_BG + 31 * 2048), 0);
    text_clear(&con);

    REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE;
    REG_BG0CNT_SUB = 0 | (0 << 2) | (31 << 8);
    text_load_tiles(SUB_BG);
    text_load_palette(CV_PAL_BG_SUB);
    CV_PAL_BG_SUB[0] = RGB15(2, 2, 6);
    text_init(&sub, (u16 *)(SUB_BG + 31 * 2048), 0);
    text_clear(&sub);

    g3_setup_lights(0);
    glClearColor(3, 3, 9, 31);
    glClearPolyID(0);
    fx_bright(FX_BOTH, 0);
}

// Choose a peg; the second choice swaps the strings on the two pegs.
static void choose(void)
{
    if (pick < 0)
    {
        pick = cur;
        return;
    }
    if (pick != cur)
        for (int i = 0; i < STRINGS; i++)
            peg_of[i] = peg_of[i] == pick ? cur : peg_of[i] == cur ? pick : peg_of[i];
    pick = -1;
}

static LtScene act4_loop(const Act4Step *steps, int nsteps, int end_frame, Act4Outcome *out)
{
    setup();
    static const int start[STRINGS] = { 1, 3, 0, 2 };
    for (int i = 0; i < STRINGS; i++)
        peg_of[i] = start[i];
    cur = 0;
    pick = -1;
    int hold = 0, f = 0, clear_frame = -1;
    bool cleared = false, measured = false, was_touching = false;
    u32 hpx = 0, dpx = 0;

    text_put(&con, 0, 0, TP_YELLOW, "ACT 4  UNTANGLE THE STRINGS");
    text_put(&sub, 1, 0, TP_YELLOW, "ACT 4  UNTANGLE THE STRINGS");
    text_put(&sub, 1, 2, TP_GRAY, "L/R OR TAP: CHOOSE A PEG");
    text_put(&sub, 1, 3, TP_GRAY, "A OR TAP: SWAP THE TWO STRINGS");
    text_put(&sub, 1, 4, TP_GRAY, "NO STRINGS CROSSING: CLEAR");
    text_put(&sub, 1, 23, TP_GRAY, steps ? "REHEARSAL" : "B: BACK");

    for (;; f++)
    {
        u16 pressed = cv_in.pressed;
        bool tap = false;
        if (!steps && f == 0)
            pressed = 0;  // the key that opened the act
        if (steps)
        {
            pressed = 0;
            for (int i = 0; i < nsteps; i++)
                if (steps[i].frame == f)
                    pressed |= steps[i].keys;
        }
        else
        {
            tap = cv_in.touching && !was_touching;
            was_touching = cv_in.touching;
        }

        if (!cleared)
        {
            if (tap && cv_in.ty >= 48)
            {
                cur = m_clamp(cv_in.tx / 64, 0, STRINGS - 1);
                choose();
            }
            if (pressed & KEY_LEFT)
                cur = (cur + STRINGS - 1) % STRINGS;
            if (pressed & KEY_RIGHT)
                cur = (cur + 1) % STRINGS;
            if (pressed & KEY_A)
                choose();
        }

        hold = crossings() == 0 ? hold + 1 : 0;
        if (hold >= CLEAR_HOLD && !cleared)
        {
            cleared = true;
            clear_frame = f;
            text_put(&con, 0, 0, TP_GREEN, "ACT 4  CLEAR!              ");
            text_put(&sub, 1, 23, TP_GREEN, "A: ACT 5   B: BACK    ");
        }

        if (cleared && f == clear_frame + CAP_AFTER)
            cap_start();
        if (cleared && f == clear_frame + READ_AFTER && cap_done())
        {
            hpx = count_col(hcol, H_Y - 4, H_Y + 4);
            dpx = count_col(dcol, D_Y0 - 2, D_Y1 + 3);
            measured = true;
            text_printf(&con, 0, 21, TP_WHITE, "HLINE %4lu %s", (unsigned long)hpx, hpx ? "OK" : "NG");
            text_printf(&con, 0, 22, TP_WHITE, "DLINE %4lu %s", (unsigned long)dpx, dpx ? "OK" : "NG");
        }

        sub_draw();
        draw_scene();

        volatile CvResult *r = CV_RESULT;
        r->demo_id = ACT4_ID;
        r->frame = cv_frame;
        r->values[0] = cleared;
        r->values[1] = hpx;
        r->values[2] = dpx;
        r->values[3] = (u32)(peg_of[0] | peg_of[1] << 4 | peg_of[2] << 8 | peg_of[3] << 12);
        r->values[4] = (u32)crossings();
        r->values[5] = measured;
        r->magic = CV_RESULT_MAGIC;
        DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));

        oam_commit();
        sys_vsync();
        sys_handle_lid();
        input_update();
        if (steps)
        {
            if ((cleared && f >= clear_frame + 30) || f >= end_frame)
            {
                if (out)
                {
                    out->cleared = cleared;
                    out->clear_frame = clear_frame;
                    out->measured = measured;
                    out->hline = hpx;
                    out->dline = dpx;
                }
                return SC_TITLE;
            }
        }
        else if (cleared && (cv_in.pressed & KEY_A))
            return SC_ACT5;
        else if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}

LtScene scene_act4(void)
{
    return act4_loop(NULL, 0, 0, NULL);
}

void act4_replay(const Act4Step *steps, int n, int end_frame, Act4Outcome *out)
{
    act4_loop(steps, n, end_frame, out);
}
