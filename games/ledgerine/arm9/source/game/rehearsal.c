// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/input.h"
#include "core/sdcard.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "game/rehearsal.h"
#include "game/rooms.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

static CvText main_text, sub_text;

void rehearsal_run(void)
{
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);
    text_center(&main_text, 2, TP_YELLOW, "REHEARSAL");

    ledger_clear();
    for (int i = 0; i < LG_ROOMS; i++)
    {
        lg_rooms[i]->audit();
        lg_res[LV_DONE] |= 1u << i;
    }
    // The vault takes over the VRAM map while it runs and the screens are left in pieces: set them up again.
    scene_text_screens(&main_text, &sub_text);
    fx_bright(FX_BOTH, 0);
    text_center(&main_text, 2, TP_YELLOW, "REHEARSAL");
    for (int i = 0; i < LG_ROOMS; i++)
    {
        text_printf(&main_text, 2, 5 + 2 * i, TP_GRAY, "ROOM %d %-16s", i + 1, lg_rooms[i]->name);
        text_printf(&main_text, 28, 5 + 2 * i, TP_GREEN, "DONE");
    }
    scene_frame_end();
    lg_res[LV_DONE] |= LV_DONE_REHEARSAL;
    ledger_publish(REHEARSAL_ID, cv_frame);
}

void rehearsal_idle(void)
{
    sd_result_save("ledgerine", "rehearsal");   // only with cleancart/ledgerine/ on the card
    for (;;)
        scene_frame_end();
}

LgScene scene_rehearsal(void)
{
    rehearsal_run();
    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & (KEY_B | KEY_START))
            return SC_TITLE;
    }
}
