// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <string.h>

#include "game/order.h"

int hm_chapter = 1, hm_index = 1;
u64 hm_done_bits;

#define ENTRY 40
#define HEADER 8

static u32 rd32(const u8 *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

int order_count(void)
{
    return (int)rd32(cv_orders + 4);
}

int order_chapter_size(int chapter)
{
    int n = 0;
    for (int i = 0; i < order_count(); i++)
        if (cv_orders[HEADER + i * ENTRY] == chapter)
            n++;
    return n;
}

int order_find(int chapter, int index, HmOrder *out)
{
    for (int i = 0; i < order_count(); i++)
    {
        const u8 *e = cv_orders + HEADER + i * ENTRY;
        if (e[0] == chapter && e[1] == index)
        {
            if (out)
            {
                out->chapter = e[0];
                out->index = e[1];
                out->budget = e[2];
                out->passes = e[3];
                out->slack = e[4];
                out->par = (u16)(e[6] | e[7] << 8);
                memcpy(out->colour, e + 8, HM_ROWS);
                memcpy(out->weave, e + 12, HM_ROWS);
                memcpy(out->dye, e + 16, HM_ROWS);
                memcpy(out->name, e + 24, 16);
                out->name[15] = 0;
            }
            return i;
        }
    }
    return -1;
}

int order_dist(int col, int a, int b)
{
    int n = hm_col_count[col];
    int d = ((b - a) % n + n) % n;
    return d < n - d ? d : n - d;
}

bool order_col_active(int chapter, int col)
{
    return chapter >= 1 && chapter <= HM_CHAPTERS && (hm_active_cols[chapter] >> col & 1);
}

u32 order_ticks(int program, int passes)
{
    return (u32)hm_program_ticks[program] * (u32)passes;
}

bool order_valid(int weave, int program, int lever, int budget, int passes)
{
    return program >= weave
           && order_ticks(program, passes) <= (u32)(budget + HM_LEVER_LINES * lever) * HM_LINE_CYCLES;
}

bool order_lever_needed(int weave, int budget, int passes)
{
    return !order_valid(weave, weave, 0, budget, passes);
}

bool order_dye_valid(int v)
{
    return hm_dye_line[v] >= HM_DYE_FIRST && hm_dye_line[v] <= HM_DYE_LAST;
}

HmPlan order_plan(const HmOrder *o, int r)
{
    HmPlan p = { 0, 0, 0, 0 };
    int w = o->weave[r];
    int steps = order_dist(COL_COLOUR, 0, o->colour[r]) + order_dist(COL_WEAVE, 0, w);
    p.lever = order_lever_needed(w, o->budget, o->passes);
    p.program = (u8)w;
    if (order_col_active(o->chapter, COL_PROGRAM))
    {
        int best = -1, bd = 99;
        for (int m = 0; m < hm_col_count[COL_PROGRAM]; m++)
            if (order_valid(w, m, p.lever, o->budget, o->passes) && order_dist(COL_PROGRAM, 0, m) < bd)
            {
                bd = order_dist(COL_PROGRAM, 0, m);
                best = m;
            }
        if (best >= 0)
        {
            steps += bd;
            p.program = (u8)best;
        }
    }
    steps += p.lever;
    if (o->dye[r])
    {
        int best = -1, bd = 99;
        for (int v = 0; v < hm_col_count[COL_DYE]; v++)
            if (order_dye_valid(v) && order_dist(COL_DYE, 0, v) < bd)
            {
                bd = order_dist(COL_DYE, 0, v);
                best = v;
            }
        p.dye = (u8)best;
        steps += bd;
    }
    p.steps = (u8)steps;
    return p;
}

int order_par(const HmOrder *o)
{
    int sum = 0;
    for (int r = 0; r < HM_ROWS; r++)
        sum += order_plan(o, r).steps;
    return sum;
}

static u32 fnv_word(u32 h, u32 w)
{
    for (int k = 0; k < 4; k++)
        h = (h ^ (w >> (8 * k) & 255)) * 0x01000193u;
    return h;
}

u32 order_plan_hash(void)
{
    u32 h = 0x811C9DC5u;
    for (int ch = 1; ch <= HM_CHAPTERS; ch++)
    {
        HmOrder o;
        if (order_find(ch, 1, &o) < 0)
            continue;
        for (int r = 0; r < HM_ROWS; r++)
            h = fnv_word(h, order_ticks(order_plan(&o, r).program, o.passes));
    }
    return h;
}

static int cell(const u8 *cur, int r, int c)
{
    return cur[r * HM_COLS + c];
}

bool order_row_done(const HmOrder *o, const u8 *cur, int r)
{
    int w = o->weave[r];
    if (cell(cur, r, COL_COLOUR) != o->colour[r] || cell(cur, r, COL_WEAVE) != w)
        return false;
    bool lever_on = order_col_active(o->chapter, COL_LEVER) && cell(cur, r, COL_LEVER);
    if (lever_on && !order_lever_needed(w, o->budget, o->passes))
        return false;   // a lever the row does not need only wastes a use
    int program = order_col_active(o->chapter, COL_PROGRAM) ? cell(cur, r, COL_PROGRAM) : w;
    if (!order_valid(w, program, lever_on, o->budget, o->passes))
        return false;
    int dye = order_col_active(o->chapter, COL_DYE) ? cell(cur, r, COL_DYE) : 0;
    return o->dye[r] ? order_dye_valid(dye) : dye == 0;
}

int order_levers(const u8 *cur)
{
    int n = 0;
    for (int r = 0; r < HM_ROWS; r++)
        n += cell(cur, r, COL_LEVER) != 0;
    return n;
}

bool order_solved(const HmOrder *o, const u8 *cur)
{
    for (int r = 0; r < HM_ROWS; r++)
        if (!order_row_done(o, cur, r))
            return false;
    return order_levers(cur) <= o->slack;
}

u32 order_row_ticks(const HmOrder *o, const u8 *cur, int r)
{
    int program = order_col_active(o->chapter, COL_PROGRAM) ? cell(cur, r, COL_PROGRAM) : cell(cur, r, COL_WEAVE);
    return order_ticks(program, o->passes);
}

int order_done_count(void)
{
    return __builtin_popcountll(hm_done_bits);
}

bool order_was_done(int chapter, int index)
{
    int n = order_find(chapter, index, NULL);
    return n >= 0 && (hm_done_bits >> n & 1);
}
