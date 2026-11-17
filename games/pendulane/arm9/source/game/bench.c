// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The bench ledger: runs the whole bench (once per session) and prints every measured cost next to the book value
// (GBATEK), with a seal per chapter. This is a look at the measurement, not a game; the verdict is judge.py's.

#include <nds.h>
#include <limits.h>

#include "assets.h"
#include "core/input.h"
#include "game/device.h"
#include "game/ledger.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

PdScene scene_bench(void)
{
    CvText top, bot;
    scene_text_screens(&top, &bot);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, "BENCH LEDGER");
    text_center(&top, 3, TP_GRAY, "MEASURED NEXT TO THE BOOK");
    if (!ledger_measured())
    {
        text_center(&bot, 8, TP_YELLOW, "RUNNING THE BENCH...");
        text_center(&bot, 10, TP_GRAY, "ABOUT A SECOND");
        scene_frame_end();
        ledger_measure();
        text_clear(&bot);
    }
    int y = 4;
    int chapter = 0;
    for (int i = 0; i < ledger_row_count; i++)
    {
        const LedgerRow *r = &ledger_rows[i];
        if (r->chapter != chapter)
        {
            chapter = r->chapter;
            y++;
        }
        s32 v = ledger_value(r);
        if (v == INT32_MIN)
            text_printf(&top, 1, y, TP_RED, "%-13s   --- BOOK %ld", r->label, (long)r->book);
        else
            text_printf(&top, 1, y, ledger_row_ok(r) ? TP_GREEN : TP_RED, "%-13s %5ld BOOK %ld", r->label, (long)v, (long)r->book);
        y++;
    }
    for (int ch = 1; ch <= PD_CHAPTERS; ch++)
        text_printf(&bot, 2, 3 + ch, ledger_chapter_gold(ch) ? TP_YELLOW : TP_GRAY, "%-10s %s", pd_chapter_name[ch - 1],
                    ledger_chapter_gold(ch) ? "GOLD SEAL" : "NO SEAL");
    text_center(&bot, 12, TP_GRAY, "GREEN = INSIDE THE JUDGE WINDOW");
    text_center(&bot, 20, TP_GRAY, "B: BACK");
    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
