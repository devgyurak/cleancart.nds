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
#include "gfx/gl3d.h"
#include "gfx/loom.h"
#include "gfx/loom3d.h"
#include "judge/timing.h"
#include "gfx/text.h"

void (*hm_play_hook)(void);
static PlayView pv;
static bool lamp_done, lamp_ok;      // the motor lamp (run_motor())
static u32 lamp_measured, lamp_planned;
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
        text_printf(&bot_text, col_x[COL_PROGRAM], y + 1, TP_GRAY, "T%lu", (unsigned long)order_row_ticks(o, pv.cur, r));
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
        text_printf(&bot_text, 17, 17, lamp_done ? (lamp_ok ? TP_GREEN : TP_RED) : TP_GRAY, "MOTOR %s",
                    lamp_done ? (lamp_ok ? "OK" : "CHECK") : "--");
    }
    else
    {
        text_put(&bot_text, 1, 19, TP_GRAY, "UP/DOWN CELL  L/R TURN");
        text_put(&bot_text, 1, 21, TP_GRAY, "A DELIVER B LIST SEL VIEW");
    }
}

// The cloth on the top screen: four bands of thread woven by gfx/loom.c (raster interrupt, or the CPU-drawn blueprint
// with Select), the order name above and the chapter below.
static bool blueprint;
static u8 shown_colour[HM_ROWS], shown_weave[HM_ROWS];

static void sync_cloth(bool force)
{
    bool changed = force;
    for (int r = 0; r < HM_ROWS; r++)
    {
        u8 c = pv.cur[r * HM_COLS + COL_COLOUR], w = pv.cur[r * HM_COLS + COL_WEAVE];
        if (force || c != shown_colour[r] || w != shown_weave[r])
        {
            loom_row(r, c, w);
            shown_colour[r] = c;
            shown_weave[r] = w;
            changed = true;
        }
    }
    if (changed && !force)
        loom_refresh();
}

// The programs the rows run: the cell in the chapters that have the column, else the loom's own (the weave's).
static void row_programs(u8 *programs, u8 *dyed)
{
    for (int r = 0; r < HM_ROWS; r++)
    {
        programs[r] = pv.cur[r * HM_COLS + (order_col_active(pv.order.chapter, COL_PROGRAM) ? COL_PROGRAM : COL_WEAVE)];
        int dye = order_col_active(pv.order.chapter, COL_DYE) ? pv.cur[r * HM_COLS + COL_DYE] : 0;
        dyed[r] = dye >= 2 && dye <= 4;   // lines 192, 200, 213 (assets: hm_dye_line)
    }
}

// The motor runs when the cloth is delivered: the programs through the geometry engine, one lever kick per lever pulled.
// The lamp compares the measured cycles with the GBATEK costs; it is information only and never blocks the delivery.
static void run_motor(void)
{
    u8 programs[HM_ROWS], dyed[HM_ROWS];
    row_programs(programs, dyed);
    lamp_measured = timing_motor_run(programs, pv.order.passes, &lamp_planned);
    for (int i = order_levers(pv.cur); i > 0; i--)
        timing_lever_kick();
    lamp_ok = lamp_measured * 20 >= lamp_planned * 19 && lamp_measured * 10 <= lamp_planned * 11;
    lamp_done = true;
}

static void draw_cloth(void)
{
    u8 programs[HM_ROWS], dyed[HM_ROWS], colours[HM_ROWS];
    row_programs(programs, dyed);
    for (int r = 0; r < HM_ROWS; r++)
        colours[r] = pv.cur[r * HM_COLS + COL_COLOUR];
    loom3d_draw(colours, programs, dyed);
    g3_flush(false);
    text_clear(&top_text);
    const HmOrder *o = &pv.order;
    text_printf(&top_text, 1, 0, TP_YELLOW, "%s", o->name);
    text_printf(&top_text, 1, 1, TP_GRAY, "CH %d %s", o->chapter, hm_chapter_name[o->chapter - 1]);
    text_put(&top_text, 20, 1, TP_GRAY, blueprint ? "BLUEPRINT" : "RASTER");
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
    display_3d_top(&top_text, &bot_text);
    fx_bright(FX_BOTH, 0);
    lamp_done = false;
    loom_begin(16, HM_ROWS, 40);
    sync_cloth(true);
    loom_show(blueprint);
    for (;;)
    {
        sync_cloth(false);
        draw_cloth();
        draw_table();
        scene_frame_end();
        if (hm_play_hook)
            hm_play_hook();
        apply_input();
        if (cv_in.pressed & KEY_SELECT)
        {
            blueprint = !blueprint;
            loom_show(blueprint);
        }
        if (cv_in.pressed & KEY_A)
        {
            if (pv.done)
            {
                snd_sfx(CV_SFX_CLICK, 80, 64);
                if (hm_index < order_chapter_size(hm_chapter))
                {
                    hm_index++;
                    load_order();
                    lamp_done = false;
                    loom_begin(16, HM_ROWS, 40);
                    sync_cloth(true);
                    loom_show(blueprint);
                }
                else
                    return SC_SELECT;
            }
            else if (order_solved(&pv.order, pv.cur))
            {
                pv.done = true;
                run_motor();
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
