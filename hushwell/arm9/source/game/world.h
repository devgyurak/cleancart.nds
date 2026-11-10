// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Shared state of a running chapter (world.c, render.c, chfx.c, hud.c).

#ifndef CV_WORLD_H
#define CV_WORLD_H

#include <nds.h>
#include <stdbool.h>

#include "core/cvmath.h"

// Map file (tools/cvtools/maps.py)
enum { CELL_FLOOR, CELL_WALL, CELL_WATER, CELL_GROUND, CELL_FENCE, CELL_DOOR, CELL_SHELF };

enum {
    O_START, O_EXIT, O_DOOR, O_KEY, O_RADIO, O_TAPE, O_FLARES, O_BATTERY,
    O_MONSTER, O_LAMP, O_PROJECTOR, O_CAR, O_PEW, O_BED, O_TREE, O_WELL,
    O_VISITOR, O_SWARM, O_ALTAR, O_CODEDOOR,
};

#define MF_OUTDOOR 1
#define MF_RAIN    2
#define MF_WATER   4
#define MF_FILM    8
#define MF_NIGHT   16
#define MF_WELL    32
#define MF_PHANTOM 64

#define KEY_CHASE 3   // key z: picking it up starts the chase

// Main OBJ VRAM: sprite sheet at 0, photograph bitmap at 8 KB (128-byte units).
#define CV_PHOTO_TILE 64

#define MAP_MAX   32
#define MAX_OBJS  64

typedef struct {
    u8 type, x, y, param;
} CvObj;

typedef struct {
    void *blob;
    int chapter, w, h, nobj, flags, song;
    u16 fog, clear;
    int ambient, facing;
    u8 cells[MAP_MAX * MAP_MAX];   // mutable copy: doors open
    const CvObj *obj;
} CvMap;

// Monster states
enum { MS_GONE, MS_WANDER, MS_HUNT, MS_CHASE, MS_STUN, MS_RETREAT, MS_PASS };

typedef struct {
    bool active;
    CvVec pos;       // feet on the floor
    int yaw;
    int state, timer;
    int tx, ty;      // target cell
    int nx, ny;      // next cell on the path
    int repath;
    int anim;
    int attack_cool;
    int lost;        // frames without hearing or seeing the player
    int32_t speed;
    int growl;
} Monster;

typedef struct {
    CvVec pos;       // eye position
    int yaw, pitch;
    int32_t bob;
    int step;        // footstep phase
    bool hush, run, aim;
    int noise;       // loudness of the last sound the player made (0-10)
    int hurt;        // red flash frames
    int dead;        // death animation frames
    bool light;      // flashlight on
    int flicker;
} Player;

typedef struct {
    bool alive;
    CvVec pos, vel;
    int life;        // burning frames left once it lands
    bool landed;
} Flare;

enum { PK_STEAM, PK_SPARK, PK_MOTE, PK_BUG };

typedef struct {
    CvVec pos, vel;
    int32_t size;
    u16 color;
    u8 life, max, kind;
} Particle;

#define MAX_FLARES    2
#define MAX_PARTICLES 96
#define MAX_PINS      12

typedef struct {
    CvMap map;
    int chapter;
    Player pl;
    Monster mo;
    Monster visitor;          // chapter 4 night visitor (harmless)
    Flare fl[MAX_FLARES];
    Particle pa[MAX_PARTICLES];
    int pa_next;
    int frame;
    bool chase;
    bool night;
    bool wbuffer;
    bool swapped;
    int fear;                 // 0-100, from the monster's distance and state
    int threat;               // 0-100 radio static level
    const char *message;
    int message_timer;
    int lightning;            // flash frames
    int peek;                 // keyhole peek frames
    CvVec peek_eye;
    int peek_yaw;
    bool film_seen;
    bool code_mode;           // chapter 3 code lock on the touch screen
    int code_digits[3];
    int code_door;            // object index
    bool recorder;            // bottom screen shows the recorder
    int tape_playing;         // frames left
    bool tape_reverse;
    bool debug_lights, debug_mask_off, console;
    int pins_x[MAX_PINS], pins_y[MAX_PINS];
    int npins;
    u8 explored[MAP_MAX * MAP_MAX];
    int swarm_count;          // chapter 5 bugs drawn per frame
    bool overflow_seen;
    int at_well;              // chapter 5: frames standing at the well
    int lamp_flicker;
    bool look_touch;          // from the run start (settings or replay)
    int night_mode;
} CvWorld;

extern CvWorld W;

// Code of the chapter 3 lock: heard on the reversed tape.
#define CODE_A 4
#define CODE_B 1
#define CODE_C 7

// world.c
bool world_cell_solid(int x, int y);
int  world_cell(int x, int y);
bool world_obj_taken(int i);
void world_open_door(int obj);
bool world_load_demo(int chapter);   // map + state only, for feature demos
void world_message(const char *msg, int frames);
void particle(CvVec pos, CvVec vel, u16 color, int life, int32_t size, int kind);

// render.c
void render_init(void);
void render_world(void);
void render_camera(CvVec eye, int yaw, int pitch);
int  render_cells_drawn(void);
void render_creature(const Monster *m, bool visitor);

// chfx.c: per-chapter hardware set-up
void chfx_setup(int chapter);
void chfx_frame(void);
void chfx_teardown(void);
bool chfx_dual(void);                 // chapter 5 chase: both screens 3D
void chfx_dual_draw(void);            // render the next view (after the update)
void chfx_dual_regs(void);            // right after VBlank: routing + capture
void chfx_pause(void);                // entering pause: stop the afterimage
void chfx_film(void);                 // chapter 3 projector
void chfx_text(void);                 // redraw the message line

// hud.c: bottom screen
void hud_init(void);
void hud_frame(void);
void hud_touch(void);
void hud_console_print(int line, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif // CV_WORLD_H
