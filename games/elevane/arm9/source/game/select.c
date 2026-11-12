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

static const char *const chapter_name[] = { "", "APPRENTICE", "WAREHOUSE", "CLOCK TOWER", "BRIDGES", "THE CITY" };

EvScene scene_select(void)
{
    CvText main_text, sub_text;
    scene_text_screens(&main_text, &sub_text);
    text_center(&sub_text, 6, TP_GRAY, "LEFT/RIGHT: CHAPTER");
    text_center(&sub_text, 8, TP_GRAY, "UP/DOWN: CHOOSE   A: START");
    text_center(&sub_text, 10, TP_GRAY, "B: BACK");
    fx_bright(FX_BOTH, 0);

    if (ev_chapter < 1 || ev_chapter > 5 || pz_chapter_size(ev_chapter) == 0)
        ev_chapter = 1;
    int sel = ev_index - 1;
    for (;;)
    {
        int n = pz_chapter_size(ev_chapter);
        if (sel < 0 || sel >= n)
            sel = 0;
        text_clear(&main_text);
        char head[40];
        snprintf(head, sizeof(head), "CHAPTER %d  %s", ev_chapter, chapter_name[ev_chapter]);
        text_center(&main_text, 2, TP_YELLOW, head);
        for (int i = 0; i < n; i++)
        {
            PzPuzzle p;
            if (pz_find(ev_chapter, i + 1, &p) >= 0)
            {
                char row[48];
                snprintf(row, sizeof(row), "%c %d-%02d %-14s", i == sel ? '>' : ' ', p.chapter, p.index, p.name);
                text_put(&main_text, 4, 5 + 2 * i, i == sel ? TP_YELLOW : TP_GRAY, row);
            }
        }
        scene_frame_end();
        if (cv_in.pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))
            snd_sfx(CV_SFX_CLICK, 80, 64);
        if (cv_in.pressed & KEY_UP)
            sel = (sel + n - 1) % n;
        if (cv_in.pressed & KEY_DOWN)
            sel = (sel + 1) % n;
        int step = (cv_in.pressed & KEY_RIGHT) ? 1 : (cv_in.pressed & KEY_LEFT) ? -1 : 0;
        for (int tries = 0; step && tries < 5; tries++)
        {
            ev_chapter = (ev_chapter - 1 + step + 5) % 5 + 1;
            if (pz_chapter_size(ev_chapter) > 0)
            {
                sel = 0;
                break;
            }
        }
        if (cv_in.pressed & KEY_A)
        {
            ev_index = sel + 1;
            return SC_PLAY;
        }
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
