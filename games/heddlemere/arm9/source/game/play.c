// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "core/input.h"
#include "core/snd.h"
#include "core/sys.h"
#include "game/order.h"
#include "game/play.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

void (*hm_play_hook)(void);
static PlayView pv;
static CvText top_text, bot_text;

void play_view(PlayView *v)
{
    *v = pv;
}

int play_stars(int moves, int par)
{
    return moves <= par ? 3 : moves <= par + 3 ? 2 : 1;
}

int play_cells(int chapter, u8 *out)
{
    int n = 0;
    for (int r = 0; r < HM_ROWS; r++)
        for (int c = 0; c < HM_COLS; c++)
            if (order_col_active(chapter, c))
                out[n++] = (u8)(r * HM_COLS + c);
    return n;
}

static bool load_order(void)
{
    memset(&pv, 0, sizeof(pv));
    return order_find(hm_chapter, hm_index, &pv.order) >= 0;
}

// Column x positions on the bottom screen (8 px cells).
static const u8 col_x[HM_COLS] = { 3, 10, 18, 25, 28 };

static void draw_table(void)
{
    text_clear(&bot_text);
    const HmOrder *o = &pv.order;
    text_printf(&bot_text, 1, 0, TP_YELLOW, "%d-%02d %s", o->chapter, o->index, o->name);
    for (int c = 0; c < HM_COLS; c++)
        if (order_col_active(o->chapter, c))
            text_put(&bot_text, col_x[c], 2, TP_GRAY, hm_col_title[c]);
    u8 cells[HM_NFIELDS];
    int n = play_cells(o->chapter, cells);
    for (int r = 0; r < HM_ROWS; r++)
    {
        int y = 4 + r * 3;
        bool ok = order_row_done(o, pv.cur, r);
        text_printf(&bot_text, 0, y, ok ? TP_GREEN : TP_GRAY, "%d", r + 1);
        // The order's own values in gray under the cells that matter.
        text_put(&bot_text, col_x[COL_COLOUR], y + 1, TP_GRAY, hm_col_value[COL_COLOUR][o->colour[r]]);
        text_put(&bot_text, col_x[COL_WEAVE], y + 1, TP_GRAY, hm_col_value[COL_WEAVE][o->weave[r]]);
        if (o->dye[r])
            text_put(&bot_text, col_x[COL_DYE], y + 1, TP_GRAY, "QUIET");
        for (int c = 0; c < HM_COLS; c++)
        {
            if (!order_col_active(o->chapter, c))
                continue;
            bool here = pv.sel < n && cells[pv.sel] == r * HM_COLS + c;
            text_put(&bot_text, col_x[c], y, here ? TP_YELLOW : TP_WHITE, hm_col_value[c][pv.cur[r * HM_COLS + c]]);
        }
    }
    text_printf(&bot_text, 1, 17, TP_WHITE, "STEPS %d  PAR %d", pv.moves, o->par);
    if (pv.done)
    {
        text_printf(&bot_text, 1, 19, TP_GREEN, "DELIVERED  %.*s", play_stars(pv.moves, o->par), "***");
        text_put(&bot_text, 1, 21, TP_GRAY, "A: NEXT  B: LIST");
    }
    else
    {
        text_put(&bot_text, 1, 19, TP_GRAY, "UP/DOWN CELL  L/R TURN");
        text_put(&bot_text, 1, 21, TP_GRAY, "A: DELIVER  B: LIST");
    }
}

// The cloth on the top screen: one band per row (the raster weave replaces this, see AGENTS.md).
static void draw_cloth(void)
{
    text_clear(&top_text);
    const HmOrder *o = &pv.order;
    text_printf(&top_text, 1, 0, TP_YELLOW, "%s", o->name);
    text_printf(&top_text, 1, 22, TP_GRAY, "CHAPTER %d  %s", o->chapter, hm_chapter_name[o->chapter - 1]);
    for (int r = 0; r < HM_ROWS; r++)
    {
        text_printf(&top_text, 2, 3 + r * 4, TP_WHITE, "ROW %d  %s %s", r + 1, hm_col_value[COL_COLOUR][pv.cur[r * HM_COLS + COL_COLOUR]],
                    hm_col_value[COL_WEAVE][pv.cur[r * HM_COLS + COL_WEAVE]]);
        text_printf(&top_text, 2, 4 + r * 4, TP_GRAY, "TICKS %lu", (unsigned long)order_row_ticks(o, pv.cur, r));
    }
}

static void apply_input(void)
{
    if (pv.done)
        return;
    u8 cells[HM_NFIELDS];
    int n = play_cells(pv.order.chapter, cells);
    if (cv_in.repeat & KEY_DOWN)
        pv.sel = (pv.sel + 1) % n;
    if (cv_in.repeat & KEY_UP)
        pv.sel = (pv.sel + n - 1) % n;
    int step = (cv_in.repeat & KEY_RIGHT) ? 1 : (cv_in.repeat & KEY_LEFT) ? -1 : 0;
    if (step)
    {
        int f = cells[pv.sel];
        int cnt = hm_col_count[f % HM_COLS];
        pv.cur[f] = (u8)((pv.cur[f] + step + cnt) % cnt);
        pv.moves++;
        snd_sfx(CV_SFX_CLICK, 80, 64);
    }
}

HmScene scene_play(void)
{
    if (!load_order())
        return SC_SELECT;
    scene_text_screens(&top_text, &bot_text);
    fx_bright(FX_BOTH, 0);
    for (;;)
    {
        draw_cloth();
        draw_table();
        scene_frame_end();
        if (hm_play_hook)
            hm_play_hook();
        apply_input();
        if (cv_in.pressed & KEY_A)
        {
            if (pv.done)
            {
                snd_sfx(CV_SFX_CLICK, 80, 64);
                if (hm_index < order_chapter_size(hm_chapter))
                {
                    hm_index++;
                    load_order();
                }
                else
                    return SC_SELECT;
            }
            else if (order_solved(&pv.order, pv.cur))
            {
                pv.done = true;
                hm_done_bits |= 1ull << order_find(pv.order.chapter, pv.order.index, NULL);
                snd_sfx(CV_SFX_CHIME, 100, 64);
            }
            else
                snd_sfx(CV_SFX_CLICK, 40, 64);
        }
        if (cv_in.pressed & KEY_B)
            return SC_SELECT;
    }
}
