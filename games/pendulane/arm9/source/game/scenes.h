// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Scene functions: each runs until it returns the next scene.

#ifndef PD_SCENES_H
#define PD_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_BENCH,
    SC_CREDITS,
} PdScene;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

PdScene scene_title(void);
PdScene scene_bench(void);
PdScene scene_credits(void);

#endif // PD_SCENES_H
