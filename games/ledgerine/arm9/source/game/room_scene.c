// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The room scene: the machine's ledger on the top screen, dials and lever on the
// bottom one. Closing the books runs the room's audit and goes on to the next room.

#include <nds.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/ledger.h"
#include "game/rooms.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

LgScene scene_room(void)
{
    CvText main_text, sub_text;
    const Room *room = lg_rooms[lg_room];
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);

    text_printf(&main_text, 2, 2, TP_YELLOW, "ROOM %d  %s", lg_room + 1, room->name);
    text_printf(&main_text, 2, 4, TP_GRAY, "%s", room->tower);
    text_printf(&main_text, 2, 5, TP_GRAY, "%s", room->gbatek);
    text_center(&sub_text, 10, TP_GRAY, "A: CLOSE THE BOOKS");
    text_center(&sub_text, 12, TP_GRAY, "B: BACK TO THE TITLE");

    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & KEY_A)
        {
            snd_sfx(CV_SFX_STAMP, 110, 64);
            room->audit();
            lg_res[LV_DONE] |= 1u << lg_room;
            if (lg_room + 1 < LG_ROOMS)
            {
                lg_room++;
                return SC_ROOM;
            }
            return SC_CREDITS;
        }
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
