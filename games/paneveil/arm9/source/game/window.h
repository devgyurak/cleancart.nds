// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The window orders (the `cv_windows` blob, tools/pvorder/pack.py) and the settings model:
// PV_NFIELDS settings, each cycling through a few values (assets.h: pv_field_count[]).
// A window is done when every setting equals the order's target; the cost of a window
// is the number of single steps taken (par: the fewest possible).

#ifndef PV_WINDOW_H
#define PV_WINDOW_H

#include <nds.h>
#include <stdbool.h>

#include "assets.h"

// Setting indices, in the order of tools/pvorder/model.py FIELDS.
enum {
    F_SIDE, F_TINT, F_MOTIF, F_REPEAT, F_FLIP, F_EDGE,
    F_COLOUR, F_CUTOUT, F_PETALS, F_RINGS, F_DECAL, F_GRAIN,
};

typedef struct {
    u8 chapter;               // 1-5
    u8 index;                 // 1-6 within the chapter
    u8 par;                   // fewest single steps from the start
    u8 target[PV_NFIELDS];
    char name[16];
} PvWindow;

#define PV_CHAPTERS 5

int win_count(void);                        // all windows
int win_chapter_size(int chapter);          // windows of a chapter (0 when none)
int win_find(int chapter, int index, PvWindow *out);   // index 1-based; returns the global 0-based number or -1

// Fewest single steps between two values of a setting (the cycle can be turned either way).
int win_dist(int field, int a, int b);
// Steps needed from `cur` to the target of `w`.
int win_remaining(const PvWindow *w, const u8 *cur);

// Windows baked (fired in the kiln) since the game started or the QA run began: bit n = window n (0-based).
extern u32 pv_baked_bits;
int win_baked_count(void);
bool win_was_baked(int chapter, int index);

// The window being played (set by the window list, read by the play scene) and the view mode.
extern int pv_chapter, pv_index;
extern bool pv_view3d;

#endif // PV_WINDOW_H
