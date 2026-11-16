// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The slice of Corvane's game state that the boss and the actor helpers touch.

#ifndef CV_GAME_H
#define CV_GAME_H

#include <nds.h>
#include <stdbool.h>

#define CV_STAGES 5

typedef struct {
    int stage;            // 1-5
    u32 score;
    int lives;
    int bombs;
    u32 stage_kills, stage_spawned;
} CvGame;

extern CvGame cv_game;

#endif // CV_GAME_H
