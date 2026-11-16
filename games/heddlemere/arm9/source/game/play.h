// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The play scene: one order at a time. D-pad up/down chooses a cell, left/right turns it one step
// (either way), A delivers the cloth when it matches the order, B returns to the list.

#ifndef HM_PLAY_H
#define HM_PLAY_H

#include <nds.h>
#include <stdbool.h>

#include "game/order.h"

typedef struct {
    HmOrder order;
    u8 cur[HM_NFIELDS];
    int sel;           // index into the list of turnable cells (play_cells())
    int moves;         // single steps taken
    bool done;         // the cloth is delivered
} PlayView;

void play_view(PlayView *v);

// The turnable cells of the chapter, row-major: fills `out` with flat indices, returns the count.
int play_cells(int chapter, u8 *out);

// The rehearsal and the QA pilot replace the input here, after the real input was read each frame.
extern void (*hm_play_hook)(void);

// Stars for a delivered order: 3 within par, 2 within par + 3, else 1.
int play_stars(int moves, int par);

#endif // HM_PLAY_H
