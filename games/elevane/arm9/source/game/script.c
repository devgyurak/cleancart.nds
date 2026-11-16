// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <string.h>

#include "game/play.h"
#include "game/puzzle.h"
#include "game/script.h"

static unsigned seen_serial = ~0u;
static ScrPhase phase;
static int tick, wait;

// The next cell the stored solution has and the pile does not, in the order the game
// takes it: part, layer, row, column (stack puzzles: column by column, raised from the top).
static bool next_goal(int *part, int *x, int *y, int *z)
{
    for (int k = 0; k < play_nparts(); k++)
    {
        const PzPart *p = play_part_n(k);
        const PzGrid *g = play_grid_n(k);
        if (play_mode() == 0)
        {
            int target[PZ_MAX_DIM][PZ_MAX_DIM];
            memset(target, 0, sizeof(target));
            for (int i = 0; i < p->nsol; i++)
                target[p->sol[3 * i + 1]][p->sol[3 * i]]++;
            for (int yy = 0; yy < p->d; yy++)
                for (int xx = 0; xx < p->w; xx++)
                    if (pz_height(g, xx, yy) < target[yy][xx])
                    {
                        *part = k; *x = xx; *y = yy; *z = -1;
                        return true;
                    }
        }
        else
        {
            for (int zz = 0; zz < p->h; zz++)
                for (int yy = 0; yy < p->d; yy++)
                    for (int xx = 0; xx < p->w; xx++)
                    {
                        bool want = false;
                        for (int i = 0; i < p->nsol; i++)
                            want |= p->sol[3 * i] == xx && p->sol[3 * i + 1] == yy && p->sol[3 * i + 2] == zz;
                        if (want && !(g->cell[yy][xx] >> zz & 1))
                        {
                            *part = k; *x = xx; *y = yy; *z = zz;
                            return true;
                        }
                    }
        }
    }
    return false;
}

ScrPhase script_step(u16 *keys, bool advance)
{
    *keys = 0;
    if (play_serial() != seen_serial)
    {
        seen_serial = play_serial();
        phase = SCR_BUILD;
        tick = wait = 0;
    }
    if (++tick % 3)
        return phase;
    switch (phase)
    {
        case SCR_BUILD:
        {
            int k, x, y, z;
            if (!next_goal(&k, &x, &y, &z))
            {
                phase = SCR_WAIT;
                wait = 0;
                break;
            }
            if (k != play_part_index())
            {
                *keys = KEY_Y;   // a Y tap: the next part
                break;
            }
            int cx, cy;
            play_cursor(&cx, &cy);
            if (z >= 0 && play_layer() < z)
                *keys = KEY_Y | KEY_UP;
            else if (z >= 0 && play_layer() > z)
                *keys = KEY_Y | KEY_DOWN;
            else if (cx < x)  *keys = KEY_RIGHT;
            else if (cx > x)  *keys = KEY_LEFT;
            else if (cy < y)  *keys = KEY_UP;
            else if (cy > y)  *keys = KEY_DOWN;
            else              *keys = KEY_A;
            break;
        }
        case SCR_WAIT:
            // Look around once (R then L back to the front), let a few front/plan pairs be
            // captured, then deliver.
            wait++;
            if (wait == 5)
                *keys = KEY_R;
            else if (wait == 15)
                *keys = KEY_L;
            else if (wait > 40)
            {
                if (play_matched())
                    *keys = KEY_A;
                phase = SCR_DELIVER;
                wait = 0;
            }
            break;
        case SCR_DELIVER:
            if (++wait > 20)
            {
                *keys = advance ? KEY_A : KEY_START;
                phase = SCR_DONE;
            }
            break;
        default:
            break;
    }
    return phase;
}
