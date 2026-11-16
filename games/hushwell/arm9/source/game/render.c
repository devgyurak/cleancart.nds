// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// First-person rendering of the grid world. Walls, floors and ceilings are
// written straight to the geometry command ports with a colour per vertex:
// the flashlight cone, the emergency lamps, burning flares and lightning
// are evaluated in software once per grid corner each frame. Props and
// the creature itself are display lists lit by the four hardware lights.

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "core/save.h"
#include "game/game.h"
#include "game/world.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"
#include "gfx/oam.h"
#include "gfx/terrain.h"

#define F CV_FX

#define GX_PORT(cmd) (*(vu32 *)(0x04000400 + ((cmd) << 2)))
#define GXP_BEGIN    GX_PORT(GX_BEGIN)
#define GXP_END      GX_PORT(GX_END)
#define GXP_COLOR    GX_PORT(GX_COLOR)
#define GXP_TEXCOORD GX_PORT(GX_TEXCOORD)
#define GXP_VTX16    GX_PORT(GX_VTX16)

#define WALL_H     F(1.1)
#define WATER_Y    (-F(0.22))
#define RADIUS     7          // cells drawn around the player
#define GRID       (RADIUS * 2 + 2)
#define MAX_LAMPS  8

// Light at each grid corner for the floor (0) and ceiling (1) heights.
static u16 light_grid[2][GRID][GRID];
static int grid_x0, grid_y0;
static int cells_drawn;

static CvVec lamps[MAX_LAMPS];
static int nlamps;

static CvVec cam_eye, cam_fwd, beam_dir;
static bool rear_view;   // dual-screen chase: looking back, no flashlight
static int light_r, light_g, light_b;   // flashlight colour (0-31)

int render_cells_drawn(void)
{
    return cells_drawn;
}

void render_init(void)
{
    nlamps = 0;
    for (int i = 0; i < W.map.nobj && nlamps < MAX_LAMPS; i++)
        if (W.map.obj[i].type == O_LAMP)
            lamps[nlamps++] = m_vec(CV_FXI(W.map.obj[i].x) + F(0.5), WALL_H - F(0.1),
                                    CV_FXI(W.map.obj[i].y) + F(0.5));
    // Warm white with a sixth of the firmware favourite colour.
    u16 c = cv_game.light_color;
    light_r = (31 * 5 + (c & 31)) / 6;
    light_g = (29 * 5 + ((c >> 5) & 31)) / 6;
    light_b = (22 * 5 + ((c >> 10) & 31)) / 6;
}

void render_camera(CvVec eye, int yaw, int pitch)
{
    int32_t cp = m_cos(pitch), sp = m_sin(pitch);
    CvVec f = m_vec(CV_FXMUL(m_sin(yaw), cp), -sp, -CV_FXMUL(m_cos(yaw), cp));
    cam_eye = eye;
    cam_fwd = f;
    // The flashlight is held a little low: its beam points 10 degrees down.
    beam_dir = m_norm(m_add(f, m_vec(0, -F(0.18), 0)), F(1));
    g3_camera(eye, m_add(eye, f), W.pl.aim ? 54 : 64, F(11));
}

// ---------------------------------------------------------------------------
// Software lighting

static inline int clamp31(int v)
{
    return v < 0 ? 0 : (v > 31 ? 31 : v);
}

static u16 light_at(int32_t x, int32_t y, int32_t z)
{
    if (W.debug_lights)
        return RGB15(28, 28, 28);
    int amb = W.map.ambient + cv_save.brightness * 2;
    int r = amb, g = amb, b = amb + 1;

    // Units of 1/64 world unit keep the squares inside 32 bits.
    int32_t dx = (x - cam_eye.x) >> 6, dy = (y - cam_eye.y) >> 6, dz = (z - cam_eye.z) >> 6;
    int32_t d2 = dx * dx + dy * dy + dz * dz;

    // A little light always reaches the walls right around you.
    if (d2 < 64 * 64 * 2)
    {
        int near = 6 - d2 / (64 * 64 * 2 / 6);
        r += near;
        g += near;
        b += near;
    }

    bool beam = W.pl.light && !rear_view && cv_game.battery > 0 && !W.pl.flicker && !W.peek;
    if (beam)
    {
        int32_t zz = (dx * beam_dir.x + dy * beam_dir.y + dz * beam_dir.z) >> 12;
        if (zz > 8)
        {
            int32_t z2 = zz * zz;
            int32_t lat2 = d2 - z2;
            if (lat2 < z2)
            {
                // cone: 1 in the hot centre, falling to 0 at about 32 degrees
                int cone = 256 - m_div(lat2 * 640, z2);
                int soft = 256 - m_div(lat2 * 256, z2);
                int fall = 256 - zz * 256 / (64 * 8);
                if (fall > 0)
                {
                    int i = ((cone > 0 ? cone : 0) * 24 + (soft > 0 ? soft : 0) * 10) * fall >> 16;
                    r += i * light_r / 31;
                    g += i * light_g / 31;
                    b += i * light_b / 31;
                }
            }
        }
    }

    for (int k = 0; k < nlamps; k++)
    {
        int32_t lx = (x - lamps[k].x) >> 6, ly = (y - lamps[k].y) >> 6, lz = (z - lamps[k].z) >> 6;
        int32_t l2 = lx * lx + ly * ly + lz * lz;
        const int32_t r2 = 64 * 64 * 9;   // three cells
        if (l2 < r2)
        {
            int i = (r2 - l2) * (W.lamp_flicker ? 8 : 20) / r2;
            r += i;
            g += i / 6;
            b += i / 8;
        }
    }
    for (int k = 0; k < MAX_FLARES; k++)
    {
        const Flare *fl = &W.fl[k];
        if (!fl->alive)
            continue;
        int32_t lx = (x - fl->pos.x) >> 6, ly = (y - fl->pos.y) >> 6, lz = (z - fl->pos.z) >> 6;
        int32_t l2 = lx * lx + ly * ly + lz * lz;
        const int32_t r2 = 64 * 64 * 20;
        if (l2 < r2)
        {
            int i = (r2 - l2) * ((W.frame & 4) ? 30 : 24) / r2;
            r += i;
            g += i * 2 / 3;
            b += i / 4;
        }
    }
    if (W.lightning)
    {
        r += W.lightning * 2;
        g += W.lightning * 2;
        b += W.lightning * 3;
    }
    return RGB15(clamp31(r), clamp31(g), clamp31(b));
}

static void compute_lights(void)
{
    int px = W.pl.pos.x >> 12, py = W.pl.pos.z >> 12;
    grid_x0 = px - RADIUS;
    grid_y0 = py - RADIUS;
    for (int j = 0; j < GRID; j++)
    {
        for (int i = 0; i < GRID; i++)
        {
            int32_t x = CV_FXI(grid_x0 + i), z = CV_FXI(grid_y0 + j);
            light_grid[0][j][i] = light_at(x, 0, z);
            light_grid[1][j][i] = (W.map.flags & MF_OUTDOOR) ? light_grid[0][j][i] : light_at(x, WALL_H, z);
        }
    }
}

static inline u16 corner(int level, int x, int y)
{
    int i = x - grid_x0, j = y - grid_y0;
    if (i < 0 || j < 0 || i >= GRID || j >= GRID)
        return 0;
    return light_grid[level][j][i];
}

// ---------------------------------------------------------------------------
// Grid geometry (direct port writes)

static inline void vtx(int32_t x, int32_t y, int32_t z)
{
    GXP_VTX16 = (u32)(x & 0xFFFF) | ((u32)(y & 0xFFFF) << 16);
    GXP_VTX16 = (u32)(z & 0xFFFF);
}

static inline u32 tc(int s, int t)
{
    return ((u32)(s << 4) & 0xFFFF) | ((u32)(t << 4) << 16);
}

// Untextured ground takes its colour from a tint (31 = unchanged).
static int tint_r = 31, tint_g = 31, tint_b = 31;

static inline u16 tinted(u16 c)
{
    if (tint_r == 31 && tint_g == 31 && tint_b == 31)
        return c;
    return RGB15((c & 31) * tint_r >> 5, ((c >> 5) & 31) * tint_g >> 5, ((c >> 10) & 31) * tint_b >> 5);
}

// Horizontal quad over the cell at height y (cell-local coordinates).
static void flat(int cx, int cy, int32_t y, int level, int ts)
{
    GXP_BEGIN = GX_QUADS;
    GXP_COLOR = tinted(corner(level, cx, cy));
    GXP_TEXCOORD = tc(0, 0);
    vtx(0, y, 0);
    GXP_COLOR = tinted(corner(level, cx, cy + 1));
    GXP_TEXCOORD = tc(0, ts);
    vtx(0, y, F(1));
    GXP_COLOR = tinted(corner(level, cx + 1, cy + 1));
    GXP_TEXCOORD = tc(ts, ts);
    vtx(F(1), y, F(1));
    GXP_COLOR = tinted(corner(level, cx + 1, cy));
    GXP_TEXCOORD = tc(ts, 0);
    vtx(F(1), y, 0);
    GXP_END = 0;
}

// Vertical face on edge `side` (0 north, 1 east, 2 south, 3 west) of the cell.
static void face(int cx, int cy, int side, int32_t y0, int32_t y1, int ts)
{
    static const s8 ax[4] = { 0, 1, 1, 0 }, az[4] = { 0, 0, 1, 1 };
    static const s8 bx[4] = { 1, 1, 0, 0 }, bz[4] = { 0, 1, 1, 0 };
    int ux = ax[side], uz = az[side], vx = bx[side], vz = bz[side];
    int top = y1 > F(0.5) ? 1 : 0;
    GXP_BEGIN = GX_QUADS;
    GXP_COLOR = corner(0, cx + ux, cy + uz);
    GXP_TEXCOORD = tc(0, ts);
    vtx(CV_FXI(ux), y0, CV_FXI(uz));
    GXP_COLOR = corner(0, cx + vx, cy + vz);
    GXP_TEXCOORD = tc(ts, ts);
    vtx(CV_FXI(vx), y0, CV_FXI(vz));
    GXP_COLOR = corner(top, cx + vx, cy + vz);
    GXP_TEXCOORD = tc(ts, 0);
    vtx(CV_FXI(vx), y1, CV_FXI(vz));
    GXP_COLOR = corner(top, cx + ux, cy + uz);
    GXP_TEXCOORD = tc(0, 0);
    vtx(CV_FXI(ux), y1, CV_FXI(uz));
    GXP_END = 0;
}

static bool walkable(int c)
{
    return c == CELL_FLOOR || c == CELL_WATER || c == CELL_GROUND;
}

static bool cell_visible(int x, int y)
{
    int32_t cx = CV_FXI(x) + F(0.5) - cam_eye.x, cz = CV_FXI(y) + F(0.5) - cam_eye.z;
    int32_t along = (int32_t)(((int64_t)cx * cam_fwd.x + (int64_t)cz * cam_fwd.z) >> 12);
    if (along < -F(1.2))
        return false;
    return CV_FXMUL(cx, cx) + CV_FXMUL(cz, cz) < F(64);
}

enum { PASS_FLOOR, PASS_WATER, PASS_WALL, PASS_WOOD, PASS_FENCE, PASS_CEIL, PASS_N };

static CvTex wall_tex(void)
{
    if (W.map.flags & MF_WATER)
        return TEX_TILES;
    if (W.chapter == 3)
        return TEX_WOOD;   // panelled stacks; the archive layout has no 4x4 slot
    return TEX_WALL;
}

static CvTex floor_tex(void)
{
    switch (W.chapter)
    {
        case 2: return TEX_TILES;
        case 3: return TEX_WOOD;
        case 0: case 4: return TEX_COUNT;
        default: return TEX_FLOOR;
    }
}

static void draw_grid(void)
{
    int px = W.pl.pos.x >> 12, py = W.pl.pos.z >> 12;
    bool outdoor = (W.map.flags & MF_OUTDOOR) != 0;
    cells_drawn = 0;
    for (int pass = 0; pass < PASS_N; pass++)
    {
        CvTex tex = TEX_COUNT;
        int ts = 64;
        u32 fmt = POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1) | POLY_FOG;
        switch (pass)
        {
            case PASS_FLOOR: tex = floor_tex(); break;
            case PASS_WATER: tex = wall_tex(); break;
            case PASS_WALL:  tex = outdoor ? TEX_BRANCH : wall_tex(); break;
            case PASS_WOOD:  tex = TEX_WOOD; fmt = POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2) | POLY_FOG; break;
            case PASS_FENCE: tex = TEX_BRANCH; break;
            case PASS_CEIL:
                if (outdoor)
                    continue;
                tex = TEX_COUNT;
                break;
        }
        if (W.chapter == 4 && (pass == PASS_WOOD))
            fmt |= POLY_TOON_HIGHLIGHT;
        ts = g3_texture_size(tex) > 32 || tex == TEX_BRANCH ? 64 : 32;
        // Wet asphalt on the road, dark soil in the garden.
        tint_r = tint_g = tint_b = 31;
        if (pass == PASS_FLOOR && tex == TEX_COUNT)
        {
            tint_r = W.chapter == 4 ? 12 : 13;
            tint_g = W.chapter == 4 ? 15 : 14;
            tint_b = W.chapter == 4 ? 10 : 17;
        }
        g3_texture_bind(tex);
        glPolyFmt(fmt);
        for (int y = py - RADIUS; y <= py + RADIUS; y++)
        {
            for (int x = px - RADIUS; x <= px + RADIUS; x++)
            {
                int c = world_cell(x, y);
                if (!walkable(c) || !cell_visible(x, y))
                    continue;
                glPushMatrix();
                glTranslatef32(CV_FXI(x), 0, CV_FXI(y));
                if (pass == PASS_FLOOR)
                {
                    cells_drawn++;
                    if (c != CELL_WATER)
                        flat(x, y, 0, 0, ts);
                    else
                        flat(x, y, WATER_Y, 0, ts);
                }
                else if (pass == PASS_CEIL)
                {
                    flat(x, y, WALL_H, 1, ts);
                }
                else
                {
                    static const s8 ddx[4] = { 0, 1, 0, -1 }, ddy[4] = { -1, 0, 1, 0 };
                    for (int s = 0; s < 4; s++)
                    {
                        int n = world_cell(x + ddx[s], y + ddy[s]);
                        if (pass == PASS_WATER)
                        {
                            if (c == CELL_WATER && n != CELL_WATER && walkable(n))
                                face(x, y, s, WATER_Y, 0, ts / 4);
                            continue;
                        }
                        bool want = (pass == PASS_WALL && n == CELL_WALL)
                                 || (pass == PASS_WOOD && (n == CELL_DOOR || n == CELL_SHELF))
                                 || (pass == PASS_FENCE && n == CELL_FENCE);
                        if (want)
                            face(x, y, s, c == CELL_WATER ? WATER_Y : 0,
                                 outdoor && pass == PASS_FENCE ? F(0.9) : WALL_H, ts);
                    }
                }
                glPopMatrix(1);
            }
        }
    }
}

static void draw_water(void)
{
    if (!(W.map.flags & MF_WATER))
        return;
    int px = W.pl.pos.x >> 12, py = W.pl.pos.z >> 12;
    g3_texture_bind(TEX_COUNT);
    // Translucent surface; polygon IDs alternate so neighbours blend.
    int id = 0;
    for (int y = py - RADIUS; y <= py + RADIUS; y++)
    {
        for (int x = px - RADIUS; x <= px + RADIUS; x++)
        {
            if (world_cell(x, y) != CELL_WATER || !cell_visible(x, y))
                continue;
            glPolyFmt(POLY_ALPHA(12) | POLY_CULL_NONE | POLY_ID(40 + (id++ & 3)) | POLY_FOG
                      | POLY_TRANS_SET_DEPTH);
            glPushMatrix();
            glTranslatef32(CV_FXI(x), 0, CV_FXI(y));
            int32_t wave = CV_FXMUL(F(0.02), m_sin(W.frame * 300 + (x + y) * 4000));
            GXP_BEGIN = GX_QUADS;
            for (int k = 0; k < 4; k++)
            {
                static const s8 qx[4] = { 0, 0, 1, 1 }, qz[4] = { 0, 1, 1, 0 };
                u16 l = corner(0, x + qx[k], y + qz[k]);
                GXP_COLOR = l | RGB15(2, 4, 5);
                vtx(CV_FXI(qx[k]), -F(0.03) + wave, CV_FXI(qz[k]));
            }
            GXP_END = 0;
            glPopMatrix(1);
        }
    }
}

// ---------------------------------------------------------------------------
// Props and the creature

static void set_model_lights(void)
{
    // Lights are set with an identity modelview so their directions stay in
    // view space: light 0 is the flashlight pointing into the screen.
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    bool beam = W.pl.light && !rear_view && cv_game.battery > 0 && !W.pl.flicker && !W.debug_lights;
    glLight(0, beam ? RGB15(light_r, light_g, light_b) : RGB15(2, 2, 2), 0, 0, -floattov10(0.99));
    glLight(1, nlamps ? RGB15(10 - (W.lamp_flicker ? 6 : 0), 1, 1) : RGB15(1, 1, 2),
            0, -floattov10(0.99), 0);
    glLight(2, W.lightning ? RGB15(20, 22, 31) : 0, floattov10(0.5), -floattov10(0.6), -floattov10(0.6));
    bool flare = W.fl[0].alive || W.fl[1].alive;
    glLight(3, flare ? RGB15(31, 18, 6) : (W.debug_lights ? RGB15(24, 24, 24) : 0),
            0, floattov10(0.6), -floattov10(0.8));
}

static int32_t prop_visible(CvVec at)
{
    CvVec d = m_sub(at, cam_eye);
    int32_t along = (int32_t)(((int64_t)d.x * cam_fwd.x + (int64_t)d.z * cam_fwd.z) >> 12);
    if (along < -F(0.5) || m_abs(d.x) > F(8) || m_abs(d.z) > F(8))
        return 0;
    return 1;
}

#define LIT (POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1 | POLY_FORMAT_LIGHT2 | POLY_FORMAT_LIGHT3)

static void draw_model_at(int mdl, CvVec at, int yaw, CvTex tex, int id, u32 extra)
{
    g3_texture_bind(tex);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(id) | LIT | POLY_FOG | extra);
    g3_push_at(at);
    if (yaw)
        g3_rot_y(yaw);
    g3_draw(&cv_models[mdl]);
    g3_pop();
}

static void draw_tree(CvVec at)
{
    // Two crossed alpha-tested quads of bare branches.
    g3_texture_bind(TEX_BRANCH);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(3) | POLY_FOG);
    glPushMatrix();
    glTranslatef32(at.x, 0, at.z);
    for (int k = 0; k < 2; k++)
    {
        int32_t sx = k ? 0 : F(0.6), sz = k ? F(0.6) : 0;
        GXP_BEGIN = GX_QUADS;
        GXP_COLOR = light_at(at.x, 0, at.z);
        GXP_TEXCOORD = tc(0, 32);
        vtx(-sx, 0, -sz);
        GXP_TEXCOORD = tc(32, 32);
        vtx(sx, 0, sz);
        GXP_COLOR = light_at(at.x, F(1.8), at.z);
        GXP_TEXCOORD = tc(32, 0);
        vtx(sx, F(2.2), sz);
        GXP_TEXCOORD = tc(0, 0);
        vtx(-sx, F(2.2), -sz);
        GXP_END = 0;
    }
    glPopMatrix(1);
}

static void draw_objects(void)
{
    for (int i = 0; i < W.map.nobj; i++)
    {
        const CvObj *o = &W.map.obj[i];
        CvVec at = m_vec(CV_FXI(o->x) + F(0.5), 0, CV_FXI(o->y) + F(0.5));
        if (!prop_visible(at) || world_obj_taken(i))
            continue;
        int spin = W.frame * 200;
        int32_t hover = F(0.25) + CV_FXMUL(F(0.04), m_sin(W.frame * 400));
        switch (o->type)
        {
            // Items carry polygon IDs 16-23: edge marking outlines them.
            case O_KEY:
                draw_model_at(MDL_KEY, m_add(at, m_vec(0, hover, 0)), spin, TEX_COUNT, 16, 0);
                break;
            case O_TAPE:
                draw_model_at(MDL_TAPE, m_add(at, m_vec(0, hover, 0)), spin, TEX_COUNT, 17, 0);
                break;
            case O_FLARES:
                draw_model_at(MDL_FLAREBOX, m_add(at, m_vec(0, F(0.05), 0)), 2000, TEX_COUNT, 18, 0);
                break;
            case O_BATTERY:
                draw_model_at(MDL_BATTERY, m_add(at, m_vec(0, hover, 0)), spin, TEX_COUNT, 19, 0);
                break;
            case O_RADIO:
                draw_model_at(MDL_RADIO, at, 0, TEX_WOOD, 20, 0);
                break;
            case O_BED:
                draw_model_at(MDL_BED, at, (o->x & 1) ? 8192 : 0, TEX_COUNT, 4, 0);
                break;
            case O_CAR:
                if (W.intro != 1)   // you are sitting in it
                    draw_model_at(MDL_CAR, at, 0, TEX_COUNT, 4, 0);
                break;
            case O_PEW:
                draw_model_at(MDL_PEW, at, 0, TEX_WOOD, 5, W.chapter == 4 ? POLY_TOON_HIGHLIGHT : 0);
                break;
            case O_ALTAR:
                draw_model_at(MDL_ALTAR, at, 0, TEX_WOOD, 5, W.chapter == 4 ? POLY_TOON_HIGHLIGHT : 0);
                break;
            case O_PROJECTOR:
                draw_model_at(MDL_PROJECTOR, at, 16384, TEX_COUNT, 6, 0);
                break;
            case O_WELL:
                draw_model_at(MDL_WELL, at, 0, TEX_COUNT, 7, 0);
                break;
            case O_LAMP:
                draw_model_at(MDL_LAMP, m_vec(at.x, WALL_H, at.z), 0, TEX_COUNT, 8, 0);
                break;
            case O_TREE:
                draw_tree(at);
                break;
            case O_EXIT:
                if (W.chapter == 0)
                    draw_model_at(MDL_GATE, m_vec(CV_FXI(o->x) + F(1), 0, at.z), 0, TEX_COUNT, 9,
                                  0);
                break;
        }
    }
}

// The creature: a hierarchy of parts pushed and popped on the matrix stack.
static void draw_creature(const Monster *m, bool visitor)
{
    if (!m->active || m->state == MS_GONE || !prop_visible(m->pos))
        return;
    int a = m->anim * (m->state == MS_CHASE ? 900 : 500);
    bool stunned = m->state == MS_STUN;
    u32 extra = stunned && W.chapter != 4 ? POLY_TOON_HIGHLIGHT : 0;
    CvTex tex = visitor ? TEX_COUNT : TEX_FLESH;
    // The visitor is black with a glowing outline (edge marking, IDs 48+).
    int id = visitor ? 48 : 24;
    u32 lights = visitor ? 0 : LIT;
    g3_texture_bind(tex);
    if (visitor)
        GFX_COLOR = 0;
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(id) | lights | POLY_FOG | extra);
    g3_push_at(m->pos);
    g3_rot_y(-m->yaw);
    if (stunned)
        g3_rot_z(CV_FXMUL(900, m_sin(m->anim * 3000)));
    // Hunched sway.
    glTranslatef32(0, F(0.02) * (m_sin(a) >> 11), 0);
    g3_draw(&cv_models[MDL_M_TORSO]);
    // Head, twitching.
    glPushMatrix();
    glTranslatef32(0, F(1.12), -F(0.08));
    g3_rot_y(CV_FXMUL(1200, m_sin(m->anim * 1700)));
    g3_rot_x(CV_FXMUL(800, m_sin(m->anim * 2300)) + 1500);
    g3_draw(&cv_models[MDL_M_HEAD]);
    glPopMatrix(1);
    // Arms: shoulder -> elbow, long and swinging.
    for (int s = -1; s <= 1; s += 2)
    {
        glPushMatrix();
        glTranslatef32(s * F(0.22), F(0.98), 0);
        g3_rot_x(CV_FXMUL(5000, m_sin(a + (s > 0 ? 16384 : 0))) - (m->state == MS_CHASE ? 6000 : 0));
        g3_rot_z(s * 1200);
        g3_draw(&cv_models[MDL_M_ARM]);
        glTranslatef32(0, -F(0.44), 0);
        g3_rot_x(-3000 - (m_sin(a) >> 3));
        g3_draw(&cv_models[MDL_M_FOREARM]);
        glPopMatrix(1);
    }
    // Legs.
    for (int s = -1; s <= 1; s += 2)
    {
        glPushMatrix();
        glTranslatef32(s * F(0.1), F(0.55), 0);
        g3_rot_x(CV_FXMUL(4000, m_sin(a + (s > 0 ? 0 : 16384))));
        g3_draw(&cv_models[MDL_M_LEG]);
        glPopMatrix(1);
    }
    g3_pop();
}

void render_creature(const Monster *m, bool visitor)
{
    draw_creature(m, visitor);
}

static void draw_translucent(void)
{
    draw_water();
    // A shadow polygon under the creature when the flashlight is on it.
    if (W.mo.active && W.mo.state != MS_GONE && W.pl.light && prop_visible(W.mo.pos)
        && !(W.map.flags & MF_WATER))
        shadow_volume(m_add(W.mo.pos, m_vec(0, F(1.0), 0)), F(0.22), 0, 24);   // spares the creature (ID 24)

    for (int i = 0; i < MAX_FLARES; i++)
    {
        const Flare *f = &W.fl[i];
        if (!f->alive)
            continue;
        g3_texture_bind(TEX_GLOW);
        bb_draw(f->pos, F(0.5) + ((W.frame & 3) << 9), RGB15(31, 18, 8), 18, 44 + i, 32);
        g3_texture_bind(TEX_FLAME);
        bb_draw(f->pos, F(0.12), RGB15(31, 31, 24), 31, 46 + i, 16);
    }
    for (int k = 0; k < nlamps; k++)
    {
        if (!prop_visible(lamps[k]))
            continue;
        g3_texture_bind(TEX_GLOW);
        bb_draw(m_add(lamps[k], m_vec(0, -F(0.05), 0)), W.lamp_flicker ? F(0.12) : F(0.28),
                RGB15(31, 6, 4), 20, 50 + (k & 7), 32);
    }
    for (int kind = PK_STEAM; kind <= PK_MOTE; kind++)
    {
        CvTex t = kind == PK_STEAM ? TEX_STEAM : (kind == PK_SPARK ? TEX_FLAME : TEX_COUNT);
        g3_texture_bind(t);
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            const Particle *p = &W.pa[i];
            if (!p->life || p->kind != kind || !prop_visible(p->pos))
                continue;
            int alpha = kind == PK_STEAM ? 2 + 10 * p->life / p->max : 4 + 27 * p->life / p->max;
            // IDs 33-39: the water uses 40-43 and equal IDs don't blend.
            bb_draw(p->pos, p->size, p->color, alpha, 33 + (i % 7), t == TEX_STEAM ? 32 : 16);
        }
    }
}

static void draw_swarm(void)
{
    // Chapter 5: a swarm of bugs on the walls near the 'q' marks. Each is a
    // single triangle; the count grows until the polygon RAM overflows,
    // then backs off (the overflow flag is read every frame).
    if (!(W.map.flags & MF_WELL))
        return;
    int sx = -1, sy = -1;
    for (int i = 0; i < W.map.nobj; i++)
        if (W.map.obj[i].type == O_SWARM)
        {
            sx = W.map.obj[i].x;
            sy = W.map.obj[i].y;
        }
    if (sx < 0)
        return;
    CvVec c = m_vec(CV_FXI(sx) + F(0.5), F(0.5), CV_FXI(sy) + F(0.5));
    if (m_dist(c, W.pl.pos) > F(5))
    {
        W.swarm_count = 120;
        return;
    }
    if (g3_overflowed())
    {
        W.overflow_seen = true;
        W.swarm_count -= 60;
    }
    else if (W.swarm_count < 1700)
    {
        W.swarm_count += 12;
    }
    g3_texture_bind(TEX_COUNT);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(60));
    glPushMatrix();
    glTranslatef32(c.x, c.y, c.z);
    GXP_BEGIN = GX_TRIS;
    u32 h = 0x9E3779B9u;
    for (int i = 0; i < W.swarm_count; i++)
    {
        h = h * 1664525u + 1013904223u;
        int ang = (h >> 8) & 0x7FFF;
        int32_t rad = F(0.3) + ((h >> 4) & 0xFFF) / 4;
        int32_t y = ((int32_t)(h & 0x7FF) - 0x400) * 2 + CV_FXMUL(F(0.05), m_sin(W.frame * 700 + i * 300));
        int32_t x = CV_FXMUL(rad, m_cos(ang + W.frame * 40));
        int32_t z = CV_FXMUL(rad, m_sin(ang + W.frame * 40));
        GXP_COLOR = (i & 3) ? RGB15(4, 3, 2) : RGB15(10, 2, 2);
        vtx(x, y, z);
        vtx(x + F(0.04), y + F(0.02), z);
        vtx(x, y + F(0.05), z + F(0.03));
    }
    GXP_END = 0;
    glPopMatrix(1);
}

// ---------------------------------------------------------------------------
// Main-screen sprites

static void draw_sprites(void)
{
    oam_clear(ENG_MAIN);
    if (W.peek)
    {
        // Keyhole view: an OBJ window sprite (scaled 2x) is the only place
        // where the 3D layer shows.
        oam_affine(ENG_MAIN, 0, 0, 512, 512);
        oam_add(ENG_MAIN, 128 - 64, 96 - 64, SPR_64x64, CV_SPR_KEYHOLE, 0, 0,
                SPRF_WINDOW | SPRF_AFFINE | SPRF_DOUBLE, 0);
        return;
    }
    if (W.pl.aim)
        oam_add(ENG_MAIN, 120, 88, SPR_16x16, CV_SPR_CROSS, 0, 0, 0, 0);
    if (W.chapter == 3 && W.film_seen && W.message_timer)
    {
        // The torn photograph from the film: a 16bpp bitmap OBJ, mosaic
        // while fear is high.
        oam_add(ENG_MAIN, 184, 8, SPR_64x64, CV_PHOTO_TILE, 15, 0,
                SPRF_BITMAP | (W.fear > 40 ? SPRF_MOSAIC : 0), 0);
    }
    if (W.chapter == 3 && W.fear > 30)
    {
        // A patient's portrait flickers in at the edge of the view; the
        // same 8bpp tiles through four extended OBJ palettes: as taken,
        // faded, wrong, negative, worse the closer the creature is.
        int pal = W.fear < 45 ? 0 : (W.fear < 60 ? 1 : (W.fear < 80 ? 2 : 3));
        if ((W.frame & 31) < 26 || pal < 2)
            oam_add(ENG_MAIN, 12, 10, SPR_32x32, CV_PORTRAIT_TILE, pal, 0,
                    SPRF_8BPP | (pal == 3 ? SPRF_MOSAIC : 0), 0);
    }
    if ((W.map.flags & MF_WELL) && W.swarm_count > 200)
    {
        // Bugs crawling over the lens: many sprites on the same lines run
        // past the per-scanline OBJ limit, and the far end drops out.
        for (int i = 0; i < 110; i++)
        {
            int x = (i * 37 + W.frame * (1 + (i & 3))) % 256;
            int y = 150 + ((i * 13) % 12) + (m_sin(W.frame * 900 + i * 2000) >> 10);
            oam_add(ENG_MAIN, x - 4, y, SPR_8x8, CV_SPR_BUG, 0, 0, 0, 0);
        }
    }
}

// ---------------------------------------------------------------------------

// Chapter 5 opening: a round brick shaft with iron rungs, seen while
// climbing down it. Rings are drawn one world unit at a time so every
// vertex stays inside the 4.12 range of the current matrix.
void render_shaft(CvVec eye, int yaw, int f)
{
    set_model_lights();
    render_camera(eye, yaw, 2400);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    g3_texture_bind(TEX_WALL);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1) | POLY_FOG);
    const int32_t r = F(0.62);
    for (int k = -1; k < 9; k++)
    {
        glPushMatrix();
        glTranslatef32(eye.x, CV_FXI(k), eye.z);
        GXP_BEGIN = GX_QUADS;
        for (int s = 0; s < 10; s++)
        {
            int a0 = s * 3277, a1 = (s + 1) * 3277;
            int32_t x0 = CV_FXMUL(r, m_cos(a0)), z0 = CV_FXMUL(r, m_sin(a0));
            int32_t x1 = CV_FXMUL(r, m_cos(a1)), z1 = CV_FXMUL(r, m_sin(a1));
            for (int v = 0; v < 4; v++)
            {
                int32_t vy = (v == 1 || v == 2) ? F(1) : 0;
                int32_t dy = m_abs(CV_FXI(k) + vy - eye.y);
                // The flashlight pool slides down the bricks with you.
                int l = 17 - (dy >> 10) * 3 - ((s * 7 + k * 3) & 3);
                l = l < 1 ? 1 : l;
                GXP_COLOR = RGB15(l, l * 3 / 4, l * 3 / 5);
                GXP_TEXCOORD = tc(v == 2 || v == 3 ? 32 : 0, vy ? 0 : 64);
                vtx(v == 2 || v == 3 ? x1 : x0, vy, v == 2 || v == 3 ? z1 : z0);
            }
        }
        GXP_END = 0;
        // Iron rungs on one side.
        g3_texture_bind(TEX_COUNT);
        for (int q = 0; q < 3; q++)
        {
            int32_t ry = q * F(0.33);
            GXP_BEGIN = GX_QUADS;
            GXP_COLOR = RGB15(10, 6, 4);
            vtx(-F(0.2), ry, -F(0.55));
            vtx(F(0.2), ry, -F(0.55));
            vtx(F(0.2), ry + F(0.03), -F(0.45));
            vtx(-F(0.2), ry + F(0.03), -F(0.45));
            GXP_END = 0;
        }
        g3_texture_bind(TEX_WALL);
        glPopMatrix(1);
    }
    (void)f;
}

void render_set_rear(bool rear)
{
    rear_view = rear;
}

void render_world(void)
{
    Player *p = &W.pl;
    CvVec eye = W.peek ? W.peek_eye : p->pos;
    int yaw = W.peek ? W.peek_yaw : p->yaw;
    if (rear_view)
        yaw = (yaw + 16384) & 0x7FFF;
    // Hit: the view jolts for a moment.
    if (p->hurt > 30)
        yaw += (p->hurt & 2) ? 300 : -300;
    if ((W.frame & 15) == 0)
        W.lamp_flicker = (W.frame * 2654435761u >> 28) < 3;

    set_model_lights();
    render_camera(eye, yaw, p->pitch);
    compute_lights();

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    draw_grid();
    draw_objects();
    draw_creature(&W.mo, false);
    draw_creature(&W.visitor, true);
    draw_swarm();
    draw_translucent();
    draw_sprites();
}
