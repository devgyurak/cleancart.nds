// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Scene functions: each runs until it returns the next scene.

#ifndef LG_SCENES_H
#define LG_SCENES_H

#include "gfx/text.h"

typedef enum {
    SC_TITLE,
    SC_ROOM,
    SC_MENU,
    SC_REHEARSAL,
    SC_CREDITS,
} LgScene;

// The room scene's argument: which room (0..LG_ROOMS-1) it plays next.
extern int lg_room;

// Resets both engines to a known state (no effects, windows, scroll, OAM).
void display_reset(void);

// One VBlank frame: OAM, vsync, lid, input.
void scene_frame_end(void);
// Resets the display, then sets up BG0 text on both engines.
void scene_text_screens(CvText *main_text, CvText *sub_text);

LgScene scene_title(void);
LgScene scene_room(void);
LgScene scene_menu(void);
LgScene scene_rehearsal(void);
LgScene scene_credits(void);

#endif // LG_SCENES_H
