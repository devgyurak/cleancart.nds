// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef EV_PLAY_H
#define EV_PLAY_H

#include <nds.h>
#include <stdbool.h>

#include "game/puzzle.h"

// Which puzzle scene_play() starts with (set by the puzzle list) and which view mode.
extern int ev_chapter, ev_index;
extern bool ev_view3d;   // false: 2D grid only, no 3D at all

// Hooks and read-outs for the rehearsal, the auto QA pilot and the showcase.
extern void (*ev_play_hook)(void);        // called once per frame after the real input was read
extern u64 ev_delivered_bits;             // bit n = puzzle n (file order) delivered since boot / the last QA reset
unsigned play_delivered_count(void);
bool play_was_delivered(int chapter, int index);
unsigned play_serial(void);               // changes when a puzzle is loaded
int play_mode(void);                      // 0 piles (stack), 1 layers
int play_nparts(void);
int play_part_index(void);
int play_layer(void);                     // work layer, -1 = TOP
void play_cursor(int *x, int *y);
bool play_matched(void);                  // every part matches its order
bool play_delivered(void);
int play_blocks(void);                    // all parts
int play_par(void);

// The part being built, for the showcase mode and the scripted player.
const PzGrid *play_grid(void);
const PzPart *play_part(void);
const PzGrid *play_grid_n(int k);
const PzPart *play_part_n(int k);

// Showcase presets and the numbers one run measures (the rehearsal block carries them).
enum { SHOW_HDMA, SHOW_REAR };
typedef struct {
    int polys[2], verts[2];      // left pass, right pass
    bool over[2];
    u32 holes_p, holes_q;        // preset 1: sub BG holes after capture-only / blend frames
    u32 left, right, hash;       // preset 1: opaque pixels left/right of x = 128 and the picture hash;
                                 // preset 2: left = opaque pixels (every 3rd) of the bank captured last
} ShowStats;
void showcase_run(int preset, int frames, ShowStats *st);

#endif // EV_PLAY_H
