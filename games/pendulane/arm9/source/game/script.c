// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "game/device.h"
#include "game/play.h"
#include "game/script.h"

void script_step(u16 *keys, bool qa)
{
    static u32 tick;
    (void)qa;
    *keys = 0;
    if ((++tick & 3) != 0 || !pd_cur_dev || !pd_cur_plan)
        return;
    const PdDevice *d = pd_cur_dev;
    PdPlan want;
    if (!dev_solve(d, pd_cur_plan, &want))
        return;
    int slot = -1;
    for (int s = 0; s < d->nslots; s++)
        if (pd_cur_plan->sel[s] != want.sel[s])
        {
            slot = s;
            break;
        }
    if (slot < 0)
    {
        *keys = KEY_A;
        return;
    }
    if (pd_cursor < slot)
        *keys = KEY_DOWN;
    else if (pd_cursor > slot)
        *keys = KEY_UP;
    else
    {
        int n = d->nopt[slot];
        int diff = (want.sel[slot] - pd_cur_plan->sel[slot] + n) % n;
        *keys = diff <= n / 2 ? KEY_RIGHT : KEY_LEFT;
    }
}
