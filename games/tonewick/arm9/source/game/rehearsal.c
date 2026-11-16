// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/input.h"
#include "core/sdcard.h"
#include "core/sys.h"
#include "game/lab.h"
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

    lab_clear();
    tw_res[TV_LINES] = 0;
    for (int i = 0; i < TW_ROOMS; i++)
    {
        // The script solves every line with the GBATEK model (as the QA pilot does), then the district is audited.
        for (int p = 0; p < tw_rooms[i]->n_puzzles; p++)
        {
            int a, b;
            if (puzzle_solution(&tw_rooms[i]->puzzles[p], &a, &b))
                tw_res[TV_LINES]++;
        }
        tw_rooms[i]->audit();
        tw_res[TV_DONE] |= 1u << i;
        scene_frame_end();
    }
    for (int i = 0; i < TW_ROOMS; i++)
    {
        text_printf(&main_text, 2, 5 + 2 * i, TP_GRAY, "DISTRICT %d %-14s", i + 1, tw_rooms[i]->name);
        text_printf(&main_text, 28, 5 + 2 * i, TP_GREEN, "DONE");
    }
    scene_frame_end();
    tw_res[TV_DONE] |= TV_DONE_REHEARSAL;
    tw_res[TV_REHEARSED] = 1;
    lab_publish(REHEARSAL_ID, cv_frame);
}

void rehearsal_idle(void)
{
    sd_result_save("tonewick", "rehearsal");   // only with cleancart/tonewick/ on the card
    for (;;)
        scene_frame_end();
}

TwScene scene_rehearsal(void)
{
    rehearsal_run();
    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & (KEY_B | KEY_START))
            return SC_TITLE;
    }
}
