// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>

#include "assets.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/save.h"
#include "core/snd.h"
#include "game/lamps.h"
#include "game/model.h"
#include "game/scenes.h"
#include "game/voyage.h"
#include "gfx/fx.h"
#include "gfx/gl3d.h"
#include "gfx/text.h"

int by_chapter = 1;
int by_approach = 0;
u32 by_cleared_bits;
void (*by_play_hook)(void);

static CvText top, bottom;
static int cursor, gate_no, mistakes, last_mistakes;
static ByGate gate_now;
static bool almanac_open;

// Progress lives in the shared save block: continue_stage 1 marks "there is progress", continue_score holds the
// cleared bits (Buoyane has no stage to continue, so the Corvane fields are free). Demo builds never touch the chip.
void by_progress_load(void)
{
    by_cleared_bits = cv_save.continue_stage ? (cv_save.continue_score & ((1u << (BY_CHAPTERS * BY_APPROACHES)) - 1)) : 0;
}

static void progress_save(void)
{
#ifndef CV_DEMO
    cv_save.continue_stage = 1;
    cv_save.continue_score = by_cleared_bits;
    qa_save_written(save_write());
#endif
}

int by_cleared_count(void)
{
    return __builtin_popcount(by_cleared_bits);
}

int by_chapter_cleared(int chapter)
{
    return __builtin_popcount((by_cleared_bits >> ((chapter - 1) * BY_APPROACHES)) & ((1u << BY_APPROACHES) - 1));
}

int by_gate_mistakes(void)
{
    return last_mistakes;
}

u16 by_pilot_keys(u32 f)
{
    if ((f & 7) != 4)
        return 0;
    int want = by_almanac_answer(by_chapter, gate_now);
    return cursor != want ? KEY_RIGHT : KEY_A;
}

static const char type_letter[BY_TYPES] = { 'A', 'B', 'C', 'D', 'E', 'F' };

static void draw_chart(void)
{
    text_clear(&bottom);
    text_printf(&bottom, 1, 1, TP_YELLOW, "CHART  %s", by_chapter_name[by_chapter - 1]);
    text_printf(&bottom, 1, 3, TP_WHITE, "GATE %d OF %d    VIEW %d", gate_no + 1, BY_GATES, gate_now.view + 1);
    text_printf(&bottom, 1, 5, TP_GRAY, "NAME THE LIGHT IN THE 3D VIEW:");
    for (int i = 0; i < 3; i++)
    {
        int x = 3 + i * 9;
        text_printf(&bottom, x, 8, i == cursor ? TP_YELLOW : TP_WHITE, "%c LIGHT %c", i == cursor ? '>' : ' ', type_letter[gate_now.opt[i]]);
        if (almanac_open)
            text_printf(&bottom, x, 10, TP_GRAY, "%s", by_look_word(by_chapter, by_look_of(by_chapter, gate_now.opt[i], gate_now.view)));
    }
    text_printf(&bottom, 1, 14, TP_GRAY, almanac_open ? "SELECT: HIDE THE ALMANAC" : "SELECT: OPEN THE ALMANAC");
    text_printf(&bottom, 1, 16, TP_GRAY, "LEFT RIGHT CHOOSE  A NAME IT");
    text_printf(&bottom, 1, 18, TP_GRAY, "B OR START: BACK TO THE CHART");
    if (mistakes)
        text_printf(&bottom, 1, 21, TP_RED, "WRONG ANSWERS %d", mistakes);
}

ByScene scene_voyage(void)
{
    display_3d_top(&top, &bottom);
    mistakes = 0;
    cursor = 0;
    almanac_open = false;
    fx_bright(FX_BOTH, 0);
    for (gate_no = 0; gate_no < BY_GATES;)
    {
        gate_now = by_gate(by_chapter, by_approach, gate_no);
        cursor = 0;
        text_clear(&top);
        text_printf(&top, 1, 0, TP_YELLOW, "%s", by_approach_name[by_chapter - 1][by_approach]);
        text_printf(&top, 1, 23, TP_GRAY, "VIEW %d  WHICH LIGHT IS IT?", gate_now.view + 1);
        bool done = false;
        while (!done)
        {
            lamps_draw(by_chapter, gate_now.truth, gate_now.view);
            g3_flush(false);
            draw_chart();
            scene_frame_end();
            if (by_play_hook)
                by_play_hook();
            if (cv_in.pressed & (KEY_B | KEY_START))
            {
                last_mistakes = mistakes;
                lamps_reset();
                return SC_CHART;
            }
            if (cv_in.pressed & KEY_SELECT)
                almanac_open = !almanac_open;
            if (cv_in.pressed & KEY_RIGHT)
                cursor = (cursor + 1) % 3;
            if (cv_in.pressed & KEY_LEFT)
                cursor = (cursor + 2) % 3;
            if (cv_in.pressed & KEY_A)
            {
                if (gate_now.opt[cursor] == gate_now.truth)
                {
                    snd_sfx(CV_SFX_CHIME, 100, 64);
                    done = true;
                }
                else
                {
                    snd_sfx(CV_SFX_CLICK, 100, 64);
                    mistakes++;
                }
            }
        }
        gate_no++;
    }
    by_cleared_bits |= 1u << ((by_chapter - 1) * BY_APPROACHES + by_approach);
    progress_save();
    last_mistakes = mistakes;
    lamps_reset();
    return SC_CHART;
}

// ---------------------------------------------------------------------------------------------
// The chart: five chapters, six approaches each.

ByScene scene_chart(void)
{
    CvText t, b;
    display_3d_top(&t, &b);
    lamps_reset();
    fx_bright(FX_BOTH, 0);
    for (;;)
    {
        text_clear(&t);
        text_center(&t, 1, TP_YELLOW, "THE CHART");
        for (int ch = 1; ch <= BY_CHAPTERS; ch++)
        {
            bool cur = ch == by_chapter;
            text_printf(&t, 2, 4 + ch * 2 - 1, cur ? TP_YELLOW : TP_WHITE, "%c %d %-10s %d/%d", cur ? '>' : ' ', ch,
                        by_chapter_name[ch - 1], by_chapter_cleared(ch), BY_APPROACHES);
        }
        text_printf(&t, 2, 19, TP_GRAY, "UP DOWN: CHAPTER  LEFT RIGHT: APPROACH");
        text_printf(&t, 2, 21, TP_GRAY, "A: SAIL  B: TITLE");
        text_clear(&b);
        text_printf(&b, 1, 1, TP_YELLOW, "%s", by_chapter_name[by_chapter - 1]);
        for (int ap = 0; ap < BY_APPROACHES; ap++)
        {
            bool cur = ap == by_approach;
            bool done = (by_cleared_bits >> ((by_chapter - 1) * BY_APPROACHES + ap)) & 1;
            text_printf(&b, 2, 4 + ap * 2, cur ? TP_YELLOW : TP_WHITE, "%c %s %s", cur ? '>' : ' ', by_approach_name[by_chapter - 1][ap], done ? "*" : "");
        }
        g3_flush(false);
        scene_frame_end();
        if (cv_in.pressed & KEY_UP)
            by_chapter = by_chapter > 1 ? by_chapter - 1 : BY_CHAPTERS;
        if (cv_in.pressed & KEY_DOWN)
            by_chapter = by_chapter < BY_CHAPTERS ? by_chapter + 1 : 1;
        if (cv_in.pressed & KEY_LEFT)
            by_approach = by_approach > 0 ? by_approach - 1 : BY_APPROACHES - 1;
        if (cv_in.pressed & KEY_RIGHT)
            by_approach = by_approach < BY_APPROACHES - 1 ? by_approach + 1 : 0;
        if (cv_in.pressed & KEY_A)
            return SC_VOYAGE;
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
