// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Scene functions: each runs until it returns the next scene.

#ifndef LT_SCENES_H
#define LT_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_JUDGE,
    SC_ACT1,
    SC_ACT2,
    SC_ACT3,
    SC_ACT4,
    SC_ACT5,
    SC_MENU,
    SC_REHEARSAL,
    SC_CREDITS,
} LtScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

LtScene scene_title(void);
LtScene scene_judge(void);
LtScene scene_act1(void);
LtScene scene_act2(void);
LtScene scene_act3(void);
LtScene scene_act4(void);
LtScene scene_act5(void);
LtScene scene_menu(void);
LtScene scene_rehearsal(void);
LtScene scene_credits(void);

#endif // LT_SCENES_H
