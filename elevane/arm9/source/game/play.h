// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#ifndef EV_PLAY_H
#define EV_PLAY_H

#include <stdbool.h>

#include "game/puzzle.h"

// Which puzzle scene_play() starts with (set by the puzzle list) and which view mode.
extern int ev_chapter, ev_index;
extern bool ev_view3d;   // false: 2D grid only, no 3D at all

// The structure being built, for the showcase mode.
const PzGrid *play_grid(void);
const PzPart *play_part(void);

#endif // EV_PLAY_H
