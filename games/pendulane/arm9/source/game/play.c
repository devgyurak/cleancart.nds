// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// One contraption: pick a station for every slot so the plan's ticks equal the pendulum's swing. The device is solved
// when they do; X runs the bench once for the chapter (the measured costs next to the book costs and a seal).

#include <nds.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "game/ledger.h"
#include "game/play.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

int pd_chapter = 1;
int pd_index = 1;
void (*pd_play_hook)(void);
const PdDevice *pd_cur_dev;
const PdPlan *pd_cur_plan;
int pd_cursor;
int pd_last_hits;

static void draw_ledger(CvText *bot, int chapter)
{
    text_clear(bot);
    text_printf(bot, 1, 1, TP_YELLOW, "BENCH LEDGER  %s", pd_chapter_name[chapter - 1]);
    if (!ledger_measured())
    {
        text_center(bot, 8, TP_GRAY, "X: RUN ON THE BENCH");
        text_center(bot, 10, TP_GRAY, "MEASURED COST NEXT TO THE BOOK");
        return;
    }
    int y = 3;
    for (int i = 0; i < ledger_row_count; i++)
    {
        const LedgerRow *r = &ledger_rows[i];
        if (r->chapter != chapter)
            continue;
        s32 v = ledger_value(r);
        if (v == INT32_MIN)
            text_printf(bot, 1, y, TP_RED, "%-17s  ---  BOOK %ld", r->label, (long)r->book);
        else
            text_printf(bot, 1, y, ledger_row_ok(r) ? TP_GREEN : TP_RED, "%-17s %5ld  BOOK %ld", r->label, (long)v, (long)r->book);
        y++;
    }
    text_center(bot, 20, ledger_chapter_gold(chapter) ? TP_YELLOW : TP_GRAY,
                ledger_chapter_gold(chapter) ? "GOLD SEAL" : "NO SEAL");
}

PdScene scene_play(void)
{
    CvText top, bot;
    int idx = dev_index(pd_chapter, pd_index);
    const PdDevice *d = dev_get(idx);
    PdPlan plan;
    plan_start(d, &plan);
    pd_cur_dev = d;
    pd_cur_plan = &plan;
    pd_cursor = 0;

    scene_text_screens(&top, &bot);
    draw_ledger(&bot, pd_chapter);
    fx_bright(FX_BOTH, -16);
    int msg_until = 0;
    bool advancing = false;
    const char *msg = "";
    for (int f = 0;; f++)
    {
        if (f < 20)
            fx_bright(FX_BOTH, -(16 * (19 - f) / 20));
        int total = plan_total(d, &plan);

        text_clear(&top);
        text_printf(&top, 1, 0, TP_CYAN, "CH %d  %s", pd_chapter, pd_chapter_name[pd_chapter - 1]);
        text_printf(&top, 1, 2, TP_YELLOW, "%s  %d/%d", d->name, pd_index, dev_chapter_size(pd_chapter));
        text_printf(&top, 1, 4, TP_WHITE, "PENDULUM %4d TICKS", d->target);
        text_printf(&top, 1, 5, total == d->target ? TP_GREEN : TP_WHITE, "PLAN     %4d TICKS", total);
        text_gauge(&top, 1, 6, 30, total > 2 * d->target ? 2 * d->target : total, 2 * d->target, total == d->target ? TP_GREEN : TP_GRAY);
        for (int s = 0; s < d->nslots; s++)
        {
            const PdStation *st = plan_station(d, &plan, s);
            text_printf(&top, 1, 9 + s * 2, s == pd_cursor ? TP_YELLOW : TP_WHITE, "%c %d %-12s %4d", s == pd_cursor ? '>' : ' ', s + 1, st->name, st->cost);
        }
        text_printf(&top, 1, 22, TP_GRAY, "PAR %d CHANGES  A: RING", d->par);
        if (f < msg_until)
            text_center(&top, 20, TP_YELLOW, msg);

        scene_frame_end();
        if (pd_play_hook)
            pd_play_hook();

        if (f < msg_until)
        {
            if (advancing && f + 1 == msg_until)
            {
                if (pd_index < dev_chapter_size(pd_chapter))
                {
                    pd_index++;
                    return SC_PLAY;
                }
                return SC_RHYTHM;
            }
            continue;
        }
        if (cv_in.pressed & KEY_B)
            return SC_SELECT;
        if (cv_in.pressed & KEY_UP)
            pd_cursor = (pd_cursor + d->nslots - 1) % d->nslots;
        if (cv_in.pressed & KEY_DOWN)
            pd_cursor = (pd_cursor + 1) % d->nslots;
        if (cv_in.pressed & KEY_RIGHT)
            plan.sel[pd_cursor] = (plan.sel[pd_cursor] + 1) % d->nopt[pd_cursor];
        if (cv_in.pressed & KEY_LEFT)
            plan.sel[pd_cursor] = (plan.sel[pd_cursor] + d->nopt[pd_cursor] - 1) % d->nopt[pd_cursor];
        if (cv_in.pressed & (KEY_LEFT | KEY_RIGHT))
            snd_sfx(CV_SFX_CLICK, 80, 64);
        if (cv_in.pressed & KEY_X)
        {
            text_center(&bot, 8, TP_YELLOW, "RUNNING THE BENCH...");
            scene_frame_end();
            ledger_measure();
            draw_ledger(&bot, pd_chapter);
        }
        if (cv_in.pressed & KEY_A)
        {
            if (plan_ok(d, &plan))
            {
                dev_mark_solved(idx);
                snd_sfx(CV_SFX_CHIME, 110, 64);
                msg = "THE BELL RINGS";
                msg_until = f + 50;
                advancing = true;
            }
            else
            {
                snd_sfx(CV_SFX_CLICK, 60, 64);
                msg = total > d->target ? "TOO MANY TICKS" : "TOO FEW TICKS";
                msg_until = f + 30;
            }
        }
    }
}
