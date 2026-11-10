// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Procedural meshes, compiled once into geometry-FIFO display lists.

#ifndef CV_MODELS_H
#define CV_MODELS_H

#include "gfx/gl3d.h"

typedef enum {
    MDL_PLAYER,
    MDL_TURRET,
    MDL_DRONE,
    MDL_AIRSHIP,
    MDL_FIGHTER,
    MDL_STATION,
    MDL_WALLGUN,
    MDL_BOSS_CORE,
    MDL_BOSS_ARM,
    MDL_BOSS_CLAW,
    MDL_BOSS_SHIELD,
    MDL_BOSS_PANEL,
    MDL_FORT,       // stage 1 boss: coastal fortress gun
    MDL_FLAGSHIP,   // stage 2 boss
    MDL_DEBRIS,
    MDL_CUBE,
    MDL_COUNT
} CvModelId;

extern CvModel cv_models[MDL_COUNT];

// Builds every model. `player_color` comes from the firmware's favourite
// colour setting.
void models_build(u16 player_color);

#endif // CV_MODELS_H
