// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Scene functions: each runs until it returns the next scene.

#ifndef HM_SCENES_H
#define HM_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_SELECT,
    SC_PLAY,
    SC_CREDITS,
} HmScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up the 3D top screen (BG3 text over BG0 3D, GL state at the defaults of
// the loom scenes) and a text bottom screen.
void display_3d_top(CvText *top, CvText *bot);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

HmScene scene_title(void);
HmScene scene_select(void);
HmScene scene_play(void);
HmScene scene_credits(void);

#endif // HM_SCENES_H
