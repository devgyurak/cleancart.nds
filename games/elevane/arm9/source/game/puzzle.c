// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <string.h>

#include "assets.h"
#include "game/puzzle.h"

#define PZ_MAGIC 0x5A505645u   // 'EVPZ'

static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }

int pz_count(void)
{
    if (rd32(cv_puzzles) != PZ_MAGIC)
        return 0;
    return rd16(cv_puzzles + 6);
}

bool pz_get(int n, PzPuzzle *out)
{
    if (n < 0 || n >= pz_count())
        return false;
    const u8 *p = cv_puzzles + rd32(cv_puzzles + 8 + 4 * n);
    memset(out, 0, sizeof(*out));
    out->chapter = p[0];
    out->index = p[1];
    out->mode = p[2];
    out->nparts = p[3];
    out->reference = p[4] & 1;
    int nl = p[5];
    if (nl > PZ_NAME_MAX || out->nparts == 0 || out->nparts > PZ_MAX_PARTS)
        return false;
    memcpy(out->name, p + 6, nl);
    p += (6 + nl + 3) & ~3;
    for (int k = 0; k < out->nparts; k++)
    {
        PzPart *pt = &out->part[k];
        pt->w = p[0];
        pt->d = p[1];
        pt->h = p[2];
        pt->par = p[3];
        pt->front = p + 4;
        pt->plan = p + 4 + pt->w;
        pt->nsol = p[4 + pt->w + pt->d];
        pt->sol = p + 5 + pt->w + pt->d;
        p += (5 + pt->w + pt->d + 3 * pt->nsol + 3) & ~3;
    }
    return true;
}

int pz_find(int chapter, int index, PzPuzzle *out)
{
    for (int i = 0, n = pz_count(); i < n; i++)
    {
        PzPuzzle t;
        if (pz_get(i, &t) && t.chapter == chapter && t.index == index)
        {
            if (out)
                *out = t;
            return i;
        }
    }
    return -1;
}

int pz_chapter_size(int chapter)
{
    int c = 0;
    for (int i = 0, n = pz_count(); i < n; i++)
    {
        PzPuzzle t;
        if (pz_get(i, &t) && t.chapter == chapter)
            c++;
    }
    return c;
}

int pz_height(const PzGrid *g, int x, int y)
{
    int n = 0;
    for (int z = 0; z < PZ_MAX_DIM; z++)
        n += g->cell[y][x] >> z & 1;
    return n;
}

void pz_set_height(PzGrid *g, int x, int y, int h)
{
    g->cell[y][x] = h <= 0 ? 0 : (u8)((1u << h) - 1);
}

void pz_views(const PzPart *p, const PzGrid *g, u8 *front, u8 *plan)
{
    memset(front, 0, p->w);
    memset(plan, 0, p->d);
    for (int y = 0; y < p->d; y++)
        for (int x = 0; x < p->w; x++)
            if (g->cell[y][x])
            {
                front[x] |= g->cell[y][x];
                plan[y] |= 1 << x;
            }
}

int pz_front_cell(const PzPart *p, const PzGrid *g, int x, int z)
{
    bool have = false;
    for (int y = 0; y < p->d; y++)
        have |= g->cell[y][x] >> z & 1;
    bool want = p->front[x] >> z & 1;
    return have == want ? 0 : have ? 1 : 2;
}

int pz_plan_cell(const PzPart *p, const PzGrid *g, int x, int y)
{
    bool have = g->cell[y][x] != 0;
    bool want = p->plan[y] >> x & 1;
    return have == want ? 0 : have ? 1 : 2;
}

bool pz_matches(const PzPart *p, const PzGrid *g)
{
    u8 f[PZ_MAX_DIM], pl[PZ_MAX_DIM];
    pz_views(p, g, f, pl);
    return memcmp(f, p->front, p->w) == 0 && memcmp(pl, p->plan, p->d) == 0 && pz_supported(p, g);
}

int pz_blocks(const PzGrid *g, const PzPart *p)
{
    int n = 0;
    for (int y = 0; y < p->d; y++)
        for (int x = 0; x < p->w; x++)
            n += pz_height(g, x, y);
    return n;
}

// Flood fill from the floor layer through face-adjacent blocks; `reach` gets the cells reached.
static int flood(const PzPart *p, const PzGrid *g, PzGrid *reach)
{
    u8 stack[PZ_MAX_DIM * PZ_MAX_DIM * PZ_MAX_DIM][3];
    int sp = 0, reached = 0;
    memset(reach, 0, sizeof(*reach));
    for (int y = 0; y < p->d; y++)
        for (int x = 0; x < p->w; x++)
            if (g->cell[y][x] & 1)
            {
                reach->cell[y][x] |= 1;
                stack[sp][0] = x; stack[sp][1] = y; stack[sp][2] = 0; sp++;
            }
    while (sp)
    {
        sp--;
        int x = stack[sp][0], y = stack[sp][1], z = stack[sp][2];
        reached++;
        static const s8 nb[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
        for (int i = 0; i < 6; i++)
        {
            int nx = x + nb[i][0], ny = y + nb[i][1], nz = z + nb[i][2];
            if (nx < 0 || ny < 0 || nz < 0 || nx >= p->w || ny >= p->d || nz >= p->h)
                continue;
            if ((g->cell[ny][nx] >> nz & 1) && !(reach->cell[ny][nx] >> nz & 1))
            {
                reach->cell[ny][nx] |= 1 << nz;
                stack[sp][0] = nx; stack[sp][1] = ny; stack[sp][2] = nz; sp++;
            }
        }
    }
    return reached;
}

bool pz_supported(const PzPart *p, const PzGrid *g)
{
    PzGrid reach;
    return flood(p, g, &reach) == pz_blocks(g, p);
}

void pz_floating(const PzPart *p, const PzGrid *g, PzGrid *out)
{
    PzGrid reach;
    flood(p, g, &reach);
    memset(out, 0, sizeof(*out));
    for (int y = 0; y < p->d; y++)
        for (int x = 0; x < p->w; x++)
            out->cell[y][x] = g->cell[y][x] & ~reach.cell[y][x];
}

int pz_top(const PzGrid *g, int x, int y)
{
    int t = 0;
    for (int z = 0; z < PZ_MAX_DIM; z++)
        if (g->cell[y][x] >> z & 1)
            t = z + 1;
    return t;
}

int pz_stars(int blocks, int par)
{
    return blocks <= par ? 3 : blocks <= par + 2 ? 2 : 1;
}
