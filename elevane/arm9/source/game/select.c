// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Puzzle list: chapter 1 for now (the chapters join as their puzzles land).

#include <nds.h>
#include <stdio.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/play.h"
#include "game/puzzle.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

EvScene scene_select(void)
{
    CvText main_text, sub_text;
    scene_text_screens(&main_text, &sub_text);
    text_center(&main_text, 3, TP_YELLOW, "CHOOSE AN ORDER");
    text_center(&sub_text, 8, TP_GRAY, "UP/DOWN: CHOOSE   A: START");
    text_center(&sub_text, 10, TP_GRAY, "B: BACK");
    fx_bright(FX_BOTH, 0);

    int n = pz_chapter_size(ev_chapter);
    if (n == 0)
        return SC_TITLE;
    int sel = ev_index - 1;
    if (sel < 0 || sel >= n)
        sel = 0;
    for (;;)
    {
        for (int i = 0; i < n; i++)
        {
            PzPuzzle p;
            if (!pz_find(ev_chapter, i + 1, &p))
            {
                char row[48];
                snprintf(row, sizeof(row), "%c %d-%02d %-14s", i == sel ? '>' : ' ', p.chapter, p.index, p.name);
                text_put(&main_text, 4, 7 + 2 * i, i == sel ? TP_YELLOW : TP_GRAY, row);
            }
        }
        scene_frame_end();
        if (cv_in.pressed & (KEY_UP | KEY_DOWN))
            snd_sfx(CV_SFX_CLICK, 80, 64);
        if (cv_in.pressed & KEY_UP)
            sel = (sel + n - 1) % n;
        if (cv_in.pressed & KEY_DOWN)
            sel = (sel + 1) % n;
        if (cv_in.pressed & KEY_A)
        {
            ev_index = sel + 1;
            return SC_PLAY;
        }
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
