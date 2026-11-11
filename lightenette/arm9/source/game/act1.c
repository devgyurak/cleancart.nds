// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 1 prototype: a paper marionette on the top screen, four touch sliders on
// the bottom screen. Match the puppet's limbs to the shadow on the screen.
//
// The marionette is four jointed paper limbs (3D quads with the generated paper
// texture) plus a torso and head. Each limb hangs from a string drawn twice: a
// dark quad and a bright line polygon, so a renderer that draws no line
// polygons still shows the string. The score is the overlap (intersection over
// union, 0-100) of the limbs' silhouette with the target's silhouette, counted
// on a coarse grid in software, so it never depends on what a renderer draws.
//
// Result block (CvResult at CV_RESULT_ADDR, demo_id ACT1_ID), refreshed every
// frame: values[0] score, [1] 1 once cleared, [2..5] limb angles + 90.

#include <nds.h>

#include "core/cvmath.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/sys.h"
#include "game/act1.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define MAIN_BG ((u8 *)0x06000000)
#define SUB_BG  ((u8 *)0x06200000)

#define ACT1_ID 0x31544341u  // "ACT1"

// Pixel coordinates -> 4.12 clip space for the identity projection.
#define PX(x) ((x) * 32 - 4096)
#define PY(y) (4096 - (y) * 128 / 3)

enum { L_ARM, R_ARM, L_LEG, R_LEG, LIMBS };

typedef struct {
    int px, py;   // pivot (shoulder / hip)
    int len, w;
    int side;     // +1 right, -1 left: positive angles swing outwards
} Limb;

static const Limb limbs[LIMBS] = {
    [L_ARM] = { 114, 76, 32, 11, -1 },
    [R_ARM] = { 142, 76, 32, 11, +1 },
    [L_LEG] = { 121, 108, 42, 13, -1 },
    [R_LEG] = { 135, 108, 42, 13, +1 },
};

// String attachment points on the crossbar.
static const int bar_x[LIMBS] = { 92, 164, 118, 138 };
#define BAR_Y 30

// Target pose in degrees (the shadow) and what the sliders start at.
static const int target[LIMBS] = { 50, 35, 25, -20 };

#define SLIDER_TOP 24   // touch y of the slider ends
#define SLIDER_BOT 168
#define ANGLE_MAX  80
#define CLEAR_SCORE 85
#define CLEAR_HOLD  20

static CvText con;      // main engine text (BG1)
static CvText sub;      // sub engine text (BG0)
static int angle[LIMBS];
static int score, hold;
static bool cleared;

static int angle_from_ty(int ty)
{
    ty = m_clamp(ty, SLIDER_TOP, SLIDER_BOT);
    return ANGLE_MAX - (ty - SLIDER_TOP) * (2 * ANGLE_MAX) / (SLIDER_BOT - SLIDER_TOP);
}

static int ty_from_angle(int a)
{
    return SLIDER_TOP + (ANGLE_MAX - a) * (SLIDER_BOT - SLIDER_TOP) / (2 * ANGLE_MAX);
}

// Unit vectors (4.12) of a limb at `deg`: d along the limb, n across it.
static void limb_axes(const Limb *l, int deg, int *dx, int *dy, int *nx, int *ny)
{
    int a = deg * l->side * 32768 / 360;
    int s = m_sin(a), c = m_cos(a);
    *dx = s;
    *dy = c;
    *nx = c;
    *ny = -s;
}

static void tip(int i, int deg, int *x, int *y)
{
    int dx, dy, nx, ny;
    limb_axes(&limbs[i], deg, &dx, &dy, &nx, &ny);
    *x = limbs[i].px + ((dx * limbs[i].len) >> 12);
    *y = limbs[i].py + ((dy * limbs[i].len) >> 12);
}

typedef struct { int dx, dy, nx, ny; } Axes;

static void axes_of(const int *pose, Axes *out)
{
    for (int i = 0; i < LIMBS; i++)
        limb_axes(&limbs[i], pose[i], &out[i].dx, &out[i].dy, &out[i].nx, &out[i].ny);
}

static bool in_limb(int i, const Axes *ax, int cx, int cy)
{
    int vx = cx - limbs[i].px, vy = cy - limbs[i].py;
    int along = (vx * ax->dx + vy * ax->dy) >> 12;
    int across = (vx * ax->nx + vy * ax->ny) >> 12;
    return along >= 0 && along <= limbs[i].len && across >= -limbs[i].w / 2
           && across <= limbs[i].w / 2;
}

// Intersection over union of the two limb silhouettes, 0-100.
static int silhouette_score(const int *pose)
{
    Axes pa[LIMBS], ta[LIMBS];
    axes_of(pose, pa);
    axes_of(target, ta);
    int inter = 0, uni = 0;
    for (int cy = 44; cy < 164; cy += 3)
    {
        for (int cx = 48; cx < 208; cx += 3)
        {
            bool a = false, b = false;
            for (int i = 0; i < LIMBS; i++)
            {
                a = a || in_limb(i, &pa[i], cx, cy);
                b = b || in_limb(i, &ta[i], cx, cy);
            }
            inter += a && b;
            uni += a || b;
        }
    }
    return uni ? inter * 100 / uni : 0;
}

// --- Drawing -----------------------------------------------------------

static void color(int r, int g, int b)
{
    glColor3b(r, g, b);
}

static void quad4(const int p[4][2], int z, bool uv)
{
    static const int u[4] = { 0, 0, 32, 32 }, v[4] = { 0, 32, 32, 0 };
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++)
    {
        if (uv)
            glTexCoord2t16(inttot16(u[i]), inttot16(v[i]));
        glVertex3v16(PX(p[i][0]), PY(p[i][1]), z);
    }
    glEnd();
}

static void rect(int x0, int y0, int x1, int y1, int z, bool uv)
{
    const int p[4][2] = { { x0, y0 }, { x0, y1 }, { x1, y1 }, { x1, y0 } };
    quad4(p, z, uv);
}

static void limb_quad(int i, int deg, int z)
{
    int dx, dy, nx, ny;
    limb_axes(&limbs[i], deg, &dx, &dy, &nx, &ny);
    int hw = limbs[i].w / 2, len = limbs[i].len;
    int px = limbs[i].px, py = limbs[i].py;
    int ex = (dx * len) >> 12, ey = (dy * len) >> 12;
    int wx = (nx * hw) >> 12, wy = (ny * hw) >> 12;
    const int p[4][2] = {
        { px - wx, py - wy },
        { px + ex - wx, py + ey - wy },
        { px + ex + wx, py + ey + wy },
        { px + wx, py + wy },
    };
    quad4(p, z, true);
}

// A string is a dark quad (works on every renderer) with a line polygon in
// front (a triangle with two equal vertices).
static void string_draw(int x0, int y0, int x1, int y1)
{
    glBindTexture(0, 0);
    color(60, 40, 25);
    const int q[4][2] = { { x0 - 1, y0 }, { x1 - 1, y1 }, { x1 + 1, y1 }, { x0 + 1, y0 } };
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++)
        glVertex3v16(PX(q[i][0]), PY(q[i][1]), -80);
    glEnd();
    color(255, 215, 90);
    glBegin(GL_TRIANGLES);
    glVertex3v16(PX(x0), PY(y0), -120);
    glVertex3v16(PX(x1), PY(y1), -120);
    glVertex3v16(PX(x1), PY(y1), -120);
    glEnd();
}

static void draw_scene(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));

    // Backlit screen and the target shadow.
    glBindTexture(0, 0);
    color(255, 196, 120);
    rect(48, 40, 208, 176, 400, false);
    color(70, 40, 30);
    for (int i = 0; i < LIMBS; i++)
    {
        int dx, dy, nx, ny;
        limb_axes(&limbs[i], target[i], &dx, &dy, &nx, &ny);
        int hw = limbs[i].w / 2, len = limbs[i].len;
        int ex = (dx * len) >> 12, ey = (dy * len) >> 12;
        int wx = (nx * hw) >> 12, wy = (ny * hw) >> 12;
        int px = limbs[i].px, py = limbs[i].py;
        const int p[4][2] = {
            { px - wx, py - wy }, { px + ex - wx, py + ey - wy },
            { px + ex + wx, py + ey + wy }, { px + wx, py + wy },
        };
        quad4(p, 300, false);
    }

    // Puppet: limbs, torso, head in paper.
    g3_texture_bind(TEX_PAPER);
    color(255, 255, 255);
    for (int i = 0; i < LIMBS; i++)
        limb_quad(i, angle[i], 0);
    rect(115, 70, 141, 108, 20, true);
    rect(119, 50, 137, 69, 20, true);

    // Crossbar and strings.
    glBindTexture(0, 0);
    color(90, 60, 40);
    rect(84, BAR_Y - 2, 172, BAR_Y + 2, -60, false);
    for (int i = 0; i < LIMBS; i++)
    {
        int tx, ty;
        tip(i, angle[i], &tx, &ty);
        string_draw(bar_x[i], BAR_Y + 2, tx, ty);
        color(230, 60, 50);  // handle marker at the limb tip
        rect(tx - 2, ty - 2, tx + 3, ty + 3, -140, false);
    }
    g3_flush(false);
}

// --- Bottom screen sliders ---------------------------------------------

static void slider_draw(int c)
{
    text_fill(&sub, 8 * c, 3, 8, 19, 0, TP_WHITE);
    for (int r = 3; r < 22; r++)
        text_put(&sub, 8 * c + 4, r, TP_GRAY, ":");
    int row = ty_from_angle(angle[c]) / 8;
    text_fill(&sub, 8 * c + 3, row, 3, 1, UI_SOLID, TP_YELLOW);
}

static void setup(void)
{
    display_reset();
    vram_layout(VL_TERRAIN);
    g3_textures_reset();
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_OUTLINE | GL_ANTIALIAS | GL_TOON_HIGHLIGHT
              | GL_CLEAR_BMP);
    glEnable(GL_TEXTURE_2D | GL_BLEND | GL_ALPHA_TEST);
    g3_texture_load(TEX_PAPER);

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

    glClearColor(2, 2, 6, 31);
    glClearPolyID(0);
    fx_bright(FX_BOTH, 0);
}

// Shared loop: live pen input, or the touch script when `steps` is given.
static LtScene act1_loop(const Act1Step *steps, int nsteps, int end_frame, Act1Outcome *out)
{
    setup();
    for (int i = 0; i < LIMBS; i++)
        angle[i] = 0;
    score = hold = 0;
    cleared = false;

    text_put(&con, 1, 0, TP_YELLOW, "ACT 1  MATCH THE SHADOW");
    text_put(&sub, 1, 0, TP_CYAN, "DRAG A SLIDER");
    static const char *const names[LIMBS] = { "L ARM", "R ARM", "L LEG", "R LEG" };
    for (int i = 0; i < LIMBS; i++)
    {
        text_put(&sub, 8 * i + 1, 1, TP_WHITE, names[i]);
        slider_draw(i);
    }
    text_put(&sub, 1, 23, TP_GRAY, steps ? "REHEARSAL" : "B: BACK");

    int f = 0, clear_frame = -1;
    for (;; f++)
    {
        bool touching = cv_in.touching;
        int tx = cv_in.tx, ty = cv_in.ty;
        if (steps)
        {
            touching = false;
            for (int i = 0; i < nsteps && steps[i].frame <= f; i++)
            {
                touching = steps[i].ty != 0;
                tx = steps[i].tx;
                ty = steps[i].ty;
            }
        }
        if (touching && !cleared)
        {
            int c = tx / 64;
            if (c >= 0 && c < LIMBS && ty >= SLIDER_TOP - 8)
            {
                angle[c] = angle_from_ty(ty);
                slider_draw(c);
            }
        }

        score = silhouette_score(angle);
        hold = score >= CLEAR_SCORE ? hold + 1 : 0;
        if (hold >= CLEAR_HOLD && !cleared)
        {
            cleared = true;
            clear_frame = f;
            text_put(&con, 1, 0, TP_GREEN, "ACT 1  CLEAR!          ");
            text_put(&sub, 1, 23, TP_GREEN, "A: ACT 2   B: BACK ");
        }
        text_printf(&con, 1, 22, cleared ? TP_GREEN : TP_WHITE, "MATCH %3d%%", score);

        draw_scene();

        volatile CvResult *r = CV_RESULT;
        r->demo_id = ACT1_ID;
        r->frame = cv_frame;
        r->values[0] = (u32)score;
        r->values[1] = cleared;
        for (int i = 0; i < LIMBS; i++)
            r->values[2 + i] = (u32)(angle[i] + 90);
        r->magic = CV_RESULT_MAGIC;

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
                    out->score = score;
                    out->cleared = cleared;
                    out->clear_frame = clear_frame;
                }
                return SC_TITLE;
            }
        }
        else if (cleared && (cv_in.pressed & KEY_A))
            return SC_ACT2;
        else if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}

LtScene scene_act1(void)
{
    return act1_loop(NULL, 0, 0, NULL);
}

void act1_replay(const Act1Step *steps, int n, int end_frame, Act1Outcome *out)
{
    act1_loop(steps, n, end_frame, out);
}
