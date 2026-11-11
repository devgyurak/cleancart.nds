// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Scene functions: each runs until it returns the next scene.

#ifndef LT_SCENES_H
#define LT_SCENES_H

typedef enum {
    SC_TITLE,
    SC_JUDGE,
} LtScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

LtScene scene_title(void);
LtScene scene_judge(void);

#endif // LT_SCENES_H
