// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Scene functions: each runs until it returns the next scene.

#ifndef EV_SCENES_H
#define EV_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_CREDITS,
} EvScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

EvScene scene_title(void);
EvScene scene_credits(void);

#endif // EV_SCENES_H
