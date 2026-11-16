// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Scene functions: each runs until it returns the next scene.

#ifndef PV_SCENES_H
#define PV_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_SELECT,
    SC_PLAY,
    SC_CREDITS,
} PvScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

PvScene scene_title(void);
PvScene scene_select(void);
PvScene scene_play(void);
PvScene scene_credits(void);

#endif // PV_SCENES_H
