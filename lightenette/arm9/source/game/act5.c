// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 5: the finale. The three motions together on one dark stage. Three paper
// dolls hang on line-polygon strings above three plinths, each plinth with its
// own edge-marking colour (polygon IDs 8, 16 and 24); the stage is the 3D layer
// with the brightness decrease effect and a translucent lantern OBJ floats over
// it. Move the lantern over every doll (D-pad or drag) to light it, and turn
// the doll under the lantern (L, R or A, or tap the lower half of the bottom
// screen) until each doll shows the side the bottom screen asks for. Doll C is
// a stitched doll: its back is the plain mirror 32>16, which on hardware shows
// the cut line at its left edge (melonDS #2008). Reversed texture coordinates
// are written into the vertices; the TEXIMAGE_PARAM flip bits are not used.
//
// The act stays playable on every renderer: the bottom screen states each
// doll's side and whether it is lit, and the strings are a dark quad under a
// line polygon.
//
// After the clear the lantern parks over empty backdrop and the act captures
// its own display with BLDCNT 0100, 0140 and 01C1. The first capture also grades
// the seam pixels per doll (expected 0/0/18), the pixels of the three edge
// colours (> 0, GBATEK) and the string line pixels (information only).
//
// Result block (CvResult at CV_RESULT_ADDR, demo_id ACT5_ID), every frame:
// values[0] cleared, [1] lit dolls (bits), [2] seam (u8x3), [3..5] edge pixels,
// [6] string line pixels, [7..9] lantern pixel NONE/ALPHA/DARK, [10..11]
// backdrop pixel no effect / darkened, [12] captures finished.

#include <nds.h>
#include <stdio.h>

#include "core/hw.h"
#include "core/input.h"
#include "core/sys.h"
#include "game/act5.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"
#include "judge/judge.h"

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

#define ACT5_ID 0x35544341u  // "ACT5"

// Identity projection: clip x/y map straight to screen pixels (see act2.c).
#define PX(x) ((x) * 32 - 4096)
#define PY(y) (4096 - (y) * 128 / 3)
#define CAP   ((const u16 *)0x06860000)   // bank D in LCDC mode

#define DOLLS       3
#define DOLL_Y      72
#define TURN_FRAMES 10
#define CLEAR_HOLD  20
#define LAMP_W      64
#define SPEED       2
#define LIT_RADIUS  24     // lantern centre to doll centre, pixels
#define LIT_FRAMES  15

// After the clear: parking spot of the lantern (top-left), the pixels read.
#define PARK_X    192
#define PARK_Y    124
#define LAMP_PX_X 232
#define LAMP_PX_Y 144
#define OUT_PX_X  140
#define OUT_PX_Y  150

#define MODE_FRAMES 14     // BLDCNT set, capture at +4, read at +10
#define CAP_AFTER   4
#define READ_AFTER  10

static const int doll_x[DOLLS] = { 48, 120, 192 };
static const bool want_back[DOLLS] = { true, false, true };
static const char doll_name[DOLLS] = { 'A', 'B', 'C' };
static const int span_front[2] = { 16, 32 };
static const int span_back[DOLLS][2] = { { 31, 15 }, { 31, 15 }, { 32, 16 } };

static const u16 magenta = RGB15(31, 0, 31);
static const u16 edge_col[3] = { RGB15(31, 31, 0), RGB15(0, 31, 31), RGB15(12, 31, 12) };
static const u16 string_col = RGB15(26, 26, 22);
static const u16 bldcnt[3] = { 0x0100, 0x0140, 0x01C1 };

static u16 card[64 * 16] __attribute__((aligned(32)));
static int card_tex;

static CvText con;   // main engine text (BG1)
static CvText sub;   // sub engine text (BG0)

static bool back[DOLLS];
static bool lit[DOLLS];
static int anim[DOLLS];        // frames left of a turn, 0 = at rest
static int near_frames[DOLLS];
static int lx, ly;             // lantern sprite top-left

static void make_card(void)
{
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int cx = x - 16;
            u16 c = 0;
            if (x == 15 || x == 32)
                c = 0x8000 | magenta;
            else if (cx >= 0 && cx < 16)
            {
                bool head = (cx - 7) * (cx - 7) + (y - 4) * (y - 4) <= 6;
                bool body = cx >= 5 && cx <= 9 && y >= 7 && y <= 12;
                bool arm = y == 8 && cx >= 9 && cx <= 13;
                bool legs = y >= 13 && y <= 14 && (cx == 5 || cx == 6 || cx == 8 || cx == 9);
                c = 0x8000 | (head || body || arm || legs ? RGB15(6, 4, 3) : RGB15(28, 25, 18));
            }
            card[y * 64 + x] = c;
        }
    }
    DC_FlushRange(card, sizeof(card));
    glGenTextures(1, &card_tex);
    glBindTexture(0, card_tex);
    glTexImage2D(0, 0, GL_RGBA, 64, 16, 0, TEXGEN_TEXCOORD, card);
}

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

static void string_draw(int x0, int y0, int x1, int y1)
{
    glColor3b(60, 40, 25);
    glBegin(GL_QUADS);
    glVertex3v16(PX(x0 - 1), PY(y0), -200);
    glVertex3v16(PX(x1 - 1), PY(y1), -200);
    glVertex3v16(PX(x1 + 1), PY(y1), -200);
    glVertex3v16(PX(x0 + 1), PY(y0), -200);
    glEnd();
    color(string_col);
    glBegin(GL_TRIANGLES);
    glVertex3v16(PX(x0), PY(y0), -256);
    glVertex3v16(PX(x1), PY(y1), -256);
    glVertex3v16(PX(x1), PY(y1), -256);
    glEnd();
}

static int turn_width(int i)
{
    if (!anim[i])
        return 16;
    int p = TURN_FRAMES - anim[i];
    return p < 5 ? 16 * (4 - p) / 5 : 16 * (p - 4) / 5;
}

// The doll the lantern is nearest to (horizontally).
static int nearest(void)
{
    int cx = lx + LAMP_W / 2, best = 0, bd = 1 << 20;
    for (int i = 0; i < DOLLS; i++)
    {
        int d = cx - (doll_x[i] + 8);
        d = d < 0 ? -d : d;
        if (d < bd)
        {
            bd = d;
            best = i;
        }
    }
    return best;
}

static void draw_scene(int sel)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Cards: polygon ID 0 is the clear-plane ID, so edge marking skips them
    // and the cut lines stay the only thing at their sides.
    glBindTexture(0, card_tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    for (int i = 0; i < DOLLS; i++)
    {
        int w = turn_width(i);
        if (w < 1)
            continue;
        if (lit[i])
            glColor3b(255, 255, 255);
        else
            glColor3b(120, 120, 120);
        const int *s = back[i] ? span_back[i] : span_front;
        int x0 = doll_x[i] + (16 - w) / 2, x1 = x0 + w;
        glBegin(GL_QUADS);
        glTexCoord2t16(inttot16(s[0]), 0);
        glVertex3v16(PX(x0), PY(DOLL_Y), 0);
        glTexCoord2t16(inttot16(s[0]), inttot16(16));
        glVertex3v16(PX(x0), PY(DOLL_Y + 18), 0);
        glTexCoord2t16(inttot16(s[1]), inttot16(16));
        glVertex3v16(PX(x1), PY(DOLL_Y + 18), 0);
        glTexCoord2t16(inttot16(s[1]), 0);
        glVertex3v16(PX(x1), PY(DOLL_Y), 0);
        glEnd();
    }

    // Plinths: IDs 8, 16, 24 pick edge colours 1, 2, 3. Unbind through libnds:
    // it skips glBindTexture() of the texture it believes is still bound.
    glBindTexture(0, 0);
    for (int i = 0; i < DOLLS; i++)
    {
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(8 * (i + 1)));
        color(i == 1 ? RGB15(5, 8, 13) : i == 2 ? RGB15(6, 11, 6) : RGB15(12, 7, 5));
        rect(doll_x[i] - 4, 100, doll_x[i] + 20, 116, 0);
    }

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor3b(90, 60, 40);
    rect(36, 36, 220, 40, -128);
    for (int i = 0; i < DOLLS; i++)
        string_draw(doll_x[i] + 8, 40, doll_x[i] + 8, DOLL_Y);
    color(RGB15(30, 12, 10));
    rect(doll_x[sel] + 5, 56, doll_x[sel] + 11, 62, -100);
    g3_flush(false);

    oam_clear(ENG_MAIN);
    oam_add(ENG_MAIN, lx, ly, SPR_64x64, 0, 15, 1, SPRF_BLEND, 0);
}

static u16 cap_px(int x, int y)
{
    return CAP[y * 256 + x] & 0x7FFF;
}

static u32 count_seam(int doll)
{
    u32 n = 0;
    for (int y = DOLL_Y - 2; y < DOLL_Y + 21; y++)
        for (int x = doll_x[doll] - 4; x < doll_x[doll] + 20; x++)
            n += cap_px(x, y) == magenta;
    return n;
}

static u32 count_edge(int k)
{
    u32 n = 0;
    for (int y = 94; y < 122; y++)
        for (int x = 30; x < 230; x++)
            n += cap_px(x, y) == edge_col[k];
    return n;
}

static u32 count_lines(void)
{
    u32 n = 0;
    for (int i = 0; i < DOLLS; i++)
        for (int y = 41; y < DOLL_Y; y++)
            for (int x = doll_x[i] + 5; x < doll_x[i] + 12; x++)
                n += cap_px(x, y) == string_col;
    return n;
}

static void set_blend(int mode, int evy)
{
    REG_BLDCNT = bldcnt[mode];
    REG_BLDALPHA = 8 | (16 << 8);
    REG_BLDY = evy;
}

static void sub_draw(int sel)
{
    for (int i = 0; i < DOLLS; i++)
    {
        int x = 1 + 10 * i;
        bool ok = lit[i] && back[i] == want_back[i] && !anim[i];
        text_printf(&sub, x, 6, i == sel ? TP_YELLOW : TP_WHITE, "%cDOLL %c", i == sel ? '>' : ' ',
                    doll_name[i]);
        text_printf(&sub, x, 8, TP_WHITE, "NOW %-5s", back[i] ? "BACK" : "FRONT");
        text_printf(&sub, x, 9, TP_CYAN, "AIM %-5s", want_back[i] ? "BACK" : "FRONT");
        text_printf(&sub, x, 10, lit[i] ? TP_GREEN : TP_GRAY, "%-9s", lit[i] ? "LIT" : "DARK");
        text_printf(&sub, x, 12, ok ? TP_GREEN : TP_RED, "%-6s", ok ? "OK" : "TODO");
    }
}

static void setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST | GL_OUTLINE);
    for (int k = 0; k < 3; k++)
        glSetOutlineColor(k + 1, edge_col[k]);
    make_card();
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
    set_blend(2, 6);   // darken: the look of the act (lighter than the judge)
}

static LtScene act5_loop(const Act5Step *steps, int nsteps, int end_frame, Act5Outcome *out)
{
    setup();
    lx = 128 - LAMP_W / 2;
    ly = 96 - LAMP_W / 2;
    for (int i = 0; i < DOLLS; i++)
    {
        back[i] = lit[i] = false;
        anim[i] = near_frames[i] = 0;
    }
    int f = 0, hold = 0, clear_frame = -1, mode_t0 = 0, cap_mode = -1;
    bool cleared = false, measured = false;
    u32 seam = 0, edge[3] = { 0, 0, 0 }, lines = 0;
    u16 lamp[3] = { 0 }, out_none = 0, out_dark = 0;

    text_put(&con, 0, 0, TP_YELLOW, "ACT 5  THE FINALE");
    text_put(&sub, 1, 0, TP_YELLOW, "ACT 5  THE FINALE");
    text_put(&sub, 1, 2, TP_GRAY, "D-PAD OR DRAG: MOVE THE LANTERN");
    text_put(&sub, 1, 3, TP_GRAY, "LIGHT EVERY DOLL, HOLD A MOMENT");
    text_put(&sub, 1, 4, TP_GRAY, "L/R/A OR TAP: TURN THE DOLL");
    text_put(&sub, 1, 15, TP_GRAY, "DOLL C IS STITCHED: ITS BACK");
    text_put(&sub, 1, 16, TP_GRAY, "MAY SHOW A SEAM. THAT IS OK.");
    text_put(&sub, 1, 23, TP_GRAY, steps ? "REHEARSAL" : "B: BACK");

    for (;; f++)
    {
        u16 held = cv_in.held, pressed = cv_in.pressed;
        bool tap = false;
        if (steps)
        {
            held = pressed = 0;
            for (int i = 0; i < nsteps; i++)
            {
                if (f >= steps[i].frame && f < steps[i].frame + steps[i].len)
                    held |= steps[i].keys;
                if (f == steps[i].frame)
                    pressed |= steps[i].keys;
            }
        }
        else
        {
            if (f == 0)
                pressed = 0;  // the key that opened the act
            static bool was_touching;
            if (cv_in.touching && cv_in.ty < 140)
            {
                lx = cv_in.tx - LAMP_W / 2;
                ly = cv_in.ty - LAMP_W / 2;
            }
            tap = cv_in.touching && !was_touching && cv_in.ty >= 140;
            was_touching = cv_in.touching;
        }

        int sel = nearest();
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
            sel = nearest();

            int cx = lx + LAMP_W / 2, cy = ly + LAMP_W / 2;
            for (int i = 0; i < DOLLS; i++)
            {
                int dx = cx - (doll_x[i] + 8), dy = cy - (DOLL_Y + 9);
                if (dx * dx + dy * dy <= LIT_RADIUS * LIT_RADIUS && !lit[i] && ++near_frames[i] >= LIT_FRAMES)
                    lit[i] = true;
            }
            if ((pressed & (KEY_L | KEY_R | KEY_A)) || tap)
                if (!anim[sel])
                    anim[sel] = TURN_FRAMES;
        }

        for (int i = 0; i < DOLLS; i++)
        {
            if (!anim[i])
                continue;
            if (TURN_FRAMES - anim[i] == 4)  // the card is edge-on: flip
                back[i] = !back[i];
            anim[i]--;
        }

        bool match = true;
        for (int i = 0; i < DOLLS; i++)
            match = match && lit[i] && back[i] == want_back[i] && !anim[i];
        hold = match ? hold + 1 : 0;
        if (hold >= CLEAR_HOLD && !cleared)
        {
            cleared = true;
            clear_frame = f;
            lx = PARK_X;
            ly = PARK_Y;
            mode_t0 = f + 2;
            text_put(&con, 0, 0, TP_GREEN, "ACT 5  CLEAR!          ");
            text_put(&sub, 1, 23, TP_GREEN, "A: TITLE   B: BACK    ");
        }

        // After the clear: three captures, one per BLDCNT mode.
        if (cleared && !measured)
        {
            int m = (f - mode_t0) / MODE_FRAMES, t = (f - mode_t0) % MODE_FRAMES;
            if (f >= mode_t0 && m < 3)
            {
                if (t == 0)
                    set_blend(m, 10);
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
                    {
                        out_none = cap_px(OUT_PX_X, OUT_PX_Y);
                        seam = count_seam(0) | count_seam(1) << 8 | count_seam(2) << 16;
                        for (int k = 0; k < 3; k++)
                            edge[k] = count_edge(k);
                        lines = count_lines();
                    }
                    if (m == 2)
                    {
                        out_dark = cap_px(OUT_PX_X, OUT_PX_Y);
                        measured = true;
                        bool seam_ok = seam == 0x120000;
                        bool edge_ok = edge[0] && edge[1] && edge[2];
                        bool blend_ok = lamp[0] == lamp[1] && lamp[1] == lamp[2];
                        text_printf(&con, 0, 18, TP_WHITE, "SEAM %2lu %2lu %2lu %s", seam & 255,
                                    seam >> 8 & 255, seam >> 16 & 255, seam_ok ? "OK" : "NG");
                        text_printf(&con, 0, 19, TP_WHITE, "EDGE %lu %lu %lu %s", (unsigned long)edge[0],
                                    (unsigned long)edge[1], (unsigned long)edge[2], edge_ok ? "OK" : "NG");
                        text_printf(&con, 0, 20, TP_WHITE, "LINE %lu", (unsigned long)lines);
                        text_printf(&con, 0, 21, TP_WHITE, "LAMP %04X %04X %04X %s", lamp[0], lamp[1],
                                    lamp[2], blend_ok ? "OK" : "NG");
                        text_printf(&con, 0, 22, TP_WHITE, "DARK %04X %04X %s", out_none, out_dark,
                                    out_dark != out_none ? "OK" : "NG");
                    }
                }
            }
        }

        sub_draw(sel);
        draw_scene(sel);

        volatile CvResult *r = CV_RESULT;
        r->demo_id = ACT5_ID;
        r->frame = cv_frame;
        r->values[0] = cleared;
        r->values[1] = (u32)(lit[0] | lit[1] << 1 | lit[2] << 2);
        r->values[2] = seam;
        r->values[3] = edge[0];
        r->values[4] = edge[1];
        r->values[5] = edge[2];
        r->values[6] = lines;
        r->values[7] = lamp[0];
        r->values[8] = lamp[1];
        r->values[9] = lamp[2];
        r->values[10] = out_none;
        r->values[11] = out_dark;
        r->values[12] = measured;
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
                    out->seam = seam;
                    for (int k = 0; k < 3; k++)
                        out->edge[k] = edge[k];
                    out->lines = lines;
                    for (int k = 0; k < 3; k++)
                        out->lamp[k] = lamp[k];
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

LtScene scene_act5(void)
{
    return act5_loop(NULL, 0, 0, NULL);
}

void act5_replay(const Act5Step *steps, int n, int end_frame, Act5Outcome *out)
{
    act5_loop(steps, n, end_frame, out);
}
