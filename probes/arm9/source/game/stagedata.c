// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Stage data and the actor helpers the boss uses (copied from Corvane's
// game/stage.c: the stage loop itself is not part of the probes). The 3d-fifo,
// 3d-clip and 3d-matrix probes load stage files and run the boss through them.

#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/cart.h"
#include "core/cvmath.h"
#include "core/hw.h"
#include "core/nitro.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/game.h"
#include "game/stage.h"

CvStage S;

#define F CV_FX

typedef struct {
    int hp, score;
} EnemyDef;

static const EnemyDef edef[] = {
    [E_TURRET]  = { 3, 100 },
    [E_DRONE]   = { 2, 150 },
    [E_AIRSHIP] = { 30, 1500 },
    [E_METEOR]  = { 4, 80 },
    [E_STATION] = { 40, 2000 },
    [E_WALLGUN] = { 4, 200 },
    [E_FIGHTER] = { 2, 250 },
    [E_BOSS]    = { 1, 0 },
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
