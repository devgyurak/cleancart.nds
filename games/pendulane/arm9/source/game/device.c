// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include "game/device.h"

static bool solved[PD_NUM_DEVICES];

int dev_count(void)
{
    return PD_NUM_DEVICES;
}

const PdDevice *dev_get(int index)
{
    return &pd_devices[index];
}

int dev_chapter_size(int chapter)
{
    int n = 0;
    for (int i = 0; i < PD_NUM_DEVICES; i++)
        n += pd_devices[i].chapter == chapter;
    return n;
}

int dev_first_of_chapter(int chapter)
{
    for (int i = 0; i < PD_NUM_DEVICES; i++)
        if (pd_devices[i].chapter == chapter)
            return i;
    return -1;
}

int dev_index(int chapter, int n)
{
    int first = dev_first_of_chapter(chapter);
    if (first < 0 || n < 1 || n > dev_chapter_size(chapter))
        return -1;
    return first + n - 1;
}

void plan_start(const PdDevice *d, PdPlan *p)
{
    (void)d;
    for (int i = 0; i < PD_MAXSLOTS; i++)
        p->sel[i] = 0;
}

const PdStation *plan_station(const PdDevice *d, const PdPlan *p, int slot)
{
    return &pd_stations[d->opt[slot][p->sel[slot]]];
}

int plan_total(const PdDevice *d, const PdPlan *p)
{
    int t = 0;
    for (int s = 0; s < d->nslots; s++)
        t += plan_station(d, p, s)->cost;
    return t;
}

bool plan_ok(const PdDevice *d, const PdPlan *p)
{
    return plan_total(d, p) == d->target;
}

bool dev_solve(const PdDevice *d, const PdPlan *from, PdPlan *to)
{
    // Depth-first over at most 4^6 plans; keeps the one with the fewest slots changed from `from`.
    int best = 99;
    PdPlan cur = *from;
    bool found = false;
    int n = d->nslots;
    u32 total = 1;
    for (int s = 0; s < n; s++)
        total *= d->nopt[s];
    for (u32 code = 0; code < total; code++)
    {
        u32 c = code;
        int changes = 0;
        for (int s = 0; s < n; s++)
        {
            cur.sel[s] = (u8)(c % d->nopt[s]);
            c /= d->nopt[s];
            changes += cur.sel[s] != from->sel[s];
        }
        if (plan_ok(d, &cur) && changes < best)
        {
            best = changes;
            *to = cur;
            found = true;
        }
    }
    return found;
}

void dev_solved_clear(void)
{
    for (int i = 0; i < PD_NUM_DEVICES; i++)
        solved[i] = false;
}

void dev_mark_solved(int index)
{
    if (index >= 0 && index < PD_NUM_DEVICES)
        solved[index] = true;
}

bool dev_is_solved(int index)
{
    return index >= 0 && index < PD_NUM_DEVICES && solved[index];
}

int dev_solved_count(void)
{
    int n = 0;
    for (int i = 0; i < PD_NUM_DEVICES; i++)
        n += solved[i];
    return n;
}

int dev_chapter_solved(int chapter)
{
    int n = 0;
    for (int i = 0; i < PD_NUM_DEVICES; i++)
        n += solved[i] && pd_devices[i].chapter == chapter;
    return n;
}

bool dev_chapter_done(int chapter)
{
    return dev_chapter_solved(chapter) == dev_chapter_size(chapter);
}
