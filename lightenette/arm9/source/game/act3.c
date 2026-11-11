// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 3: find the marks. The stage is a 3D scene on BG0 with the brightness
// decrease effect (BLDCNT 01C1), so it is nearly black. The lantern is a
// semi-transparent OBJ (64x64 glow): move it with the D-pad (or drag on the
// bottom screen) over the three hidden marks and keep it there until each one
// is found. Where the lantern is, the dark stage shows through its glow.
//
// The act stays playable on every renderer: finding a mark is decided in
// software from the lantern position, and the bottom screen is a map of the
// stage with the marks (X) and the lantern (@).
//
// After the clear the lantern parks over empty backdrop and the act captures
// its own display with BLDCNT 0100 (no effect), 0140 (alpha) and 01C1 (darken)
// and reads the lantern pixel in each. GBATEK: a semi-transparent OBJ blends
// with the 3D layer below it whatever the effect bits say and skips the darken
// effect, so the three pixels are equal (melonDS #2503: GL renderers show
// the backdrop in the NONE and DARK captures; DeSmuME darkens it in DARK).
//
// Result block (CvResult at CV_RESULT_ADDR, demo_id ACT3_ID), every frame:
// values[0] cleared, [1] marks found, [2..4] lantern pixel NONE/ALPHA/DARK,
// [5..6] backdrop pixel no effect / darkened, [7] captures finished.

#include <nds.h>
#include <stdio.h>

#include "core/hw.h"
#include "core/input.h"
#include "core/sys.h"
#include "game/act3.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"
#include "judge/judge.h"

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

#define ACT3_ID 0x33544341u  // "ACT3"

// Identity projection: clip x/y map straight to screen pixels (see act2.c).
#define PX(x) ((x) * 32 - 4096)
#define PY(y) (4096 - (y) * 128 / 3)
#define CAP   ((const u16 *)0x06860000)   // bank D in LCDC mode

#define MARKS       3
#define MARK_SIZE   12
#define FIND_RADIUS 16     // lantern centre to mark centre, pixels
#define FIND_FRAMES 15     // frames inside the radius until a mark is found
#define SPEED       2      // lantern pixels per frame (D-pad)
#define LAMP_W      64

// Parking spot after the clear (top-left of the sprite) and the pixels read.
#define PARK_X 192
#define PARK_Y 112
#define LAMP_PX_X 232
#define LAMP_PX_Y 132
#define OUT_PX_X  140
#define OUT_PX_Y  140

#define MODE_FRAMES 14     // BLDCNT set, capture at +4, read at +10
#define CAP_AFTER   4
#define READ_AFTER  10

static const int mark_x[MARKS] = { 48, 112, 200 };   // centres
static const int mark_y[MARKS] = { 48, 128, 56 };

static const u16 bldcnt[3] = { 0x0100, 0x0140, 0x01C1 };

static CvText con;   // main engine text (BG1)
static CvText sub;   // sub engine text (BG0)

static int lx, ly;                // lantern sprite top-left
static int near_frames[MARKS];
static bool found[MARKS];

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

// The stage: dim curtains and a floor board at the sides, plain backdrop where
// the lantern parks and where the judge pixel is read. A mark is a small
// painted diamond (two triangles) in a warm colour.
static void draw_scene(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));

    color(RGB15(14, 5, 6));
    rect(0, 0, 24, 189, 100);
    rect(232, 0, 256, 189, 100);
    color(RGB15(9, 7, 5));
    rect(24, 168, 232, 189, 100);
    rect(24, 30, 232, 36, 100);

    for (int i = 0; i < MARKS; i++)
    {
        int cx = mark_x[i], cy = mark_y[i], h = MARK_SIZE / 2;
        color(found[i] ? RGB15(31, 28, 8) : RGB15(24, 14, 6));
        glBegin(GL_TRIANGLES);
        glVertex3v16(PX(cx), PY(cy - h), 0);
        glVertex3v16(PX(cx - h), PY(cy), 0);
        glVertex3v16(PX(cx + h), PY(cy), 0);
        glVertex3v16(PX(cx - h), PY(cy), 0);
        glVertex3v16(PX(cx), PY(cy + h), 0);
        glVertex3v16(PX(cx + h), PY(cy), 0);
        glEnd();
    }
    g3_flush(false);

    oam_clear(ENG_MAIN);
    oam_add(ENG_MAIN, lx, ly, SPR_64x64, 0, 15, 1, SPRF_BLEND, 0);
}

static u16 cap_px(int x, int y)
{
    return CAP[y * 256 + x] & 0x7FFF;
}

static void set_blend(int mode)
{
    REG_BLDCNT = bldcnt[mode];
    REG_BLDALPHA = 8 | (16 << 8);
    REG_BLDY = 10;
}

// The bottom screen is a map of the stage, one text cell per 8x8 pixels.
static void sub_draw(void)
{
    for (int y = 6; y < 22; y++)
        text_put(&sub, 0, y, TP_GRAY, "                                ");
    for (int i = 0; i < MARKS; i++)
        text_put(&sub, mark_x[i] / 8, mark_y[i] / 8 + 4, found[i] ? TP_GREEN : TP_YELLOW,
                 found[i] ? "*" : "X");
    text_put(&sub, (lx + LAMP_W / 2) / 8, (ly + LAMP_W / 2) / 8 + 4, TP_CYAN, "@");
}

static void setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP
              | GL_OUTLINE | GL_TEXTURE_2D);
    glEnable(GL_BLEND | GL_ALPHA_TEST);
    judge_lamp_load();

    REG_DISPCNT = MODE_0_3D | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D;
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
    set_blend(2);   // darken: the usual look of the act
}

static LtScene act3_loop(const Act3Step *steps, int nsteps, int end_frame, Act3Outcome *out)
{
    setup();
    lx = 128 - LAMP_W / 2;
    ly = 96 - LAMP_W / 2;
    for (int i = 0; i < MARKS; i++)
    {
        near_frames[i] = 0;
        found[i] = false;
    }
    int f = 0, clear_frame = -1, nfound = 0, mode_t0 = 0;
    bool cleared = false, measured = false;
    u16 lamp[3] = { 0 }, out_none = 0, out_dark = 0;
    int cap_mode = -1;

    text_put(&con, 0, 0, TP_YELLOW, "ACT 3  FIND THE MARKS");
    text_put(&sub, 1, 0, TP_YELLOW, "ACT 3  FIND THE MARKS");
    text_put(&sub, 1, 2, TP_GRAY, "D-PAD OR DRAG: MOVE THE LANTERN");
    text_put(&sub, 1, 3, TP_GRAY, "HOLD IT OVER EACH X A MOMENT");
    text_put(&sub, 1, 4, TP_GRAY, "MAP: X MARK  * FOUND  @ LANTERN");
    text_put(&sub, 1, 23, TP_GRAY, steps ? "REHEARSAL" : "B: BACK");

    for (;; f++)
    {
        u16 held = cv_in.held;
        if (steps)
        {
            held = 0;
            for (int i = 0; i < nsteps; i++)
                if (f >= steps[i].frame && f < steps[i].frame + steps[i].len)
                    held |= steps[i].keys;
        }
        else if (cv_in.touching)
        {
            lx = cv_in.tx - LAMP_W / 2;
            ly = cv_in.ty - LAMP_W / 2;
        }

        if (!cleared)
        {
            if (held & KEY_LEFT)
                lx -= SPEED;
            if (held & KEY_RIGHT)
                lx += SPEED;
            if (held & KEY_UP)
                ly -= SPEED;
            if (held & KEY_DOWN)
                ly += SPEED;
            lx = m_clamp(lx, -LAMP_W / 2, 256 - LAMP_W / 2);
            ly = m_clamp(ly, -LAMP_W / 2, 189 - LAMP_W / 2);

            int cx = lx + LAMP_W / 2, cy = ly + LAMP_W / 2;
            for (int i = 0; i < MARKS; i++)
            {
                int dx = cx - mark_x[i], dy = cy - mark_y[i];
                bool near = dx * dx + dy * dy <= FIND_RADIUS * FIND_RADIUS;
                if (near && !found[i] && ++near_frames[i] >= FIND_FRAMES)
                {
                    found[i] = true;
                    nfound++;
                }
            }
            if (nfound == MARKS)
            {
                cleared = true;
                clear_frame = f;
                lx = PARK_X;
                ly = PARK_Y;
                mode_t0 = f + 2;
                text_put(&con, 0, 0, TP_GREEN, "ACT 3  CLEAR!          ");
                text_put(&sub, 1, 23, TP_GREEN, "A: TITLE   B: BACK    ");
            }
        }

        // After the clear: three captures, one per BLDCNT mode.
        if (cleared && !measured)
        {
            int m = (f - mode_t0) / MODE_FRAMES, t = (f - mode_t0) % MODE_FRAMES;
            if (f >= mode_t0 && m < 3)
            {
                if (t == 0)
                    set_blend(m);
                if (t == CAP_AFTER)
                {
                    REG_DISPCAPCNT = (1u << 31) | (3 << 16) | (3 << 20);  // source A to bank D
                    cap_mode = m;
                }
                if (t == READ_AFTER && cap_mode == m)
                {
                    bool done = !(REG_DISPCAPCNT & (1u << 31));
                    lamp[m] = done ? cap_px(LAMP_PX_X, LAMP_PX_Y) : 0xFFFF;
                    if (m == 0)
                        out_none = cap_px(OUT_PX_X, OUT_PX_Y);
                    if (m == 2)
                        out_dark = cap_px(OUT_PX_X, OUT_PX_Y);
                    if (m == 2)
                    {
                        measured = true;
                        bool blend_ok = lamp[0] == lamp[1] && lamp[1] == lamp[2];
                        text_printf(&con, 0, 20, TP_WHITE, "LAMP %04X %04X %04X", lamp[0],
                                    lamp[1], lamp[2]);
                        text_put(&con, 21, 20, blend_ok ? TP_GREEN : TP_RED, blend_ok ? "OK" : "NG");
                        text_printf(&con, 0, 21, TP_WHITE, "DARK %04X %04X", out_none, out_dark);
                        text_put(&con, 15, 21, out_dark != out_none ? TP_GREEN : TP_RED,
                                 out_dark != out_none ? "OK" : "NG");
                    }
                }
            }
        }

        text_printf(&con, 0, 22, TP_GRAY, "MARKS %d/%d", nfound, MARKS);
        sub_draw();
        draw_scene();

        volatile CvResult *r = CV_RESULT;
        r->demo_id = ACT3_ID;
        r->frame = cv_frame;
        r->values[0] = cleared;
        r->values[1] = (u32)nfound;
        r->values[2] = lamp[0];
        r->values[3] = lamp[1];
        r->values[4] = lamp[2];
        r->values[5] = out_none;
        r->values[6] = out_dark;
        r->values[7] = measured;
        r->magic = CV_RESULT_MAGIC;

        oam_commit();
        sys_vsync();
        sys_handle_lid();
        input_update();
        if (steps)
        {
            if (measured || f >= end_frame)
            {
                if (out)
                {
                    out->cleared = cleared;
                    out->clear_frame = clear_frame;
                    out->measured = measured;
                    out->found = nfound;
                    for (int i = 0; i < 3; i++)
                        out->lamp[i] = lamp[i];
                    out->out_none = out_none;
                    out->out_dark = out_dark;
                }
                return SC_TITLE;
            }
        }
        else if ((cv_in.pressed & KEY_B) || (cleared && (cv_in.pressed & KEY_A)))
            return SC_TITLE;
    }
}

LtScene scene_act3(void)
{
    return act3_loop(NULL, 0, 0, NULL);
}

void act3_replay(const Act3Step *steps, int n, int end_frame, Act3Outcome *out)
{
    act3_loop(steps, n, end_frame, out);
}
