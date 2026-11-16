// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The play scene: one window at a time. D-pad up/down chooses a setting, left/right turns it one
// step (either way), A fires the window in the kiln when it matches the order, B returns to the
// list, Select switches between the 3D window and the 2D schematic (both show and solve the same
// state).

#ifndef PV_PLAY_H
#define PV_PLAY_H

#include <nds.h>
#include <stdbool.h>

#include "game/window.h"

typedef struct {
    PvWindow win;
    u8 cur[PV_NFIELDS];
    int sel;           // the chosen setting
    int moves;         // single steps taken
    bool baked;        // the window is fired
} PlayView;

void play_view(PlayView *v);

// The rehearsal replaces the input here, after the real input was read each frame.
extern void (*pv_play_hook)(void);

// Stars for a finished window: 3 within par, 2 within par + 3, else 1.
int play_stars(int moves, int par);

#endif // PV_PLAY_H
