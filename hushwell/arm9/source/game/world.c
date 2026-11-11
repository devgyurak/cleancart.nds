// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// The chapter loop: map, player, the creature in the dark, items, doors,
// radios, flares, pause, and the per-frame submission.

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cart.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/nitro.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "core/timer.h"
#include "game/game.h"
#include "game/world.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/oam.h"

CvWorld W;

#define F CV_FX

#define EYE_Y      F(0.52)
#define HUSH_Y     F(0.30)
#define PL_RADIUS  F(0.24)
#define WALK       F(0.034)
#define RUN        F(0.060)
#define HUSHWALK   F(0.016)
#define TURN       260

static u8 blockers[MAP_MAX * MAP_MAX];
static u8 dist_field[MAP_MAX * MAP_MAX];

// ---------------------------------------------------------------------------
// Map

static bool map_load(int chapter, CvMap *m)
{
    char name[16];
    snprintf(name, sizeof(name), "ch%d.map", chapter);
    memset(m, 0, sizeof(*m));

    u32 size = 0;
    void *blob = NULL;
    sys_wram_to_arm9();
    if (cart_ready())
    {
        // Raw cart-DMA path, decompressed through shared WRAM.
        u32 csize;
        void *comp = cart_load(name, &csize);
        if (comp)
        {
            size = lz_size(comp);
            blob = memalign(32, (size + 31) & ~31);
            if (blob && size <= 32768)
            {
                swiDecompressLZSSWram(comp, (void *)0x03000000);
                swiFastCopy((void *)0x03000000, blob, ((size + 31) & ~31) >> 2);
                DC_FlushRange(blob, (size + 31) & ~31);
            }
            else if (blob)
            {
                swiDecompressLZSSWram(comp, blob);
            }
            free(comp);
        }
    }
    if (!blob)
        blob = nitro_load_lz(name, &size);
    sys_wram_to_arm7();
    if (!blob)
        return false;

    const u8 *b = blob;
    const u16 *h = blob;
    if (*(const u32 *)b != 0x504D5748u)
    {
        free(blob);
        return false;
    }
    m->blob = blob;
    m->chapter = h[3];
    m->w = h[4];
    m->h = h[5];
    m->nobj = h[6] > MAX_OBJS ? MAX_OBJS : h[6];
    m->flags = h[7];
    m->song = h[8];
    m->fog = h[9];
    m->clear = h[10];
    m->ambient = b[22];
    m->facing = b[23];
    if (m->w > MAP_MAX || m->h > MAP_MAX)
    {
        free(blob);
        m->blob = NULL;
        return false;
    }
    memcpy(m->cells, b + 24, m->w * m->h);
    m->obj = (const CvObj *)(b + 24 + m->w * m->h);
    return true;
}

int world_cell(int x, int y)
{
    if (x < 0 || y < 0 || x >= W.map.w || y >= W.map.h)
        return CELL_WALL;
    return W.map.cells[y * W.map.w + x];
}

bool world_cell_solid(int x, int y)
{
    int c = world_cell(x, y);
    if (c != CELL_FLOOR && c != CELL_WATER && c != CELL_GROUND)
        return true;
    return blockers[y * W.map.w + x] != 0;
}

bool world_obj_taken(int i)
{
    return i < 32 ? (cv_game.taken_lo >> i) & 1 : (cv_game.taken_hi >> (i - 32)) & 1;
}

static void obj_take(int i)
{
    if (i < 32)
        cv_game.taken_lo |= 1u << i;
    else
        cv_game.taken_hi |= 1u << (i - 32);
}

void world_message(const char *msg, int frames)
{
    W.message = msg;
    W.message_timer = frames;
}

static int cell_of(int32_t v)
{
    return v >> 12;
}

static int32_t cell_center(int c)
{
    return CV_FXI(c) + F(0.5);
}

static int angle_of_facing(int f)
{
    return (f & 3) * 8192;
}

static CvVec forward_of(int yaw)
{
    return m_vec(m_sin(yaw), 0, -m_cos(yaw));
}

// ---------------------------------------------------------------------------
// Effects

void particle(CvVec pos, CvVec vel, u16 color, int life, int32_t size, int kind)
{
    Particle *p = &W.pa[W.pa_next];
    W.pa_next = (W.pa_next + 1) % MAX_PARTICLES;
    p->pos = pos;
    p->vel = vel;
    p->color = color;
    p->life = p->max = (u8)life;
    p->size = size;
    p->kind = (u8)kind;
}

static void particles_update(void)
{
    // Dust drifting through the flashlight beam, steam over warm water.
    if (W.pl.light && (W.frame & 7) == 0 && !(W.map.flags & MF_OUTDOOR))
    {
        CvVec f = forward_of(W.pl.yaw);
        CvVec at = m_add(W.pl.pos, m_vec(f.x + rng_range(-F(0.4), F(0.4)),
                                         rng_range(-F(0.3), F(0.3)),
                                         f.z + rng_range(-F(0.4), F(0.4))));
        particle(at, m_vec(rng_range(-80, 80), -40, rng_range(-80, 80)), RGB15(28, 26, 20), 90,
                 F(0.012), PK_MOTE);
    }
    if ((W.map.flags & MF_WATER) && (W.frame & 3) == 0)
    {
        int x = cell_of(W.pl.pos.x) + rng_range(-4, 5);
        int y = cell_of(W.pl.pos.z) + rng_range(-4, 5);
        if (world_cell(x, y) == CELL_WATER)
            particle(m_vec(CV_FXI(x) + rng_range(0, F(1)), F(-0.1), CV_FXI(y) + rng_range(0, F(1))),
                     m_vec(rng_range(-40, 40), F(0.004), rng_range(-40, 40)), RGB15(24, 26, 26),
                     120, F(0.3), PK_STEAM);
    }
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &W.pa[i];
        if (!p->life)
            continue;
        p->life--;
        p->pos = m_add(p->pos, p->vel);
        if (p->kind == PK_SPARK)
            p->vel.y -= 60;
        if (p->kind == PK_STEAM)
            p->size += 60;
    }
}

// ---------------------------------------------------------------------------
// Line of sight and paths on the grid

static bool line_of_sight(CvVec a, CvVec b)
{
    CvVec d = m_sub(b, a);
    int32_t len = m_len(m_vec(d.x, 0, d.z));
    int steps = (len >> 10) + 1;   // every quarter cell
    for (int i = 1; i < steps; i++)
    {
        int32_t x = a.x + m_div((int32_t)((int64_t)d.x * i), steps);
        int32_t z = a.z + m_div((int32_t)((int64_t)d.z * i), steps);
        int c = world_cell(cell_of(x), cell_of(z));
        if (c == CELL_WALL || c == CELL_DOOR || c == CELL_SHELF)
            return false;
    }
    return true;
}

// Breadth-first distances from (tx, ty) over walkable cells.
static void build_field(int tx, int ty)
{
    static u16 queue[MAP_MAX * MAP_MAX];
    int w = W.map.w, h = W.map.h;
    memset(dist_field, 0xFF, sizeof(dist_field));
    if (tx < 0 || ty < 0 || tx >= w || ty >= h)
        return;
    int head = 0, tail = 0;
    dist_field[ty * w + tx] = 0;
    queue[tail++] = ty * w + tx;
    while (head < tail)
    {
        int c = queue[head++];
        int x = c % w, y = c / w;
        static const s8 dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; k++)
        {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || world_cell_solid(nx, ny))
                continue;
            int n = ny * w + nx;
            if (dist_field[n] != 0xFF)
                continue;
            dist_field[n] = dist_field[c] + 1 > 254 ? 254 : dist_field[c] + 1;
            queue[tail++] = n;
        }
    }
}

static void monster_next_step(Monster *m)
{
    int x = cell_of(m->pos.x), y = cell_of(m->pos.z);
    build_field(m->tx, m->ty);
    int w = W.map.w;
    int best = dist_field[y * w + x];
    m->nx = x;
    m->ny = y;
    static const s8 dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    for (int k = 0; k < 4; k++)
    {
        int nx = x + dx[k], ny = y + dy[k];
        if (nx < 0 || ny < 0 || nx >= w || ny >= W.map.h)
            continue;
        int d = dist_field[ny * w + nx];
        if (d < best)
        {
            best = d;
            m->nx = nx;
            m->ny = ny;
        }
    }
}

static void random_cell(int *ox, int *oy, int near_x, int near_y, int min_dist)
{
    for (int tries = 0; tries < 40; tries++)
    {
        int x = rng_range(0, W.map.w), y = rng_range(0, W.map.h);
        if (world_cell_solid(x, y))
            continue;
        if (m_abs(x - near_x) + m_abs(y - near_y) < min_dist)
            continue;
        *ox = x;
        *oy = y;
        return;
    }
    *ox = near_x;
    *oy = near_y;
}

// ---------------------------------------------------------------------------
// The creature in the dark

static void monster_set(Monster *m, int state, int timer)
{
    m->state = state;
    m->timer = timer;
    m->repath = 0;
}

static void hear(int loudness)
{
    Player *p = &W.pl;
    p->noise = loudness;
    Monster *m = &W.mo;
    if (!m->active || loudness <= 0)
        return;
    int32_t d = m_dist(m->pos, m_vec(p->pos.x, 0, p->pos.z));
    if (d < loudness * F(1.25) && (m->state == MS_WANDER || m->state == MS_HUNT))
    {
        m->tx = cell_of(p->pos.x);
        m->ty = cell_of(p->pos.z);
        if (m->state != MS_HUNT)
            snd_sfx(CV_SFX_GROWL, 70, 64);
        monster_set(m, MS_HUNT, 0);
        m->lost = 0;
    }
}

static int pan_of(CvVec at)
{
    // Stereo position from the camera's right vector.
    CvVec d = m_sub(at, W.pl.pos);
    CvVec r = m_vec(m_cos(W.pl.yaw), 0, m_sin(W.pl.yaw));
    int32_t side = CV_FXMUL(d.x, r.x) + CV_FXMUL(d.z, r.z);
    return m_clamp(64 + (side >> 6), 8, 120);
}

static void sound_at(int id, CvVec at, int base_vol)
{
    int32_t d = m_dist(at, W.pl.pos);
    int vol = base_vol - (d >> 8) * 4;
    if (vol > 8)
        snd_sfx(id, vol, pan_of(at));
}

static void player_hurt(void)
{
    Player *p = &W.pl;
    if (p->dead || cv_game.health <= 0)
        return;
    cv_game.health--;
    p->hurt = 40;
    snd_sfx(CV_SFX_HIT, 120, 64);
    snd_sfx(CV_SFX_SCREAM, 110, pan_of(W.mo.pos));
    if (cv_game.health <= 0)
        p->dead = 1;
}

static void monster_update(Monster *m)
{
    if (!m->active || m->state == MS_GONE)
        return;
    Player *p = &W.pl;
    CvVec feet = m_vec(p->pos.x, 0, p->pos.z);
    int32_t d = m_dist(m->pos, feet);
    int px = cell_of(p->pos.x), py = cell_of(p->pos.z);
    m->anim++;
    if (m->attack_cool)
        m->attack_cool--;

    if (m->state == MS_PASS)
    {
        // Prologue: it crosses the road once, then is gone.
        m->pos.x -= F(0.045);
        m->yaw = 24576;
        if (m->pos.x < F(1.5))
            m->state = MS_GONE;
        return;
    }

    bool sees = false;
    if (m->state != MS_STUN && d < F(7))
    {
        bool lit = p->light && cv_game.battery > 0;
        int32_t range = lit ? F(6.5) : (p->hush ? F(1.2) : F(2.6));
        if (d < range && line_of_sight(m->pos, feet))
            sees = true;
    }

    switch (m->state)
    {
        case MS_WANDER:
            m->speed = F(0.020);
            if (sees)
            {
                monster_set(m, MS_CHASE, 0);
                snd_sfx(CV_SFX_GROWL, 120, pan_of(m->pos));
            }
            else if (cell_of(m->pos.x) == m->tx && cell_of(m->pos.z) == m->ty)
            {
                random_cell(&m->tx, &m->ty, m->tx, m->ty, 4);
            }
            break;
        case MS_HUNT:
            m->speed = F(0.030);
            if (sees)
                monster_set(m, MS_CHASE, 0);
            else if ((cell_of(m->pos.x) == m->tx && cell_of(m->pos.z) == m->ty) || ++m->lost > 600)
            {
                monster_set(m, MS_WANDER, 0);
                random_cell(&m->tx, &m->ty, m->tx, m->ty, 3);
            }
            break;
        case MS_CHASE:
            // In the chase it is a little slower than you running: turns cost
            // you ground, a clean line keeps you ahead.
            m->speed = W.chase ? F(0.044) : F(0.040);
            m->tx = px;
            m->ty = py;
            if (sees)
                m->lost = 0;
            else if (++m->lost > 200 && !W.chase)
            {
                monster_set(m, MS_HUNT, 0);
                m->lost = 0;
            }
            break;
        case MS_STUN:
            m->speed = 0;
            if (--m->timer <= 0)
            {
                monster_set(m, W.chase ? MS_CHASE : MS_RETREAT, 240);
                random_cell(&m->tx, &m->ty, px, py, 8);
            }
            break;
        case MS_RETREAT:
            m->speed = F(0.040);
            if (--m->timer <= 0)
            {
                monster_set(m, W.chase ? MS_CHASE : MS_WANDER, 0);
                random_cell(&m->tx, &m->ty, m->tx, m->ty, 4);
            }
            break;
    }

    // Move toward the centre of the next cell on the path.
    if (m->speed)
    {
        if (--m->repath <= 0)
        {
            monster_next_step(m);
            m->repath = m->state == MS_CHASE ? 8 : 24;
        }
        CvVec want = m_vec(cell_center(m->nx), 0, cell_center(m->ny));
        bool direct = m->nx == cell_of(m->pos.x) && m->ny == cell_of(m->pos.z) && m->state == MS_CHASE;
        if (direct)
            want = feet;   // same cell: go straight for the player
        CvVec dir = m_sub(want, m->pos);
        int32_t len = m_len(dir);
        // Never walk into the camera: anything that close fills the screen
        // after near-plane clipping. It strikes from arm's length.
        if (direct && len < F(0.45))
            len = 0;
        if (len > F(0.02))
        {
            CvVec step = m_norm(dir, len < m->speed ? len : m->speed);
            m->pos = m_add(m->pos, step);
            int target_yaw = m_atan2(dir.x, -dir.z);
            int diff = (s16)((target_yaw - m->yaw) * 2) / 2;
            m->yaw += diff / 6;
        }
        else
        {
            m->repath = 0;
        }
    }

    // Footfalls and breathing, placed in the stereo field.
    if (m->speed && (m->anim % (m->state == MS_CHASE ? 18 : 30)) == 0)
        sound_at(W.map.flags & MF_WATER ? CV_SFX_SPLASH : CV_SFX_STEP, m->pos, 110);
    if (--m->growl <= 0)
    {
        sound_at(CV_SFX_GROWL, m->pos, 120);
        m->growl = 300 + rng_range(0, 400);
    }

    // Only a creature that is after you strikes. Wandering into you makes
    // it notice you; backing off (stunned or retreating) it passes by.
    if (d < F(0.9) && m->state == MS_WANDER && !p->dead)
    {
        monster_set(m, MS_CHASE, 0);
        snd_sfx(CV_SFX_GROWL, 127, pan_of(m->pos));
    }
    if (d < F(0.55) && !m->attack_cool && (m->state == MS_HUNT || m->state == MS_CHASE) && !p->dead)
    {
        player_hurt();
        m->attack_cool = 180;
        // It staggers back after a strike; in the chase only briefly.
        monster_set(m, MS_RETREAT, W.chase ? 70 : 200);
        random_cell(&m->tx, &m->ty, px, py, W.chase ? 4 : 8);
    }
}

static void visitor_update(void)
{
    // The night visitor never attacks: it follows at a distance and the
    // flashlight dies while it is close.
    Monster *v = &W.visitor;
    if (!v->active)
        return;
    v->anim++;
    CvVec feet = m_vec(W.pl.pos.x, 0, W.pl.pos.z);
    int32_t d = m_dist(v->pos, feet);
    if (d > F(2.2))
    {
        CvVec step = m_norm(m_sub(feet, v->pos), F(0.012));
        CvVec np = m_add(v->pos, step);
        if (!world_cell_solid(cell_of(np.x), cell_of(np.z)))
            v->pos = np;
    }
    v->yaw = m_atan2(feet.x - v->pos.x, -(feet.z - v->pos.z));
    if (d < F(3) && (W.frame & 31) == 0 && cv_game.battery > 0)
        cv_game.battery--;
}

// ---------------------------------------------------------------------------
// Flares

static void fire_flare(void)
{
    if (cv_game.flares <= 0)
    {
        snd_sfx(CV_SFX_LOCKED, 80, 64);
        world_message("NO FLARES LEFT", 60);
        return;
    }
    for (int i = 0; i < MAX_FLARES; i++)
    {
        Flare *f = &W.fl[i];
        if (f->alive)
            continue;
        cv_game.flares--;
        CvVec fw = forward_of(W.pl.yaw);
        f->alive = true;
        f->landed = false;
        f->pos = m_add(W.pl.pos, m_scale(fw, F(0.3)));
        f->vel = m_vec(CV_FXMUL(fw.x, F(0.22)), F(0.02), CV_FXMUL(fw.z, F(0.22)));
        f->life = 480;
        snd_sfx(CV_SFX_FLARE, 127, 64);
        hear(9);

        // Hit test: the hardware box test of the creature's bounding box in a
        // narrow pick projection around the crosshair.
        Monster *m = &W.mo;
        if (m->active && m->state != MS_GONE && m->state != MS_PASS
            && m_dist(m->pos, W.pl.pos) < F(9) && line_of_sight(m->pos, W.pl.pos))
        {
            render_camera(W.pl.pos, W.pl.yaw, W.pl.pitch);
            g3_reload(128, 96, 24);
            bool hit = g3_box_test(m_add(m->pos, m_vec(-F(0.3), 0, -F(0.3))),
                                   m_vec(F(0.6), F(1.3), F(0.6)));
            g3_reload(0, 0, 0);
            if (hit)
            {
                monster_set(m, MS_STUN, W.chase ? 150 : 320);
                snd_sfx(CV_SFX_SCREAM, 127, pan_of(m->pos));
                f->pos = m_add(m->pos, m_vec(0, F(0.8), 0));
                f->vel = m_vec(0, 0, 0);
                f->landed = true;
            }
        }
        return;
    }
}

static void flares_update(void)
{
    for (int i = 0; i < MAX_FLARES; i++)
    {
        Flare *f = &W.fl[i];
        if (!f->alive)
            continue;
        if (!f->landed)
        {
            CvVec np = m_add(f->pos, f->vel);
            f->vel.y -= F(0.006);
            if (world_cell_solid(cell_of(np.x), cell_of(np.z)))
            {
                f->vel = m_vec(-f->vel.x / 4, f->vel.y, -f->vel.z / 4);
                np = f->pos;
            }
            if (np.y < F(0.05))
            {
                np.y = F(0.05);
                f->landed = true;
            }
            f->pos = np;
        }
        if ((W.frame & 1) == 0)
            particle(f->pos, m_vec(rng_range(-F(0.01), F(0.01)), F(0.02), rng_range(-F(0.01), F(0.01))),
                     (W.frame & 2) ? RGB15(31, 20, 6) : RGB15(31, 8, 4), 18, F(0.05), PK_SPARK);
        if (--f->life <= 0)
            f->alive = false;
    }
}

// ---------------------------------------------------------------------------
// Player

static bool blocked_at(int32_t x, int32_t z)
{
    int32_t r = PL_RADIUS;
    return world_cell_solid(cell_of(x - r), cell_of(z - r))
        || world_cell_solid(cell_of(x + r), cell_of(z - r))
        || world_cell_solid(cell_of(x - r), cell_of(z + r))
        || world_cell_solid(cell_of(x + r), cell_of(z + r));
}

static void try_move(int32_t dx, int32_t dz)
{
    Player *p = &W.pl;
    if (!blocked_at(p->pos.x + dx, p->pos.z))
        p->pos.x += dx;
    if (!blocked_at(p->pos.x, p->pos.z + dz))
        p->pos.z += dz;
}

static void reveal(void)
{
    // The hand-drawn map fills in around the player and down the beam.
    int px = cell_of(W.pl.pos.x), py = cell_of(W.pl.pos.z);
    for (int y = py - 1; y <= py + 1; y++)
        for (int x = px - 1; x <= px + 1; x++)
            if (x >= 0 && y >= 0 && x < W.map.w && y < W.map.h)
                W.explored[y * W.map.w + x] = 1;
    if (!W.pl.light || cv_game.battery <= 0)
        return;
    CvVec f = forward_of(W.pl.yaw);
    for (int k = 1; k <= 8; k++)
    {
        int32_t x = W.pl.pos.x + f.x * k / 2, z = W.pl.pos.z + f.z * k / 2;
        int cx = cell_of(x), cy = cell_of(z);
        if (cx < 0 || cy < 0 || cx >= W.map.w || cy >= W.map.h)
            break;
        W.explored[cy * W.map.w + cx] = 1;
        for (int s = -1; s <= 1; s += 2)
        {
            int sx = cell_of(x + s * f.z / 2), sy = cell_of(z - s * f.x / 2);
            if (sx >= 0 && sy >= 0 && sx < W.map.w && sy < W.map.h)
                W.explored[sy * W.map.w + sx] = 1;
        }
        if (world_cell_solid(cx, cy))
            break;
    }
}

static int obj_at(int x, int y, int type)
{
    for (int i = 0; i < W.map.nobj; i++)
    {
        const CvObj *o = &W.map.obj[i];
        if (o->x == x && o->y == y && o->type == type && !world_obj_taken(i))
            return i;
    }
    return -1;
}

static void open_door(int i)
{
    const CvObj *o = &W.map.obj[i];
    obj_take(i);
    W.map.cells[o->y * W.map.w + o->x] = CELL_FLOOR;
    snd_sfx(CV_SFX_DOOR, 110, 64);
    hear(5);
}

void world_open_door(int obj)
{
    open_door(obj);
}

static bool clear_pending, dead_pending;
static int last_x, last_y;      // touch look drag
static bool was_touching;

static void start_chase(void)
{
    W.chase = true;
    Monster *m = &W.mo;
    // It comes from the far side of the map.
    int px = cell_of(W.pl.pos.x), py = cell_of(W.pl.pos.z);
    int sx = px, sy = py;
    random_cell(&sx, &sy, px, py, 10);
    m->active = true;
    m->pos = m_vec(cell_center(sx), 0, cell_center(sy));
    monster_set(m, MS_CHASE, 0);
    m->attack_cool = 120;
    snd_bgm(CV_SONG_CHASE);
    snd_sfx(CV_SFX_SCREAM, 127, pan_of(m->pos));
    world_message("RUN.", 120);
    W.lightning = 10;
}

static void pickups(void)
{
    int px = cell_of(W.pl.pos.x), py = cell_of(W.pl.pos.z);
    for (int i = 0; i < W.map.nobj; i++)
    {
        const CvObj *o = &W.map.obj[i];
        if (o->x != px || o->y != py || world_obj_taken(i))
            continue;
        switch (o->type)
        {
            case O_KEY:
                obj_take(i);
                cv_game.keys |= 1 << o->param;
                snd_sfx(CV_SFX_PICKUP, 110, 64);
                hear(2);
                if (o->param == KEY_CHASE)
                    start_chase();
                else
                    world_message("FOUND A KEY", 90);
                break;
            case O_TAPE:
                obj_take(i);
                cv_game.tapes |= 1 << o->param;
                snd_sfx(CV_SFX_PICKUP, 110, 64);
                world_message("FOUND A TAPE  (X: RECORDER)", 120);
                break;
            case O_FLARES:
                obj_take(i);
                cv_game.flares += 2;
                snd_sfx(CV_SFX_PICKUP, 110, 64);
                world_message("FLARES +2", 90);
                break;
            case O_BATTERY:
                obj_take(i);
                cv_game.battery = 100;
                snd_sfx(CV_SFX_PICKUP, 110, 64);
                world_message("FRESH BATTERY", 90);
                break;
            case O_EXIT:
                if (W.chase || W.chapter == 0 || (cv_game.keys & (1 << KEY_CHASE)))
                    clear_pending = true;
                break;
            case O_WELL:
                W.at_well++;
                break;
        }
    }
}

static void interact(void)
{
    Player *p = &W.pl;
    CvVec f = forward_of(p->yaw);
    int tx = cell_of(p->pos.x + CV_FXMUL(f.x, F(0.75)));
    int ty = cell_of(p->pos.z + CV_FXMUL(f.z, F(0.75)));
    int px = cell_of(p->pos.x), py = cell_of(p->pos.z);

    int i = obj_at(tx, ty, O_DOOR);
    if (i >= 0)
    {
        if (cv_game.keys & (1 << W.map.obj[i].param))
        {
            open_door(i);
            world_message("UNLOCKED", 60);
        }
        else
        {
            // Locked: look through the keyhole for a moment.
            snd_sfx(CV_SFX_LOCKED, 100, 64);
            hear(3);
            W.peek = 150;
            W.peek_yaw = p->yaw;
            W.peek_eye = m_add(m_vec(cell_center(tx), F(0.45), cell_center(ty)), m_scale(f, F(0.8)));
            world_message("LOCKED. YOU PEEK THROUGH THE KEYHOLE.", 150);
        }
        return;
    }
    i = obj_at(tx, ty, O_CODEDOOR);
    if (i >= 0)
    {
        W.code_mode = true;
        W.code_door = i;
        W.recorder = false;
        world_message("A DIAL LOCK. SET IT ON THE TOUCH SCREEN.", 150);
        return;
    }
    for (int k = 0; k < 2; k++)
    {
        int cx = k ? px : tx, cy = k ? py : ty;
        if ((i = obj_at(cx, cy, O_RADIO)) >= 0)
        {
            game_checkpoint(i);
            snd_sfx(CV_SFX_STATIC, 110, 64);
            world_message(cv_game.attract || cv_game.playback ? "THE RADIO HISSES."
                                                               : "PROGRESS SAVED. THE RADIO HISSES.", 120);
            return;
        }
        if ((i = obj_at(cx, cy, O_PROJECTOR)) >= 0)
        {
            chfx_film();
            W.film_seen = true;
            world_message("THE FILM ENDS. NUMBERS SCRATCHED: 4 . 1 . 7", 240);
            return;
        }
    }
}

static void player_update(void)
{
    Player *p = &W.pl;
    if (p->dead)
    {
        p->dead++;
        p->pos.y -= p->pos.y > F(0.15) ? F(0.01) : 0;
        p->pitch = m_clamp(p->pitch + 60, -4000, 3000);
        return;
    }
    if (W.peek)
    {
        W.peek--;
        return;
    }

    bool look_touch = W.look_touch && !W.code_mode && !W.recorder;
    p->hush = (cv_in.held & KEY_L) != 0;
    p->run = (cv_in.held & KEY_B) && !p->hush && !(cv_in.held & KEY_R);
    p->aim = (cv_in.held & KEY_R) != 0;

    if (cv_in.held & KEY_LEFT)
        p->yaw -= TURN;
    if (cv_in.held & KEY_RIGHT)
        p->yaw += TURN;
    if (look_touch && cv_in.touching)
    {
        if (was_touching)
        {
            p->yaw += (cv_in.tx - last_x) * 64;
            p->pitch = m_clamp(p->pitch + (cv_in.ty - last_y) * 48, -2800, 2800);
        }
        last_x = cv_in.tx;
        last_y = cv_in.ty;
        was_touching = true;
    }
    else
    {
        was_touching = false;
    }
    p->yaw &= 0x7FFF;
    if (!cv_in.touching || !look_touch)
        p->pitch -= p->pitch / 8;

    int32_t speed = p->hush ? HUSHWALK : (p->run ? RUN : WALK);
    bool in_water = world_cell(cell_of(p->pos.x), cell_of(p->pos.z)) == CELL_WATER;
    if (in_water)
        speed = speed * 3 / 4;
    int dir = (cv_in.held & KEY_UP) ? 1 : ((cv_in.held & KEY_DOWN) ? -1 : 0);
    if (dir)
    {
        CvVec f = forward_of(p->yaw);
        try_move(CV_FXMUL(f.x, speed) * dir, CV_FXMUL(f.z, speed) * dir);
        p->step += speed;
        if (p->step > F(0.62))
        {
            p->step = 0;
            int loud = p->hush ? 0 : (p->run ? 7 : 3);
            if (in_water && loud)
                loud += 2;
            if (loud)
                snd_sfx(in_water ? CV_SFX_SPLASH : CV_SFX_STEP, p->run ? 90 : 50, 64);
            hear(loud);
        }
    }
    int32_t eye = p->hush ? HUSH_Y : EYE_Y;
    p->bob = dir ? CV_FXMUL(F(0.02), m_sin(W.frame * (p->run ? 1500 : 900))) : 0;
    p->pos.y += (eye + p->bob - p->pos.y) / 4;

    if (cv_in.pressed & KEY_Y)
    {
        p->light = !p->light;
        snd_sfx(CV_SFX_CURSOR, 90, 64);
    }
    if (cv_in.pressed & KEY_X)
    {
        W.recorder = !W.recorder;
        W.code_mode = false;
    }
    if (cv_in.pressed & KEY_A)
    {
        if (p->aim)
            fire_flare();
        else
            interact();
    }

    // Breathing into the microphone is a sound like any other (the level is
    // part of the input log, so replays hear it too).
    if (W.mic_on && cv_in.mic >= 3 && (W.frame & 15) == 0)
    {
        hear(cv_in.mic + 3);
        if (!W.mic_warned && W.mo.active && W.mo.state != MS_GONE)
        {
            world_message("IT HEARD YOUR BREATH.", 90);
            W.mic_warned = true;
        }
    }

    // Battery drains while the light is on; low batteries flicker. The
    // flicker interval comes straight from the timer 0 counter (display only).
    if (p->light && (W.frame % 90) == 0 && cv_game.battery > 0)
        cv_game.battery--;
    p->flicker = cv_game.battery < 20 && p->light && (CV_TM_DATA(0) & 0x1F00) == 0 ? 1 : 0;
    if (p->hurt)
        p->hurt--;
    reveal();
    pickups();
}

// ---------------------------------------------------------------------------
// Pause

enum { PAUSE_RESUME, PAUSE_STEP, PAUSE_RETIRE, PAUSE_QA_CLEAR };

static void wake_noop(void) {}

static int pause_menu(void)
{
    timer_playtime_pause(true);
    replay_suspend(true);   // paused frames are not part of the run
    chfx_pause();
    snd_master(32);
    int idle = 0;
    int result = PAUSE_RESUME;
    for (;;)
    {
        world_message(W.debug_lights ? "PAUSED (LIGHTS ON)  START:BACK SELECT:QUIT R:STEP"
                                     : "PAUSED  START:BACK SELECT:QUIT R:STEP X:LIGHTS Y:MASK B:INFO", 1);
        chfx_text();
        if (!chfx_dual())
            hud_frame();   // in the dual-screen chase engine B shows the capture
        oam_commit();
        sys_vsync();
        sys_handle_lid();
        input_update();
        if (cv_in.pressed)
            idle = 0;
        if (cv_in.pressed & KEY_START)
            break;
        if (cv_in.pressed & KEY_SELECT)
        {
            result = PAUSE_RETIRE;
            break;
        }
        // QA shortcut: hold L+R and press A to clear the chapter at once.
        if ((cv_in.held & (KEY_L | KEY_R)) == (KEY_L | KEY_R) && (cv_in.pressed & KEY_A))
        {
            result = PAUSE_QA_CLEAR;
            break;
        }
        if ((cv_in.pressed & KEY_R) && !(cv_in.held & KEY_L))
        {
            result = PAUSE_STEP;
            break;
        }
        // Debug views for checking dark scenes.
        if (cv_in.pressed & KEY_X)
            W.debug_lights = !W.debug_lights;
        if (cv_in.pressed & KEY_Y)
            W.debug_mask_off = !W.debug_mask_off;
        if (cv_in.pressed & KEY_B)
            W.console = !W.console;
        if (++idle > 600)
        {
            // Long pause: backlights off, CPU halted until a key IRQ.
            powerOff(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
            irq_set_keys(0x03FF, wake_noop);
            swiIntrWait(1, IRQ_KEYS);
            irq_set_keys(0, NULL);
            powerOn(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
            idle = 0;
        }
    }
    W.message_timer = 0;
    snd_master(cv_save.volume);
    timer_playtime_pause(false);
    if (result != PAUSE_STEP)
        replay_suspend(false);
    return result;
}

// ---------------------------------------------------------------------------

static u8 run_options;

static void world_init(int chapter, const CvMap *map)
{
    memset(&W, 0, sizeof(W));
    W.look_touch = (run_options & RUN_OPT_LOOK_TOUCH) != 0;
    W.night_mode = (run_options >> RUN_OPT_NIGHT_SHIFT) & 3;
    W.mic_on = (run_options & RUN_OPT_MIC) != 0;
    W.map = *map;
    W.chapter = chapter;
    cv_game.chapter_frames = 0;
    cv_game.cleared = false;
    clear_pending = dead_pending = false;
    was_touching = false;
    last_x = last_y = 0;

    memset(blockers, 0, sizeof(blockers));
    Player *p = &W.pl;
    p->yaw = angle_of_facing(W.map.facing);
    p->light = !(W.map.flags & MF_OUTDOOR) || chapter == 4;
    int radio = cv_game.radio;
    for (int i = 0; i < W.map.nobj; i++)
    {
        const CvObj *o = &W.map.obj[i];
        switch (o->type)
        {
            case O_START:
                if (radio < 0)
                    p->pos = m_vec(cell_center(o->x), EYE_Y, cell_center(o->y));
                break;
            case O_RADIO:
                if (i == radio)
                    p->pos = m_vec(cell_center(o->x), EYE_Y, cell_center(o->y));
                break;
            case O_DOOR:
            case O_CODEDOOR:
                if (world_obj_taken(i))
                    W.map.cells[o->y * W.map.w + o->x] = CELL_FLOOR;
                break;
            case O_CAR: case O_PEW: case O_BED: case O_TREE: case O_ALTAR: case O_PROJECTOR:
                blockers[o->y * W.map.w + o->x] = 1;
                break;
            case O_MONSTER:
            {
                Monster *m = &W.mo;
                m->active = chapter > 0 || (W.map.flags & MF_PHANTOM);
                m->pos = m_vec(cell_center(o->x), 0, cell_center(o->y));
                m->yaw = 0;
                m->tx = o->x;
                m->ty = o->y;
                m->growl = 200;
                monster_set(m, chapter == 0 ? MS_GONE : MS_WANDER, 0);
                break;
            }
            case O_VISITOR:
                W.visitor.pos = m_vec(cell_center(o->x), 0, cell_center(o->y));
                break;
        }
    }
    // A radio save sits beside the radio, not inside its box.
    if (radio >= 0)
    {
        CvVec f = forward_of(p->yaw);
        if (!blocked_at(p->pos.x + CV_FXMUL(f.x, F(0.3)), p->pos.z + CV_FXMUL(f.z, F(0.3))))
            p->pos = m_add(p->pos, m_scale(f, F(0.3)));
    }
    int hour = input_rtc_hour();
    W.night = (W.map.flags & MF_NIGHT)
           && (W.night_mode == 1 || (W.night_mode == 0 && hour >= 0 && hour < 5));
    W.visitor.active = W.night;
    W.visitor.state = MS_WANDER;   // MS_GONE (0) would hide it from the renderer
    W.code_digits[0] = W.code_digits[1] = W.code_digits[2] = 0;
}

bool world_load_demo(int chapter)
{
    // Feature demos reuse the real map loader and renderer.
    static CvMap map;
    free(map.blob);
    map.blob = NULL;
    if (!map_load(chapter, &map))
        return false;
    world_init(chapter, &map);
    W.pl.light = true;
    render_init();
    return true;
}

// ---------------------------------------------------------------------------
// Scripted openings. Input is read live and not logged (a replay starts
// after them), and the world is put back exactly as it was.

static void intro_frame_end(void)
{
    oam_commit();
    sys_vsync();
    input_update();
}

static bool intro_skip(int f)
{
    return f > 30 && (cv_in.pressed & (KEY_A | KEY_START));
}

static void intro_car(void)
{
    // Inside the car: rain on the windshield, the wipers, the radio breaking
    // up, and a shape crossing the road in the headlights.
    Player saved = W.pl;
    Monster mo = W.mo;
    W.intro = 1;
    W.pl.pos = m_vec(F(5.5), F(0.44), F(12.62));
    W.pl.yaw = 0;
    W.pl.light = true;   // the headlights
    u16 backdrop = CV_PAL_BG_MAIN[0];
    CV_PAL_BG_MAIN[0] = 0;
    // The windshield is WIN0; outside it only the text layer shows: the
    // dark cabin with the radio's words.
    fx_win_rect(0, 18, 14, 238, 142);
    REG_WININ = 0x3F;
    REG_WINOUT = 0x02;
    REG_DISPCNT |= DISPLAY_WIN0_ON;
    world_message("...SURVEY TEAM TWO... THE WELL IS OPEN... DO NOT COME...", 170);
    snd_sfx(CV_SFX_STATIC, 100, 64);
    snd_sfx(CV_SFX_VOICE, 90, 64);
    for (int f = 0; f < 330; f++)
    {
        W.frame++;
        if (W.message_timer)
            W.message_timer--;
        if (f == 150)
        {
            W.mo.active = true;
            W.mo.state = MS_PASS;
            W.mo.pos = m_vec(F(9.5), 0, F(7.5));
            W.lightning = 14;
            snd_sfx(CV_SFX_THUNDER, 127, 64);
        }
        if (f == 200)
            world_message("YOU STOP THE CAR. THE GATE IS AHEAD.", 130);
        monster_update(&W.mo);
        if (W.lightning)
            W.lightning--;
        chfx_frame();
        render_world();
        g3_flush(W.wbuffer);
        // Two wiper blades sweeping over the windshield (affine OBJs).
        int sweep = 1200 + (((m_sin(f * 420) + 4096) * 6800) >> 13);
        oam_affine(ENG_MAIN, 1, sweep & 0x7FFF, 512, 512);
        oam_affine(ENG_MAIN, 2, (sweep + 2200) & 0x7FFF, 512, 512);
        oam_add(ENG_MAIN, 66 - 64, 140 - 64, SPR_64x64, CV_SPR_WIPER, 0, 0, SPRF_AFFINE | SPRF_DOUBLE, 1);
        oam_add(ENG_MAIN, 156 - 64, 140 - 64, SPR_64x64, CV_SPR_WIPER, 0, 0, SPRF_AFFINE | SPRF_DOUBLE, 2);
        intro_frame_end();
        if (intro_skip(f))
            break;
    }
    REG_DISPCNT &= ~DISPLAY_WIN0_ON;
    REG_WININ = REG_WINOUT = 0;
    CV_PAL_BG_MAIN[0] = backdrop;
    W.pl = saved;
    W.mo = mo;
    W.intro = 0;
    W.frame = 0;
    W.lightning = 0;
    W.message_timer = 0;
}

static void intro_shaft(void)
{
    // Chapter 5 begins with the climb down the vertical shaft under the
    // sanatorium, then the tunnels.
    W.intro = 2;
    world_message("THE SHAFT UNDER THE WELL HOUSE GOES DOWN AND DOWN.", 200);
    oam_clear(ENG_MAIN);
    int start_yaw = W.pl.yaw;
    for (int f = 0; f < 260; f++)
    {
        if (W.message_timer)
            W.message_timer--;
        chfx_text();
        int32_t y = F(7.0) - (int32_t)((int64_t)F(6.45) * f / 260);
        render_shaft(m_vec(W.pl.pos.x, y, W.pl.pos.z), (start_yaw + f * 50) & 0x7FFF, f);
        g3_flush(W.wbuffer);
        if (f > 230)
            fx_bright(FX_MAIN, -(f - 230) / 2);
        intro_frame_end();
        if (intro_skip(f))
            break;
    }
    fx_bright(FX_MAIN, 0);
    W.intro = 0;
    W.message_timer = 0;
}

static void console_lines(u32 render_us)
{
    hud_console_print(0, "F%5lu %4luUS C%3d", (unsigned long)W.frame, (unsigned long)render_us,
                      render_cells_drawn());
    hud_console_print(1, "POLY %4d VTX %4d", g3_poly_count(), g3_vert_count());
    hud_console_print(2, "MON %d %3ld,%3ld T%2d,%2d", W.mo.state, (long)(W.mo.pos.x >> 8),
                      (long)(W.mo.pos.z >> 8), W.mo.tx, W.mo.ty);
    hud_console_print(3, "RNG %08lX BEAT %lu", (unsigned long)rng_state(), (unsigned long)cv_beats);
}

CvScene scene_chapter(void)
{
    int n = cv_game.chapter;
    static CvMap map;

    fx_bright(FX_BOTH, -16);
    if (!map_load(n, &map))
        return SC_TITLE;

    // Input log first: playback replays the RTC hour and the start state.
    bool replaying = cv_game.attract || cv_game.playback;
    u32 seed = cv_save.seed ^ ((u32)n * 7919u);
    if (cv_game.attract)
    {
        u32 size;
        char name[16];
        snprintf(name, sizeof(name), "demo%d.rpl", n);
        static void *demo_blob;
        free(demo_blob);
        demo_blob = nitro_load(name, &size);
        if (!demo_blob || !replay_play_start(demo_blob, size))
        {
            free(map.blob);
            return SC_TITLE;
        }
    }
    else if (cv_game.playback)
    {
        if (!replay_play_start(cv_replay_buf, cv_replay_size))
        {
            free(map.blob);
            return SC_TITLE;
        }
    }
    if (replaying)
    {
        const CvRunStart *st = replay_run_start();
        cv_game.health = st->health;
        cv_game.flares = st->flares;
        cv_game.battery = st->battery;
        cv_game.tapes = st->tapes;
        cv_game.keys = st->keys;
        cv_game.radio = st->radio == 0xFF ? -1 : st->radio;
        cv_game.taken_lo = st->taken_lo;
        cv_game.taken_hi = st->taken_hi;
        run_options = st->options;
        seed = replay_seed();
    }
    else
    {
        CvRunStart st = {
            .chapter = (u8)n, .health = (u8)cv_game.health, .flares = (u8)cv_game.flares,
            .radio = cv_game.radio < 0 ? 0xFF : (u8)cv_game.radio, .keys = cv_game.keys,
            .battery = (u8)cv_game.battery, .tapes = cv_game.tapes,
            .taken_lo = cv_game.taken_lo, .taken_hi = cv_game.taken_hi,
            .options = (u8)((cv_save.look_touch ? RUN_OPT_LOOK_TOUCH : 0)
                          | ((cv_save.night_mode & 3) << RUN_OPT_NIGHT_SHIFT)
                          | (cv_save.mic_on ? RUN_OPT_MIC : 0)),
        };
        run_options = st.options;
        replay_record_start(seed, &st);
    }
    rng_seed(seed);

    world_init(n, &map);
    render_init();
    chfx_setup(n);
    hud_init();
    snd_bgm(map.song);
    timer_playtime_start(cv_save.total_playtime & 0xFFFF);
    timer_beat_start(1);

    if (cv_game.attract)
        world_message("DEMONSTRATION", 1200);
    else if (cv_game.playback)
        world_message("REPLAY", 1200);
    else if (n == 0)
        world_message("Y: FLASHLIGHT  A: EXAMINE  X: MAP/RECORDER", 240);
    else
        world_message("L: HUSH  B: RUN  R+A: FLARE", 200);

    // Openings at a chapter start (not in demos or replays).
    if (!replaying && cv_game.radio < 0 && (n == 0 || n == 5))
    {
        replay_suspend(true);
        fx_bright(FX_BOTH, 0);
        if (n == 0)
            intro_car();
        else
            intro_shaft();
        replay_suspend(false);
    }

    for (int f = 0; f < 16; f++)
    {
        fx_bright(FX_BOTH, -16 + f);
        render_world();
        g3_flush(W.wbuffer);
        hud_frame();
        oam_commit();
        sys_vsync();
    }
    fx_bright(FX_BOTH, 0);

    CvScene next = SC_CLEAR;
    bool step_mode = false;
    bool lid_was = false;
    u32 last_beats = cv_beats;
    int beat_hz = 1;
    for (;;)
    {
        // Closing the lid pauses the game, then sleeps until it opens; when
        // it opens again, the creature has come closer.
        bool lid_pause = cv_lid_closed && !replaying;
        sys_handle_lid();
        input_update();
        cv_game.chapter_frames++;
        // The lid state is part of the input log, so the creature's move on
        // a closed lid replays too.
        if (cv_in.lid && !lid_was && W.mo.active && W.mo.state != MS_GONE && !W.chase)
        {
            monster_set(&W.mo, MS_HUNT, 0);
            W.mo.tx = cell_of(W.pl.pos.x);
            W.mo.ty = cell_of(W.pl.pos.z);
        }
        lid_was = cv_in.lid;

        if (replaying)
        {
            if ((~REG_KEYINPUT & 0x3FF) || replay_finished()
                || (cv_game.attract && W.frame > 1080))
            {
                replay_stop();
                next = cv_game.playback ? SC_RECORDS : SC_TITLE;
                break;
            }
        }
        else if ((cv_in.pressed & KEY_START) || step_mode || lid_pause)
        {
            int r = pause_menu();
            if (r == PAUSE_RETIRE)
            {
                next = SC_TITLE;
                break;
            }
            if (r == PAUSE_QA_CLEAR)
            {
                cv_game.cleared = true;
                if (n == 5)
                    cv_game.ending = (cv_game.tapes & CV_ALL_TAPES) == CV_ALL_TAPES ? END_WELL : END_ESCAPE;
                next = SC_CLEAR;
                break;
            }
            step_mode = r == PAUSE_STEP;
            if (step_mode)
            {
                // Clears the R press; the stepped frame itself is logged.
                input_update();
                replay_suspend(false);
            }
        }
        if (cv_in.pressed & KEY_SELECT)
        {
            // Swap the screens: the 3D view moves to the touch screen.
            W.swapped = !W.swapped;
            if (W.swapped)
                lcdMainOnBottom();
            else
                lcdMainOnTop();
        }

        timer_prof_begin(false);
        W.frame++;
        if (W.message_timer)
            W.message_timer--;

        player_update();
        hud_touch();
        monster_update(&W.mo);
        visitor_update();
        flares_update();
        particles_update();

        // Prologue: a shape crosses the road ahead in a lightning flash.
        if ((W.map.flags & MF_PHANTOM) && W.mo.state == MS_GONE && W.mo.active
            && W.pl.pos.z < F(9.5) && !W.film_seen)
        {
            W.film_seen = true;
            monster_set(&W.mo, MS_PASS, 0);
            W.mo.pos = m_vec(F(10.5), 0, F(4.5));
            W.lightning = 14;
            snd_sfx(CV_SFX_THUNDER, 127, 64);
        }
        if ((W.map.flags & MF_OUTDOOR) && rng_range(0, 900) == 0 && !W.lightning)
        {
            W.lightning = 10;
            snd_sfx(CV_SFX_THUNDER, 100, rng_range(30, 100));
        }
        if (W.lightning)
            W.lightning--;

        // Fear drives the radio static and the heartbeat tempo.
        int32_t md = W.mo.active && W.mo.state != MS_GONE ? m_dist(W.mo.pos, W.pl.pos) : F(30);
        int fear = 100 - (md >> 10) * 3;
        if (W.chase)
            fear += 30;
        W.fear = m_clamp(fear, 0, 100);
        W.threat = m_clamp(W.fear + rng_range(-6, 7), 0, 100);
        int want_hz = 1 + W.fear / 30;
        if (want_hz != beat_hz)
        {
            beat_hz = want_hz;
            timer_beat_start(beat_hz);
        }
        if (cv_beats != last_beats)
        {
            last_beats = cv_beats;
            if (W.fear > 25)
                snd_sfx(CV_SFX_HEART, 40 + W.fear * 3 / 4, 64);
        }

        // Chapter 5: standing at the well ends the night.
        if (W.at_well > 0 && !clear_pending)
        {
            bool all = (cv_game.tapes & CV_ALL_TAPES) == CV_ALL_TAPES;
            if (!all)
            {
                cv_game.ending = END_ESCAPE;
                clear_pending = true;
            }
            else
            {
                world_message("THE VOICES ARE BELOW.  A: GO DOWN   B: CLIMB OUT", 2);
                if (cv_in.pressed & KEY_A)
                {
                    cv_game.ending = END_WELL;
                    clear_pending = true;
                }
                else if (cv_in.pressed & KEY_B)
                {
                    cv_game.ending = END_ESCAPE;
                    clear_pending = true;
                }
            }
            W.at_well = 0;
        }
        if (W.pl.dead > 90)
            dead_pending = true;

        chfx_frame();
        timer_prof_begin(true);
        if (chfx_dual())
        {
            // Both screens 3D: the view ahead and the view behind on
            // alternate frames (display capture copies the other one).
            chfx_dual_draw();
        }
        else
        {
            render_world();
            g3_flush(W.wbuffer);
        }
        u32 render_us = timer_prof_end_us();
        if (W.console && !chfx_dual())
            console_lines(render_us);
        if (!chfx_dual())
            hud_frame();
        oam_commit();
        sys_vsync();
        chfx_dual_regs();   // in VBlank: screen routing and capture for the frame just drawn

        if (clear_pending)
        {
            cv_game.cleared = true;
            next = SC_CLEAR;
            break;
        }
        if (dead_pending)
        {
            next = SC_DEATH;
            break;
        }
    }

    if (replay_recording())
    {
        // Keep the run on the save chip for "replay last run". On a small
        // chip (melonDS: 8 KB) only the start of a long run fits.
        u32 size;
        const void *blob = replay_blob(save_replay_capacity(), &size);
        replay_stop();
        save_replay(blob, size);
    }
    replay_stop();
    if (cv_game.playback)
        next = SC_RECORDS;
    cv_game.playback = false;

    cv_save.total_playtime = timer_playtime();
    if (!cv_game.attract)
        cv_save.run_seconds += cv_game.chapter_frames / 60;
    timer_beat_stop();
    fx_transition_out(24);
    chfx_teardown();
    free(map.blob);
    oam_clear(ENG_MAIN);
    oam_clear(ENG_SUB);
    oam_flush_now();
    if (cv_game.attract)
    {
        cv_game.attract = false;
        next = SC_TITLE;
    }
    return next;
}
