// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The contraption list: chapters left and right, contraptions up and down, A starts, B goes back to the title.

#include <nds.h>

#include "core/input.h"
#include "core/snd.h"
#include "game/play.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

static int first_unsolved(int chapter)
{
    for (int n = 1, size = dev_chapter_size(chapter); n <= size; n++)
        if (!dev_is_solved(dev_index(chapter, n)))
            return n;
    return 1;
}

PdScene scene_select(void)
{
    CvText top, bot;
    scene_text_screens(&top, &bot);
    pd_index = first_unsolved(pd_chapter);
    fx_bright(FX_BOTH, -16);
    for (int f = 0;; f++)
    {
        if (f < 20)
            fx_bright(FX_BOTH, -(16 * (19 - f) / 20));
        text_clear(&top);
        text_clear(&bot);
        text_center(&top, 1, TP_YELLOW, "THE WORKSHOP");
        text_printf(&top, 1, 3, TP_CYAN, "CHAPTER %d  %s", pd_chapter, pd_chapter_name[pd_chapter - 1]);
        text_printf(&top, 1, 4, TP_GRAY, "%d OF %d RUNG", dev_chapter_solved(pd_chapter), dev_chapter_size(pd_chapter));
        for (int n = 1, size = dev_chapter_size(pd_chapter); n <= size; n++)
        {
            int idx = dev_index(pd_chapter, n);
            const PdDevice *d = dev_get(idx);
            text_printf(&top, 2, 7 + n * 2, n == pd_index ? TP_YELLOW : TP_WHITE, "%c %-18s %s", n == pd_index ? '>' : ' ', d->name,
                        dev_is_solved(idx) ? "RUNG" : "");
        }
        text_center(&bot, 8, TP_GRAY, "LEFT RIGHT: CHAPTER   UP DOWN: DEVICE");
        text_center(&bot, 10, TP_GRAY, "A: START   B: TITLE");
        scene_frame_end();
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
        if (cv_in.pressed & (KEY_LEFT | KEY_RIGHT))
        {
            pd_chapter += (cv_in.pressed & KEY_RIGHT) ? 1 : -1;
            if (pd_chapter < 1)
                pd_chapter = PD_CHAPTERS;
            if (pd_chapter > PD_CHAPTERS)
                pd_chapter = 1;
            pd_index = first_unsolved(pd_chapter);
            snd_sfx(CV_SFX_CLICK, 80, 64);
        }
        if (cv_in.pressed & KEY_UP)
            pd_index = pd_index > 1 ? pd_index - 1 : dev_chapter_size(pd_chapter);
        if (cv_in.pressed & KEY_DOWN)
            pd_index = pd_index < dev_chapter_size(pd_chapter) ? pd_index + 1 : 1;
        if (cv_in.pressed & KEY_A)
        {
            snd_sfx(CV_SFX_CHIME, 100, 64);
            return SC_PLAY;
        }
    }
}
