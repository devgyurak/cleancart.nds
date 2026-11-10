// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Shared state of a running stage (stage.c, boss.c, stagefx.c).

#ifndef CV_STAGE_H
#define CV_STAGE_H

#include <nds.h>
#include <stdbool.h>

#include "core/cvmath.h"

// Stage file (tools/cvtools/stages.py)
#define ST_FLAG_TERRAIN  1
#define ST_FLAG_CORRIDOR 2
#define ST_FLAG_SPACE    4
#define ST_FLAG_SEA      8

typedef struct {
    u16 row;
    u8 type;
    s8 x, y;      // quarter units
    u8 pattern, count, param;
} __attribute__((packed)) CvSpawn;

typedef struct {
    void *blob;
    int number, rows, cols, flags, boss, song;
    u16 fog, clear;
    const u8 *heights;
    const CvSpawn *spawns;
    int num_spawns;
} CvStageData;

// Enemy types
enum { E_NONE, E_TURRET, E_DRONE, E_AIRSHIP, E_METEOR, E_STATION, E_WALLGUN,
       E_FIGHTER, E_BOSS };

// Bullet patterns
enum { P_AIMED, P_SPREAD3, P_RING, P_SPIRAL, P_NONE, P_RAIN };

#define MAX_ENEMIES 40
#define MAX_BULLETS 120
#define MAX_SHOTS   24
#define MAX_MISSILES 6
#define MAX_BLASTS  24
#define MAX_DEBRIS  480
#define MAX_PARTICLES 112

// Particle kinds
enum { PK_GLOW, PK_SMOKE, PK_STREAK };

typedef struct {
    CvVec pos, vel;
    int32_t size, grow;   // 20.12, grow per frame
    u16 color;
    u8 life, max, kind;
} Particle;

typedef struct {
    bool alive;
    u8 type, pattern, count, param;
    CvVec pos, vel;
    int hp, hp_max;
    int timer, fire_timer;
    int angle;
    int flash;
    bool locked;
    int score;
} Enemy;

typedef struct {
    bool alive;
    u8 kind;     // 0 round, 1 needle, 2 big
    CvVec pos, vel;
    u16 life;
} Bullet;

typedef struct {
    bool alive;
    CvVec pos, vel;
    int power, life;
} Shot;

typedef struct {
    bool alive;
    CvVec pos, vel;
    int target, life;
} Missile;

typedef struct {
    bool alive;
    CvVec pos;
    int frame, size;
} Blast;

typedef struct {
    bool alive;
    CvVec pos, vel;
    int spin, life;
} Debris;

typedef struct {
    CvVec pos;
    int32_t speed;
    int invuln;
    int charge, hold;
    int boost;
    int dead_timer;
    int roll;
    int shot_cool, missile_cool;
    int bank;      // visual roll angle
    bool aiming;   // stage 4 touch aim active
    int aim_x, aim_y;
} Player;

typedef struct {
    CvStageData data;
    int stage;
    Player pl;
    Enemy en[MAX_ENEMIES];
    Bullet bu[MAX_BULLETS];
    Shot sh[MAX_SHOTS];
    Missile mi[MAX_MISSILES];
    Blast bl[MAX_BLASTS];
    Debris de[MAX_DEBRIS];
    int spawn_index;
    int frame;
    int cam_yaw;
    bool night;
    bool boss_active, boss_dead;
    int boss_index;
    int clear_timer;
    int hit_shake;
    const char *message;
    int message_timer;
    bool swapped;         // stage 4: 3D on the touch screen
    bool wbuffer;
    int32_t arena_z;      // boss arena anchor
    u32 bullets_peak;
    bool overload;
    Particle pa[MAX_PARTICLES];
    int pa_next;          // ring allocator: the oldest particle is reused
    int flash;            // bomb flash (master brightness)
    int cutin;            // pilot cut-in (bitmap OBJ) timer
} CvStage;

// Main OBJ VRAM: sprite sheet at 0, 8bpp meteor at 4 KB, cut-in at 8 KB.
#define CV_METEOR_TILE  128   // 32-byte units
#define CV_CUTIN_TILE   64    // 128-byte bitmap units

extern CvStage S;

bool stage_load(int n, CvStageData *out);
void stage_free(CvStageData *d);

// Actors
int  enemy_spawn(int type, CvVec pos, int pattern, int count, int param);
void enemy_damage(int index, int dmg);
void bullet_fire(CvVec from, CvVec vel, int kind);
void bullet_aimed(CvVec from, int32_t speed, int spread_angle, int kind);
void blast_spawn(CvVec pos, int size);
void debris_burst(CvVec pos, int n);
void particle(CvVec pos, CvVec vel, u16 color, int life, int32_t size, int32_t grow, int kind);
void sparks(CvVec pos, int n, u16 color, int32_t speed);
void stage_message(const char *msg, int frames);

// Boss (boss.c)
void boss_spawn(int type, CvVec pos);
void boss_update(void);
void boss_draw(void);
bool boss_hit_test(CvVec shot_pos, int sx, int sy, int dmg);
void boss_radar(void);
int  boss_hp(void);
int  boss_hp_max(void);
CvVec boss_pos(void);

// Stage visuals (stagefx.c)
void sfx_setup(int stage);
void sfx_frame(void);
void sfx_teardown(void);

#endif // CV_STAGE_H
