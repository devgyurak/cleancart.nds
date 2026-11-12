// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The stage loop: player, enemies, bullets, collisions, lock-on, pause and
// the per-frame 3D/2D submission.

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cart.h"
#include "core/cvmath.h"
#include "core/dma.h"
#include "core/hw.h"
#include "core/input.h"
#include "core/irq.h"
#include "core/nitro.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "core/timer.h"
#include "core/qa.h"
#include "game/qa_tune.h"
#include "game/game.h"
#include "game/hud.h"
#include "game/stage.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/models.h"
#include "gfx/oam.h"
#include "gfx/terrain.h"
#include "gfx/vram.h"

CvStage S;

#define F CV_FX

typedef struct {
    int hp, score, model;
    int32_t radius;
    int fire;
} EnemyDef;

static const EnemyDef edef[] = {
    [E_TURRET]  = { 3, 100, MDL_TURRET, F(0.7), 90 },
    [E_DRONE]   = { 2, 150, MDL_DRONE, F(0.7), 100 },
    [E_AIRSHIP] = { 30, 1500, MDL_AIRSHIP, F(2.2), 46 },
    [E_METEOR]  = { 4, 80, -1, F(0.8), 0 },
    [E_STATION] = { 40, 2000, MDL_STATION, F(2.0), 26 },
    [E_WALLGUN] = { 4, 200, MDL_WALLGUN, F(0.6), 70 },
    [E_FIGHTER] = { 2, 250, MDL_FIGHTER, F(0.7), 80 },
    [E_BOSS]    = { 1, 0, -1, F(1.0), 0 },
};

// ---------------------------------------------------------------------------
// Stage data

bool stage_load(int n, CvStageData *out)
{
    char name[16];
    snprintf(name, sizeof(name), "stage%d.lz", n);
    memset(out, 0, sizeof(*out));

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
            if (blob)
            {
                if (size <= 32768)
                {
                    swiDecompressLZSSWram(comp, (void *)0x03000000);
                    swiFastCopy((void *)0x03000000, blob, ((size + 31) & ~31) >> 2);
                }
                else
                {
                    swiDecompressLZSSWram(comp, blob);
                }
                DC_FlushRange(blob, (size + 31) & ~31);
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
    if (*(const u32 *)b != 0x54535643u)
    {
        free(blob);
        return false;
    }
    out->blob = blob;
    out->number = h[3];
    out->rows = h[4];
    out->cols = h[5];
    out->num_spawns = h[6];
    out->flags = h[7];
    out->boss = h[8];
    out->song = h[9];
    out->fog = h[10];
    out->clear = h[11];
    out->heights = b + 24;
    out->spawns = (const CvSpawn *)(b + 24 + out->rows * out->cols);
    return true;
}

void stage_free(CvStageData *d)
{
    free(d->blob);
    d->blob = NULL;
}

// ---------------------------------------------------------------------------
// Actors

void stage_message(const char *msg, int frames)
{
    S.message = msg;
    S.message_timer = frames;
}

int enemy_spawn(int type, CvVec pos, int pattern, int count, int param)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &S.en[i];
        if (e->alive)
            continue;
        memset(e, 0, sizeof(*e));
        e->alive = true;
        e->type = type;
        e->pattern = pattern;
        e->count = count;
        e->param = param;
        e->pos = pos;
        e->hp = e->hp_max = edef[type].hp * (cv_save.difficulty ? 2 : 1);
        e->score = edef[type].score;
        e->fire_timer = 30 + rng_range(0, 60);
        e->angle = rng_range(0, 32768);
        cv_game.stage_spawned++;
        return i;
    }
    return -1;
}

void particle(CvVec pos, CvVec vel, u16 color, int life, int32_t size, int32_t grow, int kind)
{
    Particle *p = &S.pa[S.pa_next];
    S.pa_next = (S.pa_next + 1) % MAX_PARTICLES;
    p->pos = pos;
    p->vel = vel;
    p->color = color;
    p->life = p->max = (u8)life;
    p->size = size;
    p->grow = grow;
    p->kind = (u8)kind;
}

void sparks(CvVec pos, int n, u16 color, int32_t speed)
{
    for (int i = 0; i < n; i++)
    {
        CvVec v = m_vec(rng_range(-speed, speed), rng_range(-speed, speed),
                        rng_range(-speed, speed));
        particle(pos, v, color, 12 + rng_range(0, 14), F(0.12) + rng_range(0, F(0.1)),
                 -F(0.006), PK_GLOW);
    }
}

void blast_spawn(CvVec pos, int size)
{
    // Fireball core, a fast shock flash and a spray of embers.
    bool big = size > 1;
    particle(pos, m_vec(0, 0, 0), RGB15(31, 31, 24), big ? 12 : 8, big ? F(0.9) : F(0.5),
             big ? F(0.16) : F(0.09), PK_GLOW);
    particle(pos, m_vec(0, F(0.01), 0), RGB15(31, 16, 4), big ? 22 : 14, big ? F(0.6) : F(0.35),
             big ? F(0.05) : F(0.03), PK_GLOW);
    sparks(pos, big ? 16 : 8, (S.frame & 1) ? RGB15(31, 24, 6) : RGB15(31, 12, 2),
           big ? F(0.16) : F(0.1));
    if (big)
        for (int k = 0; k < 4; k++)
            particle(m_add(pos, m_vec(rng_range(-F(0.5), F(0.5)), rng_range(0, F(0.4)), 0)),
                     m_vec(0, F(0.02), 0), RGB15(20, 20, 22), 40, F(0.5), F(0.03), PK_SMOKE);
    for (int i = 0; i < MAX_BLASTS; i++)
    {
        if (S.bl[i].alive)
            continue;
        S.bl[i].alive = true;
        S.bl[i].pos = pos;
        S.bl[i].frame = 0;
        S.bl[i].size = size;
        return;
    }
}

void debris_burst(CvVec pos, int n)
{
    for (int i = 0; i < MAX_DEBRIS && n > 0; i++)
    {
        Debris *d = &S.de[i];
        if (d->alive)
            continue;
        d->alive = true;
        d->pos = pos;
        d->vel = m_vec(rng_range(-F(0.12), F(0.12)), rng_range(-F(0.08), F(0.14)),
                       rng_range(-F(0.12), F(0.12)));
        d->spin = rng_range(0, 32768);
        d->life = 60 + rng_range(0, 90);
        n--;
    }
}

void enemy_damage(int index, int dmg)
{
    Enemy *e = &S.en[index];
    if (!e->alive || e->type == E_BOSS)
        return;
    e->hp -= dmg;
    e->flash = 4;
    if (e->hp <= 0)
    {
        e->alive = false;
        cv_game.score += e->score;
        cv_game.stage_kills++;
        int big = e->type == E_AIRSHIP || e->type == E_STATION;
        blast_spawn(e->pos, big ? 2 : 1);
        if (big)
        {
            snd_sfx_at(CV_SFX_BIGBOOM, e->pos.x - S.pl.pos.x, m_dist(e->pos, S.pl.pos));
            debris_burst(e->pos, 24);
        }
        else
        {
            snd_sfx_at(CV_SFX_EXPLODE, e->pos.x - S.pl.pos.x, m_dist(e->pos, S.pl.pos));
        }
    }
    else
    {
        snd_sfx(CV_SFX_HIT, 60, 64);
        sparks(e->pos, 4, RGB15(31, 31, 12), F(0.08));
    }
}

void bullet_fire(CvVec from, CvVec vel, int kind)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &S.bu[i];
        if (b->alive)
            continue;
        b->alive = true;
        b->kind = kind;
        b->pos = from;
        b->vel = vel;
        b->life = 300;
        return;
    }
}

void bullet_aimed(CvVec from, int32_t speed, int spread_angle, int kind)
{
    CvVec target = S.pl.pos;
    CvVec d = m_norm(m_sub(target, from), speed);
    if (spread_angle)
    {
        int32_t c = m_cos(spread_angle), s = m_sin(spread_angle);
        d = m_vec(CV_FXMUL(d.x, c) - CV_FXMUL(d.z, s), d.y,
                  CV_FXMUL(d.x, s) + CV_FXMUL(d.z, c));
    }
    bullet_fire(from, d, kind);
}

static void enemy_fire(Enemy *e)
{
    int32_t speed = cv_save.difficulty ? F(0.14) : F(0.11);
    switch (e->pattern)
    {
        case P_AIMED:
            bullet_aimed(e->pos, speed, 0, 0);
            break;
        case P_SPREAD3:
            for (int k = -1; k <= 1; k++)
                bullet_aimed(e->pos, speed, k * 1100, 1);
            break;
        case P_RING:
            for (int k = 0; k < 12; k++)
            {
                int a = k * 32768 / 12 + e->timer * 97;
                bullet_fire(e->pos, m_vec(CV_FXMUL(F(0.06), m_cos(a)),
                                          CV_FXMUL(F(0.06), m_sin(a)), F(0.09)), 0);
            }
            break;
        case P_SPIRAL:
            for (int k = 0; k < 3; k++)
            {
                int a = e->timer * 900 + k * 32768 / 3;
                bullet_fire(e->pos, m_vec(CV_FXMUL(F(0.07), m_cos(a)),
                                          CV_FXMUL(F(0.07), m_sin(a)), F(0.08)), 1);
            }
            break;
        case P_RAIN:
            // A horizontal line of bullets on one scanline band.
            for (int k = -4; k <= 4; k++)
                bullet_fire(m_vec(S.pl.pos.x + k * F(0.7), S.pl.pos.y + F(0.2), e->pos.z),
                            m_vec(0, 0, F(0.1)), 2);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Player

static void player_hit(void)
{
    Player *p = &S.pl;
    if (p->invuln || p->dead_timer || p->boost > 20)
        return;
    blast_spawn(p->pos, 2);
    debris_burst(p->pos, 12);
    snd_sfx(CV_SFX_BIGBOOM, 127, 64);
    cv_game.lives -= qa_tune()->lives_lost_per_hit;
    cv_game.bombs = 2;
    p->invuln = 150;
    p->charge = 0;
    S.hit_shake = 20;
    if (cv_game.lives < 0)
        p->dead_timer = 120;
    // Mosaic wobble on hit (2D layers; the BG mosaic bit is per layer).
    for (int i = 1; i < 4; i++)
        ((vu16 *)0x04000008)[i] |= 1 << 6;
    fx_mosaic(FX_MAIN, 6, 6);
}

static void fire_shot(CvVec pos, CvVec vel, int power)
{
    for (int i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = &S.sh[i];
        if (s->alive)
            continue;
        s->alive = true;
        s->pos = pos;
        s->vel = vel;
        s->power = power;
        s->life = 50;
        particle(pos, m_vec(0, 0, -F(0.05)), power > 1 ? RGB15(31, 30, 16) : RGB15(18, 31, 31),
                 power > 1 ? 8 : 3, power > 1 ? F(0.8) : F(0.3), -F(0.02), PK_GLOW);
        return;
    }
}

static void fire_missile(int target)
{
    for (int i = 0; i < MAX_MISSILES; i++)
    {
        Missile *m = &S.mi[i];
        if (m->alive)
            continue;
        m->alive = true;
        m->pos = m_add(S.pl.pos, m_vec(0, F(-0.2), 0));
        m->vel = m_vec(rng_range(-F(0.1), F(0.1)), F(0.08), -F(0.1));
        m->target = target;
        m->life = 150;
        snd_sfx(CV_SFX_MISSILE, 100, 64);
        return;
    }
}

static void use_bomb(void)
{
    if (cv_game.bombs <= 0 || S.pl.dead_timer)
        return;
    cv_game.bombs--;
    snd_sfx(CV_SFX_BOMB, 127, 64);
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (S.bu[i].alive)
        {
            S.bu[i].alive = false;
            cv_game.score += 10;
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &S.en[i];
        if (e->alive && m_dist(e->pos, S.pl.pos) < F(24))
            enemy_damage(i, 10);
    }
    if (S.boss_active)
        boss_hit_test(boss_pos(), -1, -1, 25);
    S.pl.invuln = 90;
    S.flash = 14;
    S.cutin = 70;
    CvVec ahead = m_add(S.pl.pos, m_vec(0, 0, -F(2.5)));
    particle(ahead, m_vec(0, 0, -F(0.15)), RGB15(16, 26, 31), 20, F(0.8), F(0.16), PK_GLOW);
    particle(ahead, m_vec(0, 0, -F(0.1)), RGB15(31, 31, 31), 12, F(0.4), F(0.14), PK_GLOW);
    sparks(m_add(S.pl.pos, m_vec(0, 0, -F(2))), 28, RGB15(24, 30, 31), F(0.3));
}

static int32_t pl_xmin, pl_xmax, pl_ymin, pl_ymax;

static void touch_aim_fire(void)
{
    // Stage 4: the touch point on the (swapped) 3D screen gives a camera-
    // space ray. The direction test multiplies it by the camera-to-world
    // rotation loaded as the vector matrix.
    int32_t dx = (cv_in.tx - 128) * F(1.0) / 166;
    int32_t dy = -(cv_in.ty - 96) * F(1.0) / 166;
    CvVec cam = m_norm(m_vec(dx, dy, -F(1)), F(1) - 16);

    CvVec r = g3_cam_right(), u = g3_cam_up(), f = g3_cam_forward();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    MATRIX_LOAD4x3 = r.x; MATRIX_LOAD4x3 = r.y; MATRIX_LOAD4x3 = r.z;
    MATRIX_LOAD4x3 = u.x; MATRIX_LOAD4x3 = u.y; MATRIX_LOAD4x3 = u.z;
    MATRIX_LOAD4x3 = -f.x; MATRIX_LOAD4x3 = -f.y; MATRIX_LOAD4x3 = -f.z;
    MATRIX_LOAD4x3 = 0; MATRIX_LOAD4x3 = 0; MATRIX_LOAD4x3 = 0;
    CvVec w = g3_vec_test(cam.x, cam.y, cam.z);
    glPopMatrix(1);

    CvVec dir = m_norm(w, F(0.6));
    fire_shot(m_add(S.pl.pos, m_vec(0, F(0.1), -F(0.4))), dir, 1);
    S.pl.aim_x = cv_in.tx;
    S.pl.aim_y = cv_in.ty;
}

static void try_lock_on(void)
{
    int32_t dx, dz;
    if (!hud_touch_to_world(cv_in.tx, cv_in.ty, &dx, &dz))
        return;
    CvVec at = m_add(S.pl.pos, m_vec(dx, 0, dz));
    int best = -1;
    int32_t bd = F(5);
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &S.en[i];
        if (!e->alive || e->type == E_METEOR)
            continue;
        CvVec flat = m_vec(e->pos.x - at.x, 0, e->pos.z - at.z);
        int32_t d = m_len(flat);
        if (d < bd)
        {
            bd = d;
            best = i;
        }
    }
    if (S.boss_active && S.pl.missile_cool == 0)
    {
        CvVec bp = boss_pos();
        if (m_len(m_vec(bp.x - at.x, 0, bp.z - at.z)) < F(6))
        {
            snd_sfx(CV_SFX_LOCKON, 100, 64);
            fire_missile(-1);
            S.pl.missile_cool = 24;
            stage_message("LOCK ON: TARGET", 40);
            return;
        }
    }
    if (best >= 0 && S.pl.missile_cool == 0)
    {
        S.en[best].locked = true;
        snd_sfx(CV_SFX_LOCKON, 100, 64);
        fire_missile(best);
        S.pl.missile_cool = 24;
        stage_message("LOCK ON", 40);
    }
}

static void player_update(void)
{
    Player *p = &S.pl;
    u16 k = cv_in.held;

    if (p->dead_timer)
    {
        p->dead_timer--;
        return;
    }
    if (p->invuln)
        p->invuln--;
    if (p->shot_cool)
        p->shot_cool--;
    if (p->missile_cool)
        p->missile_cool--;

    int32_t mv = F(0.09);
    if (p->boost)
    {
        p->boost--;
        mv = F(0.16);
    }
    else if ((cv_in.pressed & KEY_B))
    {
        p->boost = 36;
        snd_sfx(CV_SFX_CHARGE, 70, 64);
    }

    int target_bank = 0;
    if (k & KEY_LEFT)  { p->pos.x -= mv; target_bank = 3000; }
    if (k & KEY_RIGHT) { p->pos.x += mv; target_bank = -3000; }
    if (k & KEY_UP)    p->pos.y += mv;
    if (k & KEY_DOWN)  p->pos.y -= mv;
    p->bank += (target_bank - p->bank) / 6;
    if (p->boost > 20)
        p->roll += 2400;
    else
        p->roll = 0;

    if (S.data.flags & ST_FLAG_CORRIDOR)
    {
        int32_t half = terrain_corridor_half(p->pos.z) - F(0.6);
        pl_xmin = -half;
        pl_xmax = half;
    }
    p->pos.x = m_clamp(p->pos.x, pl_xmin, pl_xmax);
    p->pos.y = m_clamp(p->pos.y, pl_ymin, pl_ymax);
    if (S.data.flags & ST_FLAG_TERRAIN)
    {
        int32_t ground = terrain_height(p->pos.x, p->pos.z) + F(0.6);
        if (p->pos.y < ground)
            p->pos.y = ground;
    }

    // Forward motion stops in the boss arena.
    if (!S.boss_active)
        p->pos.z -= p->speed + (p->boost ? F(0.05) : 0);

    // Fire: taps and the first 20 frames of a hold shoot; a longer hold
    // charges; releasing a full charge fires the charge shot.
    if (k & KEY_A)
    {
        p->hold++;
        if (p->hold <= 20 && p->shot_cool == 0)
        {
            fire_shot(m_add(p->pos, m_vec(F(-0.35), 0, -F(0.5))), m_vec(0, 0, -F(0.6)), 1);
            fire_shot(m_add(p->pos, m_vec(F(0.35), 0, -F(0.5))), m_vec(0, 0, -F(0.6)), 1);
            snd_sfx(CV_SFX_SHOT, 50, 64);
            p->shot_cool = 5;
        }
        else if (p->hold > 20 && p->charge < 60)
        {
            p->charge++;
        }
    }
    else
    {
        if (p->charge >= 60)
        {
            fire_shot(m_add(p->pos, m_vec(0, 0, -F(0.6))), m_vec(0, 0, -F(0.5)), 8);
            snd_sfx(CV_SFX_CHARGE, 127, 64);
        }
        p->hold = 0;
        p->charge = 0;
    }

    if (cv_in.pressed & KEY_X)
        use_bomb();

    // L/R orbit the camera; it eases back when released.
    if (k & KEY_L)
        S.cam_yaw = m_clamp(S.cam_yaw + 200, -5400, 5400);
    else if (k & KEY_R)
        S.cam_yaw = m_clamp(S.cam_yaw - 200, -5400, 5400);
    else
        S.cam_yaw -= S.cam_yaw / 12;

    if (cv_in.touching)
    {
        if (S.swapped)
        {
            p->aiming = true;
            if (p->shot_cool == 0)
            {
                touch_aim_fire();
                p->shot_cool = 6;
            }
        }
        else if (p->missile_cool == 0)
        {
            try_lock_on();
        }
    }
    else
    {
        p->aiming = false;
    }
}

// ---------------------------------------------------------------------------
// World update

static void spawn_from_script(void)
{
    int row_now = (-S.pl.pos.z) >> 12;
    while (S.spawn_index < S.data.num_spawns)
    {
        const CvSpawn *sp = &S.data.spawns[S.spawn_index];
        if (sp->row > row_now + 26)
            break;
        S.spawn_index++;
        CvVec pos = m_vec(sp->x * F(0.25), sp->y * F(0.25) + F(1.5), -CV_FXI(sp->row));
        if (sp->type == E_BOSS)
        {
            boss_spawn(sp->param, pos);
            continue;
        }
        int n = sp->count ? sp->count : 1;
        for (int k = 0; k < n; k++)
        {
            CvVec p = pos;
            p.x += (k - (n - 1) / 2) * (sp->param ? sp->param * F(0.1) : F(1.2));
            p.z -= k * F(1.0);
            if (sp->type == E_TURRET)
                p.y = terrain_height(p.x, p.z);
            if (sp->type == E_WALLGUN)
            {
                int32_t half = terrain_corridor_half(p.z) - F(0.4);
                p.x = sp->x < 0 ? -half : half;
            }
            int idx = enemy_spawn(sp->type, p, sp->pattern, 1, sp->param);
            if (idx >= 0 && sp->type == E_METEOR)
                S.en[idx].vel = m_vec(rng_range(-F(0.02), F(0.02)), rng_range(-F(0.015), F(0.015)), F(0.03));
        }
    }
}

static void enemies_update(void)
{
    Player *p = &S.pl;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &S.en[i];
        if (!e->alive || e->type == E_BOSS)
            continue;
        e->timer++;
        if (e->flash)
            e->flash--;

        switch (e->type)
        {
            case E_DRONE:
                e->pos.x += CV_FXMUL(F(0.04), m_sin(e->timer * 300 + i * 4000));
                e->pos.y += CV_FXMUL(F(0.02), m_cos(e->timer * 260));
                e->pos.z += F(0.025);
                break;
            case E_AIRSHIP:
                e->pos.z -= CV_FXMUL(p->speed, F(0.8));
                if (e->timer > 700)
                    e->pos.z += F(0.05);
                break;
            case E_FIGHTER:
                if (e->timer < 70)
                    e->vel = m_norm(m_sub(p->pos, e->pos), F(0.13));
                e->pos = m_add(e->pos, e->vel);
                break;
            case E_METEOR:
                e->pos = m_add(e->pos, e->vel);
                e->angle += 300;
                break;
            case E_STATION:
                e->angle += 120;
                e->pos.z -= CV_FXMUL(p->speed, F(0.5));
                break;
            default:
                break;
        }

        // Leave once they fall behind the player: anything between the ship
        // and the camera would fill the screen, cut by the near plane.
        if (e->pos.z > p->pos.z + edef[e->type].radius + F(0.8)
            || e->pos.z < p->pos.z - F(60))
        {
            e->alive = false;
            continue;
        }

        int32_t dist = m_dist(e->pos, p->pos);
        if (dist < edef[e->type].radius + F(0.45))
        {
            player_hit();
            enemy_damage(i, 5);
            continue;
        }

        if (edef[e->type].fire && e->pos.z < p->pos.z - F(2) && dist < F(28))
        {
            if (--e->fire_timer <= 0)
            {
                enemy_fire(e);
                int base = edef[e->type].fire;
                base = cv_save.difficulty ? base * 2 / 3 : base * 4 / 3;
                e->fire_timer = base + rng_range(0, base / 2);
                if (e->pattern == P_SPIRAL)
                    e->fire_timer = 6;
            }
        }
    }
}

static void bullets_update(void)
{
    Player *p = &S.pl;
    u32 n = 0;
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &S.bu[i];
        if (!b->alive)
            continue;
        n++;
        b->pos = m_add(b->pos, b->vel);
        if (--b->life == 0 || b->pos.z > p->pos.z + F(1.5))
        {
            b->alive = false;
            continue;
        }
        if (!p->dead_timer && m_abs(b->pos.z - p->pos.z) < F(0.6)
            && m_dist(b->pos, p->pos) < (b->kind == 2 ? F(0.5) : F(0.36)))
        {
            b->alive = false;
            player_hit();
        }
    }
    if (n > S.bullets_peak)
        S.bullets_peak = n;
}

static void shots_update(void)
{
    for (int i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = &S.sh[i];
        if (!s->alive)
            continue;
        s->pos = m_add(s->pos, s->vel);
        if (--s->life == 0)
        {
            s->alive = false;
            continue;
        }
        int32_t hit_r = s->power > 1 ? F(0.9) : F(0.35);
        for (int j = 0; j < MAX_ENEMIES; j++)
        {
            Enemy *e = &S.en[j];
            if (!e->alive || e->type == E_BOSS)
                continue;
            if (m_abs(e->pos.z - s->pos.z) > edef[e->type].radius + F(1))
                continue;
            if (m_dist(e->pos, s->pos) < edef[e->type].radius + hit_r)
            {
                enemy_damage(j, s->power);
                if (s->power <= 1)
                    s->alive = false;
                break;
            }
        }
        if (s->alive && S.boss_active)
        {
            int sx = -1, sy = -1;
            g3_project(s->pos, &sx, &sy, NULL);
            if (boss_hit_test(s->pos, sx, sy, s->power))
                s->alive = s->power > 1;
        }
    }

    for (int i = 0; i < MAX_MISSILES; i++)
    {
        Missile *m = &S.mi[i];
        if (!m->alive)
            continue;
        CvVec target;
        bool boss_target = m->target < 0;
        if (boss_target)
            target = boss_pos();
        else if (S.en[m->target].alive)
            target = S.en[m->target].pos;
        else
            target = m_add(m->pos, m_vec(0, 0, -F(4)));
        CvVec want = m_norm(m_sub(target, m->pos), F(0.32));
        m->vel = m_vec(m->vel.x + (want.x - m->vel.x) / 6, m->vel.y + (want.y - m->vel.y) / 6,
                       m->vel.z + (want.z - m->vel.z) / 6);
        m->pos = m_add(m->pos, m->vel);
        if (--m->life == 0)
        {
            m->alive = false;
            continue;
        }
        if (!boss_target && S.en[m->target].alive && m_dist(m->pos, target) < F(0.8))
        {
            S.en[m->target].locked = false;
            enemy_damage(m->target, 6);
            blast_spawn(m->pos, 1);
            m->alive = false;
        }
        else if (boss_target && boss_hit_test(m->pos, -1, -1, 6))
        {
            blast_spawn(m->pos, 1);
            m->alive = false;
        }
    }
}

static void particles_update(void)
{
    Player *pl = &S.pl;
    // Engine exhaust: two flickering flames, hotter and longer while boosting.
    if (!pl->dead_timer)
    {
        int32_t back = F(0.62);
        for (int s = -1; s <= 1; s += 2)
        {
            CvVec at = m_add(pl->pos, m_vec(s * F(0.1), F(0.03), back));
            u16 c = pl->boost ? RGB15(16, 24, 31) : ((S.frame + s) & 2 ? RGB15(31, 22, 6) : RGB15(31, 12, 2));
            particle(at, m_vec(rng_range(-F(0.01), F(0.01)), rng_range(-F(0.01), F(0.01)),
                               pl->boost ? F(0.12) : F(0.05)),
                     c, pl->boost ? 16 : 10, pl->boost ? F(0.24) : F(0.16), -F(0.012), PK_GLOW);
        }
    }
    // Speed streaks rushing past in the open stages.
    if (!(S.data.flags & ST_FLAG_CORRIDOR) && (S.frame % (pl->boost ? 1 : 3)) == 0)
    {
        CvVec at = m_add(pl->pos, m_vec(rng_range(-F(7), F(7)), rng_range(-F(2), F(5)), -F(22)));
        particle(at, m_vec(0, 0, F(0.45)), (S.data.flags & ST_FLAG_SPACE) ? RGB15(22, 24, 31)
                                                                        : RGB15(31, 31, 31),
                 50, F(0.9), 0, PK_STREAK);
    }
    // Missile smoke trails.
    for (int i = 0; i < MAX_MISSILES; i++)
        if (S.mi[i].alive && (S.frame & 1))
            particle(S.mi[i].pos, m_vec(0, F(0.005), 0), RGB15(24, 24, 26), 24, F(0.15),
                     F(0.015), PK_SMOKE);

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &S.pa[i];
        if (!p->life)
            continue;
        p->life--;
        p->pos = m_add(p->pos, p->vel);
        p->vel = m_vec(p->vel.x - p->vel.x / 16, p->vel.y - p->vel.y / 16,
                       p->kind == PK_STREAK ? p->vel.z : p->vel.z - p->vel.z / 16);
        p->size += p->grow;
        if (p->size < F(0.02))
            p->life = 0;
    }
}

static void effects_update(void)
{
    particles_update();
    for (int i = 0; i < MAX_BLASTS; i++)
    {
        Blast *b = &S.bl[i];
        if (b->alive && ++b->frame >= 24)
            b->alive = false;
    }
    for (int i = 0; i < MAX_DEBRIS; i++)
    {
        Debris *d = &S.de[i];
        if (!d->alive)
            continue;
        d->pos = m_add(d->pos, d->vel);
        d->vel.y -= F(0.004);
        d->spin += 900;
        if (--d->life == 0)
            d->alive = false;
    }
}

// ---------------------------------------------------------------------------
// Rendering

static void camera_update(void)
{
    Player *p = &S.pl;
    CvVec eye, target;
    if (S.data.flags & ST_FLAG_CORRIDOR)
    {
        eye = m_add(p->pos, m_vec(CV_FXMUL(F(2.4), m_sin(S.cam_yaw)), F(0.9),
                                  CV_FXMUL(F(3.2), m_cos(S.cam_yaw))));
        target = m_add(p->pos, m_vec(0, F(0.2), -F(4)));
    }
    else
    {
        eye = m_add(p->pos, m_vec(CV_FXMUL(F(4.5), m_sin(S.cam_yaw)), F(2.2),
                                  CV_FXMUL(F(4.5), m_cos(S.cam_yaw))));
        target = m_add(p->pos, m_vec(0, F(0.3), -F(3)));
    }
    g3_camera(eye, target, 62, F(48));
}

static void draw_enemy(Enemy *e)
{
    int mdl = edef[e->type].model;
    if (mdl < 0)
        return;
    CvTex tex = TEX_COUNT;
    u32 extra = 0;
    int alpha = 31;
    switch (e->type)
    {
        case E_TURRET:  tex = TEX_STRIPES; break;
        case E_WALLGUN: tex = TEX_STRIPES; break;
        case E_FIGHTER: tex = TEX_STRIPES; break;
        case E_DRONE:   tex = TEX_HULL; break;
        case E_AIRSHIP: tex = TEX_HULL; alpha = 20; break;
        case E_STATION: extra = POLY_TOON_HIGHLIGHT; break;
        default: break;
    }
    g3_texture_bind(tex);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_BACK | POLY_ID(e->type == E_STATION ? 8 : 12 + (e - S.en) % 4)
              | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1 | POLY_FORMAT_LIGHT2 | POLY_FORMAT_LIGHT3
              | extra | ((S.data.flags & ST_FLAG_SPACE) ? 0 : POLY_FOG)
              | (alpha < 31 ? POLY_TRANS_SET_DEPTH : 0));
    g3_push_at(e->pos);
    if (e->type == E_TURRET || e->type == E_FIGHTER || e->type == E_WALLGUN)
    {
        CvVec d = m_sub(S.pl.pos, e->pos);
        g3_rot_y(m_atan2(-d.x, -d.z));
    }
    else
    {
        g3_rot_y(e->angle);
    }
    if (e->flash)
        glScalef32(F(1.15), F(1.15), F(1.15));
    g3_draw(&cv_models[mdl]);
    g3_pop();
}

static void draw_player(void)
{
    Player *p = &S.pl;
    if (p->dead_timer || (p->invuln && (S.frame & 4)))
        return;
    g3_texture_bind(TEX_COUNT);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(16)
              | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1 | POLY_FORMAT_LIGHT2
              | POLY_FORMAT_LIGHT3);
    g3_push_at(p->pos);
    g3_rot_z(p->bank + p->roll);
    g3_draw(&cv_models[MDL_PLAYER]);
    g3_pop();
}

static void draw_translucent(void)
{
    // Shots: glowing A3I5 billboards. Stage 3 has no texture VRAM (the
    // nebula owns it), so shots there are small translucent quads.
    bool space = (S.data.flags & ST_FLAG_SPACE) && S.stage == 3;
    g3_texture_bind(TEX_GLOW);
    for (int i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = &S.sh[i];
        if (s->alive)
            bb_draw(s->pos, space ? (s->power > 1 ? F(0.3) : F(0.08)) : (s->power > 1 ? F(0.7) : F(0.22)),
                    s->power > 1 ? RGB15(31, 28, 10) : RGB15(20, 31, 31), space ? 20 : 31, 50, 16);
    }
    // Particles. Different polygon IDs per particle so overlapping
    // translucent quads keep blending.
    for (int kind = 0; kind < 3; kind++)
    {
        if (space && kind != PK_STREAK)
            continue;   // stage 3 draws its glow particles as sprites
        CvTex bound = kind == PK_GLOW ? TEX_GLOW : (kind == PK_SMOKE ? TEX_SMOKE : TEX_COUNT);
        g3_texture_bind(bound);
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            Particle *p = &S.pa[i];
            if (!p->life || p->kind != kind)
                continue;
            // Small sparks keep the A3I5 glow; big flashes use the smoother
            // 5-bit-alpha flare so they don't show alpha steps.
            if (kind == PK_GLOW)
            {
                CvTex want = p->size > F(0.35) ? TEX_FLARE : TEX_GLOW;
                if (want != bound)
                {
                    g3_texture_bind(want);
                    bound = want;
                }
            }
            int alpha = 2 + 29 * p->life / p->max;
            if (kind == PK_STREAK)
            {
                // Thin streak along the flight axis.
                glPushMatrix();
                glTranslatef32(p->pos.x, p->pos.y, p->pos.z);
                glPolyFmt(POLY_ALPHA(alpha > 12 ? 12 : alpha) | POLY_CULL_NONE | POLY_ID(33 + (i & 15)));
                GFX_COLOR = p->color;
                glBegin(GL_QUADS);
                glVertex3v16(0, 0, 0);
                glVertex3v16(F(0.02), F(0.02), 0);
                glVertex3v16(F(0.02), F(0.02), p->size);
                glVertex3v16(0, 0, p->size);
                glEnd();
                glPopMatrix(1);
                continue;
            }
            bb_draw(p->pos, p->size, p->color, alpha, 33 + (i % 24), bound == TEX_GLOW ? 16 : 32);
        }
    }
    if (space)
        return;
    for (int i = 0; i < MAX_MISSILES; i++)
        if (S.mi[i].alive)
            bb_draw(S.mi[i].pos, F(0.18), RGB15(31, 16, 8), 31, 51, 16);
    // Smoke: A5I3 billboards on big blasts.
    g3_texture_bind(TEX_SMOKE);
    for (int i = 0; i < MAX_BLASTS; i++)
    {
        Blast *b = &S.bl[i];
        if (b->alive && b->size > 1)
            bb_draw(m_add(b->pos, m_vec(0, b->frame * F(0.03), 0)),
                    F(0.6) + b->frame * F(0.06), RGB15(31, 31, 31), 26 - b->frame, 52 + (i & 7), 32);
    }
}

static void draw_debris(void)
{
    // Direct port writes; in the stage 5 rampage this deliberately runs the
    // polygon RAM past its 2048 limit.
    g3_texture_bind(TEX_COUNT);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(60));
    for (int i = 0; i < MAX_DEBRIS; i++)
    {
        Debris *d = &S.de[i];
        if (!d->alive)
            continue;
        g3_push_at(d->pos);
        g3_rot_x(d->spin);
        g3_rot_y(d->spin >> 1);
        GFX_COLOR = RGB15(31, 18 + (i & 7), 6);
        GFX_BEGIN = GX_QUADS;
        GFX_VERTEX16 = (u32)(F(-0.12) & 0xFFFF) | ((u32)(F(-0.05) & 0xFFFF) << 16);
        GFX_VERTEX16 = 0;
        GFX_VERTEX16 = (u32)(F(0.12) & 0xFFFF) | ((u32)(F(-0.05) & 0xFFFF) << 16);
        GFX_VERTEX16 = 0;
        GFX_VERTEX16 = (u32)(F(0.08) & 0xFFFF) | ((u32)(F(0.06) & 0xFFFF) << 16);
        GFX_VERTEX16 = 0;
        GFX_VERTEX16 = (u32)(F(-0.08) & 0xFFFF) | ((u32)(F(0.06) & 0xFFFF) << 16);
        GFX_VERTEX16 = 0;
        GFX_END = 0;
        g3_pop();
    }
}

static void draw_sprites(void)
{
    oam_clear(ENG_MAIN);
    int affine = 1;
    // Enemy bullets: 2D sprites at their projected 3D positions.
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &S.bu[i];
        int sx, sy;
        if (!b->alive || !g3_project(b->pos, &sx, &sy, NULL))
            continue;
        if (b->kind == 2)
            oam_add(ENG_MAIN, sx - 8, sy - 8, SPR_16x16, CV_SPR_BIGSHOT, 0, 0, 0, 0);
        else
            oam_add(ENG_MAIN, sx - 4, sy - 4, SPR_8x8,
                    b->kind ? CV_SPR_NEEDLE : CV_SPR_BULLET, 0, 0, 0, 0);
    }
    // Explosions; big ones scale up through an affine matrix.
    for (int i = 0; i < MAX_BLASTS; i++)
    {
        Blast *b = &S.bl[i];
        int sx, sy;
        if (!b->alive || !g3_project(b->pos, &sx, &sy, NULL))
            continue;
        int tile = CV_SPR_EXPLODE0 + (b->frame / 6) * (CV_SPR_EXPLODE1 - CV_SPR_EXPLODE0);
        if (b->size > 1 && affine < 31)
        {
            oam_affine(ENG_MAIN, affine, b->frame * 400, 256 + b->frame * 16, 256 + b->frame * 16);
            oam_add(ENG_MAIN, sx - 16, sy - 16, SPR_16x16, tile, 0, 1,
                    SPRF_AFFINE | SPRF_DOUBLE, affine);
            affine++;
        }
        else
        {
            oam_add(ENG_MAIN, sx - 8, sy - 8, SPR_16x16, tile, 0, 1, 0, 0);
        }
    }
    // Meteors: 8bpp affine sprites, recoloured by extended OBJ palettes.
    if (S.data.flags & ST_FLAG_SPACE)
    {
        for (int i = 0; i < MAX_ENEMIES && affine < 31; i++)
        {
            Enemy *e = &S.en[i];
            int sx, sy;
            int32_t depth;
            if (!e->alive || e->type != E_METEOR || !g3_project(e->pos, &sx, &sy, &depth))
                continue;
            int32_t scale = m_div(256 * 10, (depth >> 12) + 1);
            scale = m_clamp(scale, 48, 480);
            oam_affine(ENG_MAIN, affine, e->angle, scale, scale);
            oam_add(ENG_MAIN, sx - 32, sy - 32, SPR_32x32, CV_METEOR_TILE,
                    e->param & 3, 2, SPRF_AFFINE | SPRF_DOUBLE | SPRF_8BPP
                    | (e->flash ? SPRF_MOSAIC : 0), affine);
            affine++;
        }
    }
    if ((S.data.flags & ST_FLAG_SPACE) && S.stage == 3)
    {
        int n = 0;
        for (int i = 0; i < MAX_PARTICLES && n < 28; i++)
        {
            Particle *p = &S.pa[i];
            int sx, sy;
            if (!p->life || p->kind != PK_GLOW || !g3_project(p->pos, &sx, &sy, NULL))
                continue;
            oam_add(ENG_MAIN, sx - 4, sy - 4, SPR_8x8, CV_SPR_SPARK, 0, 1,
                    p->life < p->max / 2 ? SPRF_BLEND : 0, 0);
            n++;
        }
    }
    if (S.cutin)
    {
        // Pilot cut-in: 64x64 16bpp bitmap OBJ sliding in from the right.
        int x = 184 + (S.cutin > 60 ? (S.cutin - 60) * 8 : (S.cutin < 10 ? (10 - S.cutin) * 8 : 0));
        oam_add(ENG_MAIN, x, 8, SPR_64x64, CV_CUTIN_TILE, 15, 0, SPRF_BITMAP, 0);
    }
    if (S.swapped && S.pl.aiming)
        oam_add(ENG_MAIN, S.pl.aim_x - 8, S.pl.aim_y - 8, SPR_16x16, CV_SPR_CROSS, 0, 0, 0, 0);
    // Lock reticles over locked enemies.
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &S.en[i];
        int sx, sy;
        if (e->alive && e->locked && g3_project(e->pos, &sx, &sy, NULL))
            oam_add(ENG_MAIN, sx - 8, sy - 8, SPR_16x16, CV_SPR_LOCK, 0, 0,
                    (S.frame & 8) ? SPRF_BLEND : 0, 0);
    }
}

static void draw_hud(void)
{
    oam_clear(ENG_SUB);
    hud_frame();
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &S.en[i];
        if (!e->alive || e->type == E_BOSS)
            continue;
        hud_blip(e->pos.x - S.pl.pos.x, e->pos.z - S.pl.pos.z, 0, e->locked);
    }
    if (S.boss_active)
        boss_radar();
    hud_radar_scroll(S.pl.pos.x, S.pl.pos.z);

    CvHudState h = {
        .score = cv_game.score,
        .hiscore = cv_save.records[0].score > cv_game.score ? cv_save.records[0].score : cv_game.score,
        .lives = cv_game.lives,
        .bombs = cv_game.bombs,
        .charge = S.pl.charge,
        .charge_max = 60,
        .boss_hp = S.boss_active ? boss_hp() : 0,
        .boss_max = S.boss_active ? boss_hp_max() : 0,
        .stage = S.stage,
        .seconds = cv_game.stage_frames / 60,   // stage time: replays match
        .message = S.message_timer ? S.message : NULL,
    };
    hud_draw(&h);
}

static void render(void)
{
    camera_update();

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    if (S.data.flags & ST_FLAG_CORRIDOR)
        glTranslatef32(0, CV_FXI(S.frame & 31) << 4, 0);  // scrolling floor
    glMatrixMode(GL_MODELVIEW);

    if (S.data.flags & (ST_FLAG_TERRAIN | ST_FLAG_CORRIDOR))
    {
        if (S.data.flags & ST_FLAG_TERRAIN)
            g3_texture_bind(TEX_TERRAIN);
        terrain_draw(S.pl.pos.z, S.data.flags & ST_FLAG_CORRIDOR ? 18 : 26, true);
    }

    for (int i = 0; i < MAX_ENEMIES; i++)
        if (S.en[i].alive && S.en[i].type != E_AIRSHIP)
            draw_enemy(&S.en[i]);
    if (S.boss_active || S.boss_dead)
        boss_draw();
    draw_player();
    draw_debris();
    // Translucent last (manual sort order = submission order).
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (S.en[i].alive && S.en[i].type == E_AIRSHIP)
            draw_enemy(&S.en[i]);
    if (S.data.flags & ST_FLAG_CORRIDOR)
        shadow_volume(S.pl.pos, F(0.45), 0);
    draw_translucent();

    // Shake the 3D layer (BG0 scroll) when hit.
    if (S.hit_shake)
    {
        S.hit_shake--;
        REG_BG0HOFS = (S.hit_shake & 2) ? 3 : -3;
    }
    else
    {
        REG_BG0HOFS = 0;
    }

    draw_sprites();
    draw_hud();
}

// ---------------------------------------------------------------------------
// Pause

enum { PAUSE_RESUME, PAUSE_STEP, PAUSE_RETIRE, PAUSE_QA_CLEAR };

static void wake_noop(void) {}

static int pause_menu(void)
{
    timer_playtime_pause(true);
    snd_master(32);
    stage_message("PAUSED  SELECT:QUIT  R:STEP", 1);
    draw_hud();
    oam_commit();
    int idle = 0;
    int result = PAUSE_RESUME;
    for (;;)
    {
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
        // QA shortcut: hold L+R and press A to clear the stage at once.
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
    S.message_timer = 0;
    snd_master(cv_save.volume);
    timer_playtime_pause(false);
    return result;
}

// ---------------------------------------------------------------------------

static void stage_init(int n, const CvStageData *data)
{
    memset(&S, 0, sizeof(S));
    S.data = *data;
    S.stage = n;
    cv_game.stage_kills = cv_game.stage_spawned = cv_game.stage_frames = 0;
    cv_game.stage_start_score = cv_game.score;
    cv_game.cleared = false;

    rng_seed(cv_game.attract ? replay_seed() : 0xC0A5u + (u32)n * 7919u);

    Player *p = &S.pl;
    p->pos = m_vec(0, F(2.6), F(2));
    p->speed = F(0.07);
    p->invuln = 90;

    pl_xmin = -F(6);
    pl_xmax = F(6);
    pl_ymin = F(0.9);
    pl_ymax = F(5.2);
    if (S.data.flags & ST_FLAG_CORRIDOR)
    {
        pl_ymin = F(0.6);
        pl_ymax = F(4.2);
        p->pos.y = F(2.0);
    }
    if (S.data.flags & ST_FLAG_SPACE)
        pl_ymin = -F(1.5);
}

CvScene scene_stage(void)
{
    int n = cv_game.stage;
    CvStageData data;

    fx_bright(FX_BOTH, -16);
    if (!stage_load(n, &data))
        return SC_TITLE;

    stage_init(n, &data);

    // Input log first: playback must also replay the RTC hour that decides
    // the stage 2 sky, which sfx_setup() reads.
    bool replaying = cv_game.attract || cv_game.playback;
    if (cv_game.attract)
    {
        u32 size;
        char name[16];
        snprintf(name, sizeof(name), "demo%d.rpl", n);
        static void *demo_blob;
        free(demo_blob);
        demo_blob = nitro_load(name, &size);
        if (!demo_blob || !replay_play_start(demo_blob, size))
            return SC_TITLE;
    }
    else if (cv_game.playback)
    {
        if (!replay_play_start(cv_replay_buf, cv_replay_size))
            return SC_TITLE;
        cv_game.lives = replay_lives();
        cv_game.bombs = replay_bombs();
    }
    else
    {
        replay_record_start(0xC0A5u + (u32)n * 7919u, n, cv_game.lives, cv_game.bombs);
    }
    rng_seed(replaying ? replay_seed() : 0xC0A5u + (u32)n * 7919u);

    sfx_setup(n);
    hud_init(n);
    snd_bgm(data.song);
    timer_playtime_start(cv_save.total_playtime & 0xFFFF);

    if (cv_game.attract)
        stage_message("DEMONSTRATION", 1200);
    else if (cv_game.playback)
        stage_message("REPLAY", 1200);
    else
        stage_message(S.swapped ? "TOUCH THE SCREEN TO AIM" : "TOUCH RADAR TO LOCK ON", 180);

    for (int f = 0; f < 16; f++)
    {
        fx_bright(FX_BOTH, -16 + f);
        render();
        g3_flush(S.wbuffer);
        oam_commit();
        sys_vsync();
    }
    fx_bright(FX_BOTH, 0);

    CvScene next = SC_RESULT;
    bool step_mode = false;
    for (;;)
    {
        // Closing the lid pauses the game, then sleeps until it opens.
        bool lid_pause = cv_lid_closed && !replaying;
        sys_handle_lid();
        input_update();
        cv_game.stage_frames++;

        if (replaying)
        {
            // Any real key ends the demonstration / replay.
            if ((~REG_KEYINPUT & 0x3FF) || replay_finished()
                || (cv_game.attract && S.frame > 1100))
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
                next = SC_RESULT;
                break;
            }
            step_mode = r == PAUSE_STEP;
            if (step_mode)
                input_update();
        }
        if (!replaying && (cv_in.pressed & KEY_SELECT))
            hud_console_toggle();

        timer_prof_begin(false);
        S.frame++;
        if (S.message_timer)
            S.message_timer--;

        spawn_from_script();
        player_update();
        enemies_update();
        if (S.boss_active || S.boss_dead)
            boss_update();
        bullets_update();
        shots_update();
        effects_update();
        sfx_frame();

        timer_prof_begin(true);
        render();
        u32 render_us = timer_prof_end_us();

        if (S.overload)
        {
            // 3D limits: score keeps flowing while the hardware drops
            // polygons past the 2048 limit.
            if (g3_overflowed() || g3_poly_count() >= 2040)
            {
                cv_game.overload_bonus += 50;
                cv_game.score += 50;
                stage_message("OVERLOAD BONUS", 2);
            }
        }

        if (hud_console_on())
        {
            hud_console_print(0, "FRAME %5lu  RENDER %4luUS", (unsigned long)S.frame,
                              (unsigned long)render_us);
            hud_console_print(1, "POLY %4d VTX %4d", g3_poly_count(), g3_vert_count());
            hud_console_print(2, "BULLETS PEAK %3lu", (unsigned long)S.bullets_peak);
            hud_console_print(3, "BEATS %5lu RNG %08lX", (unsigned long)cv_beats,
                              (unsigned long)rng_state());
            hud_console_print(4, "Z %6ld X %5ld Y %5ld", (long)(S.pl.pos.z >> 12),
                              (long)(S.pl.pos.x >> 8), (long)(S.pl.pos.y >> 8));
        }

        g3_flush(S.wbuffer);
        oam_commit();
        sys_vsync();
        // Bullet palette cycling, written during VBlank: rims and cores pulse.
        {
            static const u16 rim_pink[4] = { RGB15(31, 5, 15), RGB15(31, 12, 20), RGB15(31, 20, 26), RGB15(31, 12, 20) };
            static const u16 rim_blue[4] = { RGB15(5, 15, 31), RGB15(12, 22, 31), RGB15(20, 28, 31), RGB15(12, 22, 31) };
            static const u16 core[4] = { RGB15(31, 31, 31), RGB15(31, 28, 31), RGB15(31, 31, 22), RGB15(31, 28, 31) };
            int k = (S.frame >> 2) & 3;
            CV_PAL_OBJ_MAIN[3] = rim_pink[k];
            CV_PAL_OBJ_MAIN[5] = rim_blue[k];
            CV_PAL_OBJ_MAIN[1] = core[k];
            CV_PAL_OBJ_MAIN[7] = (S.frame & 4) ? RGB15(31, 20, 4) : RGB15(31, 12, 2);
        }
        fx_mosaic(FX_MAIN, 0, 0);
        fx_bright(FX_MAIN, S.flash > 0 ? S.flash-- : 0);
        if (S.cutin)
            S.cutin--;

        if (S.pl.dead_timer == 1)
        {
            next = SC_CONTINUE;
            break;
        }
        if (S.boss_dead && ++S.clear_timer > 200)
        {
            cv_game.cleared = true;
            next = SC_RESULT;
            break;
        }
        // Safety: the run ends if the arena is reached with no boss alive.
        if (!S.boss_active && !S.boss_dead && S.spawn_index >= S.data.num_spawns
            && (-S.pl.pos.z >> 12) > S.data.rows + 4)
        {
            cv_game.cleared = true;
            next = SC_RESULT;
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
    if (cv_game.playback && next != SC_RECORDS)
        next = SC_RECORDS;
    cv_game.playback = false;

    cv_save.total_playtime = timer_playtime();
    fx_transition_out(24);
    sfx_teardown();
    snd_echo(false);
    snd_stream_stop();
    timer_beat_stop();
    stage_free(&S.data);
    oam_clear(ENG_MAIN);
    oam_clear(ENG_SUB);
    oam_flush_now();
    if (cv_game.attract)
        cv_game.attract = false;
    return next;
}
