// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Procedural meshes, compiled once into geometry-FIFO display lists.

#ifndef CV_MODELS_H
#define CV_MODELS_H

#include "gfx/gl3d.h"

typedef enum {
    MDL_M_TORSO,      // the creature, drawn as a hierarchy of parts
    MDL_M_HEAD,
    MDL_M_ARM,
    MDL_M_FOREARM,
    MDL_M_LEG,
    MDL_KEY,
    MDL_TAPE,
    MDL_FLAREBOX,
    MDL_BATTERY,
    MDL_RADIO,
    MDL_BED,
    MDL_CAR,
    MDL_PEW,
    MDL_ALTAR,
    MDL_PROJECTOR,
    MDL_WELL,
    MDL_LAMP,
    MDL_GATE,
    MDL_CUBE,
    MDL_COUNT
} CvModelId;

extern CvModel cv_models[MDL_COUNT];

void models_build(void);

#endif // CV_MODELS_H
