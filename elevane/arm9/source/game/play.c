// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Playing one puzzle (piles, chapters 1-3). Top screen: front view, bottom screen:
// plan view, both 3D through the display capture (gfx/dual.c). A text layer shows
// the same state as a 2D grid; Select switches to a screen pair that is only that
// grid, which solves every puzzle without 3D.

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/hw.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/snd.h"
#include "assets.h"
#include "game/play.h"
#include "game/puzzle.h"
#include "game/scenes.h"
#include "gfx/dual.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"
#include "gfx/text.h"
#include "gfx/vram.h"

#define F CV_FX

#define MAIN_BG       ((u8 *)0x06000000)
#define MAP_TOP       28   // 2 KB blocks of main BG VRAM
#define MAP_BOTTOM    29
#define HOLD_CLEAR    30   // frames of touch that empty a column

int ev_chapter = 1, ev_index = 1;
void (*ev_play_hook)(void);
bool ev_view3d = true;

static PzPuzzle pz;
static const PzPart *part;
static PzGrid grid;
static int cur_x, cur_y;
static bool delivered, matched;
static int touch_x = -1, touch_y = -1, touch_frames;
static int delivered_blocks;
static bool want_showcase;
static bool resume;       // coming back from the showcase: keep the pile
static bool want_list;   // Start: back to the puzzle list
static CvText top_text, bot_text;

// Cell size and the view cameras, from the grid.
static int32_t cell_u(void)
{
    int m = part->w > part->d ? part->w : part->d;
    return F(2.4) / m;
}

// Both views are orthographic elevations (a drafting view): what the silhouette
// check sees is exactly what is drawn. VIEW_H is half the visible height.
#define VIEW_H F(1.7)

static void ortho_camera(CvVec eye, CvVec at, int upx, int upy, int upz)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof32(-VIEW_H * 4 / 3, VIEW_H * 4 / 3, -VIEW_H, VIEW_H, F(0.1), F(20));
    glViewport(0, 0, 255, 191);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(eye.x, eye.y, eye.z, at.x, at.y, at.z, upx, upy, upz);
}

// Floor centre of every column as the plan camera sees it, for the touch screen.
static int plan_sx[PZ_MAX_DIM][PZ_MAX_DIM], plan_sy[PZ_MAX_DIM][PZ_MAX_DIM];
static int plan_pitch = 1;

// GL position of a cell's corner: x right, y up, z towards the front-view camera.
static int32_t gl_x(int x) { return x * cell_u(); }
static int32_t gl_z(int y) { return (part->d - 1 - y) * cell_u(); }

static void colour(u16 c) { GFX_COLOR = c; }

static void quad(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                 int32_t x2, int32_t y2, int32_t z2, int32_t x3, int32_t y3, int32_t z3)
{
    glVertex3v16(x0, y0, z0); glVertex3v16(x1, y1, z1);
    glVertex3v16(x2, y2, z2); glVertex3v16(x3, y3, z3);
}

// One block: the faces the two views can see (top, front, right, left).
static void cube(int x, int y, int z, bool wrong)
{
    int32_t u = cell_u(), g = u / 14;
    int32_t x0 = gl_x(x) + g, x1 = gl_x(x) + u - g;
    int32_t y0 = z * u + g, y1 = z * u + u - g;
    int32_t z0 = gl_z(y) + g, z1 = gl_z(y) + u - g;
    int lit = z * 2 + (part->d - 1 - y);   // taller and nearer blocks read lighter
    if (lit > 8)
        lit = 8;
    u16 top = wrong ? RGB15(31, 11 + lit / 2, 9) : RGB15(20 + lit, 17 + lit, 9 + lit / 2);
    u16 front = wrong ? RGB15(25, 6 + lit / 2, 5) : RGB15(16 + lit, 12 + lit, 6 + lit / 2);
    u16 side = wrong ? RGB15(18, 4, 4) : RGB15(12, 9, 5);
    glBegin(GL_QUADS);
    colour(top);   quad(x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0);
    colour(front); quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1);
    colour(side);  quad(x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1);
    colour(side);  quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0);
    glEnd();
}

static bool block_wrong(int x, int y, int z)
{
    return pz_front_cell(part, &grid, x, z) == 1 || pz_plan_cell(part, &grid, x, y) == 1;
}

static void draw_structure(void)
{
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
    for (int y = part->d - 1; y >= 0; y--)
        for (int x = 0; x < part->w; x++)
            for (int z = 0; z < part->h; z++)
                if (grid.cell[y][x] >> z & 1)
                    cube(x, y, z, block_wrong(x, y, z));
}

static void draw_front_view(void)
{
    int32_t u = cell_u(), S = u * (part->w > part->d ? part->w : part->d);
    int32_t cx = part->w * u / 2, zc = part->d * u / 2;
    int32_t cy = S * 2 / 5;
    glClearColor(3, 4, 10, 31);   // DUAL_FRONT_CLEAR
    ortho_camera(m_vec(cx, cy, zc + F(6)), m_vec(cx, cy, zc), 0, F(1), 0);
    // The order: a blueprint wall behind the structure. Cells nobody has filled stay blue.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
    glBegin(GL_QUADS);
    int32_t zb = -u / 3;
    for (int x = 0; x < part->w; x++)
        for (int z = 0; z < part->h; z++)
            if (part->front[x] >> z & 1)
            {
                colour(RGB15(7, 13, 24));
                quad(gl_x(x) + u / 20, z * u + u / 20, zb, gl_x(x) + u - u / 20, z * u + u / 20, zb,
                     gl_x(x) + u - u / 20, z * u + u - u / 20, zb, gl_x(x) + u / 20, z * u + u - u / 20, zb);
            }
    // The floor line.
    colour(RGB15(12, 12, 14));
    quad(-u / 2, -u / 12, zb, part->w * u + u / 2, -u / 12, zb, part->w * u + u / 2, 0, zb, -u / 2, 0, zb);
    glEnd();
    draw_structure();
}

static void draw_plan_view(void)
{
    int32_t u = cell_u();
    int32_t cx = part->w * u / 2, zc = part->d * u / 2;
    glClearColor(4, 3, 8, 31);    // DUAL_PLAN_CLEAR
    ortho_camera(m_vec(cx, F(6), zc), m_vec(cx, 0, zc), 0, 0, F(-1));
    // Floor: the order is the lit tiles.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
    glBegin(GL_QUADS);
    int32_t g = u / 24;
    for (int y = 0; y < part->d; y++)
        for (int x = 0; x < part->w; x++)
        {
            colour(part->plan[y] >> x & 1 ? RGB15(8, 14, 23) : RGB15(2, 3, 7));
            quad(gl_x(x) + g, 0, gl_z(y) + u - g, gl_x(x) + u - g, 0, gl_z(y) + u - g,
                 gl_x(x) + u - g, 0, gl_z(y) + g, gl_x(x) + g, 0, gl_z(y) + g);
        }
    glEnd();
    // The cursor.
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(3));
    glBegin(GL_QUADS);
    colour(RGB15(31, 28, 6));
    int32_t cg = u / 10, up = F(0.02);
    quad(gl_x(cur_x) + cg, up, gl_z(cur_y) + u - cg, gl_x(cur_x) + u - cg, up, gl_z(cur_y) + u - cg,
         gl_x(cur_x) + u - cg, up, gl_z(cur_y) + cg, gl_x(cur_x) + cg, up, gl_z(cur_y) + cg);
    glEnd();
    draw_structure();
    // Where the touch screen should look for each column: the camera is linear.
    for (int y = 0; y < part->d; y++)
        for (int x = 0; x < part->w; x++)
        {
            plan_sx[y][x] = 128 + (int)(((int64_t)(gl_x(x) + u / 2 - cx) * 96 * 4 / 3) / VIEW_H) ;
            plan_sy[y][x] = 96 + (int)(((int64_t)(gl_z(y) + u / 2 - zc) * 96) / VIEW_H);
        }
    plan_pitch = (int)(((int64_t)u * 96 * 4 / 3) / VIEW_H);
    if (plan_pitch < 4)
        plan_pitch = 4;
}

// --- 2D grid -----------------------------------------------------------------

// Front grid: one cell per (x, z), top row = highest layer.
static void grid_front(CvText *t, int x0, int y0, int cw)
{
    for (int z = part->h - 1; z >= 0; z--)
        for (int x = 0; x < part->w; x++)
        {
            int c = pz_front_cell(part, &grid, x, z);
            bool have = false;
            for (int y = 0; y < part->d; y++)
                have |= grid.cell[y][x] >> z & 1;
            char s[4] = "   ";
            char ch = c == 1 ? 'X' : c == 2 ? '+' : have ? '#' : '.';
            for (int i = 0; i < cw && i < 3; i++)
                s[i] = ch;
            s[cw < 3 ? cw : 3] = 0;
            text_put(t, x0 + x * cw, y0 + (part->h - 1 - z), c == 1 ? TP_RED : c == 2 ? TP_CYAN : have ? TP_WHITE : TP_GRAY, s);
        }
}

// Plan grid: column heights, back row on top (as the plan view shows it).
static void grid_plan(CvText *t, int x0, int y0, int cw)
{
    for (int y = part->d - 1; y >= 0; y--)
        for (int x = 0; x < part->w; x++)
        {
            int h = pz_height(&grid, x, y);
            int c = pz_plan_cell(part, &grid, x, y);
            char ch = h ? (char)('0' + h) : (c == 2 ? '+' : '.');
            int pal = c == 1 ? TP_RED : c == 2 ? TP_CYAN : h ? TP_WHITE : TP_GRAY;
            if (x == cur_x && y == cur_y)
                pal = TP_YELLOW;
            char s[4] = "   ";
            s[cw / 2] = ch;
            s[cw < 3 ? cw : 3] = 0;
            text_put(t, x0 + x * cw, y0 + (part->d - 1 - y), pal, s);
        }
}

static void overlay(bool big)
{
    char buf[40];
    text_clear(&top_text);
    text_clear(&bot_text);
    int blocks = pz_blocks(&grid, part);

    snprintf(buf, sizeof(buf), "%d-%02d %s", pz.chapter, pz.index, pz.name);
    text_put(&top_text, 1, 0, TP_YELLOW, buf);
    snprintf(buf, sizeof(buf), "PAR %d", part->par);
    text_put(&top_text, 31 - (int)strlen(buf), 0, TP_GRAY, buf);
    text_put(&top_text, 1, 2, TP_GRAY, "FRONT");
    if (big)
        grid_front(&top_text, (32 - part->w * 3) / 2, 5, 3);
    else
        grid_front(&top_text, 1, 3, 1);

    text_put(&bot_text, 1, 0, TP_GRAY, "PLAN");
    snprintf(buf, sizeof(buf), "BLOCKS %d", blocks);
    text_put(&bot_text, 31 - (int)strlen(buf), 0, TP_WHITE, buf);
    if (big)
        grid_plan(&bot_text, (32 - part->w * 3) / 2, 4, 3);
    else
        grid_plan(&bot_text, 1, 16, 2);

    if (delivered)
    {
        int stars = pz_stars(delivered_blocks, part->par);
        snprintf(buf, sizeof(buf), "DELIVERED  %.*s", stars, "***");
        text_center(&bot_text, 20, TP_GREEN, buf);
        text_center(&bot_text, 22, TP_GRAY, "A: NEXT   B: BACK");
    }
    else
    {
        if (matched)
            text_center(&bot_text, 20, TP_GREEN, "MATCH!   A: DELIVER");
        text_center(&bot_text, 22, TP_GRAY, "A +1  B -1  HOLD TOUCH: CLEAR");
        text_center(&bot_text, 23, TP_GRAY, big ? "SELECT: 3D  X: SHOWCASE" : "SELECT: 2D  X: SHOWCASE");
    }
}

// --- input -------------------------------------------------------------------

static void set_column(int x, int y, int h)
{
    if (h < 0)
        h = 0;
    if (h > part->h)
        h = part->h;
    pz_set_height(&grid, x, y, h);
}

static bool hit_cell(int tx, int ty, int *ox, int *oy)
{
    if (ev_view3d)
    {
        int best = 1 << 30;
        for (int y = 0; y < part->d; y++)
            for (int x = 0; x < part->w; x++)
            {
                int dx = tx - plan_sx[y][x], dy = ty - plan_sy[y][x];
                int d = dx * dx + dy * dy;
                if (d < best)
                {
                    best = d;
                    *ox = x;
                    *oy = y;
                }
            }
        int lim = plan_pitch * 3 / 5;
        return best <= lim * lim;
    }
    // 2D grid: three characters per cell, rows from the back.
    int gx0 = ((32 - part->w * 3) / 2) * 8, gy0 = 4 * 8;
    int cx = (tx - gx0) / 24, row = (ty - gy0) / 8;
    if (tx < gx0 || ty < gy0 || cx >= part->w || row >= part->d)
        return false;
    *ox = cx;
    *oy = part->d - 1 - row;
    return true;
}

static void handle_input(void)
{
    if (cv_in.pressed & KEY_SELECT)
    {
        ev_view3d = !ev_view3d;
        return;   // the caller rebuilds the screens
    }
    if (cv_in.pressed & KEY_START)
    {
        want_list = true;
        return;
    }
    if (cv_in.pressed & KEY_X)
    {
        want_showcase = true;
        return;
    }
    if (delivered)
        return;
    if (cv_in.repeat & KEY_LEFT && cur_x > 0)  cur_x--;
    if (cv_in.repeat & KEY_RIGHT && cur_x < part->w - 1) cur_x++;
    if (cv_in.repeat & KEY_UP && cur_y < part->d - 1) cur_y++;      // up on the plan = further back
    if (cv_in.repeat & KEY_DOWN && cur_y > 0) cur_y--;
    if (cv_in.pressed & KEY_A)
    {
        if (matched)
        {
            delivered = true;
            delivered_blocks = pz_blocks(&grid, part);
            snd_sfx(CV_SFX_CHIME, 100, 64);
        }
        else
        {
            set_column(cur_x, cur_y, pz_height(&grid, cur_x, cur_y) + 1);
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
    }
    if (cv_in.pressed & KEY_B)
    {
        set_column(cur_x, cur_y, pz_height(&grid, cur_x, cur_y) - 1);
        snd_sfx(CV_SFX_CLICK, 60, 64);
    }
    if (cv_in.touching)
    {
        int x = 0, y = 0;
        if (hit_cell(cv_in.tx, cv_in.ty, &x, &y))
        {
            if (x != touch_x || y != touch_y)
            {
                touch_x = x;
                touch_y = y;
                touch_frames = 0;
                cur_x = x;
                cur_y = y;
                set_column(x, y, pz_height(&grid, x, y) + 1);   // a tap raises the column
                snd_sfx(CV_SFX_CLICK, 80, 64);
            }
            else if (++touch_frames == HOLD_CLEAR)
            {
                set_column(x, y, 0);                            // a long press empties it
                snd_sfx(CV_SFX_CLICK, 60, 40);
            }
        }
    }
    else
    {
        touch_x = touch_y = -1;
    }
}

// --- screens -----------------------------------------------------------------

static void enter_3d(void)
{
    display_reset();
    vram_layout(VL_DUAL3D);
    text_load_tiles(MAIN_BG);
    text_load_palette(CV_PAL_BG_MAIN);
    text_init(&top_text, (u16 *)(MAIN_BG + MAP_TOP * 2048), 0);
    text_init(&bot_text, (u16 *)(MAIN_BG + MAP_BOTTOM * 2048), 0);
    text_clear(&top_text);
    text_clear(&bot_text);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG | GL_FOG_ONLY_ALPHA | GL_CLEAR_BMP | GL_OUTLINE);
    glEnable(GL_ANTIALIAS);
    dual_begin(MAP_TOP, MAP_BOTTOM);
}

static void leave_3d(void)
{
    dual_end();
    glDisable(GL_ANTIALIAS);
}

static void enter_2d(void)
{
    scene_text_screens(&top_text, &bot_text);
}

static void load_puzzle(void)
{
    if (pz_find(ev_chapter, ev_index, &pz) < 0)
    {
        ev_chapter = 1;
        ev_index = 1;
        pz_find(1, 1, &pz);
    }
    part = &pz.part[0];
    memset(&grid, 0, sizeof(grid));
    cur_x = cur_y = 0;
    delivered = matched = false;
    touch_x = touch_y = -1;
    want_list = false;
    want_showcase = false;
}

const PzGrid *play_grid(void) { return &grid; }
const PzPart *play_part(void) { return part; }
void play_cursor(int *x, int *y) { *x = cur_x; *y = cur_y; }
bool play_matched(void) { return matched; }
bool play_delivered(void) { return delivered; }

EvScene scene_play(void)
{
    if (resume)
    {
        resume = false;
        want_showcase = want_list = false;
    }
    else
        load_puzzle();
    bool in3d = ev_view3d;
    if (in3d)
        enter_3d();
    else
        enter_2d();
    fx_bright(FX_BOTH, -16);
    fx_fade_in(FX_BOTH, 20, false);

    for (;;)
    {
        // Routing first: the bank swap and the capture must land in VBlank, before
        // the longer overlay redraw.
        bool top = in3d ? dual_begin_call() : false;
        matched = !delivered && pz_matches(part, &grid);
        overlay(!in3d);
        if (in3d)
        {
            if (top)
                draw_front_view();
            else
                draw_plan_view();
            dual_end_call();
        }
        scene_frame_end();
        if (ev_play_hook)
            ev_play_hook();   // the rehearsal drives the input
        bool was_delivered = delivered;
        handle_input();
        if (ev_view3d != in3d)
        {
            if (in3d)
                leave_3d();
            in3d = ev_view3d;
            if (in3d)
                enter_3d();
            else
                enter_2d();
        }
        if (want_showcase)
        {
            if (in3d)
                leave_3d();
            resume = true;
            return SC_SHOWCASE;
        }
        if (want_list)
        {
            if (in3d)
                leave_3d();
            return SC_SELECT;
        }
        if (was_delivered && (cv_in.pressed & KEY_A))
        {
            if (ev_index < pz_chapter_size(ev_chapter))
            {
                ev_index++;
                load_puzzle();
            }
            else
            {
                if (in3d)
                    leave_3d();
                return SC_SELECT;
            }
        }
        else if (was_delivered && (cv_in.pressed & KEY_B))
        {
            if (in3d)
                leave_3d();
            return SC_SELECT;
        }
    }
}
