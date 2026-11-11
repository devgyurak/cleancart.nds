// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Scene functions: each runs until it returns the next scene.

#ifndef LT_SCENES_H
#define LT_SCENES_H

typedef enum {
    SC_TITLE,
    SC_JUDGE,
    SC_ACT1,
    SC_ACT2,
    SC_MENU,
    SC_REHEARSAL,
} LtScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

LtScene scene_title(void);
LtScene scene_judge(void);
LtScene scene_act1(void);
LtScene scene_act2(void);
LtScene scene_menu(void);
LtScene scene_rehearsal(void);

#endif // LT_SCENES_H
