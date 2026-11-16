// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Scene functions: each runs until it returns the next scene.

#ifndef TW_SCENES_H
#define TW_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_ROOM,
    SC_MENU,
    SC_REHEARSAL,
    SC_CREDITS,
} TwScene;

// The room scene's argument: which district (0..TW_ROOMS-1) it plays next.
extern int tw_room;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

TwScene scene_title(void);
TwScene scene_room(void);
TwScene scene_menu(void);
TwScene scene_rehearsal(void);
TwScene scene_credits(void);

#endif // TW_SCENES_H
