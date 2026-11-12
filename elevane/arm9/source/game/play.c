// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Playing one puzzle. Top screen: an elevation (the front view the order asks for, or
// the right, back or left side with L/R), bottom screen: the plan view, both 3D through
// the display capture (gfx/dual.c). A text layer shows the same state as a 2D grid;
// Select switches to a screen pair that is only that grid, which solves every puzzle
// without 3D.
//
// Chapters 1-3 are piles (A raises the column, B lowers it). From chapter 4 the work
// layer exists: Y + D-pad up/down picks it (TOP = the pile behaviour), or touch the layer
// bar on the right; A/tap switches a cell of that layer on, B off, so a gap under a block
// (an arch) can be built. Chapter 5 puzzles have several structures (parts): a Y tap
// switches to the next.

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/snd.h"
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
#define BAR_X         232  // layer bar: touch x from here
#define BAR_Y0        24   // top of the highest layer button; 16 px per button

int ev_chapter = 1, ev_index = 1;
void (*ev_play_hook)(void);
unsigned ev_delivered_total;
bool ev_view3d = true;

static PzPuzzle pz;
static PzGrid grids[PZ_MAX_PARTS];
static int cur_part;
static const PzPart *part;
static PzGrid *grid;
static int cur_x, cur_y;
static int view_dir;       // 0 front, 1 right, 2 back, 3 left
static int layer = -1;     // work layer, -1 = TOP (pile behaviour)
static bool y_down, y_used;
static bool delivered, matched, part_ok, floating_any;
static int touch_x = -1, touch_y = -1, touch_frames;
static int delivered_blocks, total_par;
static bool want_showcase, resume, want_list;
static unsigned serial;
static CvText top_text, bot_text;
static PzGrid float_grid;

// --- accessors for the rehearsal, the QA pilot and the showcase -----------------

const PzGrid *play_grid(void) { return grid; }
const PzPart *play_part(void) { return part; }
const PzGrid *play_grid_n(int k) { return &grids[k]; }
const PzPart *play_part_n(int k) { return &pz.part[k]; }
int play_nparts(void) { return pz.nparts; }
int play_part_index(void) { return cur_part; }
int play_mode(void) { return pz.mode; }
int play_layer(void) { return layer; }
unsigned play_serial(void) { return serial; }
void play_cursor(int *x, int *y) { *x = cur_x; *y = cur_y; }
bool play_matched(void) { return matched; }
bool play_delivered(void) { return delivered; }
int play_blocks(void)
{
    int n = 0;
    for (int k = 0; k < pz.nparts; k++)
        n += pz_blocks(&grids[k], &pz.part[k]);
    return n;
}
int play_par(void) { return total_par; }

// --- geometry ----------------------------------------------------------------

// Cell size, from the grid.
static int32_t cell_u(void)
{
    int m = part->w > part->d ? part->w : part->d;
    if (part->h > m)
        m = part->h;
    if (m < 4)
        m = 4;
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

enum { TINT_NORMAL, TINT_WRONG, TINT_FLOAT, TINT_LAYER };

// One block with all five visible faces (the side views turn it around).
static void cube(int x, int y, int z, int tint)
{
    int32_t u = cell_u(), g = u / 14;
    int32_t x0 = gl_x(x) + g, x1 = gl_x(x) + u - g;
    int32_t y0 = z * u + g, y1 = z * u + u - g;
    int32_t z0 = gl_z(y) + g, z1 = gl_z(y) + u - g;
    int lit = z * 2 + (part->d - 1 - y);   // taller and nearer blocks read lighter
    if (lit > 8)
        lit = 8;
    u16 top, front, side, back, left;
    switch (tint)
    {
        case TINT_WRONG:
            top = RGB15(31, 11 + lit / 2, 9); front = RGB15(25, 6 + lit / 2, 5);
            side = RGB15(18, 4, 4); back = RGB15(14, 3, 3); left = RGB15(16, 4, 4);
            break;
        case TINT_FLOAT:
            top = RGB15(26, 10, 28); front = RGB15(20, 6, 23);
            side = RGB15(14, 4, 17); back = RGB15(11, 3, 14); left = RGB15(13, 4, 16);
            break;
        case TINT_LAYER:
            top = RGB15(31, 29 + lit / 4, 14); front = RGB15(26, 22 + lit / 3, 9);
            side = RGB15(19, 15, 6); back = RGB15(15, 12, 5); left = RGB15(17, 14, 6);
            break;
        default:
            top = RGB15(20 + lit, 17 + lit, 9 + lit / 2); front = RGB15(16 + lit, 12 + lit, 6 + lit / 2);
            side = RGB15(12, 9, 5); back = RGB15(9, 7, 4); left = RGB15(10, 8, 4);
            break;
    }
    glBegin(GL_QUADS);
    colour(top);   quad(x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0);
    colour(front); quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1);
    colour(side);  quad(x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1);
    colour(left);  quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0);
    colour(back);  quad(x0, y0, z0, x0, y1, z0, x1, y1, z0, x1, y0, z0);
    glEnd();
}

static int block_tint(int x, int y, int z)
{
    if (float_grid.cell[y][x] >> z & 1)
        return TINT_FLOAT;
    if (pz_front_cell(part, grid, x, z) == 1 || pz_plan_cell(part, grid, x, y) == 1)
        return TINT_WRONG;
    if (z == layer)
        return TINT_LAYER;
    return TINT_NORMAL;
}

static void draw_structure(void)
{
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
    for (int y = part->d - 1; y >= 0; y--)
        for (int x = 0; x < part->w; x++)
            for (int z = 0; z < part->h; z++)
                if (grid->cell[y][x] >> z & 1)
                    cube(x, y, z, block_tint(x, y, z));
}

static void draw_elevation(void)
{
    int32_t u = cell_u(), S = u * (part->w > part->d ? part->w : part->d);
    int32_t cx = part->w * u / 2, zc = part->d * u / 2;
    int32_t cy = S * 2 / 5;
    glClearColor(3, 4, 10, 31);   // DUAL_FRONT_CLEAR
    static const int ex[4] = { 0, 1, 0, -1 }, ez[4] = { 1, 0, -1, 0 };
    ortho_camera(m_vec(cx + ex[view_dir] * F(6), cy, zc + ez[view_dir] * F(6)), m_vec(cx, cy, zc), 0, F(1), 0);
    if (view_dir == 0)
    {
        // The order: a blueprint wall behind the structure. Cells nobody has filled stay blue.
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
        glBegin(GL_QUADS);
        int32_t zb = -u / 3;
        if (layer >= 0)
        {
            colour(RGB15(13, 11, 3));   // the work layer
            quad(-u / 2, layer * u, zb + u / 20, part->w * u + u / 2, layer * u, zb + u / 20,
                 part->w * u + u / 2, layer * u + u, zb + u / 20, -u / 2, layer * u + u, zb + u / 20);
        }
        for (int x = 0; x < part->w; x++)
            for (int z = 0; z < part->h; z++)
                if (part->front[x] >> z & 1)
                {
                    colour(RGB15(7, 13, 24));
                    quad(gl_x(x) + u / 20, z * u + u / 20, zb, gl_x(x) + u - u / 20, z * u + u / 20, zb,
                         gl_x(x) + u - u / 20, z * u + u - u / 20, zb, gl_x(x) + u / 20, z * u + u - u / 20, zb);
                }
        colour(RGB15(12, 12, 14));   // the floor line
        quad(-u / 2, -u / 12, zb, part->w * u + u / 2, -u / 12, zb, part->w * u + u / 2, 0, zb, -u / 2, 0, zb);
        glEnd();
    }
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
            plan_sx[y][x] = 128 + (int)(((int64_t)(gl_x(x) + u / 2 - cx) * 96 * 4 / 3) / VIEW_H);
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
            int c = pz_front_cell(part, grid, x, z);
            bool have = false;
            for (int y = 0; y < part->d; y++)
                have |= grid->cell[y][x] >> z & 1;
            char s[4] = "   ";
            char ch = c == 1 ? 'X' : c == 2 ? '+' : have ? '#' : '.';
            for (int i = 0; i < cw && i < 3; i++)
                s[i] = ch;
            s[cw < 3 ? cw : 3] = 0;
            text_put(t, x0 + x * cw, y0 + (part->h - 1 - z), c == 1 ? TP_RED : c == 2 ? TP_CYAN : have ? TP_WHITE : TP_GRAY, s);
        }
}

// Plan grid, back row on top. Pile columns show their height; with a work layer chosen
// every cell shows whether that layer holds a block ('#'/'.'); a column with a gap
// under its top block is drawn in cyan, a floating block makes it magenta (yellow
// cursor wins over both).
static void grid_plan(CvText *t, int x0, int y0, int cw)
{
    for (int y = part->d - 1; y >= 0; y--)
        for (int x = 0; x < part->w; x++)
        {
            int h = pz_top(grid, x, y);
            int c = pz_plan_cell(part, grid, x, y);
            char ch;
            if (layer >= 0)
                ch = grid->cell[y][x] >> layer & 1 ? '#' : (c == 2 && h == 0 ? '+' : '.');
            else
                ch = h ? (char)('0' + h) : (c == 2 ? '+' : '.');
            int pal = c == 1 ? TP_RED : c == 2 ? TP_CYAN : h ? TP_WHITE : TP_GRAY;
            if (h && grid->cell[y][x] != (u8)((1u << h) - 1) && c == 0)
                pal = TP_GREEN;   // a gap under the top block: an arch
            if (float_grid.cell[y][x])
                pal = TP_RED;
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
    static const char *const dirs[4] = { "FRONT", "RIGHT", "BACK", "LEFT" };
    text_clear(&top_text);
    text_clear(&bot_text);
    int blocks = play_blocks();

    snprintf(buf, sizeof(buf), "%d-%02d %s", pz.chapter, pz.index, pz.name);
    text_put(&top_text, 1, 0, TP_YELLOW, buf);
    snprintf(buf, sizeof(buf), "PAR %d", total_par);
    text_put(&top_text, 31 - (int)strlen(buf), 0, TP_GRAY, buf);
    snprintf(buf, sizeof(buf), "%s VIEW", dirs[big ? 0 : view_dir]);
    text_put(&top_text, 1, 2, TP_GRAY, buf);
    if (pz.nparts > 1)
    {
        snprintf(buf, sizeof(buf), "PART %d/%d%s", cur_part + 1, pz.nparts, part_ok ? " OK" : "");
        text_put(&top_text, 31 - (int)strlen(buf), 2, part_ok ? TP_GREEN : TP_GRAY, buf);
    }
    if (big)
        grid_front(&top_text, (32 - part->w * 3) / 2, 5, 3);
    else
        grid_front(&top_text, 1, 3, 1);

    text_put(&bot_text, 1, 0, TP_GRAY, "PLAN");
    snprintf(buf, sizeof(buf), "BLOCKS %d", blocks);
    text_put(&bot_text, 31 - (int)strlen(buf), 0, TP_WHITE, buf);
    if (pz.mode)
    {
        snprintf(buf, sizeof(buf), layer < 0 ? "LAYER TOP" : "LAYER %d", layer + 1);
        text_put(&bot_text, 1, 2, layer < 0 ? TP_GRAY : TP_YELLOW, buf);
        // The layer bar (touch it): top layer first, TOP at the bottom.
        for (int z = part->h - 1; z >= 0; z--)
            text_printf(&bot_text, 30, 3 + 2 * (part->h - 1 - z), z == layer ? TP_YELLOW : TP_GRAY, "%c%d",
                        z == layer ? '>' : ' ', z + 1);
        text_put(&bot_text, 30, 3 + 2 * part->h, layer < 0 ? TP_YELLOW : TP_GRAY, layer < 0 ? ">T" : " T");
    }
    if (big)
        grid_plan(&bot_text, (32 - part->w * 3) / 2, 4, 3);
    else
        grid_plan(&bot_text, 1, 21 - part->d, 2);

    if (floating_any)
        text_center(&bot_text, 19, TP_RED, "FLOATING BLOCKS!");
    if (delivered)
    {
        int stars = pz_stars(delivered_blocks, total_par);
        snprintf(buf, sizeof(buf), "DELIVERED  %.*s", stars, "***");
        text_center(&bot_text, 20, TP_GREEN, buf);
        text_center(&bot_text, 22, TP_GRAY, "A: NEXT   B: BACK");
    }
    else
    {
        if (matched)
            text_center(&bot_text, 20, TP_GREEN, "MATCH!   A: DELIVER");
        text_center(&bot_text, 22, TP_GRAY, pz.mode ? "A ON  B OFF  Y+UP/DOWN LAYER" : "A +1  B -1  HOLD TOUCH: CLEAR");
        text_center(&bot_text, 23, TP_GRAY, big ? "SELECT: 3D  X: SHOWCASE" : "SELECT: 2D  L/R: TURN  X: SHOW");
    }
}

// --- input -------------------------------------------------------------------

static void set_cell(int x, int y, int z, bool on)
{
    if (z < 0 || z >= part->h)
        return;
    if (on)
        grid->cell[y][x] |= 1 << z;
    else
        grid->cell[y][x] &= ~(1 << z);
}

// "Raise" = A / tap: with a work layer that layer's cell, otherwise the pile's next block.
static void raise(int x, int y)
{
    if (layer >= 0)
        set_cell(x, y, layer, true);
    else
        set_cell(x, y, pz_top(grid, x, y), true);
}

static void lower(int x, int y)
{
    if (layer >= 0)
        set_cell(x, y, layer, false);
    else
        set_cell(x, y, pz_top(grid, x, y) - 1, false);
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

static void set_part(int k)
{
    cur_part = k;
    part = &pz.part[k];
    grid = &grids[k];
    cur_x = cur_y = 0;
    if (layer >= part->h)
        layer = part->h - 1;
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
    if (cv_in.pressed & KEY_R)
        view_dir = (view_dir + 1) & 3;
    if (cv_in.pressed & KEY_L)
        view_dir = (view_dir + 3) & 3;

    // Y alone (pressed and released without a D-pad key) = the next part; Y + up/down =
    // the work layer (chapter 4 on).
    bool y_now = cv_in.held & KEY_Y;
    if (y_now && !y_down)
        y_used = false;
    if (y_now && pz.mode)
    {
        if (cv_in.pressed & KEY_UP)
        {
            if (layer < part->h - 1)
                layer++;
            y_used = true;
        }
        if (cv_in.pressed & KEY_DOWN)
        {
            if (layer > -1)
                layer--;
            y_used = true;
        }
    }
    if (!y_now && y_down && !y_used && pz.nparts > 1 && !delivered)
        set_part((cur_part + 1) % pz.nparts);
    y_down = y_now;

    if (delivered)
        return;
    if (!y_now)
    {
        if (cv_in.repeat & KEY_LEFT && cur_x > 0)  cur_x--;
        if (cv_in.repeat & KEY_RIGHT && cur_x < part->w - 1) cur_x++;
        if (cv_in.repeat & KEY_UP && cur_y < part->d - 1) cur_y++;      // up on the plan = further back
        if (cv_in.repeat & KEY_DOWN && cur_y > 0) cur_y--;
    }
    if (cv_in.pressed & KEY_A)
    {
        if (matched)
        {
            delivered = true;
            delivered_blocks = play_blocks();
            ev_delivered_total++;
            snd_sfx(CV_SFX_CHIME, 100, 64);
        }
        else
        {
            raise(cur_x, cur_y);
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
    }
    if (cv_in.pressed & KEY_B)
    {
        lower(cur_x, cur_y);
        snd_sfx(CV_SFX_CLICK, 60, 64);
    }
    if (cv_in.touching)
    {
        int x = 0, y = 0;
        if (pz.mode && ev_view3d && cv_in.tx >= BAR_X)
        {
            // The layer bar: rows of 16 px from the top layer down, TOP below them.
            int row = (cv_in.ty - BAR_Y0) / 16;
            if (cv_in.ty >= BAR_Y0 && row < part->h)
                layer = part->h - 1 - row;
            else if (row >= part->h && row <= part->h)
                layer = -1;
        }
        else if (hit_cell(cv_in.tx, cv_in.ty, &x, &y))
        {
            if (x != touch_x || y != touch_y)
            {
                touch_x = x;
                touch_y = y;
                touch_frames = 0;
                cur_x = x;
                cur_y = y;
                raise(x, y);   // a tap raises the column
                snd_sfx(CV_SFX_CLICK, 80, 64);
            }
            else if (++touch_frames == HOLD_CLEAR)
            {
                grid->cell[y][x] = 0;   // a long press empties it
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
    memset(grids, 0, sizeof(grids));
    total_par = 0;
    for (int k = 0; k < pz.nparts; k++)
        total_par += pz.part[k].par;
    layer = -1;
    view_dir = 0;
    y_down = y_used = false;
    set_part(0);
    delivered = matched = part_ok = floating_any = false;
    touch_x = touch_y = -1;
    want_list = false;
    want_showcase = false;
    serial++;
}

static void judge(void)
{
    part_ok = pz_matches(part, grid);
    matched = !delivered;
    for (int k = 0; k < pz.nparts && matched; k++)
        matched = pz_matches(&pz.part[k], &grids[k]);
    pz_floating(part, grid, &float_grid);
    floating_any = false;
    for (int y = 0; y < part->d; y++)
        for (int x = 0; x < part->w; x++)
            floating_any |= float_grid.cell[y][x] != 0;
}

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
        judge();
        overlay(!in3d);
        if (in3d)
        {
            if (top)
                draw_elevation();
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
