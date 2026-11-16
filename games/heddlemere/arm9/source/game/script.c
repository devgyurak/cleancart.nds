// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "game/order.h"
#include "game/play.h"
#include "game/script.h"

// Every fourth frame is a key frame; the others release, so each action is a clean press.
static u32 tick;
static int last_chapter, last_index;

// The value the plan wants in a flat cell.
static int plan_value(const HmOrder *o, int f)
{
    int r = f / HM_COLS;
    HmPlan p = order_plan(o, r);
    switch (f % HM_COLS)
    {
        case COL_COLOUR:  return o->colour[r];
        case COL_WEAVE:   return o->weave[r];
        case COL_PROGRAM: return p.program;
        case COL_LEVER:   return p.lever;
        default:          return p.dye;
    }
}

ScrPhase script_step(u16 *keys, bool advance)
{
    PlayView v;
    play_view(&v);
    *keys = 0;
    if (v.order.chapter != last_chapter || v.order.index != last_index)
    {
        last_chapter = v.order.chapter;
        last_index = v.order.index;
        tick = 0;
    }
    tick++;
    if (tick < 20 || (tick & 3) != 0)
        return v.done ? SCR_DONE : SCR_BUILD;
    if (v.done)
    {
        if (tick > 80)
            *keys = advance ? KEY_A : KEY_B;
        return SCR_DONE;
    }
    u8 cells[HM_NFIELDS];
    int n = play_cells(v.order.chapter, cells);
    // The first cell at or after the cursor that is not at the planned value, else deliver.
    int target = -1;
    for (int k = 0; k < n && target < 0; k++)
    {
        int i = (v.sel + k) % n;
        if (v.cur[cells[i]] != plan_value(&v.order, cells[i]))
            target = i;
    }
    if (target < 0)
    {
        *keys = KEY_A;
        return SCR_BUILD;
    }
    if (target != v.sel)
    {
        *keys = KEY_DOWN;
        return SCR_BUILD;
    }
    int f = cells[target];
    int cnt = hm_col_count[f % HM_COLS];
    int d = (plan_value(&v.order, f) - v.cur[f] + cnt) % cnt;
    *keys = d <= cnt - d ? KEY_RIGHT : KEY_LEFT;
    return SCR_BUILD;
}
