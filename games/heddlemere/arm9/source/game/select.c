// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Order list: the chapters and their orders (every order is open from the start).

#include <nds.h>
#include <stdio.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/scenes.h"
#include "game/order.h"
#include "gfx/fx.h"
#include "gfx/text.h"

HmScene scene_select(void)
{
    CvText main_text, sub_text;
    scene_text_screens(&main_text, &sub_text);
    text_center(&sub_text, 6, TP_GRAY, "LEFT/RIGHT: CHAPTER");
    text_center(&sub_text, 8, TP_GRAY, "UP/DOWN: CHOOSE   A: START");
    text_center(&sub_text, 10, TP_GRAY, "B: BACK");
    fx_bright(FX_BOTH, 0);

    if (hm_chapter < 1 || hm_chapter > HM_CHAPTERS || order_chapter_size(hm_chapter) == 0)
        hm_chapter = 1;
    int sel = hm_index - 1;
    for (;;)
    {
        int n = order_chapter_size(hm_chapter);
        if (sel < 0 || sel >= n)
            sel = 0;
        text_clear(&main_text);
        char head[40];
        snprintf(head, sizeof(head), "CHAPTER %d  %s", hm_chapter, hm_chapter_name[hm_chapter - 1]);
        text_center(&main_text, 2, TP_YELLOW, head);
        if (n == 0)
            text_center(&main_text, 11, TP_GRAY, "NO ORDERS YET");
        for (int i = 0; i < n; i++)
        {
            HmOrder w;
            if (order_find(hm_chapter, i + 1, &w) >= 0)
            {
                char row[48];
                snprintf(row, sizeof(row), "%c %d-%02d %-15s", i == sel ? '>' : ' ', w.chapter, w.index, w.name);
                text_put(&main_text, 4, 5 + 2 * i, i == sel ? TP_YELLOW : TP_GRAY, row);
            }
        }
        scene_frame_end();
        if (cv_in.pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))
            snd_sfx(CV_SFX_CLICK, 80, 64);
        if (n > 0 && (cv_in.pressed & KEY_UP))
            sel = (sel + n - 1) % n;
        if (n > 0 && (cv_in.pressed & KEY_DOWN))
            sel = (sel + 1) % n;
        int step = (cv_in.pressed & KEY_RIGHT) ? 1 : (cv_in.pressed & KEY_LEFT) ? -1 : 0;
        for (int tries = 0; step && tries < HM_CHAPTERS; tries++)
        {
            hm_chapter = (hm_chapter - 1 + step + HM_CHAPTERS) % HM_CHAPTERS + 1;
            if (order_chapter_size(hm_chapter) > 0)
            {
                sel = 0;
                break;
            }
        }
        if (n > 0 && (cv_in.pressed & KEY_A))
        {
            hm_index = sel + 1;
            return SC_PLAY;
        }
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
