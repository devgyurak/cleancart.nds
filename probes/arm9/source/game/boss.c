// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Stage bosses. Each boss is a tree of parts drawn with matrix push/pop;
// the same tree is mirrored in software for positions. Shots are confirmed
// with the hardware box test through a pick-matrix frustum around the
// shot's screen position. Attack patterns advance on the timer-2 beat.

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/timer.h"
#include "game/game.h"
#include "game/hud.h"
#include "game/stage.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"

#define F CV_FX
#define MAX_PARTS 24   // the final boss has 18

typedef struct {
    int parent;
    CvVec offset;      // in the parent's rotated space
    int ry, rx;        // local rotation
    int model;
    CvTex tex;
    int32_t scale;     // 20.12
    int32_t radius;
    int hp, hp_max;
    bool alive, core, translucent, damageable;
    int flash;
    // Software mirror of the world transform.
    int32_t m[3][3];
    CvVec world;
} Part;

static struct {
    int type;
    CvVec pos;
    int timer;
    u32 last_beat;
    int phase;
    Part p[MAX_PARTS];
    int n;
    int dying;
    bool rampage;
} B;

static const char *const boss_names[6] = {
    "", "SHORE FORTRESS", "FLAGSHIP KESTREL", "ORBITAL CORE",
    "GATE GUARDIAN", "THE HEART" };

// ---------------------------------------------------------------------------
// Software matrices (rows = basis, 20.12)

static void m_mul(int32_t out[3][3], int32_t a[3][3], int32_t b[3][3])
{
    int32_t r[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r[i][j] = CV_FXMUL(a[i][0], b[0][j]) + CV_FXMUL(a[i][1], b[1][j])
                    + CV_FXMUL(a[i][2], b[2][j]);
    memcpy(out, r, sizeof(r));
}

static void m_rot(int32_t out[3][3], int ry, int rx)
{
    // Row-vector convention like the DS: v' = v * Ry * Rx, matching
    // glRotateYi then glRotateXi on the matrix stack (applied to the model
    // first by X, then Y).
    int32_t cy = m_cos(ry), sy = m_sin(ry), cx = m_cos(rx), sx = m_sin(rx);
    int32_t y[3][3] = { { cy, 0, -sy }, { 0, F(1), 0 }, { sy, 0, cy } };
    int32_t x[3][3] = { { F(1), 0, 0 }, { 0, cx, sx }, { 0, -sx, cx } };
    m_mul(out, x, y);
}

static CvVec m_apply(int32_t m[3][3], CvVec v)
{
    return m_vec(CV_FXMUL(v.x, m[0][0]) + CV_FXMUL(v.y, m[1][0]) + CV_FXMUL(v.z, m[2][0]),
                 CV_FXMUL(v.x, m[0][1]) + CV_FXMUL(v.y, m[1][1]) + CV_FXMUL(v.z, m[2][1]),
                 CV_FXMUL(v.x, m[0][2]) + CV_FXMUL(v.y, m[1][2]) + CV_FXMUL(v.z, m[2][2]));
}

static void solve_world(void)
{
    // Parts are stored parents-first.
    for (int i = 0; i < B.n; i++)
    {
        Part *p = &B.p[i];
        int32_t local[3][3];
        m_rot(local, p->ry, p->rx);
        if (p->parent < 0)
        {
            p->world = m_add(B.pos, p->offset);
            memcpy(p->m, local, sizeof(local));
        }
        else
        {
            Part *q = &B.p[p->parent];
            p->world = m_add(q->world, m_apply(q->m, p->offset));
            m_mul(p->m, local, q->m);
        }
    }
}

// ---------------------------------------------------------------------------

static int add_part(int parent, CvVec offset, int model, CvTex tex, int32_t scale,
                    int hp, bool core)
{
    if (B.n >= MAX_PARTS)
        return MAX_PARTS - 1;
    Part *p = &B.p[B.n];
    memset(p, 0, sizeof(*p));
    p->parent = parent;
    p->offset = offset;
    p->model = model;
    p->tex = tex;
    p->scale = scale;
    p->radius = CV_FXMUL(cv_models[model].radius, scale);
    p->hp = p->hp_max = hp * (cv_save.difficulty ? 2 : 1);
    if (hp > 0 && p->hp_max < 1)
        p->hp = p->hp_max = 1;
    p->alive = true;
    p->core = core;
    p->damageable = hp > 0;
    return B.n++;
}

void boss_spawn(int type, CvVec pos)
{
    memset(&B, 0, sizeof(B));
    B.type = type;
    B.pos = pos;
    B.pos.z = S.pl.pos.z - F(15);
    B.pos.x = 0;
    S.boss_active = true;
    S.arena_z = B.pos.z;

    switch (type)
    {
        case 1:
        {
            B.pos.y = F(0.2);
            int body = add_part(-1, m_vec(0, 0, 0), MDL_FORT, TEX_STRIPES, F(1), 60, true);
            add_part(body, m_vec(-F(1.7), F(1.0), F(0.4)), MDL_TURRET, TEX_STRIPES, F(1), 15, false);
            add_part(body, m_vec(F(1.7), F(1.0), F(0.4)), MDL_TURRET, TEX_STRIPES, F(1), 15, false);
            break;
        }
        case 2:
        {
            B.pos.y = F(3.0);
            int hull = add_part(-1, m_vec(0, 0, 0), MDL_FLAGSHIP, TEX_HULL, F(1.6), 120, true);
            B.p[hull].translucent = true;
            add_part(hull, m_vec(-F(2.2), -F(0.6), 0), MDL_DRONE, TEX_HULL, F(1), 12, false);
            add_part(hull, m_vec(F(2.2), -F(0.6), 0), MDL_DRONE, TEX_HULL, F(1), 12, false);
            break;
        }
        case 3:
        {
            B.pos.y = F(2.0);
            add_part(-1, m_vec(0, 0, 0), MDL_STATION, TEX_COUNT, F(1.8), 150, true);
            break;
        }
        case 4:
        {
            B.pos.y = F(2.2);
            B.pos.z = S.pl.pos.z - F(11);
            int core = add_part(-1, m_vec(0, 0, 0), MDL_BOSS_CORE, TEX_ARMOR, F(1), 80, true);
            for (int s = -1; s <= 1; s += 2)
            {
                int arm = add_part(core, m_vec(s * F(0.9), 0, 0), MDL_BOSS_ARM, TEX_HULL, F(1), 25, false);
                add_part(arm, m_vec(0, 0, -F(1.6)), MDL_BOSS_CLAW, TEX_COUNT, F(1), 0, false);
            }
            break;
        }
        default:
        {
            B.pos.y = F(2.5);
            int core = add_part(-1, m_vec(0, 0, 0), MDL_BOSS_CORE, TEX_ARMOR, F(1.2), 200, true);
            int shield = add_part(core, m_vec(0, 0, 0), MDL_BOSS_SHIELD, TEX_COUNT, F(1), 0, false);
            B.p[shield].translucent = true;
            for (int k = 0; k < 4; k++)
            {
                int a = k * 8192;
                CvVec off = m_vec(CV_FXMUL(F(1.9), m_cos(a)), CV_FXMUL(F(1.9), m_sin(a)), F(0.3));
                add_part(core, off, MDL_BOSS_PANEL, TEX_PANEL, F(1), 20, false);
            }
            for (int k = 0; k < 4; k++)
            {
                int a = k * 8192 + 4096;
                CvVec off = m_vec(CV_FXMUL(F(1.2), m_cos(a)), CV_FXMUL(F(1.2), m_sin(a)), 0);
                int seg1 = add_part(core, off, MDL_BOSS_ARM, TEX_HULL, F(1), 30, false);
                int seg2 = add_part(seg1, m_vec(0, 0, -F(1.6)), MDL_BOSS_ARM, TEX_HULL, F(0.8), 0, false);
                add_part(seg2, m_vec(0, 0, -F(1.3)), MDL_BOSS_CLAW, TEX_COUNT, F(1), 0, false);
            }
            break;
        }
    }

    solve_world();
    B.last_beat = cv_beats;
    timer_beat_start(4 + type / 2);
    snd_bgm(CV_SONG_BOSS);
    snd_echo(false);
    snd_sfx(CV_SFX_WARNING, 127, 64);
    stage_message(boss_names[type % 6], 150);
}

int boss_hp(void)
{
    int hp = 0;
    for (int i = 0; i < B.n; i++)
        if (B.p[i].damageable && B.p[i].alive)
            hp += B.p[i].hp;
    return hp;
}

int boss_hp_max(void)
{
    int hp = 0;
    for (int i = 0; i < B.n; i++)
        if (B.p[i].damageable)
            hp += B.p[i].hp_max;
    return hp;
}

CvVec boss_pos(void)
{
    return B.n ? B.p[0].world : B.pos;
}

static bool shield_up(void);

// Where a shot can hurt the boss right now: the first live part that takes
// damage (not the core while the shield is up). Read-only; used by the QA autopilot.
bool boss_aim_point(CvVec *out)
{
    if (!S.boss_active || B.dying)
        return false;
    bool shielded = shield_up();
    for (int i = 0; i < B.n; i++)
    {
        const Part *p = &B.p[i];
        if (!p->alive || !p->damageable || (p->core && shielded))
            continue;
        *out = p->world;
        return true;
    }
    return false;
}

static bool shield_up(void)
{
    if (B.type != 5)
        return false;
    for (int i = 0; i < B.n; i++)
        if (B.p[i].model == MDL_BOSS_PANEL && B.p[i].alive)
            return true;
    return false;
}

static void kill_children(int parent)
{
    for (int i = 0; i < B.n; i++)
    {
        if (B.p[i].parent == parent && B.p[i].alive)
        {
            B.p[i].alive = false;
            blast_spawn(B.p[i].world, 1);
            kill_children(i);
        }
    }
}

static void part_damage(int i, int dmg)
{
    Part *p = &B.p[i];
    if (!p->alive || !p->damageable || B.dying)
        return;
    if (p->core && shield_up())
    {
        snd_sfx(CV_SFX_HIT, 40, 64);
        return;
    }
    // Hits on armless parts route to the core.
    p->hp -= dmg;
    p->flash = 6;
    if (p->hp <= 0)
    {
        p->alive = false;
        blast_spawn(p->world, 2);
        snd_sfx(CV_SFX_EXPLODE, 120, 64);
        cv_game.score += 1000;
        kill_children(i);
        if (p->core)
        {
            B.dying = 160;
            S.flash = 16;   // white-out as the core goes
            particle(p->world, m_vec(0, 0, 0), RGB15(31, 31, 31), 30, F(1.5), F(0.25), PK_GLOW);
            sparks(p->world, 40, RGB15(31, 26, 10), F(0.25));
            snd_sfx(CV_SFX_BIGBOOM, 127, 64);
            cv_game.score += 20000 * S.stage;
        }
    }
    else
    {
        snd_sfx(CV_SFX_HIT, 70, 64);
    }
}

// GL transform chain from the root down to part `i`.
static void gl_chain(int i)
{
    int path[MAX_PARTS], n = 0;
    for (int k = i; k >= 0; k = B.p[k].parent)
        path[n++] = k;
    glTranslatef32(B.pos.x, B.pos.y, B.pos.z);
    while (n--)
    {
        Part *p = &B.p[path[n]];
        glTranslatef32(p->offset.x, p->offset.y, p->offset.z);
        glRotateYi(p->ry);
        glRotateXi(p->rx);
    }
}

bool boss_hit_test(CvVec pos, int sx, int sy, int dmg)
{
    if (!S.boss_active || B.dying)
        return false;
    bool hit = false;
    bool picked = false;
    for (int i = 0; i < B.n && !hit; i++)
    {
        Part *p = &B.p[i];
        if (!p->alive || !p->damageable)
            continue;
        int32_t r = p->radius + (dmg > 1 ? F(0.6) : F(0.3));
        if (m_dist(pos, p->world) > r + F(0.5))
            continue;
        if (sx >= 0 && sy >= 0 && sx < 256 && sy < 192)
        {
            // Confirm on the geometry engine: a 6x6 pixel pick frustum
            // around the shot, and the part's box under its own matrices.
            if (!picked)
            {
                g3_reload(sx, sy, 6);
                picked = true;
            }
            glPushMatrix();
            gl_chain(i);
            int32_t s = p->scale;
            int32_t h = CV_FXMUL(cv_models[p->model].radius, s);
            if (h > F(3.5))
                h = F(3.5);
            bool inside = g3_box_test(m_vec(-h, -h, -h), m_vec(2 * h, 2 * h, 2 * h));
            glPopMatrix(1);
            if (!inside)
                continue;
        }
        part_damage(i, dmg);
        hit = true;
    }
    if (picked)
        g3_reload(0, 0, 0);
    return hit;
}

static void fire_from(CvVec at, int pattern)
{
    int32_t speed = cv_save.difficulty ? F(0.13) : F(0.1);
    switch (pattern)
    {
        case 0:
            bullet_aimed(at, speed, 0, 1);
            break;
        case 1:
            for (int k = -2; k <= 2; k++)
                bullet_aimed(at, speed, k * 900, 0);
            break;
        case 2:
            for (int k = 0; k < 16; k++)
            {
                int a = k * 2048 + B.timer * 64;
                bullet_fire(at, m_vec(CV_FXMUL(F(0.07), m_cos(a)), CV_FXMUL(F(0.07), m_sin(a)),
                                      F(0.08)), 0);
            }
            break;
        case 3:
            for (int k = 0; k < 4; k++)
            {
                int a = B.timer * 1100 + k * 8192;
                bullet_fire(at, m_vec(CV_FXMUL(F(0.08), m_cos(a)), CV_FXMUL(F(0.08), m_sin(a)),
                                      F(0.09)), 1);
            }
            break;
        default:
            // Rain: rows of large bullets sharing scanlines.
            for (int k = -5; k <= 5; k++)
                bullet_fire(m_vec(S.pl.pos.x + k * F(0.6), S.pl.pos.y + rng_range(-F(0.4), F(0.4)), at.z),
                            m_vec(0, 0, F(0.11)), 2);
            break;
    }
}

void boss_update(void)
{
    if (!S.boss_active && !S.boss_dead)
        return;
    B.timer++;
    for (int i = 0; i < B.n; i++)
        if (B.p[i].flash)
            B.p[i].flash--;

    if (B.dying)
    {
        if ((B.dying & 7) == 0)
        {
            CvVec at = m_add(B.p[0].world, m_vec(rng_range(-F(2), F(2)), rng_range(-F(1), F(2)),
                                                 rng_range(-F(1), F(1))));
            blast_spawn(at, 2);
            debris_burst(at, 10);
            snd_sfx(CV_SFX_EXPLODE, 110, rng_range(20, 108));
        }
        if (--B.dying == 0)
        {
            S.boss_active = false;
            S.boss_dead = true;
            timer_beat_stop();
            snd_bgm(CV_SONG_RESULT);
        }
        return;
    }

    // Motion and part animation per type.
    int t = B.timer;
    switch (B.type)
    {
        case 1:
            for (int i = 1; i < B.n; i++)
            {
                CvVec d = m_sub(S.pl.pos, B.p[i].world);
                B.p[i].ry = m_atan2(-d.x, -d.z);
            }
            break;
        case 2:
            B.pos.x = CV_FXMUL(F(3.5), m_sin(t * 60));
            B.p[0].ry = CV_FXMUL(1200, m_sin(t * 60));
            break;
        case 3:
            B.p[0].ry += 140;
            B.p[0].rx = CV_FXMUL(1600, m_sin(t * 50));
            if ((t % 150) == 0)
            {
                int e = enemy_spawn(E_METEOR, m_add(B.p[0].world, m_vec(rng_range(-F(3), F(3)), 0, 0)),
                                    P_NONE, 1, rng_range(0, 4));
                if (e >= 0)
                    S.en[e].vel = m_norm(m_sub(S.pl.pos, S.en[e].pos), F(0.08));
            }
            break;
        case 4:
            B.pos.x = CV_FXMUL(F(1.5), m_sin(t * 80));
            for (int i = 1; i < B.n; i++)
                if (B.p[i].model == MDL_BOSS_ARM)
                    B.p[i].ry = (B.p[i].offset.x < 0 ? 1 : -1) * (2000 + CV_FXMUL(1800, m_sin(t * 200)));
            break;
        default:
        {
            B.pos.x = CV_FXMUL(F(1.2), m_sin(t * 70));
            B.pos.y = F(2.5) + CV_FXMUL(F(0.5), m_cos(t * 90));
            B.p[0].ry = CV_FXMUL(2000, m_sin(t * 40));
            int k = 0;
            for (int i = 1; i < B.n; i++)
            {
                Part *p = &B.p[i];
                if (p->model == MDL_BOSS_ARM && p->parent == 0)
                {
                    p->ry = CV_FXMUL(3000, m_sin(t * 150 + k * 8192));
                    p->rx = CV_FXMUL(2500, m_cos(t * 120 + k * 8192));
                    k++;
                }
                else if (p->model == MDL_BOSS_ARM)
                {
                    p->rx = CV_FXMUL(4000, m_sin(t * 220 + i * 3000));
                }
                else if (p->model == MDL_BOSS_SHIELD)
                {
                    p->alive = shield_up();
                    p->ry += 300;
                }
            }
            if (!B.rampage && boss_hp() * 10 < boss_hp_max() * 3)
            {
                B.rampage = true;
                S.overload = true;
                stage_message("CORE RAMPAGE", 120);
                timer_beat_start(8);
            }
            if (B.rampage && (t & 3) == 0)
                debris_burst(m_add(B.p[0].world, m_vec(0, 0, F(0.5))), 16);
            break;
        }
    }
    solve_world();

    // Attacks on the beat (timer 2 interrupt count), one beat per frame.
    if (B.last_beat != cv_beats)
    {
        B.last_beat++;
        B.phase++;
        int beat = B.phase;
        for (int i = 0; i < B.n; i++)
        {
            Part *p = &B.p[i];
            if (!p->alive || (!p->damageable && p->model != MDL_BOSS_CLAW))
                continue;
            int pat;
            if (p->core)
                pat = (beat / 8) % 4 == 3 ? 4 : (beat % 4 == 0 ? 2 : (beat % 2 ? 3 : -1));
            else
                pat = (beat % 3 == 0) ? 1 : (beat % 3 == 1 ? 0 : -1);
            if (B.type == 1 && p->core && (B.p[1].alive || B.p[2].alive))
                pat = beat % 6 == 0 ? 2 : -1;
            if (B.rampage && p->core)
                pat = beat % 2 ? 4 : 2;
            if (pat >= 0)
                fire_from(p->world, pat);
        }
    }

    // Body contact.
    for (int i = 0; i < B.n; i++)
    {
        Part *p = &B.p[i];
        if (p->alive && !p->translucent && m_dist(p->world, S.pl.pos) < p->radius + F(0.3))
        {
            S.pl.pos.z += F(0.3);
            break;
        }
    }
    // Keep the arena in front of the player.
    if (B.type != 1 && B.pos.z > S.pl.pos.z - F(9))
        B.pos.z = S.pl.pos.z - F(9);
}

void boss_draw(void)
{
    if (!S.boss_active)
        return;
    const u32 lights = POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1 | POLY_FORMAT_LIGHT2
                     | POLY_FORMAT_LIGHT3;
    // Opaque parts first, then translucent ones (manual sort order).
    for (int pass = 0; pass < 2; pass++)
    {
        // Hierarchy: push for each part, translate+rotate, draw, recurse.
        int stack[MAX_PARTS], depth = 0;
        glPushMatrix();
        glTranslatef32(B.pos.x, B.pos.y, B.pos.z);
        for (int i = 0; i < B.n; i++)
        {
            Part *p = &B.p[i];
            // Pop back to this part's parent.
            while (depth > 0 && stack[depth - 1] != p->parent)
            {
                glPopMatrix(1);
                depth--;
            }
            glPushMatrix();
            stack[depth++] = i;
            glTranslatef32(p->offset.x, p->offset.y, p->offset.z);
            glRotateYi(p->ry);
            glRotateXi(p->rx);
            if (!p->alive || p->translucent != (pass == 1))
                continue;
            glPushMatrix();
            if (p->scale != F(1))
                glScalef32(p->scale, p->scale, p->scale);
            g3_texture_bind(p->tex);
            // Station core: toon bands. Others: highlight shading while hit.
            u32 shade = (B.type == 3 || p->flash) ? POLY_TOON_HIGHLIGHT : 0;
            int alpha = p->translucent ? (B.type == 5 ? 10 : 18) : 31;
            glPolyFmt(POLY_ALPHA(alpha) | (p->translucent ? POLY_CULL_NONE : POLY_CULL_BACK)
                      | POLY_ID(24 + i) | lights | shade
                      | (alpha < 31 ? POLY_TRANS_SET_DEPTH : 0));
            g3_draw(&cv_models[p->model]);
            glPopMatrix(1);
        }
        while (depth-- > 0)
            glPopMatrix(1);
        glPopMatrix(1);
    }
}

void boss_radar(void)
{
    if (!S.boss_active)
        return;
    CvVec w = boss_pos();
    hud_blip(w.x - S.pl.pos.x, w.z - S.pl.pos.z, 2, false);
}
