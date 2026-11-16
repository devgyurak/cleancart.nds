// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "game/play.h"
#include "game/script.h"
#include "game/window.h"

// Every fourth frame is a key frame; the others release, so each action is a clean press.
static u32 tick;
static int last_chapter, last_index;

ScrPhase script_step(u16 *keys, bool advance)
{
    PlayView v;
    play_view(&v);
    *keys = 0;
    if (v.win.chapter != last_chapter || v.win.index != last_index)
    {
        last_chapter = v.win.chapter;
        last_index = v.win.index;
        tick = 0;
    }
    tick++;
    if (tick < 20 || (tick & 3) != 0)
        return v.baked ? SCR_DONE : SCR_BUILD;
    if (v.baked)
    {
        if (tick > 80)
            *keys = advance ? KEY_A : KEY_B;
        return SCR_DONE;
    }
    // The first wrong setting at or after the cursor, else the first one.
    int target = -1;
    for (int k = 0; k < PV_NFIELDS && target < 0; k++)
    {
        int f = (v.sel + k) % PV_NFIELDS;
        if (v.cur[f] != v.win.target[f])
            target = f;
    }
    if (target < 0)
    {
        *keys = KEY_A;                       // all settings right: fire
        return SCR_BUILD;
    }
    if (target != v.sel)
    {
        *keys = KEY_DOWN;
        return SCR_BUILD;
    }
    int n = pv_field_count[target];
    int d = (v.win.target[target] - v.cur[target] + n) % n;
    *keys = d <= n - d ? KEY_RIGHT : KEY_LEFT;
    return SCR_BUILD;
}
