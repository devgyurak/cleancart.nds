// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include "assets.h"
#include "game/model.h"

#define HEADER_BYTES 16
#define GATE_BYTES 5

ByGate by_gate(int chapter, int approach, int gate)
{
    int i = ((chapter - 1) * BY_APPROACHES + approach) * BY_GATES + gate;
    const u8 *r = cv_routes + HEADER_BYTES + i * GATE_BYTES;
    ByGate g = { r[0], r[1], { r[2], r[3], r[4] } };
    return g;
}

int by_look_of(int chapter, int type, int view)
{
    return by_look[chapter - 1][type][view];
}

int by_almanac_answer(int chapter, ByGate g)
{
    int want = by_look_of(chapter, g.truth, g.view);
    for (int i = 0; i < 3; i++)
        if (by_look_of(chapter, g.opt[i], g.view) == want)
            return i;
    return 0;
}

u32 by_plan_hash(void)
{
    u32 h = 2166136261u;
    int n = HEADER_BYTES + BY_CHAPTERS * BY_APPROACHES * BY_GATES * GATE_BYTES;
    for (int i = 0; i < n; i++)
        h = (h ^ cv_routes[i]) * 16777619u;
    return h;
}
