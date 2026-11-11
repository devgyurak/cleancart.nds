// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 2: turn the dolls. Three paper dolls stand on plinths; L/R turns the
// chosen doll around until each one faces the way the bottom screen asks.
//
// A doll is a 16x16 paper card (64x16 texture, card at texels 16-31, cut lines
// at texels 15 and 32). Turning mirrors the card with reversed texture
// coordinates written into the vertices (the TEXIMAGE_PARAM flip bits are not
// used). The first two dolls turn with the clean mirror 31>15; the third is a
// "stitched" doll whose back is the plain mirror 32>16, which on hardware shows
// the cut line at the card's left edge (melonDS #2008). The act stays playable
// where a renderer draws it differently: the bottom screen states every doll's
// facing, and the strings carry a dark quad under the line polygon.
//
// After the clear the act captures its own display and counts the cut-line
// pixels per doll (expected 0/0/18) and the edge-marking pixels around the
// plinths (two edge colours: polygon IDs 8 and 16).
//
// Result block (CvResult at CV_RESULT_ADDR, demo_id ACT2_ID), every frame:
// values[0] cleared, [1] seam pixels (u8x3), [2] edge pixels, [3] doll facings,
// [4] 1 once the display capture finished (seam and edges are only valid then)
// (bit n = doll n shows its back).

#include <nds.h>
#include <stdio.h>

#include "core/hw.h"
#include "core/input.h"
#include "core/sys.h"
#include "game/act2.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"
#include "judge/capture.h"

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

#define ACT2_ID 0x32544341u  // "ACT2"

// Identity projection: clip x/y map straight to screen pixels. x is exact for
// any pixel; y is exact when y % 3 == 0.
#define PX(x) ((x) * 32 - 4096)
#define PY(y) (4096 - (y) * 128 / 3)
#define CAP   ((const u16 *)0x06860000)   // bank D in LCDC mode

#define DOLLS      3
#define DOLL_Y     72
#define TURN_FRAMES 10
#define CLEAR_HOLD 20
#define CAP_AFTER  4    // frames after the clear: start the capture
#define READ_AFTER 10   // ... and read it

static const int doll_x[DOLLS] = { 48, 120, 192 };
static const bool want_back[DOLLS] = { false, true, true };
static const char doll_name[DOLLS] = { 'A', 'B', 'C' };

// Texel columns [left, right] of a doll's card, front and back. The back of
// the third doll is the plain mirror (cut line at its left edge on hardware).
static const int span_front[2] = { 16, 32 };
static const int span_back[DOLLS][2] = { { 31, 15 }, { 31, 15 }, { 32, 16 } };

static const u16 magenta = RGB15(31, 0, 31);
static const u16 edge_col[2] = { RGB15(31, 31, 0), RGB15(0, 31, 31) };
static const u16 string_col = RGB15(26, 26, 22);

static u16 card[64 * 16] __attribute__((aligned(32)));
static int card_tex;

static CvText con;   // main engine text (BG1)
static CvText sub;   // sub engine text (BG0)

static bool back[DOLLS];
static int anim[DOLLS];   // frames left of a turn, 0 = at rest
static int sel;

static void make_card(void)
{
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int cx = x - 16;
            u16 c = 0;  // alpha 0: outside the card
            if (x == 15 || x == 32)
                c = 0x8000 | magenta;
            else if (cx >= 0 && cx < 16)
            {
                // A paper doll facing right with a raised arm, so a mirror shows.
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

// A string: a dark quad (every renderer) with a line polygon in front (a
// triangle with two equal vertices).
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

// Width of the card (px) while it turns: it narrows to nothing, flips, widens.
static int turn_width(int i)
{
    if (!anim[i])
        return 16;
    int p = TURN_FRAMES - anim[i];
    return p < 5 ? 16 * (4 - p) / 5 : 16 * (p - 4) / 5;
}

static void draw_scene(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Cards: polygon ID 0 is the clear-plane ID, so edge marking skips them
    // and the cut lines stay the only thing at their sides.
    glBindTexture(0, card_tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor3b(255, 255, 255);
    for (int i = 0; i < DOLLS; i++)
    {
        int w = turn_width(i);
        if (w < 1)
            continue;
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

    // Plinths: IDs 8 and 16 pick edge colours 1 and 2. Unbind through libnds:
    // it skips glBindTexture() of the texture it believes is still bound.
    glBindTexture(0, 0);
    for (int i = 0; i < DOLLS; i++)
    {
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(i == 1 ? 16 : 8));
        color(i == 1 ? RGB15(5, 8, 13) : RGB15(12, 7, 5));
        rect(doll_x[i] - 4, 100, doll_x[i] + 20, 116, 0);
    }

    // Crossbar, strings and the chosen doll's marker, in front.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor3b(90, 60, 40);
    rect(36, 36, 220, 40, -128);
    for (int i = 0; i < DOLLS; i++)
        string_draw(doll_x[i] + 8, 40, doll_x[i] + 8, DOLL_Y);
    color(RGB15(30, 12, 10));
    rect(doll_x[sel] + 5, 56, doll_x[sel] + 11, 62, -100);
    g3_flush(false);
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

static u32 count_edges(void)
{
    u32 n = 0;
    for (int y = 94; y < 124; y++)
        for (int x = 30; x < 230; x++)
        {
            u16 c = cap_px(x, y);
            n += c == edge_col[0] || c == edge_col[1];
        }
    return n;
}

static void sub_draw(void)
{
    for (int i = 0; i < DOLLS; i++)
    {
        int x = 1 + 10 * i;
        bool ok = back[i] == want_back[i] && !anim[i];
        text_printf(&sub, x, 6, i == sel ? TP_YELLOW : TP_WHITE, "%cDOLL %c", i == sel ? '>' : ' ',
                    doll_name[i]);
        text_printf(&sub, x, 8, TP_WHITE, "NOW %-5s", back[i] ? "BACK" : "FRONT");
        text_printf(&sub, x, 9, TP_CYAN, "AIM %-5s", want_back[i] ? "BACK" : "FRONT");
        text_printf(&sub, x, 11, ok ? TP_GREEN : TP_RED, "%-6s", ok ? "OK" : "TURN");
    }
}

static void setup(void)
{
    display_reset();
    vram_layout(VL_JUDGE);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_ANTIALIAS | GL_TOON_HIGHLIGHT | GL_CLEAR_BMP);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST | GL_OUTLINE);
    glSetOutlineColor(1, edge_col[0]);
    glSetOutlineColor(2, edge_col[1]);
    make_card();

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

static void turn(int i)
{
    if (!anim[i])
        anim[i] = TURN_FRAMES;
}

static LtScene act2_loop(const Act2Step *steps, int nsteps, int end_frame, Act2Outcome *out)
{
    setup();
    for (int i = 0; i < DOLLS; i++)
    {
        back[i] = false;
        anim[i] = 0;
    }
    sel = 0;
    int hold = 0, f = 0, clear_frame = -1;
    bool cleared = false, measured = false, was_touching = false;
    u32 seam = 0, edges = 0;

    text_put(&con, 0, 0, TP_YELLOW, "ACT 2  TURN THE DOLLS");
    text_put(&sub, 1, 0, TP_YELLOW, "ACT 2  TURN THE DOLLS");
    text_put(&sub, 1, 2, TP_GRAY, "LEFT/RIGHT OR TAP: CHOOSE");
    text_put(&sub, 1, 3, TP_GRAY, "L/R OR TAP LOWER HALF: TURN");
    text_put(&sub, 1, 15, TP_GRAY, "DOLL C IS STITCHED: ITS BACK");
    text_put(&sub, 1, 16, TP_GRAY, "MAY SHOW A SEAM. THAT IS OK.");
    text_put(&sub, 1, 23, TP_GRAY, steps ? "REHEARSAL" : "B: BACK");

    for (;; f++)
    {
        u16 pressed = cv_in.pressed;
        bool tap = false;
        int tx = cv_in.tx, ty = cv_in.ty;
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
            if (tap && ty >= 48 && tx < 240)
            {
                sel = m_clamp(tx / 80, 0, DOLLS - 1);
                if (ty >= 96)
                    turn(sel);
            }
            if (pressed & KEY_LEFT)
                sel = (sel + DOLLS - 1) % DOLLS;
            if (pressed & KEY_RIGHT)
                sel = (sel + 1) % DOLLS;
            if (pressed & (KEY_L | KEY_R))
                turn(sel);
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
            match = match && back[i] == want_back[i] && !anim[i];
        hold = match ? hold + 1 : 0;
        if (hold >= CLEAR_HOLD && !cleared)
        {
            cleared = true;
            clear_frame = f;
            text_put(&con, 0, 0, TP_GREEN, "ACT 2  CLEAR!          ");
            text_put(&sub, 1, 23, TP_GREEN, "A: ACT 3   B: BACK    ");
        }

        // Grade the picture once the act has been cleared for a few frames.
        if (cleared && f == clear_frame + CAP_AFTER)
            cap_start();
        if (cleared && f == clear_frame + READ_AFTER && cap_done())
        {
            seam = count_seam(0) | count_seam(1) << 8 | count_seam(2) << 16;
            edges = count_edges();
            measured = true;
            bool seam_ok = (seam & 0xFFFF) == 0 && (seam >> 16) > 0;   // only the stitched doll (C) may show it
            text_printf(&con, 0, 21, TP_WHITE, "SEAM %2lu %2lu %2lu %s", seam & 255,
                        seam >> 8 & 255, seam >> 16 & 255, seam_ok ? "OK" : "NG");
            text_printf(&con, 0, 22, TP_WHITE, "EDGE %4lu %s", (unsigned long)edges,
                        edges > 0 ? "OK" : "NG");
        }

        for (int i = 0; i < DOLLS; i++)
            text_printf(&con, doll_x[i] / 8, 17, TP_WHITE, "%-5s", back[i] ? "BACK" : "FRONT");
        sub_draw();
        draw_scene();

        volatile CvResult *r = CV_RESULT;
        r->demo_id = ACT2_ID;
        r->frame = cv_frame;
        r->values[0] = cleared;
        r->values[1] = seam;
        r->values[2] = edges;
        r->values[3] = back[0] | back[1] << 1 | back[2] << 2;
        r->values[4] = measured;
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
                    out->seam = seam;
                    out->edges = edges;
                }
                return SC_TITLE;
            }
        }
        else if (cleared && (cv_in.pressed & KEY_A))
            return SC_ACT3;
        else if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}

LtScene scene_act2(void)
{
    return act2_loop(NULL, 0, 0, NULL);
}

void act2_replay(const Act2Step *steps, int n, int end_frame, Act2Outcome *out)
{
    act2_loop(steps, n, end_frame, out);
}
