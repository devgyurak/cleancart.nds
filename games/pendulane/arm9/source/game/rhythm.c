// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/input.h"
#include "core/snd.h"
#include "game/play.h"
#include "game/rhythm.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

#define FIRST_NOTE 100
#define SPACING    34
#define WINDOW     6
#define FALL_ROWS  16
#define FRAMES_PER_ROW 5

int rhythm_note_frame(int i)
{
    return FIRST_NOTE + i * SPACING;
}

PdScene scene_rhythm(void)
{
    CvText top, bot;
    scene_text_screens(&top, &bot);
    bool hit[RHYTHM_NOTES] = { false };
    int hits = 0;
    const char *last = "";
    fx_bright(FX_BOTH, -16);
    snd_bgm(CV_SONG_PENDULUM);
    int end = rhythm_note_frame(RHYTHM_NOTES - 1) + 90;
    for (int f = 0; f < end; f++)
    {
        if (f < 20)
            fx_bright(FX_BOTH, -(16 * (19 - f) / 20));
        text_clear(&top);
        text_printf(&top, 1, 0, TP_CYAN, "CH %d  %s  FINALE", pd_chapter, pd_chapter_name[pd_chapter - 1]);
        text_printf(&top, 1, 2, TP_GRAY, "TAP A WHEN A TICK HITS THE LINE");
        for (int i = 0; i < RHYTHM_NOTES; i++)
        {
            int ahead = rhythm_note_frame(i) - f;
            if (hit[i] || ahead < -WINDOW || ahead > FALL_ROWS * FRAMES_PER_ROW)
                continue;
            int row = 21 - ahead / FRAMES_PER_ROW;
            text_put(&top, 15, row < 4 ? 4 : row, TP_YELLOW, "*");
        }
        text_put(&top, 12, 21, TP_WHITE, "-------");
        text_printf(&bot, 1, 6, TP_WHITE, "HITS %d / %d", hits, RHYTHM_NOTES);
        text_center(&bot, 9, TP_GREEN, last);
        scene_frame_end();
        if (cv_in.pressed & KEY_A)
        {
            last = "MISS";
            for (int i = 0; i < RHYTHM_NOTES; i++)
            {
                int diff = f - rhythm_note_frame(i);
                if (!hit[i] && diff >= -WINDOW && diff <= WINDOW)
                {
                    hit[i] = true;
                    hits++;
                    last = diff == 0 ? "PERFECT" : "GOOD";
                    snd_sfx(CV_SFX_CHIME, 90, 64);
                    break;
                }
            }
        }
    }
    snd_bgm_stop();
    pd_last_hits = hits;
    return SC_SELECT;
}
