// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <string.h>

#include "game/window.h"

int pv_chapter = 1, pv_index = 1;
bool pv_view3d = true;
u32 pv_baked_bits;

int win_baked_count(void)
{
    return __builtin_popcount(pv_baked_bits);
}

#define ENTRY 32
#define HEADER 8

static u32 rd32(const u8 *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

int win_count(void)
{
    return (int)rd32(cv_windows + 4);
}

int win_chapter_size(int chapter)
{
    int n = 0;
    for (int i = 0; i < win_count(); i++)
        if (cv_windows[HEADER + i * ENTRY] == chapter)
            n++;
    return n;
}

int win_find(int chapter, int index, PvWindow *out)
{
    for (int i = 0; i < win_count(); i++)
    {
        const u8 *e = cv_windows + HEADER + i * ENTRY;
        if (e[0] == chapter && e[1] == index)
        {
            if (out)
            {
                out->chapter = e[0];
                out->index = e[1];
                out->par = e[2];
                memcpy(out->target, e + 4, PV_NFIELDS);
                memcpy(out->name, e + 16, 16);
                out->name[15] = 0;
            }
            return i;
        }
    }
    return -1;
}

int win_dist(int field, int a, int b)
{
    int n = pv_field_count[field];
    int d = ((b - a) % n + n) % n;
    return d < n - d ? d : n - d;
}

int win_remaining(const PvWindow *w, const u8 *cur)
{
    int sum = 0;
    for (int f = 0; f < PV_NFIELDS; f++)
        sum += win_dist(f, cur[f], w->target[f]);
    return sum;
}

bool win_was_baked(int chapter, int index)
{
    int n = win_find(chapter, index, NULL);
    return n >= 0 && (pv_baked_bits >> n & 1);
}
